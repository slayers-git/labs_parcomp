#include "server.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <climits>

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

    res = ioctl (sockfd, FIONBIO, (void *)&opt);
    if (res < 0) {
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

    int opt = 1;
    res = ioctl (sockfd, FIONBIO, (void *)&opt);
    if (res < 0) {
        close (sockfd);
        return -1;
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl (m_config.addr);
    server_addr.sin_port = htons (m_config.port);
    
    res = ::connect (sockfd, (struct sockaddr *)&server_addr, sizeof (server_addr));
    if (res) {
        /* The socket is non-blocking, so have to handle EINPROGRESS as a success 
         *
         * Maybe there is a better way to do this? */
        if (errno != EINPROGRESS) {
            close (sockfd);
            return -1;
        }

        struct pollfd pollfd { };
        pollfd.fd = sockfd;
        pollfd.events = POLLOUT;
        
        res = ::poll (&pollfd, 1, 0);
        if (res < 0) {
            std::cerr << "Failed to connect.\n";
            close (sockfd);
            return -1;
        }
        if (res == 0) {
            std::cerr << "Timeout\n";
            close (sockfd);
            return -1;
        }
        
        int opt;
        socklen_t len = 4;
        res = ::getsockopt (sockfd, SOL_SOCKET, SO_ERROR, &opt, &len);
        if (res) {
            std::cerr << "getsockopt.\n";
            close (sockfd);
            return -1;
        }

        if (opt != 0) {
            std::cerr << "Failed to connect.\n";
            close (sockfd);
            return -1;
        }
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
    {
        struct pollfd server_pollfd { };
        server_pollfd.fd = m_listenerSocket;
        server_pollfd.events = POLLIN;

        m_pollfds.emplace_back (std::move (server_pollfd));
    }

    while (!m_shouldShutdown) {
        closeDeadConnections ();

        res = ::poll (m_pollfds.data (), m_pollfds.size (), INT_MAX);
        if (res < 0) {
            std::cerr << "serverLoop: poll errored: " << strerror (errno) << '\n';
            m_shouldShutdown = true;
            return -1;
        }

        for (uint32_t i = 0; i < m_pollfds.size (); ++i) {
            const auto& fd = m_pollfds[i];
            if (!fd.revents)
                continue;

            if (fd.revents != POLLIN) {
                std::cerr << "Unexpected event.\n";
                m_shouldShutdown = true;
                return -1;
            }

            if (fd.fd == m_listenerSocket) {
                res = acceptNewConnections ();
            } else {
                std::cout << "Poll received on sock: " << i << "\n";
                res = readPollData (i);
            }
        }
    }

    return 0;
}

int Server::clientLoop () {
    int res;
    struct pollfd server_pollfd {};

    server_pollfd.fd = m_clientSocket;
    server_pollfd.events = POLLIN;

    while (!m_shouldShutdown) {
        res = ::poll (&server_pollfd, 1, INT_MAX);
        if (res < 0) {
            std::cerr << "clientLoop: Poll errored: " << strerror (errno) << "\n";
            m_shouldShutdown = true;
            return -1;
        }
        
        if (server_pollfd.revents & POLLRDHUP) {
            std::cerr << "Peer closed connection\n";
            m_shouldShutdown = true;
            return -1;
        }
        if (server_pollfd.revents & POLLERR) {
            std::cerr << "Closed connection\n";
            m_shouldShutdown = true;
            return 0;
        }

        if (server_pollfd.revents & POLLIN) {
            res = receiveServerMessages ();

            if (!res) {
                std::cerr << "Peer closed connection\n";
                m_shouldShutdown = true;
            }
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
            if (errno != EWOULDBLOCK) {
                std::cout << "receiveServerMessages: recv error\n";
                res = -1;
            }

            break;
        }

        if (res == 0) {
            /* Server has terminated the connection */
            break;
        } else {
            uint32_t len = res;
            contents.append (buffer, len);
        }
    } while (true);

    if (!contents.empty ())
        std::cout << contents << "\n";

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
            if (errno != EWOULDBLOCK && errno != EAGAIN) {
                std::cerr << "Failed to create a socket for an incoming connection\n";
                return -1;
            }

            break;
        }

        int opt = 1;
        res = ioctl (new_sockfd, FIONBIO, (void *)&opt);
        if (res < 0) {
            std::cerr << "Failed to create a socket for an incoming connection\n";
            close (new_sockfd);
            return -1;
        }

        const auto port = ntohs (in_addr.sin_port);
        const auto addr = ntohl (in_addr.sin_addr.s_addr);
        std::cout << "Registring new connection: " << addr << ":" << port << '\n';

        pollfd new_pollfd {};
        new_pollfd.fd = new_sockfd;
        new_pollfd.events = POLLIN;

        m_clientSockets.emplace_back (new_sockfd);
        m_pollfds.emplace_back (std::move (new_pollfd));
    } while (new_sockfd != -1);

    return 0;
}

int Server::readPollData (uint32_t pollfd_id) {
    int res;
    bool close_connection = false;

    const auto& pollfd = m_pollfds[pollfd_id];
    auto sockfd = pollfd.fd;

    std::string contents;
    char buffer[BUFSIZ];
    do {
        res = ::recv (sockfd, buffer, BUFSIZ, 0);
        if (res < 0) {
            if (errno != EWOULDBLOCK) {
                std::cout << "readPollData: recv error.\n";
                close_connection = true;
            }

            break;
        }

        if (!res) {
            close_connection = true;
            break;
        } else {
            uint32_t len = res;
            contents.append (buffer, len);
        }
    } while (true);

    if (close_connection) {
        m_deadPollfds.emplace (pollfd_id);
        m_deadSockets.emplace (sockfd);
    }

    std::cout << "Received " << contents << " from " << sockfd <<"\n";
    return broadcast (Message ("user_1", contents));
}

int Server::broadcast (const Message& message) {
    assert (m_mode == ServerMode::Host);

    for (uint32_t i = 0; i < m_clientSockets.size (); ++i) {
        const auto sockfd = m_clientSockets[i];

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
    if (!m_deadPollfds.empty ()) {
        std::cout << "Closing dead connections.\n";

        for (auto it = m_deadPollfds.rbegin (); it != m_deadPollfds.rend (); ++it) {
            const auto id = *it;
            m_pollfds.erase (m_pollfds.begin () + id);
        }

        for (auto it = m_deadSockets.rbegin (); it != m_deadSockets.rend (); ++it) {
            const auto sockfd = *it;
            ::close (sockfd);

            m_clientSockets.erase (std::find (m_clientSockets.begin (), m_clientSockets.end (),
                        sockfd));
        }

        m_deadPollfds.clear ();
        m_deadSockets.clear ();
    }
}

int Server::sendMessageToSocket (int socket, const Message& message) {
    int res;
    std::string final_message = message.getSender () + ": " + message.getContents ();

    res = ::send (socket, final_message.data (), final_message.size (), 0);
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

int Server::send (const Message& message) {
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
