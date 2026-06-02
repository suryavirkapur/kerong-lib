#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "kerong/kerong_client.hpp"

namespace py = pybind11;

PYBIND11_MODULE(kerong, m) {
    m.doc() = "Kerong NCU16L electronic locker control library";

    py::register_exception<kerong::KerongError>(m, "KerongError", PyExc_RuntimeError);
    py::register_exception<kerong::KerongTimeoutError>(m, "KerongTimeoutError", m.attr("KerongError"));
    py::register_exception<kerong::KerongChecksumError>(m, "KerongChecksumError", m.attr("KerongError"));
    py::register_exception<kerong::KerongInvalidFrameError>(m, "KerongInvalidFrameError", m.attr("KerongError"));
    py::register_exception<kerong::KerongConnectionError>(m, "KerongConnectionError", m.attr("KerongError"));
    py::register_exception<kerong::KerongArgumentError>(m, "KerongArgumentError", m.attr("KerongError"));

    py::class_<kerong::BoardState>(m, "BoardState")
        .def(py::init<>())
        .def_readwrite("board_address", &kerong::BoardState::board_address,
                       "Address of the board that returned the state (0-15)")
        .def_readwrite("locks_1_8", &kerong::BoardState::locks_1_8,
                       "Bitmask: bit i = lock (i+1) state (0=locked, 1=unlocked)")
        .def_readwrite("locks_9_16", &kerong::BoardState::locks_9_16,
                       "Bitmask: bit i = lock (i+9) state (0=locked, 1=unlocked)")
        .def_readwrite("ir_1_8", &kerong::BoardState::ir_1_8,
                       "Bitmask: bit i = IR sensor for lock (i+1) (0=empty, 1=item)")
        .def_readwrite("ir_9_16", &kerong::BoardState::ir_9_16,
                       "Bitmask: bit i = IR sensor for lock (i+9) (0=empty, 1=item)")
        .def("is_locked", &kerong::BoardState::is_locked,
             py::arg("index"),
             "Return True if the lock at zero-based `index` (0-15) is currently locked")
        .def("is_unlocked", [](const kerong::BoardState& s, int index) {
            return !s.is_locked(index);
        }, py::arg("index"), "Return True if the lock at zero-based `index` is unlocked")
        .def("is_item_detected", &kerong::BoardState::is_item_detected,
             py::arg("index"),
             "Return True if the IR sensor at zero-based `index` (0-15) detects an item")
        .def("__repr__", [](const kerong::BoardState& s) {
            return "BoardState(board=" + std::to_string(s.board_address) +
                   ", locks_1_8=0x" + [&] {
                       std::string out;
                       const char* hex = "0123456789abcdef";
                       out += hex[(s.locks_1_8 >> 4) & 0x0F];
                       out += hex[s.locks_1_8 & 0x0F];
                       return out;
                   }() +
                   ", locks_9_16=0x" + [&] {
                       std::string out;
                       const char* hex = "0123456789abcdef";
                       out += hex[(s.locks_9_16 >> 4) & 0x0F];
                       out += hex[s.locks_9_16 & 0x0F];
                       return out;
                   }() +
                   ", ir_1_8=0x" + [&] {
                       std::string out;
                       const char* hex = "0123456789abcdef";
                       out += hex[(s.ir_1_8 >> 4) & 0x0F];
                       out += hex[s.ir_1_8 & 0x0F];
                       return out;
                   }() +
                   ", ir_9_16=0x" + [&] {
                       std::string out;
                       const char* hex = "0123456789abcdef";
                       out += hex[(s.ir_9_16 >> 4) & 0x0F];
                       out += hex[s.ir_9_16 & 0x0F];
                       return out;
                   }() + ")";
        });

    py::class_<kerong::KerongClient>(m, "KerongClient")
        .def(py::init<>())
        .def("connect", &kerong::KerongClient::connect,
             py::arg("ip_address"),
             py::arg("port"),
             py::arg("timeout_ms") = 1000,
             "Open a TCP connection to the ZNE-100TL base unit. `timeout_ms` is "
             "applied to send, receive and connect operations.")
        .def("disconnect", &kerong::KerongClient::disconnect,
             "Close the underlying TCP socket.")
        .def("unlock", &kerong::KerongClient::unlock,
             py::arg("board"), py::arg("lock"),
             "Send an unlock command to `board` (0-15) at `lock` index (0-15). "
             "No response is expected from the board.")
        .def("get_state", &kerong::KerongClient::get_state,
             py::arg("board"),
             "Query the lock + IR state of `board` (0-15) and return a BoardState.")
        .def("is_connected", &kerong::KerongClient::is_connected,
             "Return True if the client currently holds a live socket.");
}
