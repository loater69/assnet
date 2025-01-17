
# Assnet - ASynchronouS Networking

Assnet is a small and simple library for asynchronous networking in C++20 using coroutines.
It currently only works on Windows and there might be some bugs included. Use at your own risk.
However, if you do find errors, please be so kind and report them, by writing an issue.

## Using the library

To connect to a server use `assnet::stream`.

```c++
// connect to localhost on port 8080
assnet::stream stream("127.0.0.1", 8080);

// write a number
stream << 0;

// don't forget to flush
stream.flush();
```

To read data from a stream, you will have to use a coroutine.

```c++
assnet::nettask connection(assnet::stream& stream) {
    int num;
    co_await (stream >> num);

    std::cout << "received number: " << num << '\n';
}
```

Operator `co_await` checks if there is any data that can be read, if enough data is present, it will not suspend.
If it does suspend, you will have to resume the `nettask` manually (by using `assnet::nettask::make_progress()`). You can either have a vector of connections that you will resume one after the other, or you can have a dedicated thread for each connection or a mix of the two.

```c++

std::vector<std::pair<assnet::nettask, std::unique_ptr<assnet::stream>>> connections;

while (acceptor.is_open()) {

    for (size_t i = 0; i < connections.size(); ++i) {
        if (!connections[i].first.make_progress()) {
            connections.erase(connections.begin() + i);
            --i;
        }
    }

    if (auto v = acceptor.try_accept(); v) {
        auto stream = std::make_unique<assnet::stream>(std::move(v.value()));
        connections.push_back(std::make_pair(connection(*stream), std::move(stream)));
    }

    std::this_thread::yield();
}

```

If you want to accept a connection use `assnet::acceptor`;

```c++
// accept incoming connections on port 8080
assnet::acceptor acceptor(8080);

```
