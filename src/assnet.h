#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <coroutine>
#include <string>
#include <iostream>
#include <array>
#include <concepts>
#include <vector>
#include <thread>
#include <optional>

namespace assnet {
    class stream {
        SOCKET sock;
        std::array<char, 128> out;
        std::array<char, 128> in;

        size_t ow = 0;
        size_t ir = 0;
        size_t iw = 0;
    public:
        stream(const std::string& addr, uint16_t port);
        inline stream(SOCKET so) : sock{so} {}
        inline stream(stream&& s) : sock{s.sock} { s.sock = INVALID_SOCKET; }

        void flush();
        void put_n(char* p, size_t n);
        void put(char c);

        stream& operator << (std::integral auto v) {
            put_n((char*)&v, sizeof(v));

            return *this;
        }

        struct read_op;

        read_op operator >> (std::integral auto& v);

        inline ~stream() {
            closesocket(sock);
        }
    };

    struct nettask {
        struct promise_type {
            using handle = std::coroutine_handle<promise_type>;

            nettask get_return_object() { return nettask{handle::from_promise(*this)}; }
            std::suspend_never initial_suspend() { return {}; }
            std::suspend_never final_suspend() noexcept { return {}; }
            void return_void() {
                ro = nullptr;
                std::cout << "done\n";
            }
            void unhandled_exception() {}
            stream::read_op* ro;
        };

        promise_type::handle handle;

        bool try_progress();
        ~nettask() {}
    };

    struct stream::read_op {
        stream* str;

        struct read_instr {
            char* p;
            size_t n;
        };

        size_t i = 0;

        std::vector<read_instr> instr;

        bool await_ready() { return false; }
        bool await_suspend(std::coroutine_handle<nettask::promise_type> handle);
        void await_resume() {}

        bool make_progress() {
            while (str->ir != str->iw && i < instr.size()) {
                *(instr[i].p++) = str->in[str->ir++];
                str->ir &= 0b1111111;
                instr[i].n--;

                if (instr[i].n == 0) ++i;
            }

            if (i == instr.size()) return true;

            char buffer[128];

            size_t size = (128 - str->iw) + str->ir;

            int iResult = recv(str->sock, buffer, size, 0);
            if (WSAGetLastError() == WSAEWOULDBLOCK) return false;

            if (iResult < 0) {
                std::cout << WSAGetLastError() << '\n';
                std::cout << "fuck\n";
                closesocket(str->sock);
                return false;
            }

            size_t j;
            for (j = 0; j < iResult && i < instr.size(); ++j) {
                *(instr[i].p++) = buffer[j];
                instr[i].n--;

                if (instr[i].n == 0) ++i;
            }

            if (i == instr.size()) {
                for (; j < iResult; ++j) {
                    str->in[str->iw++] = buffer[j];
                    str->iw &= 0b1111111;
                }

                return true;
            }

            return false;
        }

        void wait_until_finished() {
            while (!make_progress()) std::this_thread::yield();
        }
        
        read_op& operator >> (std::integral auto& v) {
            instr.push_back({ .p = (char*)&v, .n = sizeof(v) });

            return *this;
        }
    };

    stream::read_op stream::operator>>(std::integral auto &v) {
        read_op op;
        op.str = this;

        op.instr.push_back(read_op::read_instr{ .p = (char*)&v, .n = sizeof(v) });

        return op;
    }

    class acceptor {
        SOCKET sock;
    public:
        acceptor(uint16_t port);

        inline bool is_open() { return sock != INVALID_SOCKET; }
        std::optional<stream> try_accept();
    };
}