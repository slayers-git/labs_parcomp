#ifndef __COMMON_HPP__
#define __COMMON_HPP__

#include <cstdint>
#include <string>

struct ServerConfig {
    /* Address of the server */
    uint32_t addr;
    /* Port of the server */
    uint16_t port;

    /* Max connections on the server at once */
    uint32_t max_connections = 32;
    /* Timeout in milliseconds to connect to the server */
    uint64_t timeout_ms = 3000;
};

struct Client {
    std::string name;
    uint64_t id;
};

/* High level representation of a message */
/*class Message {*/
/*private:*/
/*    std::string m_sender;*/
/*    std::string m_contents;*/
/**/
/*public:*/
/*    Message (const std::string& sender, const std::string& contents) :*/
/*        m_sender (sender), m_contents (contents) { }*/
/*    Message () = default;*/
/**/
/*    const std::string& getSender () const noexcept {*/
/*        return m_sender;*/
/*    }*/
/**/
/*    const std::string& getContents () const noexcept {*/
/*        return m_contents;*/
/*    }*/
/**/
/*    ~Message () = default;*/
/*};*/

#endif /* #define __COMMON_HPP__ */
