#include "assnet.h"
#include <memory>
#include <map>

assnet::nettask client_connection(assnet::stream& stream) {
    while (true) {
        int type;
        co_await (stream >> type);

        if (type == 1) { // msg
            int user;
            co_await (stream >> user);
            size_t len;
            co_await (stream >> len);

            std::cout << user << "> ";

            if (len != 0) {
                std::string str;
                str.resize(len);

                auto ro = stream >> str[0];

                for (size_t i = 1; i < len; ++i) {
                    ro >> str[i];
                }

                co_await ro;

                std::cout << str;
            }

            std::cout << "\n";
        } else {
            std::cout << "wrong message type\n";
        }
    }
}

void client() {
    std::cout << "ip:";

    std::string ip;
    std::cin >> ip;

    std::cout << "port:";

    int port;
    std::cin >> port;

    assnet::stream stream(ip, port);

    auto thread = std::jthread([&](std::stop_token token) {
        auto nt = client_connection(stream);

        while (!token.stop_requested() && nt.try_progress()) std::this_thread::yield();
    });

    while (true) {
        
        std::string str;
        std::getline(std::cin, str);

        if (str == "q") {
            stream << 0;
            thread.request_stop();
            break;
        }

        stream << 1;
        stream << str.size();
        for (char c : str) stream.put(c);
        stream.flush();
    }
}

assnet::nettask server_connection(std::map<int, std::pair<assnet::nettask, std::unique_ptr<assnet::stream>>>& connections, int i, assnet::stream& stream) {
    while (true) {
        int type;
        co_await (stream >> type);

        if (type == 0) {
            break;
        }

        if (type == 1) { // msg
            size_t len;
            co_await (stream >> len);

            if (len != 0) {
                std::string str;
                str.resize(len);

                auto ro = stream >> str[0];

                for (size_t i = 1; i < len; ++i) {
                    ro >> str[i];
                }

                co_await ro;

                std::cout << "received: " << str << '\n';

                for (auto&[id, ts] : connections) {
                    if (id != i) {
                        auto& stream = *ts.second;
                        stream << 1;
                        stream << i;
                        stream << str.size();
                        for (char c : str) stream.put(c);
                        stream.flush();
                    }
                }
            }
        } else {
            std::cout << "wrong message type\n";
        }
    }
}

void server() {
    std::cout << "port:";

    int port;
    std::cin >> port;

    assnet::acceptor acceptor(port);
    std::map<int, std::pair<assnet::nettask, std::unique_ptr<assnet::stream>>> tasks;

    int id = 0;

    while (acceptor.is_open()) {
        if (auto stream = acceptor.try_accept(); stream) {
            auto str = std::make_unique<assnet::stream>(std::move(stream.value()));

            tasks.insert({id, std::make_pair(server_connection(tasks, id, *str), std::move(str))});
            ++id;
        }

        for (auto it = tasks.begin(); it != tasks.end(); ) {
            if (!it->second.first.try_progress()) {
                it = tasks.erase(it);
            } else {
                ++it;
            }
        }

        std::this_thread::yield();
    }
}

int main(int argc, char const *argv[]) {
    WSAData wsadata;

    int _ = WSAStartup(MAKEWORD(2, 2), &wsadata);

    while (true) {
        std::cout << "q: quit, h: host, c: connect\n";
        char op;
        std::cin >> op;

        if (op == 'q') break;
        else if (op == 'h') server();
        else if (op == 'c') client();
        else std::cout << "wtf\n";
    }

    WSACleanup();

    return 0;
}
