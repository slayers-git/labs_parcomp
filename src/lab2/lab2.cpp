#include <cstring>
#include <future>
#include <iostream>
#include <memory>
#include <algorithm>
#include "server.hpp"

static std::unique_ptr<Server> server;
static std::thread server_thread;

static void threadedServerLoop (std::promise<int>&& return_code) {
    int res = server->run ();
    return_code.set_value (res);
};

static inline std::string& trim (std::string& str) {
    str.erase(str.begin (), std::find_if (str.begin (), str.end (), [&](char c) {
                return !std::isspace (static_cast<unsigned char>(c));
            }));
    str.erase(std::find_if (str.rbegin (), str.rend (), [&](char c){
                return !std::isspace (static_cast<unsigned char>(c));
            }).base(), str.end());

    return str;
}

static void inputLoop () {
    int res;

    while (!server->shouldShutdown ()) {
        std::string input;
        std::getline (std::cin, input);

        trim (input);
        if (input.empty ())
            continue;

        if (input == "/exit") {
            server->disconnect ();
            break;
        }

        res = server->send (Message ("user_1", input));
        if (res) {
            std::cerr << "ERROR: Didn't send.\n";
            continue;
        }
    }
}

int main (int argc, char **argv) {
    int res;
    ServerMode mode = ServerMode::Client;

    if (argc >= 2) {
        if (std::strcmp (argv[1], "host") == 0) {
            mode = ServerMode::Host;
        }
    }

    ServerConfig config;
    config.port = 13371;
    config.addr = 0x7f000001;
    config.max_connections = 32;

    /* No std::make_unique in C++11??? */
    server = std::unique_ptr<Server> (new Server);

    res = server->connect (mode, config);
    if (res) {
        std::cerr << "Failed to connect/create server\n";
        return 0;
    }

    std::promise<int> thread_promise;
    auto thread_res = thread_promise.get_future ();

    server_thread = std::thread (threadedServerLoop, std::move (thread_promise));

    if (mode == ServerMode::Client)
        inputLoop ();

    server_thread.join ();
    return thread_res.get ();
}
