#ifndef __SERVER_HPP__
#define __SERVER_HPP__

#include "common.hpp"

#include <cstdint>
#include <queue>
#include <set>
#include <sys/poll.h>
#include <thread>
#include <unordered_map>
#include <vector>

/* Either the server is actually the server, or just a client to a server */
enum class ServerMode { 
    Host,
    Client
};

class Server {
private:
    ServerMode m_mode;
    ServerConfig m_config;

    bool m_shouldShutdown = true;

    std::unordered_map<uint64_t, Client> m_clients;
    std::vector<int> m_clientSockets;

    std::vector<struct pollfd> m_pollfds;
    std::set<uint32_t> m_deadPollfds;
    std::set<uint32_t> m_deadSockets;

    /* Client thread that polls for new messages on the server */
    std::thread m_pollingThread;

    /* The socket that is listening to incoming connections */
    int m_listenerSocket;
    /* Client socket */
    int m_clientSocket;

    int serverSetup ();
    int clientSetup ();

    void serverShutdown ();
    void clientShutdown ();

    int serverLoop ();
    int clientLoop ();

    /* Stop polling for dead connections and close the related sockets */
    void closeDeadConnections ();

    /* Accept connection on listening socket */
    int acceptNewConnections ();

    /* Since there is data on the socket, read it */
    int readPollData (uint32_t id);

    /* Send a message to socket */
    int sendMessageToSocket (int socket, const Message& message);

    /* Receive queued messages from the server */
    int receiveServerMessages ();

public:
    Server () = default;
    int connect (ServerMode mode, const ServerConfig& config);
    void disconnect ();

    int run ();

    int broadcast (const Message& message);
    int send (const Message& message);

    bool shouldShutdown () const noexcept {
        return m_shouldShutdown;
    }
};

#endif /* #define __SERVER_HPP__ */
