#include <napi.h>

#include <cstdint>
#include <string>
#include <utility>

#include "kerong/kerong_client.hpp"

namespace {

kerong::BoardState JsObjectToBoardState(const Napi::Object& obj) {
    kerong::BoardState s;
    s.board_address = static_cast<uint8_t>(obj.Get("boardAddress").ToNumber().Uint32Value());
    s.locks_1_8 = static_cast<uint8_t>(obj.Get("locks_1_8").ToNumber().Uint32Value());
    s.locks_9_16 = static_cast<uint8_t>(obj.Get("locks_9_16").ToNumber().Uint32Value());
    s.ir_1_8 = static_cast<uint8_t>(obj.Get("ir_1_8").ToNumber().Uint32Value());
    s.ir_9_16 = static_cast<uint8_t>(obj.Get("ir_9_16").ToNumber().Uint32Value());
    return s;
}

Napi::Value BoardStateToJs(Napi::Env env, const kerong::BoardState& s) {
    Napi::Object obj = Napi::Object::New(env);
    obj.Set("boardAddress", Napi::Number::New(env, s.board_address));
    obj.Set("locks_1_8", Napi::Number::New(env, s.locks_1_8));
    obj.Set("locks_9_16", Napi::Number::New(env, s.locks_9_16));
    obj.Set("ir_1_8", Napi::Number::New(env, s.ir_1_8));
    obj.Set("ir_9_16", Napi::Number::New(env, s.ir_9_16));
    obj.Set("isLocked", Napi::Function::New(env, [s](const Napi::CallbackInfo& info) -> Napi::Value {
        Napi::Env env = info.Env();
        if (info.Length() < 1 || !info[0].IsNumber()) {
            Napi::TypeError::New(env, "isLocked(index: number)").ThrowAsJavaScriptException();
            return env.Undefined();
        }
        return Napi::Boolean::New(env, s.is_locked(info[0].As<Napi::Number>().Int32Value()));
    }));
    obj.Set("isUnlocked", Napi::Function::New(env, [s](const Napi::CallbackInfo& info) -> Napi::Value {
        Napi::Env env = info.Env();
        if (info.Length() < 1 || !info[0].IsNumber()) {
            Napi::TypeError::New(env, "isUnlocked(index: number)").ThrowAsJavaScriptException();
            return env.Undefined();
        }
        return Napi::Boolean::New(env, !s.is_locked(info[0].As<Napi::Number>().Int32Value()));
    }));
    obj.Set("isItemDetected", Napi::Function::New(env, [s](const Napi::CallbackInfo& info) -> Napi::Value {
        Napi::Env env = info.Env();
        if (info.Length() < 1 || !info[0].IsNumber()) {
            Napi::TypeError::New(env, "isItemDetected(index: number)").ThrowAsJavaScriptException();
            return env.Undefined();
        }
        return Napi::Boolean::New(env, s.is_item_detected(info[0].As<Napi::Number>().Int32Value()));
    }));
    return obj;
}

void RejectWithKind(Napi::Promise::Deferred& d, const char* kind, const char* msg) {
    Napi::Env env = d.Env();
    Napi::Error err = Napi::Error::New(env, msg);
    err.Set("name", kind);
    d.Reject(err.Value());
}

class ConnectWorker : public Napi::AsyncWorker {
public:
    ConnectWorker(kerong::KerongClient* client,
                  Napi::Promise::Deferred deferred,
                  std::string ip,
                  int32_t port,
                  int32_t timeout_ms)
        : Napi::AsyncWorker(deferred.Env()),
          client_(client),
          deferred_(deferred),
          ip_(std::move(ip)),
          port_(port),
          timeout_ms_(timeout_ms) {}

    void Execute() override {
        try {
            client_->connect(ip_, port_, timeout_ms_);
        } catch (const kerong::KerongTimeoutError& e) {
            kind_ = "KerongTimeoutError";
            message_ = e.what();
        } catch (const kerong::KerongConnectionError& e) {
            kind_ = "KerongConnectionError";
            message_ = e.what();
        } catch (const kerong::KerongArgumentError& e) {
            kind_ = "KerongArgumentError";
            message_ = e.what();
        } catch (const kerong::KerongError& e) {
            kind_ = "KerongError";
            message_ = e.what();
        } catch (const std::exception& e) {
            kind_ = "Error";
            message_ = e.what();
        }
    }

    void OnOK() override {
        deferred_.Resolve(Env().Undefined());
    }

    void OnError(const Napi::Error&) override {
        if (!kind_.empty()) {
            RejectWithKind(deferred_, kind_.c_str(), message_.c_str());
            return;
        }
    }

private:
    kerong::KerongClient* client_;
    Napi::Promise::Deferred deferred_;
    std::string ip_;
    int32_t port_;
    int32_t timeout_ms_;
    std::string kind_;
    std::string message_;
};

class UnlockWorker : public Napi::AsyncWorker {
public:
    UnlockWorker(kerong::KerongClient* client,
                 Napi::Promise::Deferred deferred,
                 uint8_t board,
                 uint8_t lock)
        : Napi::AsyncWorker(deferred.Env()),
          client_(client),
          deferred_(deferred),
          board_(board),
          lock_(lock) {}

    void Execute() override {
        try {
            client_->unlock(board_, lock_);
        } catch (const kerong::KerongTimeoutError& e) {
            kind_ = "KerongTimeoutError";
            message_ = e.what();
        } catch (const kerong::KerongConnectionError& e) {
            kind_ = "KerongConnectionError";
            message_ = e.what();
        } catch (const kerong::KerongArgumentError& e) {
            kind_ = "KerongArgumentError";
            message_ = e.what();
        } catch (const kerong::KerongError& e) {
            kind_ = "KerongError";
            message_ = e.what();
        } catch (const std::exception& e) {
            kind_ = "Error";
            message_ = e.what();
        }
    }

    void OnOK() override {
        deferred_.Resolve(Env().Undefined());
    }

    void OnError(const Napi::Error&) override {
        if (!kind_.empty()) {
            RejectWithKind(deferred_, kind_.c_str(), message_.c_str());
            return;
        }
    }

private:
    kerong::KerongClient* client_;
    Napi::Promise::Deferred deferred_;
    uint8_t board_;
    uint8_t lock_;
    std::string kind_;
    std::string message_;
};

class GetStateWorker : public Napi::AsyncWorker {
public:
    GetStateWorker(kerong::KerongClient* client, Napi::Promise::Deferred deferred, uint8_t board)
        : Napi::AsyncWorker(deferred.Env()),
          client_(client),
          deferred_(deferred),
          board_(board) {}

    void Execute() override {
        try {
            state_ = client_->get_state(board_);
            has_state_ = true;
        } catch (const kerong::KerongTimeoutError& e) {
            kind_ = "KerongTimeoutError";
            message_ = e.what();
        } catch (const kerong::KerongChecksumError& e) {
            kind_ = "KerongChecksumError";
            message_ = e.what();
        } catch (const kerong::KerongInvalidFrameError& e) {
            kind_ = "KerongInvalidFrameError";
            message_ = e.what();
        } catch (const kerong::KerongConnectionError& e) {
            kind_ = "KerongConnectionError";
            message_ = e.what();
        } catch (const kerong::KerongArgumentError& e) {
            kind_ = "KerongArgumentError";
            message_ = e.what();
        } catch (const kerong::KerongError& e) {
            kind_ = "KerongError";
            message_ = e.what();
        } catch (const std::exception& e) {
            kind_ = "Error";
            message_ = e.what();
        }
    }

    void OnOK() override {
        if (has_state_) {
            deferred_.Resolve(BoardStateToJs(Env(), state_));
        } else {
            RejectWithKind(deferred_, "KerongError", "get_state returned no state");
        }
    }

    void OnError(const Napi::Error&) override {
        if (!kind_.empty()) {
            RejectWithKind(deferred_, kind_.c_str(), message_.c_str());
            return;
        }
    }

private:
    kerong::KerongClient* client_;
    Napi::Promise::Deferred deferred_;
    uint8_t board_;
    kerong::BoardState state_{};
    bool has_state_ = false;
    std::string kind_;
    std::string message_;
};

class KerongClientWrap : public Napi::ObjectWrap<KerongClientWrap> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    static Napi::FunctionReference constructor;

    KerongClientWrap(const Napi::CallbackInfo& info)
        : Napi::ObjectWrap<KerongClientWrap>(info),
          client_(new kerong::KerongClient()) {}

    ~KerongClientWrap() override {
        delete client_;
    }

    Napi::Value JsConnect(const Napi::CallbackInfo& info) {
        Napi::Env env = info.Env();
        if (info.Length() < 2 || !info[0].IsString() || !info[1].IsNumber()) {
            Napi::TypeError::New(env, "connect(ip: string, port: number, timeoutMs?: number)")
                .ThrowAsJavaScriptException();
            return env.Undefined();
        }
        std::string ip = info[0].As<Napi::String>().Utf8Value();
        int64_t port = info[1].As<Napi::Number>().Int64Value();
        if (port <= 0 || port > 65535) {
            Napi::Error err = Napi::Error::New(env, "port out of range");
            err.Set("name", "KerongArgumentError");
            err.ThrowAsJavaScriptException();
            return env.Undefined();
        }
        int32_t timeout_ms = 1000;
        if (info.Length() >= 3 && info[2].IsNumber()) {
            timeout_ms = info[2].As<Napi::Number>().Int32Value();
        }
        Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
        auto* worker = new ConnectWorker(client_, deferred, ip, static_cast<int32_t>(port), timeout_ms);
        worker->Queue();
        return deferred.Promise();
    }

    Napi::Value JsDisconnect(const Napi::CallbackInfo& info) {
        Napi::Env env = info.Env();
        try {
            client_->disconnect();
        } catch (const std::exception& e) {
            Napi::Error err = Napi::Error::New(env, e.what());
            err.Set("name", "KerongError");
            err.ThrowAsJavaScriptException();
            return env.Undefined();
        }
        return env.Undefined();
    }

    Napi::Value JsUnlock(const Napi::CallbackInfo& info) {
        Napi::Env env = info.Env();
        if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
            Napi::TypeError::New(env, "unlock(board: number, lock: number)")
                .ThrowAsJavaScriptException();
            return env.Undefined();
        }
        int64_t board_raw = info[0].As<Napi::Number>().Int64Value();
        int64_t lock_raw = info[1].As<Napi::Number>().Int64Value();
        if (board_raw < 0 || board_raw > 255 || lock_raw < 0 || lock_raw > 255) {
            Napi::Error err = Napi::Error::New(env, "board/lock out of uint8 range");
            err.Set("name", "KerongArgumentError");
            err.ThrowAsJavaScriptException();
            return env.Undefined();
        }
        Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
        auto* worker = new UnlockWorker(client_, deferred,
            static_cast<uint8_t>(board_raw), static_cast<uint8_t>(lock_raw));
        worker->Queue();
        return deferred.Promise();
    }

    Napi::Value JsGetState(const Napi::CallbackInfo& info) {
        Napi::Env env = info.Env();
        if (info.Length() < 1 || !info[0].IsNumber()) {
            Napi::TypeError::New(env, "getState(board: number)")
                .ThrowAsJavaScriptException();
            return env.Undefined();
        }
        int64_t board_raw = info[0].As<Napi::Number>().Int64Value();
        if (board_raw < 0 || board_raw > 255) {
            Napi::Error err = Napi::Error::New(env, "board out of uint8 range");
            err.Set("name", "KerongArgumentError");
            err.ThrowAsJavaScriptException();
            return env.Undefined();
        }
        Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
        auto* worker = new GetStateWorker(client_, deferred, static_cast<uint8_t>(board_raw));
        worker->Queue();
        return deferred.Promise();
    }

    Napi::Value JsIsConnected(const Napi::CallbackInfo& info) {
        Napi::Env env = info.Env();
        return Napi::Boolean::New(env, client_->is_connected());
    }

private:
    kerong::KerongClient* client_;
};

Napi::FunctionReference KerongClientWrap::constructor;

Napi::Object KerongClientWrap::Init(Napi::Env env, Napi::Object exports) {
    Napi::HandleScope scope(env);

    Napi::Function ctor = DefineClass(env, "KerongClient", {
        InstanceMethod("connect", &KerongClientWrap::JsConnect),
        InstanceMethod("disconnect", &KerongClientWrap::JsDisconnect),
        InstanceMethod("unlock", &KerongClientWrap::JsUnlock),
        InstanceMethod("getState", &KerongClientWrap::JsGetState),
        InstanceMethod("isConnected", &KerongClientWrap::JsIsConnected),
    });

    constructor = Napi::Persistent(ctor);
    constructor.SuppressDestruct();

    exports.Set("KerongClient", ctor);

    exports.Set("boardStateToObject", Napi::Function::New(
        env,
        [](const Napi::CallbackInfo& info) -> Napi::Value {
            Napi::Env env = info.Env();
            if (info.Length() < 1 || !info[0].IsObject()) {
                Napi::TypeError::New(env, "boardStateToObject(state: BoardState)")
                    .ThrowAsJavaScriptException();
                return env.Undefined();
            }
            return BoardStateToJs(env, JsObjectToBoardState(info[0].As<Napi::Object>()));
        },
        "boardStateToObject"
    ));

    return exports;
}

}

NODE_API_NAMED_ADDON(kerong, KerongClientWrap)
