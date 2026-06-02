#pragma once

#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_t = SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_t = int;
#endif

namespace kerong {

constexpr uint8_t STX = 0x02;
constexpr uint8_t ETX = 0x03;
constexpr uint8_t CMD_UNLOCK = 0x31;
constexpr uint8_t CMD_GET_STATE = 0x30;
constexpr uint8_t RESP_STATE = 0x35;

struct BoardState {
    uint8_t board_address;
    uint8_t locks_1_8;
    uint8_t locks_9_16;
    uint8_t ir_1_8;
    uint8_t ir_9_16;

    BoardState();
    BoardState(uint8_t board,
               uint8_t l1_8,
               uint8_t l9_16,
               uint8_t i1_8,
               uint8_t i9_16);

    bool is_locked(int index) const;
    bool is_item_detected(int index) const;
};

class KerongError : public std::runtime_error {
public:
    explicit KerongError(const std::string& msg) : std::runtime_error(msg) {}
};

class KerongTimeoutError : public KerongError {
public:
    explicit KerongTimeoutError(const std::string& msg) : KerongError(msg) {}
};

class KerongChecksumError : public KerongError {
public:
    explicit KerongChecksumError(const std::string& msg) : KerongError(msg) {}
};

class KerongInvalidFrameError : public KerongError {
public:
    explicit KerongInvalidFrameError(const std::string& msg) : KerongError(msg) {}
};

class KerongConnectionError : public KerongError {
public:
    explicit KerongConnectionError(const std::string& msg) : KerongError(msg) {}
};

class KerongArgumentError : public KerongError {
public:
    explicit KerongArgumentError(const std::string& msg) : KerongError(msg) {}
};

class KerongClient {
public:
    KerongClient();
    ~KerongClient();

    KerongClient(const KerongClient&) = delete;
    KerongClient& operator=(const KerongClient&) = delete;
    KerongClient(KerongClient&&) = delete;
    KerongClient& operator=(KerongClient&&) = delete;

    void connect(const std::string& ip_address, int port, int timeout_ms);
    void disconnect();
    void unlock(uint8_t board, uint8_t lock_index);
    BoardState get_state(uint8_t board);

    bool is_connected() const;

private:
    socket_t sock_fd_;
    int timeout_ms_;
    mutable std::mutex mutex_;

    void send_all(const std::vector<uint8_t>& data);
    std::vector<uint8_t> recv_exact(size_t n);
    void close_socket();
    void ensure_connected() const;

    static uint8_t calc_sum(const std::vector<uint8_t>& bytes);
    static uint8_t make_unlock_addr(uint8_t board, uint8_t lock_index);
    static uint8_t make_query_addr(uint8_t board);
    static void validate_index(uint8_t board, uint8_t lock_index);
    static void validate_board(uint8_t board);
};

}
