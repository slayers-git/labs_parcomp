#include "server.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
/*#include <sys/ioctl.h>*/
#include <sys/poll.h>
#include <climits>

#include <sstream>

#include <algorithm>

constexpr unsigned LISTEN_BACKLOG_LEN = 32;

int Server::serverSetup () {
    int res;

    int sockfd = socket (AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        return -1;

    int opt = 1;
    res = setsockopt (sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&opt, sizeof (opt));
    if (res) {
        close (sockfd);
        return -1;
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons (m_config.port);
    
    res = bind (sockfd, (struct sockaddr *)&server_addr, sizeof (server_addr));
    if (res) {
        close (sockfd);
        return -1;
    }

    res = listen (sockfd, LISTEN_BACKLOG_LEN);
    if (res) {
        close (sockfd);
        return -1;
    }

    m_listenerSocket = sockfd;
    m_shouldShutdown = false;

    return 0;
}

int Server::clientSetup () {
    int res;

    int sockfd = socket (AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        return -1;

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl (m_config.addr);
    server_addr.sin_port = htons (m_config.port);
    
    res = ::connect (sockfd, (struct sockaddr *)&server_addr, sizeof (server_addr));
    if (res) {
        std::cerr << "Failed to connect.\n";
        close (sockfd);
        return -1;
    }

    m_clientSocket = sockfd;
    m_shouldShutdown = false;

    return 0;
}

void Server::serverShutdown () {
    for (uint32_t i = 0; i < m_clientSockets.size (); ++i) {
        const auto sockfd = m_clientSockets[i];
        ::close (sockfd);
    }

    ::close (m_listenerSocket);
}

void Server::clientShutdown () {
    ::close (m_clientSocket);
}

int Server::serverLoop () {
    int res;

    /* Listening socket */
    struct pollfd server_pollfd { };
    server_pollfd.fd = m_listenerSocket;
    server_pollfd.events = POLLIN;

    while (!m_shouldShutdown) {
        closeDeadConnections ();

        res = ::poll (&server_pollfd, 1, INT_MAX);
        if (res < 0) {
            std::cerr << "serverLoop: poll errored: " << strerror (errno) << '\n';
            m_shouldShutdown = true;
            return -1;
        }

        if (!server_pollfd.revents)
            continue;

        if (server_pollfd.revents != POLLIN) {
            std::cerr << "Unexpected event.\n";
            m_shouldShutdown = true;
            return -1;
        }

        res = acceptNewConnections ();
    }

    return 0;
}

int Server::clientLoop () {
    int res;
    struct pollfd server_pollfd {};

    server_pollfd.fd = m_clientSocket;
    server_pollfd.events = POLLIN;

    while (!m_shouldShutdown) {
        res = receiveServerMessages ();

        if (!res) {
            std::cerr << "Peer closed connection\n";
            m_shouldShutdown = true;
        }
    }

    return 0;
}

int Server::receiveServerMessages () {
    int res;
    char buffer[BUFSIZ];
    std::string contents;

    do {
        res = recv (m_clientSocket, buffer, BUFSIZ, 0);
        if (res < 0) {
            std::cout << "receiveServerMessages: recv error\n";
            res = -1;

            break;
        }

        if (res == 0) {
            /* Server has terminated the connection */
            break;
        } else {
            uint32_t len = res;
            contents = std::string (buffer, buffer + len);
            std::cout << contents;
        }
    } while (true);

    if (!contents.empty ())
        std::cout << contents;

    return contents.size ();
}

int Server::acceptNewConnections () {
    int res;
    int new_sockfd = -1;

    do {
        /* Accept the connection */
        struct sockaddr_in in_addr;
        socklen_t addr_len = sizeof (struct sockaddr_in);
        new_sockfd = ::accept (m_listenerSocket, (struct sockaddr *)&in_addr, &addr_len);
        if (new_sockfd < 0) {
            std::cerr << "Failed to create a socket for an incoming connection\n";
            return -1;
        }

        const auto port = ntohs (in_addr.sin_port);
        const auto addr = ntohl (in_addr.sin_addr.s_addr);
        std::cout << "Registring new connection: " << addr << ":" << port << '\n';

        m_clientThreads.emplace (new_sockfd,
                std::thread (&Server::threadServerClientLoop, this, new_sockfd));
        m_clientSockets.emplace_back (new_sockfd);
    } while (new_sockfd != -1);

    return 0;
}

void Server::threadServerClientLoop (int sockfd) {
    int res;
    bool close_connection = false;

    std::string contents;
    char buffer[BUFSIZ];
    do {
        res = ::recv (sockfd, buffer, BUFSIZ, 0);
        if (res < 0) {
            std::cout << "threadServerClientLoop: recv error.\n";
            close_connection = true;

            break;
        }

        if (!res) {
            close_connection = true;
            break;
        } else {
            uint32_t len = res;
            std::stringstream ss;
            contents = std::string (buffer, buffer + len);
            ss << "user_" << sockfd << ": " << contents << '\n';
            std::cout << "Received " << contents << " from " << sockfd <<"\n";

            broadcast (ss.str (), sockfd);
        }
    } while (true);

    if (close_connection) {
        m_deadSockets.emplace (sockfd);
    }
}

int Server::broadcast (const std::string& message, int excluded_socket) {
    assert (m_mode == ServerMode::Host);

    for (uint32_t i = 0; i < m_clientSockets.size (); ++i) {
        const auto sockfd = m_clientSockets[i];

        /* Don't send to the excluded socket */
        if (sockfd == excluded_socket)
            continue;

        /* Don't send to dead clients */
        if (m_deadSockets.find (sockfd) != m_deadSockets.end ()) {
            continue;
        }

        /* FIXME: Stop ignoring the return code */
        (void)sendMessageToSocket (sockfd, message);
    }

    return 0;
}

void Server::closeDeadConnections () {
    if (!m_deadSockets.empty ()) {
        std::cout << "Closing dead connections.\n";

        for (auto it = m_deadSockets.rbegin (); it != m_deadSockets.rend (); ++it) {
            const auto sockfd = *it;
            ::close (sockfd);

            m_clientSockets.erase (std::find (m_clientSockets.begin (), m_clientSockets.end (),
                        sockfd));

            m_clientThreads[sockfd].join ();
            m_clientThreads.erase (sockfd);
        }

        m_deadSockets.clear ();
    }
}

int Server::sendMessageToSocket (int sockfd, const std::string& message) {
    int res;

    res = ::send (sockfd, message.data (), message.size (), 0);
    if (res < 0) {
        std::cout << "sendMessageToSocket: res < 0\n";
        return -1;
    }

    return 0;
}

int Server::connect (ServerMode mode, const ServerConfig& config) {
    m_mode = mode;
    m_config = config;

    if (mode == ServerMode::Host) {
        return serverSetup ();
    } else {
        return clientSetup ();
    }
}

void Server::disconnect () {
    if (m_mode == ServerMode::Host) {
        serverShutdown ();
    } else {
        clientShutdown ();
    }

    m_shouldShutdown = true;
}

int Server::send (const std::string& message) {
    assert (m_mode == ServerMode::Client);
    return sendMessageToSocket (m_clientSocket, message);
}

int Server::run () {
    if (m_mode == ServerMode::Host) {
        return serverLoop ();
    } else {
        return clientLoop ();
    }
}
