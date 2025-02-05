// UdpSender.cpp
#include "UdpSender.h"
#include <cstring> // for memset

UdpSender::UdpSender(const std::string& ip, unsigned short port)
    : m_ip(ip), m_port(port), m_socket(INVALID_SOCKET)
{
}

UdpSender::~UdpSender() {
    closeSocket();
}

bool UdpSender::init() {
#ifdef _WIN32
    // Initialize Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (result != 0) {
        std::cerr << "[UDP] WSAStartup failed with error: " << result << std::endl;
        return false;
    }
#endif
    // Create socket
    m_socket = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (m_socket == INVALID_SOCKET) {
        std::cerr << "[UDP] Error creating socket.\n";
        closeSocket();
        return false;
    }

    // Configure remote address
    memset(&m_remoteAddr, 0, sizeof(m_remoteAddr));
    m_remoteAddr.sin_family = AF_INET;
    m_remoteAddr.sin_port = htons(m_port);

#ifdef _WIN32
    InetPtonA(AF_INET, m_ip.c_str(), &m_remoteAddr.sin_addr);
#else
    inet_pton(AF_INET, m_ip.c_str(), &m_remoteAddr.sin_addr);
#endif
    return true;
}

bool UdpSender::send(const std::string& message) {
    if (m_socket == INVALID_SOCKET) {
        return false;
    }
    int res = ::sendto(
        m_socket,
        message.data(),
        static_cast<int>(message.size()),
        0,
        reinterpret_cast<struct sockaddr*>(&m_remoteAddr),
        static_cast<socklen_t>(sizeof(m_remoteAddr))
    );
    if (res == SOCKET_ERROR) {
        std::cerr << "[UDP] sendto failed.\n";
        return false;
    }
    return true;
}

void UdpSender::closeSocket() {
    if (m_socket != INVALID_SOCKET) {
#ifdef _WIN32
        closesocket(m_socket);
        WSACleanup();
#else
        close(m_socket);
#endif
        m_socket = INVALID_SOCKET;
    }
}
