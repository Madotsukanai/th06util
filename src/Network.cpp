#include "Network.hpp"
#include "ZunResult.hpp"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

#include <miniupnpc.h>
#include <upnpcommands.h>
#include <upnpdev.h>

#include <stdarg.h>

namespace th06
{

static void NetworkLog(const char *fmt, ...)
{
    (void)fmt;
}

NetworkManager g_NetworkManager;

NetworkManager::NetworkManager()
    : m_socket(NULL), m_mappedPort(0), m_isHost(false), m_initialized(false), m_portMapped(false), m_isConnected(false),
      m_remoteCharacter(0), m_remoteShotType(0)
{
}
NetworkManager::~NetworkManager()
{
    Cleanup();
}

ZunResult NetworkManager::Initialize()
{
    if (m_initialized)
        return ZUN_SUCCESS;
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        return ZUN_ERROR;
    m_initialized = true;
    return ZUN_SUCCESS;
}

ZunResult NetworkManager::OpenPortMapping(int port)
{
    NetworkLog("OpenPortMapping: attempting UPnP port mapping for port %d\n", port);
    if (m_portMapped && m_mappedPort == port)
    {
        NetworkLog("OpenPortMapping: port %d is already mapped\n", port);
        return ZUN_SUCCESS;
    }
    ClosePortMapping();

    int error = 0;
    UPNPDev *devList = upnpDiscover(2000, NULL, NULL, UPNP_LOCAL_PORT_ANY, 0, 2, &error);
    if (devList == NULL)
    {
        NetworkLog("OpenPortMapping: upnpDiscover failed with error %d\n", error);
        return ZUN_ERROR;
    }

    UPNPUrls urls;
    IGDdatas data;
    char lanAddr[64] = {0};
    char wanAddr[64] = {0};
    memset(&urls, 0, sizeof(urls));
    memset(&data, 0, sizeof(data));

    int igdResult = UPNP_GetValidIGD(devList, &urls, &data, lanAddr, sizeof(lanAddr), wanAddr, sizeof(wanAddr));
    freeUPNPDevlist(devList);
    if (igdResult == UPNP_NO_IGD || lanAddr[0] == '\0')
    {
        NetworkLog("OpenPortMapping: UPNP_GetValidIGD failed (igdResult=%d)\n", igdResult);
        return ZUN_ERROR;
    }

    char portStr[16];
    sprintf(portStr, "%d", port);
    int mapResult = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype, portStr, portStr, lanAddr,
                                        "th06util online matching", "TCP", NULL, "0");
    FreeUPNPUrls(&urls);
    if (mapResult != UPNPCOMMAND_SUCCESS)
    {
        NetworkLog("OpenPortMapping: UPNP_AddPortMapping failed with result %d\n", mapResult);
        return ZUN_ERROR;
    }

    NetworkLog("OpenPortMapping: successfully mapped port %d (LAN addr: %s)\n", port, lanAddr);
    m_mappedPort = port;
    m_portMapped = true;
    return ZUN_SUCCESS;
}

void NetworkManager::ClosePortMapping()
{
    if (!m_portMapped)
        return;
    if (Initialize() != ZUN_SUCCESS)
    {
        m_mappedPort = 0;
        m_portMapped = false;
        return;
    }

    int error = 0;
    UPNPDev *devList = upnpDiscover(1000, NULL, NULL, UPNP_LOCAL_PORT_ANY, 0, 2, &error);
    if (devList != NULL)
    {
        UPNPUrls urls;
        IGDdatas data;
        char lanAddr[64] = {0};
        char wanAddr[64] = {0};
        memset(&urls, 0, sizeof(urls));
        memset(&data, 0, sizeof(data));

        int igdResult = UPNP_GetValidIGD(devList, &urls, &data, lanAddr, sizeof(lanAddr), wanAddr, sizeof(wanAddr));
        freeUPNPDevlist(devList);
        if (igdResult != UPNP_NO_IGD)
        {
            char portStr[16];
            sprintf(portStr, "%d", m_mappedPort);
            UPNP_DeletePortMapping(urls.controlURL, data.first.servicetype, portStr, "TCP", NULL);
            FreeUPNPUrls(&urls);
        }
    }

    m_mappedPort = 0;
    m_portMapped = false;
}

void NetworkManager::Cleanup(bool closePortMapping)
{
    if (closePortMapping)
    {
        ClosePortMapping();
    }
    if (m_socket != NULL)
    {
        closesocket((SOCKET)m_socket);
        m_socket = NULL;
    }
    m_isConnected = false;
    m_isHost = false;
    if (m_initialized)
    {
        WSACleanup();
        m_initialized = false;
    }
}

ZunResult NetworkManager::BeginConnect(const char *ip, int port)
{
    if (Initialize() != ZUN_SUCCESS)
        return ZUN_ERROR;
    if (m_socket != NULL)
    {
        closesocket((SOCKET)m_socket);
        m_socket = NULL;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET)
        return ZUN_ERROR;

    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
    addrinfo hints = {0}, *res = NULL;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    char portStr[16];
    sprintf(portStr, "%d", port);

    char cleanIp[256] = {0};
    unsigned int a, b, c, d;
    if (sscanf(ip, "%u.%u.%u.%u", &a, &b, &c, &d) == 4)
    {
        sprintf(cleanIp, "%u.%u.%u.%u", a, b, c, d);
    }
    else
    {
        strncpy(cleanIp, ip, sizeof(cleanIp) - 1);
    }

    NetworkLog("BeginConnect: attempting to connect to %s:%d (cleaned: %s)\n", ip, port, cleanIp);

    if (getaddrinfo(cleanIp, portStr, &hints, &res) != 0)
    {
        NetworkLog("BeginConnect: getaddrinfo failed for %s\n", cleanIp);
        closesocket(sock);
        return ZUN_ERROR;
    }

    sockaddr_in addr;
    memcpy(&addr, res->ai_addr, sizeof(sockaddr_in));
    freeaddrinfo(res);

    NetworkLog("BeginConnect: connecting to resolved IP...\n");
    int connRes = connect(sock, (sockaddr *)&addr, sizeof(addr));
    int connErr = WSAGetLastError();
    NetworkLog("BeginConnect: connect() returned %d, WSAGetLastError=%d\n", connRes, connErr);
    m_socket = (void *)sock;
    m_isHost = false;
    return ZUN_SUCCESS;
}

int NetworkManager::CheckConnect()
{
    if (m_socket == NULL || m_isHost)
    {
        NetworkLog("CheckConnect: early exit, m_socket=%p m_isHost=%d\n", m_socket, m_isHost);
        return -1;
    }
    SOCKET sock = (SOCKET)m_socket;

    fd_set writeSet;
    fd_set exceptSet;
    FD_ZERO(&writeSet);
    FD_ZERO(&exceptSet);
    FD_SET(sock, &writeSet);
    FD_SET(sock, &exceptSet);
    timeval timeout = {0, 100};
    int res = select((int)sock + 1, NULL, &writeSet, &exceptSet, &timeout);

    if (res > 0)
    {
        if (FD_ISSET(sock, &exceptSet))
        {
            int wsaErr = WSAGetLastError();
            NetworkLog("CheckConnect: socket in exceptSet, WSAGetLastError=%d\n", wsaErr);

            if (wsaErr != 0)
            {
                closesocket(sock);
                m_socket = NULL;
                return -1;
            }
        }
        if (FD_ISSET(sock, &writeSet) || FD_ISSET(sock, &exceptSet))
        {
            int error = 0;
            int errorSize = sizeof(error);
            if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (char *)&error, &errorSize) == SOCKET_ERROR || error != 0)
            {
                NetworkLog("CheckConnect: SO_ERROR=%d\n", error);
                closesocket(sock);
                m_socket = NULL;
                return -1;
            }
            NetworkLog("CheckConnect: connection successful!\n");
            u_long mode = 0;
            ioctlsocket(sock, FIONBIO, &mode);
            BOOL noDelay = TRUE;
            setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&noDelay, sizeof(noDelay));
            return 0;
        }
    }
    if (res == SOCKET_ERROR)
    {
        NetworkLog("CheckConnect: select returned SOCKET_ERROR=%d\n", WSAGetLastError());
        closesocket(sock);
        m_socket = NULL;
        return -1;
    }

    NetworkLog("CheckConnect: pending (res=%d)\n", res);
    return 1;
}

ZunResult NetworkManager::StartServer(int port)
{
    NetworkLog("StartServer: initializing on port %d\n", port);
    if (Initialize() != ZUN_SUCCESS)
        return ZUN_ERROR;
    if (m_socket != NULL)
    {
        closesocket((SOCKET)m_socket);
        m_socket = NULL;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET)
        return ZUN_ERROR;

    BOOL reuseAddr = TRUE;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&reuseAddr, sizeof(reuseAddr));

    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);

    sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sock, (sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        NetworkLog("StartServer: bind failed\n");
        closesocket(sock);
        return ZUN_ERROR;
    }
    if (listen(sock, 1) == SOCKET_ERROR)
    {
        NetworkLog("StartServer: listen failed\n");
        closesocket(sock);
        return ZUN_ERROR;
    }

    NetworkLog("StartServer: listening on port %d\n", port);
    m_socket = (void *)sock;
    m_isHost = true;
    NetworkLog("StartServer: calling OpenPortMapping\n");
    ZunResult mapRes = OpenPortMapping(port);
    NetworkLog("StartServer: OpenPortMapping returned %d\n", mapRes);
    return ZUN_SUCCESS;
}

bool NetworkManager::CheckForConnection()
{
    if (!m_isHost || m_socket == NULL)
        return false;
    SOCKET sock = (SOCKET)m_socket;

    SOCKET clientSocket = accept(sock, NULL, NULL);
    if (clientSocket != INVALID_SOCKET)
    {
        NetworkLog("CheckForConnection: accepted client connection!\n");
        closesocket(sock);
        m_socket = (void *)clientSocket;
        u_long mode = 0;
        ioctlsocket(clientSocket, FIONBIO, &mode);
        BOOL noDelay = TRUE;
        setsockopt(clientSocket, IPPROTO_TCP, TCP_NODELAY, (char *)&noDelay, sizeof(noDelay));
        return true;
    }
    if (WSAGetLastError() == WSAEWOULDBLOCK)
    {
        return false;
    }
    return false;
}

ZunResult NetworkManager::ExchangeHandshake(HandshakeData *local, HandshakeData *remote)
{
    if (m_socket == NULL)
        return ZUN_ERROR;
    SOCKET sock = (SOCKET)m_socket;

    if (m_isHost)
    {
        int received = 0;
        NetworkLog("ExchangeHandshake: host receiving first\n");
        while (received < (int)sizeof(HandshakeData))
        {
            fd_set readSet;
            FD_ZERO(&readSet);
            FD_SET(sock, &readSet);
            timeval tv = {10, 0};
            if (select((int)sock + 1, &readSet, NULL, NULL, &tv) <= 0)
            {
                NetworkLog("ExchangeHandshake: host recv select timeout/error\n");
                return ZUN_ERROR;
            }
            int res = recv(sock, (char *)remote + received, (int)sizeof(HandshakeData) - received, 0);
            if (res <= 0)
            {
                NetworkLog("ExchangeHandshake: host recv failed\n");
                return ZUN_ERROR;
            }
            received += res;
        }

        int sent = 0;
        NetworkLog("ExchangeHandshake: host sending\n");
        while (sent < (int)sizeof(HandshakeData))
        {
            fd_set writeSet;
            FD_ZERO(&writeSet);
            FD_SET(sock, &writeSet);
            timeval tv = {10, 0};
            if (select((int)sock + 1, NULL, &writeSet, NULL, &tv) <= 0)
            {
                NetworkLog("ExchangeHandshake: host send select timeout/error\n");
                return ZUN_ERROR;
            }
            int res = send(sock, (char *)local + sent, (int)sizeof(HandshakeData) - sent, 0);
            if (res <= 0)
            {
                NetworkLog("ExchangeHandshake: host send failed\n");
                return ZUN_ERROR;
            }
            sent += res;
        }
    }
    else
    {
        int sent = 0;
        NetworkLog("ExchangeHandshake: guest sending first\n");
        while (sent < (int)sizeof(HandshakeData))
        {
            fd_set writeSet;
            FD_ZERO(&writeSet);
            FD_SET(sock, &writeSet);
            timeval tv = {10, 0};
            if (select((int)sock + 1, NULL, &writeSet, NULL, &tv) <= 0)
            {
                NetworkLog("ExchangeHandshake: guest send select timeout/error\n");
                return ZUN_ERROR;
            }
            int res = send(sock, (char *)local + sent, (int)sizeof(HandshakeData) - sent, 0);
            if (res <= 0)
            {
                NetworkLog("ExchangeHandshake: guest send failed\n");
                return ZUN_ERROR;
            }
            sent += res;
        }

        int received = 0;
        NetworkLog("ExchangeHandshake: guest receiving\n");
        while (received < (int)sizeof(HandshakeData))
        {
            fd_set readSet;
            FD_ZERO(&readSet);
            FD_SET(sock, &readSet);
            timeval tv = {10, 0};
            if (select((int)sock + 1, &readSet, NULL, NULL, &tv) <= 0)
            {
                NetworkLog("ExchangeHandshake: guest recv select timeout/error\n");
                return ZUN_ERROR;
            }
            int res = recv(sock, (char *)remote + received, (int)sizeof(HandshakeData) - received, 0);
            if (res <= 0)
            {
                NetworkLog("ExchangeHandshake: guest recv failed\n");
                return ZUN_ERROR;
            }
            received += res;
        }
    }

    if (remote->magic != 0x54483036)
    {
        NetworkLog("ExchangeHandshake: magic mismatch (%08X)\n", remote->magic);
        return ZUN_ERROR;
    }
    m_remoteCharacter = remote->character;
    m_remoteShotType = remote->shotType;
    m_isConnected = true;
    NetworkLog("ExchangeHandshake: Handshake successful!\n");
    return ZUN_SUCCESS;
}

ZunResult NetworkManager::ExchangeInput(u16 *localInput, u16 *remoteInput)
{
    return ExchangeData(localInput, remoteInput, sizeof(u16));
}

ZunResult NetworkManager::ExchangeData(void *localData, void *remoteData, int size)
{
    if (m_socket == NULL || !m_isConnected)
    {
        memset(remoteData, 0, size);
        return ZUN_ERROR;
    }

    SOCKET sock = (SOCKET)m_socket;

    int sent = 0;
    while (sent < size)
    {
        fd_set writeSet;
        FD_ZERO(&writeSet);
        FD_SET(sock, &writeSet);
        timeval tv = {2, 0};

        if (select((int)sock + 1, NULL, &writeSet, NULL, &tv) <= 0)
        {
            NetworkLog("ExchangeData: send timeout/error\n");
            Cleanup(false);
            return ZUN_ERROR;
        }

        int res = send(sock, (char *)localData + sent, size - sent, 0);
        if (res <= 0)
        {
            NetworkLog("ExchangeData: send failed\n");
            Cleanup(false);
            return ZUN_ERROR;
        }
        sent += res;
    }

    int received = 0;
    while (received < size)
    {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(sock, &readSet);
        timeval tv = {2, 0};

        if (select((int)sock + 1, &readSet, NULL, NULL, &tv) <= 0)
        {
            NetworkLog("ExchangeData: recv timeout/error\n");
            Cleanup(false);
            return ZUN_ERROR;
        }

        int res = recv(sock, (char *)remoteData + received, size - received, 0);
        if (res <= 0)
        {
            NetworkLog("ExchangeData: recv failed\n");
            Cleanup(false);
            return ZUN_ERROR;
        }
        received += res;
    }
    return ZUN_SUCCESS;
}

ZunResult NetworkManager::GetLocalIP(char *buffer, int bufferSize)
{
    if (Initialize() != ZUN_SUCCESS)
        return ZUN_ERROR;

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock != INVALID_SOCKET)
    {
        sockaddr_in dest = {0};
        dest.sin_family = AF_INET;
        dest.sin_port = htons(53);
        dest.sin_addr.s_addr = inet_addr("8.8.8.8");

        if (connect(sock, (sockaddr *)&dest, sizeof(dest)) != SOCKET_ERROR)
        {
            sockaddr_in local = {0};
            int localLen = sizeof(local);
            if (getsockname(sock, (sockaddr *)&local, &localLen) != SOCKET_ERROR)
            {
                strncpy(buffer, inet_ntoa(local.sin_addr), bufferSize);
                buffer[bufferSize - 1] = '\0';
                closesocket(sock);
                return ZUN_SUCCESS;
            }
        }
        closesocket(sock);
    }

    char hostName[256];
    if (gethostname(hostName, sizeof(hostName)) == SOCKET_ERROR)
        return ZUN_ERROR;
    addrinfo hints = {0}, *res = NULL;
    hints.ai_family = AF_INET;
    if (getaddrinfo(hostName, NULL, &hints, &res) != 0)
        return ZUN_ERROR;

    sockaddr_in *bestAddr = (sockaddr_in *)res->ai_addr;
    for (addrinfo *ptr = res; ptr != NULL; ptr = ptr->ai_next)
    {
        sockaddr_in *addr = (sockaddr_in *)ptr->ai_addr;
        char *ipStr = inet_ntoa(addr->sin_addr);
        if (strncmp(ipStr, "127.", 4) != 0)
        {
            bestAddr = addr;
            break;
        }
    }

    strncpy(buffer, inet_ntoa(bestAddr->sin_addr), bufferSize);
    buffer[bufferSize - 1] = '\0';
    freeaddrinfo(res);
    return ZUN_SUCCESS;
}

ZunResult NetworkManager::GetPublicIP(char *buffer, int bufferSize)
{
    NetworkLog("GetPublicIP: entered\n");
    if (Initialize() != ZUN_SUCCESS)
    {
        NetworkLog("GetPublicIP: Initialize failed\n");
        return ZUN_ERROR;
    }

    int error = 0;
    UPNPDev *devList = upnpDiscover(2000, NULL, NULL, UPNP_LOCAL_PORT_ANY, 0, 2, &error);
    if (devList != NULL)
    {
        UPNPUrls urls;
        IGDdatas data;
        char lanAddr[64] = {0};
        char wanAddr[64] = {0};
        memset(&urls, 0, sizeof(urls));
        memset(&data, 0, sizeof(data));

        int igdResult = UPNP_GetValidIGD(devList, &urls, &data, lanAddr, sizeof(lanAddr), wanAddr, sizeof(wanAddr));
        freeUPNPDevlist(devList);
        if (igdResult != UPNP_NO_IGD)
        {
            char wanIp[64] = {0};
            if (UPNP_GetExternalIPAddress(urls.controlURL, data.first.servicetype, wanIp) == UPNPCOMMAND_SUCCESS &&
                wanIp[0] != '\0' && strncmp(wanIp, "127.", 4) != 0 && strncmp(wanIp, "0.", 2) != 0)
            {
                strncpy(buffer, wanIp, bufferSize - 1);
                buffer[bufferSize - 1] = '\0';
                FreeUPNPUrls(&urls);
                NetworkLog("GetPublicIP: UPnP retrieved WAN IP %s\n", buffer);
                return ZUN_SUCCESS;
            }
            NetworkLog("GetPublicIP: UPnP returned invalid IP '%s', falling back\n", wanIp);
            FreeUPNPUrls(&urls);
        }
    }

    NetworkLog("GetPublicIP: UPnP WAN IP retrieval failed, falling back to HTTP api.ipify.org\n");

    const char *hostname = "api.ipify.org";
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET)
    {
        NetworkLog("GetPublicIP: socket creation failed\n");
        return ZUN_ERROR;
    }

    addrinfo hints = {0}, *res = NULL;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(hostname, "80", &hints, &res) != 0)
    {
        NetworkLog("GetPublicIP: getaddrinfo failed for %s\n", hostname);
        closesocket(sock);
        return ZUN_ERROR;
    }
    if (connect(sock, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR)
    {
        NetworkLog("GetPublicIP: connect failed to %s\n", hostname);
        freeaddrinfo(res);
        closesocket(sock);
        return ZUN_ERROR;
    }
    freeaddrinfo(res);

    const char *request = "GET / HTTP/1.1\r\nHost: api.ipify.org\r\nConnection: close\r\n\r\n";
    send(sock, request, (int)strlen(request), 0);

    char response[512] = {0};
    int totalReceived = 0;
    int recvLen;
    while (totalReceived < (int)(sizeof(response) - 1))
    {
        recvLen = recv(sock, response + totalReceived, (int)(sizeof(response) - 1) - totalReceived, 0);
        if (recvLen <= 0)
            break;
        totalReceived += recvLen;
    }
    closesocket(sock);

    if (totalReceived > 0)
    {

        const char *body = strstr(response, "\r\n\r\n");
        if (body != NULL)
        {
            body += 4;
        }
        else
        {
            body = response;
        }

        int bodyLen = (int)strlen(body);

        while (bodyLen > 0 && (body[bodyLen - 1] == '\r' || body[bodyLen - 1] == '\n' || body[bodyLen - 1] == ' '))
        {
            bodyLen--;
        }
        if (bodyLen > 0 && bodyLen < bufferSize && strncmp(body, "127.", 4) != 0 && strncmp(body, "0.", 2) != 0)
        {
            strncpy(buffer, body, bodyLen);
            buffer[bodyLen] = '\0';
            NetworkLog("GetPublicIP: HTTP retrieved WAN IP %s\n", buffer);
            return ZUN_SUCCESS;
        }
    }
    NetworkLog("GetPublicIP: failed to retrieve valid IP from %s\n", hostname);
    return ZUN_ERROR;
}

ZunResult NetworkManager::SendPacket(const void *data, int size)
{
    if (m_socket == NULL)
    {
        NetworkLog("SendPacket: m_socket is NULL\n");
        return ZUN_ERROR;
    }
    SOCKET sock = (SOCKET)m_socket;

    int res = send(sock, (const char *)data, size, 0);
    if (res == SOCKET_ERROR)
    {
        NetworkLog("SendPacket: send failed with %d\n", WSAGetLastError());
        Cleanup(false);
        return ZUN_ERROR;
    }
    NetworkLog("SendPacket: sent %d bytes OK\n", size);
    return ZUN_SUCCESS;
}

ZunResult NetworkManager::RecvPacketNonBlocking(void *data, int size, bool *received)
{
    *received = false;
    if (m_socket == NULL)
        return ZUN_ERROR;
    SOCKET sock = (SOCKET)m_socket;

    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(sock, &readSet);
    timeval tv = {0, 0};

    int selectRes = select((int)sock + 1, &readSet, NULL, NULL, &tv);
    if (selectRes > 0)
    {
        if (FD_ISSET(sock, &readSet))
        {
            int receivedBytes = 0;
            while (receivedBytes < size)
            {
                int res = recv(sock, (char *)data + receivedBytes, size - receivedBytes, 0);
                if (res <= 0)
                {
                    NetworkLog("RecvPacketNonBlocking: recv failed or connection closed\n");
                    Cleanup(false);
                    return ZUN_ERROR;
                }
                receivedBytes += res;
            }
            *received = true;
            return ZUN_SUCCESS;
        }
    }
    if (selectRes == SOCKET_ERROR)
    {
        NetworkLog("RecvPacketNonBlocking: select failed with %d\n", WSAGetLastError());
        Cleanup(false);
        return ZUN_ERROR;
    }
    return ZUN_SUCCESS;
}

ZunResult NetworkManager::RecvPacketBlocking(void *data, int size, int timeoutMs)
{
    if (m_socket == NULL)
        return ZUN_ERROR;
    SOCKET sock = (SOCKET)m_socket;

    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(sock, &readSet);
    timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    int selectRes = select((int)sock + 1, &readSet, NULL, NULL, &tv);
    if (selectRes > 0)
    {
        if (FD_ISSET(sock, &readSet))
        {
            int receivedBytes = 0;
            while (receivedBytes < size)
            {
                int res = recv(sock, (char *)data + receivedBytes, size - receivedBytes, 0);
                if (res <= 0)
                {
                    NetworkLog("RecvPacketBlocking: recv failed or connection closed\n");
                    Cleanup(false);
                    return ZUN_ERROR;
                }
                receivedBytes += res;
            }
            return ZUN_SUCCESS;
        }
    }
    if (selectRes == 0)
    {
        NetworkLog("RecvPacketBlocking: timeout\n");
        return ZUN_ERROR;
    }
    NetworkLog("RecvPacketBlocking: select failed with %d\n", WSAGetLastError());
    Cleanup(false);
    return ZUN_ERROR;
}

int NetworkManager::NegotiateDelay(int configuredDelay)
{
    if (m_socket == NULL || !m_isConnected)
    {
        return (configuredDelay > 0) ? configuredDelay : 3;
    }

    NetworkLog("NegotiateDelay: configuredDelay = %d, isHost = %s\n", configuredDelay, m_isHost ? "true" : "false");

    DelayNegotiationPacket localPacket;
    localPacket.magic = 0x44454C59;
    localPacket.type = 3;
    localPacket.timestamp = GetTickCount();
    localPacket.delayValue = configuredDelay;

    if (SendPacket(&localPacket, sizeof(localPacket)) != ZUN_SUCCESS)
    {
        NetworkLog("NegotiateDelay: ConfigExchange send failed\n");
        return 3;
    }

    DelayNegotiationPacket remotePacket;
    if (RecvPacketBlocking(&remotePacket, sizeof(remotePacket), 3000) != ZUN_SUCCESS)
    {
        NetworkLog("NegotiateDelay: ConfigExchange recv failed\n");
        return 3;
    }

    if (remotePacket.magic != 0x44454C59 || remotePacket.type != 3)
    {
        NetworkLog("NegotiateDelay: Invalid ConfigExchange magic or type\n");
        return 3;
    }

    int remoteConfiguredDelay = remotePacket.delayValue;
    NetworkLog("NegotiateDelay: remoteConfiguredDelay = %d\n", remoteConfiguredDelay);

    if (configuredDelay > 0 || remoteConfiguredDelay > 0)
    {
        int finalDelay = configuredDelay;
        if (remoteConfiguredDelay > finalDelay)
        {
            finalDelay = remoteConfiguredDelay;
        }
        NetworkLog("NegotiateDelay: Using static delay = %d\n", finalDelay);
        return finalDelay;
    }

    if (m_isHost)
    {
        u32 maxRtt = 0;
        int successfulPings = 0;

        for (int i = 0; i < 10; i++)
        {
            DelayNegotiationPacket pingPacket;
            pingPacket.magic = 0x44454C59;
            pingPacket.type = 1;
            pingPacket.timestamp = GetTickCount();
            pingPacket.delayValue = 0;

            if (SendPacket(&pingPacket, sizeof(pingPacket)) != ZUN_SUCCESS)
            {
                break;
            }

            DelayNegotiationPacket pongPacket;
            if (RecvPacketBlocking(&pongPacket, sizeof(pongPacket), 200) == ZUN_SUCCESS)
            {
                if (pongPacket.magic == 0x44454C59 && pongPacket.type == 2)
                {
                    u32 rtt = GetTickCount() - pongPacket.timestamp;
                    if (rtt > maxRtt)
                    {
                        maxRtt = rtt;
                    }
                    successfulPings++;
                }
            }
            Sleep(10);
        }

        int decisionDelay = 3;
        if (successfulPings > 0)
        {

            decisionDelay = (maxRtt / 2 + 17 + 16) / 17;
            if (decisionDelay < 2)
                decisionDelay = 2;
            if (decisionDelay > 10)
                decisionDelay = 10;
        }

        NetworkLog("NegotiateDelay: AutoDelay RTT max = %u ms, successful = %d/10. Decision delay = %d\n", maxRtt,
                   successfulPings, decisionDelay);

        DelayNegotiationPacket decisionPacket;
        decisionPacket.magic = 0x44454C59;
        decisionPacket.type = 4;
        decisionPacket.timestamp = GetTickCount();
        decisionPacket.delayValue = decisionDelay;

        SendPacket(&decisionPacket, sizeof(decisionPacket));
        return decisionDelay;
    }
    else
    {
        DWORD startTime = GetTickCount();
        int pingCount = 0;
        int decisionDelay = 3;

        while (GetTickCount() - startTime < 3000)
        {
            DelayNegotiationPacket receivedPacket;
            if (RecvPacketBlocking(&receivedPacket, sizeof(receivedPacket), 100) == ZUN_SUCCESS)
            {
                if (receivedPacket.magic == 0x44454C59)
                {
                    if (receivedPacket.type == 1)
                    {
                        DelayNegotiationPacket pongPacket;
                        pongPacket.magic = 0x44454C59;
                        pongPacket.type = 2;
                        pongPacket.timestamp = receivedPacket.timestamp;
                        pongPacket.delayValue = 0;

                        SendPacket(&pongPacket, sizeof(pongPacket));
                        pingCount++;
                    }
                    else if (receivedPacket.type == 4)
                    {
                        decisionDelay = receivedPacket.delayValue;
                        NetworkLog("NegotiateDelay: Client received decision delay = %d\n", decisionDelay);
                        break;
                    }
                }
            }
        }
        return decisionDelay;
    }
}

} // namespace th06
