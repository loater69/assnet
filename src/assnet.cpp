#include "assnet.h"

#pragma comment(lib, "Ws2_32.lib")

assnet::stream::stream(const std::string& addr, uint16_t port) : sock{INVALID_SOCKET} {
    addrinfo *result = nullptr,
                *ptr = nullptr,
                hints;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    auto p = std::to_string(port);

    int iResult = getaddrinfo(addr.c_str(), p.c_str(), &hints, &result);
    if (iResult != 0) {
        std::cout << "getaddrinfo failed: " << iResult << '\n';
        return;
    }

    ptr = result;

    sock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

    if (sock == INVALID_SOCKET) {
        std::cout << "error creating socket: " << WSAGetLastError() << '\n';
        return;
    }

    iResult = connect(sock, ptr->ai_addr, (int)ptr->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        closesocket(sock);
        sock = INVALID_SOCKET;
    }

    freeaddrinfo(result);

    if (sock == INVALID_SOCKET) {
        std::cout << "Unable to connect to server!\n";
        return;
    }

    u_long v = true;
    if (ioctlsocket(sock, FIONBIO, &v) == SOCKET_ERROR) {
        std::cout << "ioctlsocket: " << WSAGetLastError() << '\n';
        return;
    }
}

void assnet::stream::flush() {
    send(sock, out.data(), ow, 0);
    ow = 0;
}

void assnet::stream::put_n(char* p, size_t n) {
    for (char* ptr = p; ptr != &p[n]; ++ptr) put(*ptr);
}

void assnet::stream::put(char c) {
    if (ow >= 128) flush();
    out[ow] = c;
    ++ow;
}

bool assnet::stream::read_op::await_suspend(std::coroutine_handle<nettask::promise_type> handle) {
    handle.promise().ro = this;
    return !make_progress();
}

bool assnet::nettask::try_progress() {
    if (handle.promise().ro) {
        if (handle.promise().ro->make_progress()) handle.resume();
        /* else if (!handle.promise().ro->str->is_open()) {
            handle.resume(); // one last time incase 
            return false;
        } */

        return true;
    }

    return false;
}

assnet::acceptor::acceptor(uint16_t port) : sock{INVALID_SOCKET} {
    struct addrinfo *result = nullptr;
    struct addrinfo hints;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    std::string p = std::to_string(port);

    int iResult = getaddrinfo(NULL, p.c_str(), &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        return;
    }

    sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sock == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        return;
    }

    u_long v = true;
    if (ioctlsocket(sock, FIONBIO, &v) == SOCKET_ERROR) {
        std::cout << "ioctlsocket: " << WSAGetLastError() << '\n';
        freeaddrinfo(result);
        return;
    }

    iResult = bind(sock, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(sock);
        return;
    }

    freeaddrinfo(result);

    if (listen(sock, SOMAXCONN) == SOCKET_ERROR) {
        std::cout << "fuck: " << WSAGetLastError() << '\n';
        closesocket(sock);
        return;
    }
}

std::optional<assnet::stream> assnet::acceptor::try_accept() {
    auto s = accept(sock, nullptr, nullptr);

    if (s == INVALID_SOCKET) {
        auto err = WSAGetLastError();

        if (err == WSAEWOULDBLOCK) return {};

        std::cout << "fuck me: " << err << '\n';
        closesocket(sock);
        sock = INVALID_SOCKET;
        return {};
    }

    return std::make_optional(s);
}
