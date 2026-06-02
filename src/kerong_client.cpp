#include "kerong/kerong_client.hpp"

#include <cstring>
#include <sstream>

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/time.h>
#endif

namespace kerong {

namespace {

class WsaGuard {
public:
    WsaGuard() {
#ifdef _WIN32
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    }
    ~WsaGuard() {
#ifdef _WIN32
        WSACleanup();
#endif
    }
};

bool is_timeout_error() {
#ifdef _WIN32
    int err = WSAGetLastError();
    return err == WSAETIMEDOUT || err == WSAEWOULDBLOCK;
#else
    return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS;
#endif
}

bool is_disconnect_error() {
#ifdef _WIN32
    int err = WSAGetLastError();
    return err == WSAECONNRESET || err == WSAECONNABORTED || err == WSAESHUTDOWN ||
           err == WSAENOTCONN || err == WSAENETRESET;
#else
    return errno == ECONNRESET || errno == EPIPE || errno == ENOTCONN;
#endif
}

bool socket_is_valid(socket_t fd) {
#ifdef _WIN32
    return fd != INVALID_SOCKET;
#else
    return fd >= 0;
#endif
}

void close_raw(socket_t fd) {
    if (!socket_is_valid(fd)) {
        return;
    }
#ifdef _WIN32
    closesocket(fd);
#else
    close(fd);
#endif
}

void set_socket_timeouts(socket_t fd, int timeout_ms) {
#ifdef _WIN32
    DWORD tv = static_cast<DWORD>(timeout_ms);
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&tv), sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO,
               reinterpret_cast<const char*>(&tv), sizeof(tv));
#else
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
}

}

BoardState::BoardState()
    : board_address(0), locks_1_8(0), locks_9_16(0), ir_1_8(0), ir_9_16(0) {}

BoardState::BoardState(uint8_t board, uint8_t l1_8, uint8_t l9_16, uint8_t i1_8, uint8_t i9_16)
    : board_address(board), locks_1_8(l1_8), locks_9_16(l9_16), ir_1_8(i1_8), ir_9_16(i9_16) {}

bool BoardState::is_locked(int index) const {
    if (index < 0 || index > 15) {
        return false;
    }
    if (index < 8) {
        return ((locks_1_8 >> index) & 0x01) == 0;
    }
    return ((locks_9_16 >> (index - 8)) & 0x01) == 0;
}

bool BoardState::is_item_detected(int index) const {
    if (index < 0 || index > 15) {
        return false;
    }
    if (index < 8) {
        return ((ir_1_8 >> index) & 0x01) != 0;
    }
    return ((ir_9_16 >> (index - 8)) & 0x01) != 0;
}

KerongClient::KerongClient()
    :
#ifdef _WIN32
      sock_fd_(INVALID_SOCKET),
#else
      sock_fd_(-1),
#endif
      timeout_ms_(1000) {
    static WsaGuard wsa_guard;
    (void)wsa_guard;
}

KerongClient::~KerongClient() {
    std::lock_guard<std::mutex> lock(mutex_);
    close_socket();
}

void KerongClient::close_socket() {
    if (socket_is_valid(sock_fd_)) {
        close_raw(sock_fd_);
    }
#ifdef _WIN32
    sock_fd_ = INVALID_SOCKET;
#else
    sock_fd_ = -1;
#endif
}

void KerongClient::ensure_connected() const {
    if (!socket_is_valid(sock_fd_)) {
        throw KerongConnectionError("Not connected");
    }
}

bool KerongClient::is_connected() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return socket_is_valid(sock_fd_);
}

uint8_t KerongClient::calc_sum(const std::vector<uint8_t>& bytes) {
    unsigned int total = 0;
    for (uint8_t b : bytes) {
        total += b;
    }
    return static_cast<uint8_t>(total & 0xFF);
}

uint8_t KerongClient::make_unlock_addr(uint8_t board, uint8_t lock_index) {
    return static_cast<uint8_t>(((board & 0x0F) << 4) | (lock_index & 0x0F));
}

uint8_t KerongClient::make_query_addr(uint8_t board) {
    return static_cast<uint8_t>((board & 0x0F) << 4);
}

void KerongClient::validate_index(uint8_t board, uint8_t lock_index) {
    std::ostringstream oss;
    if (board > 0x0F) {
        oss << "board address out of range (0-15): " << static_cast<int>(board);
        throw KerongArgumentError(oss.str());
    }
    if (lock_index > 0x0F) {
        oss << "lock index out of range (0-15): " << static_cast<int>(lock_index);
        throw KerongArgumentError(oss.str());
    }
}

void KerongClient::validate_board(uint8_t board) {
    if (board > 0x0F) {
        std::ostringstream oss;
        oss << "board address out of range (0-15): " << static_cast<int>(board);
        throw KerongArgumentError(oss.str());
    }
}

void KerongClient::connect(const std::string& ip_address, int port, int timeout_ms) {
    if (port <= 0 || port > 65535) {
        throw KerongArgumentError("port out of range");
    }
    if (timeout_ms < 0) {
        throw KerongArgumentError("timeout must be non-negative");
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (socket_is_valid(sock_fd_)) {
        throw KerongConnectionError("already connected");
    }

    socket_t fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (!socket_is_valid(fd)) {
        throw KerongConnectionError("failed to create socket");
    }

    set_socket_timeouts(fd, timeout_ms);

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));

    int rc = inet_pton(AF_INET, ip_address.c_str(), &addr.sin_addr);
    if (rc != 1) {
        close_raw(fd);
        throw KerongArgumentError("invalid IPv4 address: " + ip_address);
    }

#ifdef _WIN32
    u_long non_blocking = 1;
    ioctlsocket(fd, FIONBIO, &non_blocking);
#else
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif

    int connect_rc = ::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
#ifdef _WIN32
    bool in_progress = (connect_rc == SOCKET_ERROR) && (WSAGetLastError() == WSAEWOULDBLOCK);
#else
    bool in_progress = (connect_rc < 0) && (errno == EINPROGRESS);
#endif

    if (in_progress) {
        fd_set wfds;
        FD_ZERO(&wfds);
        FD_SET(fd, &wfds);
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;

        int sel = ::select(static_cast<int>(fd) + 1, nullptr, &wfds, nullptr, &tv);
        if (sel == 0) {
            close_raw(fd);
            throw KerongTimeoutError("connect timed out");
        }
        if (sel < 0) {
            int err = 0;
#ifdef _WIN32
            err = WSAGetLastError();
#else
            err = errno;
#endif
            close_raw(fd);
            std::ostringstream oss;
            oss << "select() failed during connect (errno=" << err << ")";
            throw KerongConnectionError(oss.str());
        }

        int so_err = 0;
#ifdef _WIN32
        int slen = sizeof(so_err);
        getsockopt(fd, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&so_err), &slen);
#else
        socklen_t slen = sizeof(so_err);
        getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_err, &slen);
#endif
        if (so_err != 0) {
            close_raw(fd);
            std::ostringstream oss;
            oss << "connect failed (errno=" << so_err << ")";
            throw KerongConnectionError(oss.str());
        }
    } else if (connect_rc < 0) {
        int err = 0;
#ifdef _WIN32
        err = WSAGetLastError();
#else
        err = errno;
#endif
        close_raw(fd);
        std::ostringstream oss;
        oss << "connect failed (errno=" << err << ")";
        throw KerongConnectionError(oss.str());
    }

#ifdef _WIN32
    u_long blocking = 0;
    ioctlsocket(fd, FIONBIO, &blocking);
#else
    fcntl(fd, F_SETFL, flags);
#endif

    set_socket_timeouts(fd, timeout_ms);
    sock_fd_ = fd;
    timeout_ms_ = timeout_ms;
}

void KerongClient::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    close_socket();
}

void KerongClient::send_all(const std::vector<uint8_t>& data) {
    size_t total = 0;
    while (total < data.size()) {
        int sent = ::send(sock_fd_,
                          reinterpret_cast<const char*>(data.data()) + total,
                          static_cast<int>(data.size() - total),
                          0);
        if (sent < 0) {
            if (is_timeout_error()) {
                throw KerongTimeoutError("send timed out");
            }
            if (is_disconnect_error()) {
                close_socket();
                throw KerongConnectionError("connection dropped during send");
            }
            throw KerongConnectionError("send failed");
        }
        if (sent == 0) {
            close_socket();
            throw KerongConnectionError("peer closed during send");
        }
        total += static_cast<size_t>(sent);
    }
}

std::vector<uint8_t> KerongClient::recv_exact(size_t n) {
    std::vector<uint8_t> buffer(n);
    size_t total = 0;
    while (total < n) {
        int r = ::recv(sock_fd_,
                       reinterpret_cast<char*>(buffer.data()) + total,
                       static_cast<int>(n - total),
                       0);
        if (r < 0) {
            if (is_timeout_error()) {
                throw KerongTimeoutError("receive timed out");
            }
            if (is_disconnect_error()) {
                close_socket();
                throw KerongConnectionError("connection dropped during receive");
            }
            throw KerongConnectionError("recv failed");
        }
        if (r == 0) {
            close_socket();
            throw KerongConnectionError("peer closed before all bytes received");
        }
        total += static_cast<size_t>(r);
    }
    return buffer;
}

void KerongClient::unlock(uint8_t board, uint8_t lock_index) {
    validate_index(board, lock_index);

    std::lock_guard<std::mutex> guard(mutex_);
    ensure_connected();

    std::vector<uint8_t> frame = {
        STX,
        make_unlock_addr(board, lock_index),
        CMD_UNLOCK,
        ETX
    };
    frame.push_back(calc_sum(frame));
    send_all(frame);
}

BoardState KerongClient::get_state(uint8_t board) {
    validate_board(board);

    std::lock_guard<std::mutex> lock(mutex_);
    ensure_connected();

    std::vector<uint8_t> frame = {
        STX,
        make_query_addr(board),
        CMD_GET_STATE,
        ETX
    };
    frame.push_back(calc_sum(frame));
    send_all(frame);

    auto resp = recv_exact(9);

    if (resp[0] != STX) {
        throw KerongInvalidFrameError("missing STX in response");
    }
    if (resp[1] != make_query_addr(board)) {
        throw KerongInvalidFrameError("response address does not match request");
    }
    if (resp[2] != RESP_STATE) {
        std::ostringstream oss;
        oss << "unexpected response command: 0x"
            << std::hex << static_cast<int>(resp[2]);
        throw KerongInvalidFrameError(oss.str());
    }
    if (resp[7] != ETX) {
        throw KerongInvalidFrameError("missing ETX in response");
    }

    std::vector<uint8_t> sum_input(resp.begin(), resp.begin() + 8);
    uint8_t expected = calc_sum(sum_input);
    if (expected != resp[8]) {
        std::ostringstream oss;
        oss << "checksum mismatch: expected 0x"
            << std::hex << static_cast<int>(expected)
            << " got 0x" << static_cast<int>(resp[8]);
        throw KerongChecksumError(oss.str());
    }

    return BoardState(resp[1], resp[3], resp[4], resp[5], resp[6]);
}

}
