// UdpSender.h
#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#define INVALID_SOCKET -1
#define SOCKET_ERROR   -1
typedef int SOCKET;
#endif

#include <string>
#include <iostream>

class UdpSender {
public:
    UdpSender(const std::string& ip, unsigned short port);
    ~UdpSender();
    bool init();
    bool send(const std::string& message);

private:
    void closeSocket();
    
    std::string       m_ip;
    unsigned short    m_port;
    SOCKET            m_socket;
    struct sockaddr_in m_remoteAddr;
};
