#include "maze_lan.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET LanSocket;
typedef int LanSocklen;
#define BAD_SOCKET INVALID_SOCKET
#define CloseSocket closesocket
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int LanSocket;
typedef socklen_t LanSocklen;
#define BAD_SOCKET (-1)
#define CloseSocket close
#endif

enum { LAN_MAGIC = 0x33444d01, MAX_PACKET = 8 + sizeof(MazeLanFrame) + sizeof(MazeLanLayout) };
struct MazeLan {
    LanSocket listener, peer;
    bool host, connected, failed, receivedInput, receivedFrame, receivedLayout;
    char address[MAZE_LAN_ADDRESS_CAPACITY];
    double opened, lastReceive;
    int sent, sendSize, readSize;
    unsigned char out[MAX_PACKET], in[MAX_PACKET];
    MazeLanInput input;
    MazeLanFrame frame;
    MazeLanLayout layout;
};

static bool WouldBlock(void)
{
#ifdef _WIN32
    int error = WSAGetLastError();
    return error == WSAEWOULDBLOCK || error == WSAEINPROGRESS || error == WSAEALREADY;
#else
    return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS || errno == EINTR;
#endif
}

static bool Nonblocking(LanSocket fd)
{
#ifdef _WIN32
    u_long on = 1;
    return ioctlsocket(fd, FIONBIO, &on) == 0;
#else
#ifdef SO_NOSIGPIPE
    int enabled = 1;
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled));
#endif
    return fcntl(fd, F_SETFL, O_NONBLOCK) == 0;
#endif
}

static void FindAddress(char address[16])
{
    strcpy(address, "127.0.0.1");
    LanSocket probe = socket(AF_INET, SOCK_DGRAM, 0);
    if (probe == BAD_SOCKET) return;
    // A UDP connect selects an interface without transmitting a packet.
    struct sockaddr_in route = {0};
    route.sin_family = AF_INET;
    route.sin_port = htons(MAZE_LAN_PORT);
    inet_pton(AF_INET, "192.0.2.1", &route.sin_addr);
    if (connect(probe, (struct sockaddr *)&route, sizeof(route)) == 0)
    {
        LanSocklen size = sizeof(route);
        if (getsockname(probe, (struct sockaddr *)&route, &size) == 0)
            inet_ntop(AF_INET, &route.sin_addr, address, 16);
    }
    CloseSocket(probe);
}

static bool ParseEndpoint(const char *address, struct sockaddr_in *endpoint)
{
    if (!address) return false;
    const char *separator = strchr(address, ':');
    size_t length = separator ? (size_t)(separator - address) : strlen(address);
    char ip[16];
    if (!length || length >= sizeof(ip)) return false;
    memcpy(ip, address, length);
    ip[length] = 0;
    if (inet_pton(AF_INET, ip, &endpoint->sin_addr) != 1) return false;

    unsigned int port = MAZE_LAN_PORT;
    if (separator)
    {
        const char *digits = separator + 1;
        if (!*digits || strlen(digits) > 5) return false;
        port = 0;
        for (; *digits; ++digits)
        {
            if (*digits < '0' || *digits > '9') return false;
            port = port * 10 + (unsigned int)(*digits - '0');
        }
        if (port == 0 || port > 65535) return false;
    }
    endpoint->sin_port = htons((uint16_t)port);
    return true;
}

MazeLan *OpenMazeLan(bool host, const char *address)
{
    MazeLan *lan = calloc(1, sizeof(*lan));
    if (!lan) return NULL;
#ifdef _WIN32
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
    {
        free(lan);
        return NULL;
    }
#endif
    lan->listener = lan->peer = BAD_SOCKET;
    lan->host = host;
    lan->opened = GetTime();
    LanSocket fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (host) lan->listener = fd;
    else lan->peer = fd;
    struct sockaddr_in endpoint = {0};
    endpoint.sin_family = AF_INET;
    endpoint.sin_port = htons(MAZE_LAN_PORT);
    if (fd == BAD_SOCKET || !Nonblocking(fd)) goto failed;
    if (host)
    {
#ifndef _WIN32
        int reuse = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif
        if (bind(fd, (struct sockaddr *)&endpoint, sizeof(endpoint)) != 0 || listen(fd, 1) != 0) goto failed;
        char ip[16];
        FindAddress(ip);
        snprintf(lan->address, sizeof(lan->address), "%s:%d", ip, MAZE_LAN_PORT);
    }
    else
    {
        if (!ParseEndpoint(address, &endpoint)) goto failed;
        char ip[16];
        if (!inet_ntop(AF_INET, &endpoint.sin_addr, ip, sizeof(ip))) goto failed;
        snprintf(lan->address, sizeof(lan->address), "%s:%u", ip, (unsigned int)ntohs(endpoint.sin_port));
        if (connect(fd, (struct sockaddr *)&endpoint, sizeof(endpoint)) != 0 && !WouldBlock()) goto failed;
    }
    return lan;

failed:
    CloseMazeLan(lan);
    return NULL;
}

void CloseMazeLan(MazeLan *lan)
{
    if (!lan) return;
    if (lan->peer != BAD_SOCKET) CloseSocket(lan->peer);
    if (lan->listener != BAD_SOCKET) CloseSocket(lan->listener);
#ifdef _WIN32
    WSACleanup();
#endif
    free(lan);
}

const char *MazeLanAddress(const MazeLan *lan) { return lan->address; }
bool MazeLanReady(const MazeLan *lan) { return lan && lan->connected && !lan->failed; }
bool MazeLanFailed(const MazeLan *lan) { return lan && lan->failed; }

static bool ReadPacket(MazeLan *lan, const unsigned char *data, unsigned int size)
{
    if (lan->host)
    {
        MazeLanInput input;
        if (size != sizeof(input)) return false;
        memcpy(&input, data, size);
        if (!isfinite(input.move.x) || !isfinite(input.move.y) ||
            !isfinite(input.look.x) || !isfinite(input.look.y)) return false;
        lan->input.move = input.move;
        lan->input.look.x += fmaxf(-250, fminf(250, input.look.x));
        lan->input.look.y += fmaxf(-250, fminf(250, input.look.y));
        lan->input.jump |= input.jump;
        lan->input.retry |= input.retry;
        lan->input.sprint = input.sprint;
        lan->input.continueRun |= input.continueRun;
        lan->receivedInput = true;
    }
    else
    {
        if (size != sizeof(lan->frame) && size != sizeof(lan->frame) + sizeof(lan->layout)) return false;
        memcpy(&lan->frame, data, sizeof(lan->frame));
        if (size > sizeof(lan->frame))
        {
            memcpy(&lan->layout, data + sizeof(lan->frame), sizeof(lan->layout));
            lan->receivedLayout = true;
        }
        lan->receivedFrame = true;
    }
    return true;
}

static void Connected(MazeLan *lan)
{
    int enabled = 1;
    setsockopt(lan->peer, IPPROTO_TCP, TCP_NODELAY, (const char *)&enabled, sizeof(enabled));
    lan->connected = true;
    lan->lastReceive = GetTime();
}

void PollMazeLan(MazeLan *lan)
{
    if (!lan || lan->failed) return;
    if (!lan->connected)
    {
        if (lan->host)
        {
            lan->peer = accept(lan->listener, NULL, NULL);
            if (lan->peer == BAD_SOCKET) { lan->failed = !WouldBlock(); return; }
            if (!Nonblocking(lan->peer)) { lan->failed = true; return; }
            CloseSocket(lan->listener);
            lan->listener = BAD_SOCKET;
        }
        else
        {
            struct sockaddr_in peer;
            LanSocklen size = sizeof(peer);
            if (getpeername(lan->peer, (struct sockaddr *)&peer, &size) != 0)
            {
                int error = 0;
                size = sizeof(error);
                getsockopt(lan->peer, SOL_SOCKET, SO_ERROR, (char *)&error, &size);
                lan->failed = error != 0 || GetTime() - lan->opened > 8;
                return;
            }
        }
        Connected(lan);
    }

    if (lan->sent < lan->sendSize)
    {
#ifdef MSG_NOSIGNAL
        const int flags = MSG_NOSIGNAL;
#else
        const int flags = 0;
#endif
        int n = send(lan->peer, (const char *)lan->out + lan->sent, lan->sendSize - lan->sent, flags);
        if (n > 0) lan->sent += n;
        else if (n == 0 || !WouldBlock()) { lan->failed = true; return; }
    }

    int n = recv(lan->peer, (char *)lan->in + lan->readSize, MAX_PACKET - lan->readSize, 0);
    if (n == 0 || (n < 0 && !WouldBlock())) { lan->failed = true; return; }
    if (n > 0)
    {
        lan->lastReceive = GetTime();
        lan->readSize += n;
    }
    while (lan->readSize >= 8)
    {
        uint32_t header[2];
        memcpy(header, lan->in, sizeof(header));
        unsigned int size = ntohl(header[1]);
        if (ntohl(header[0]) != LAN_MAGIC || !size || size > MAX_PACKET - 8)
        {
            lan->failed = true;
            return;
        }
        if ((unsigned int)lan->readSize < size + 8) break;
        if (!ReadPacket(lan, lan->in + 8, size)) { lan->failed = true; return; }
        lan->readSize -= size + 8;
        memmove(lan->in, lan->in + size + 8, (size_t)lan->readSize);
    }
    if (GetTime() - lan->lastReceive > 12) lan->failed = true;
}

static bool QueuePacket(MazeLan *lan, const void *data, unsigned int size, const MazeLanLayout *layout)
{
    if (!MazeLanReady(lan) || lan->sent != lan->sendSize) return false;
    unsigned int total = size + (layout ? sizeof(*layout) : 0);
    const uint32_t header[] = { htonl(LAN_MAGIC), htonl(total) };
    memcpy(lan->out, header, sizeof(header));
    memcpy(lan->out + 8, data, size);
    if (layout) memcpy(lan->out + 8 + size, layout, sizeof(*layout));
    lan->sent = 0;
    lan->sendSize = (int)total + 8;
    return true;
}

bool SendMazeLanInput(MazeLan *lan, MazeLanInput *input)
{
    if (!QueuePacket(lan, input, sizeof(*input), NULL)) return false;
    input->look = (Vector2){0};
    input->jump = input->retry = input->continueRun = 0;
    return true;
}

bool ReceiveMazeLanInput(MazeLan *lan, MazeLanInput *input)
{
    *input = GetTime() - lan->lastReceive <= 0.25 ? lan->input : (MazeLanInput){0};
    lan->input.look = (Vector2){0};
    lan->input.jump = lan->input.retry = lan->input.continueRun = 0;
    return lan->receivedInput;
}

bool SendMazeLanFrame(MazeLan *lan, const MazeLanFrame *frame, const MazeLanLayout *layout)
{
    return QueuePacket(lan, frame, sizeof(*frame), layout);
}

bool ReceiveMazeLanFrame(MazeLan *lan, MazeLanFrame *frame, MazeLanLayout *layout, bool *changed)
{
    if (!lan->receivedFrame) return false;
    *frame = lan->frame;
    *changed = lan->receivedLayout;
    if (*changed) *layout = lan->layout;
    lan->receivedLayout = lan->receivedFrame = false;
    return true;
}
