#include "assnet.h"
#include <memory>
#include <map>
#include <fstream>
#include <span>

#undef min

assnet::nettask download(assnet::stream& stream, std::string& file) {
    stream << 2;
    stream << file.size();
    stream.put_n(file.data(), file.size());

    stream.flush();

    int status;
    co_await (stream >> status);

    if (status == 0) {
        size_t len;
        co_await (stream >> len);

        std::ofstream str(file, std::ios::binary);
        std::vector<char> v;
        while (len > 0) {
            v.resize(len);

            co_await stream.read(v.data(), len);

            str.write(v.data(), v.size());

            stream << 0;
            stream.flush();

            co_await (stream >> len);
        }

        std::cout << "downloaded " << file << '\n';
    } else if (status == 404) std::cout << "error 404: file not found\n";
    else std::cout << "Unknown error: " << status << '\n';
}

assnet::nettask upload(assnet::stream& stream, std::string& file) {
    std::vector<char> data;
    std::ifstream f(file, std::ios::binary | std::ios::ate);

    if (!f.is_open()) {
        std::cout << "file not found!\n";
        co_return;
    }

    data.resize(f.tellg());
    f.seekg(0);

    f.read(data.data(), data.size());

    f.close();

    std::cout << "uploading: " << data.size() << '\n';

    stream << 1; // upload
    stream << file.size();
    stream.put_n(file.data(), file.size());
    stream << data.size();
    stream.put_n(data.data(), data.size());

    stream.flush();

    int status;
    co_await (stream >> status);

    if (status == 0) std::cout << "success\n";
    else std::cout << "failure\n";

    stream.close();
}

void client() {
    std::cout << "ip: ";

    std::string ip;
    std::cin >> ip;

    std::cout << "port: ";

    int port;
    std::cin >> port;

    char op;
    std::cout << "d: download, u: upload\n";
    std::cin >> op;

    std::string file;
    std::cout << "filename: ";
    std::cin >> file;

    assnet::stream stream(ip, port);

    if (op == 'u') {
        auto t = upload(stream, file);

        while (t.try_progress()) std::this_thread::yield();
    } else if (op == 'd') {
        auto t = download(stream, file);

        while (t.try_progress()) std::this_thread::yield();
    }
}

assnet::nettask server_connection(assnet::stream& stream) {
    int type;
    co_await (stream >> type);

    if (type == 1) { // upload
        size_t len;
        co_await (stream >> len);

        std::string file_name;
        file_name.resize(len);
        co_await stream.read(file_name.data(), len);

        size_t blen;
        co_await (stream >> blen);

        std::vector<char> buffer;
        buffer.resize(blen);
        co_await stream.read(buffer.data(), blen);

        std::ofstream file(file_name, std::ios::binary);
        file.write(buffer.data(), buffer.size());

        stream << 0;
        stream.flush();
    } else if (type == 2) { // download
        size_t len;
        co_await (stream >> len);

        std::string file_name;
        file_name.resize(len);
        co_await stream.read(file_name.data(), len);

        std::ifstream file(file_name, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            stream << 404;
            stream.flush();
        } else {
            stream << 0;

            std::vector<char> v;
            v.resize(file.tellg());
            file.seekg(0);

            file.read(v.data(), v.size());
            file.close();

            // split into chunks for big files because recv cache is limited
            std::span<char> s = v;

            while (s.size() > 0) {
                auto send = std::min(s.size(), (size_t)512);

                stream << send;
                stream.put_n(s.data(), send);
                stream.flush();

                s = s.subspan(send);

                int status;
                co_await (stream >> status);

                if (status != 0) {
                    stream.close();
                    std::cout << "shit\n";
                    co_return;
                }
            }

            stream << (size_t)0;

            stream.flush();

            std::cout << "send: " << v.size() << '\n';
        }
    } else std::cout << "Invalid type\n";
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

            tasks.insert({id, std::make_pair(server_connection(*str), std::move(str))});
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
