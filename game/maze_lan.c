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
#include <sys/select.h>
typedef int LanSocket;
typedef socklen_t LanSocklen;
#define BAD_SOCKET (-1)
#define CloseSocket close
#endif

#ifdef BENNO_IOS
#include <dns_sd.h>
#include <ifaddrs.h>
#include <net/if.h>
#endif

static bool denied;
#ifdef BENNO_IOS
static DNSServiceRef permissionBrowser;
static bool permissionChecked;

static void DNSSD_API PermissionResult(DNSServiceRef ref, DNSServiceFlags flags, uint32_t interfaceIndex,
                                      DNSServiceErrorType error, const char *name, const char *type,
                                      const char *domain, void *context)
{
    (void)ref; (void)flags; (void)interfaceIndex; (void)name; (void)type; (void)domain; (void)context;
    if (error == kDNSServiceErr_PolicyDenied) denied = true;
}
#endif

bool MazeLanAccessDenied(void)
{
#ifdef BENNO_IOS
    if (!permissionChecked && !denied)
    {
        permissionChecked = true;
        DNSServiceErrorType error = DNSServiceBrowse(&permissionBrowser, 0, 0, "_project3d._tcp",
                                                     "local.", PermissionResult, NULL);
        if (error == kDNSServiceErr_PolicyDenied) denied = true;
    }
    if (permissionBrowser && !denied)
    {
        int fd = DNSServiceRefSockFD(permissionBrowser);
        if (fd >= 0 && fd < FD_SETSIZE)
        {
            fd_set ready;
            FD_ZERO(&ready);
            FD_SET(fd, &ready);
            struct timeval immediate = {0};
            if (select(fd + 1, &ready, NULL, NULL, &immediate) > 0)
            {
                DNSServiceErrorType error = DNSServiceProcessResult(permissionBrowser);
                if (error == kDNSServiceErr_PolicyDenied) denied = true;
            }
        }
    }
#endif
    return denied;
}

void ResetMazeLanPermissionCheck(void)
{
#ifdef BENNO_IOS
    if (permissionBrowser) DNSServiceRefDeallocate(permissionBrowser);
    permissionBrowser = NULL;
    permissionChecked = false;
#endif
    denied = false;
}

static void RecordSocketError(int error)
{
#ifdef _WIN32
    if (error == WSAEACCES) denied = true;
#else
    if (error == EACCES || error == EPERM) denied = true;
#endif
}

static void RecordLastSocketError(void)
{
#ifdef _WIN32
    RecordSocketError(WSAGetLastError());
#else
    RecordSocketError(errno);
#endif
}

enum { LAN_MAGIC = 0x33444d01, MAX_PACKET = 8 + sizeof(MazeLanFrame) + sizeof(MazeLanLayout) };
struct MazeLan {
    LanSocket listener, peer, advertisement;
    bool infinite;
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
#ifdef BENNO_IOS
    struct ifaddrs *interfaces = NULL;
    if (getifaddrs(&interfaces) == 0)
    {
        bool found = false;
        for (struct ifaddrs *it = interfaces; it; it = it->ifa_next)
            if (it->ifa_addr && it->ifa_addr->sa_family == AF_INET &&
                (it->ifa_flags & IFF_UP) && strcmp(it->ifa_name, "en0") == 0)
            {
                const struct sockaddr_in *wifi = (const struct sockaddr_in *)it->ifa_addr;
                found = inet_ntop(AF_INET, &wifi->sin_addr, address, 16) != NULL;
                if (found) break;
            }
        freeifaddrs(interfaces);
        if (found) return;
    }
    strcpy(address, "127.0.0.1");
#else
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
#endif
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
    lan->listener = lan->peer = lan->advertisement = BAD_SOCKET;
    lan->host = host;
    lan->opened = GetTime();
    LanSocket fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (host) lan->listener = fd;
    else lan->peer = fd;
    struct sockaddr_in endpoint = {0};
    endpoint.sin_family = AF_INET;
    endpoint.sin_port = htons(MAZE_LAN_PORT);
    if (fd == BAD_SOCKET || !Nonblocking(fd)) { RecordLastSocketError(); goto failed; }
    if (host)
    {
#ifndef _WIN32
        int reuse = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif
        if (bind(fd, (struct sockaddr *)&endpoint, sizeof(endpoint)) != 0 || listen(fd, 1) != 0)
        {
            RecordLastSocketError();
            goto failed;
        }
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
        if (connect(fd, (struct sockaddr *)&endpoint, sizeof(endpoint)) != 0 && !WouldBlock())
        {
            RecordLastSocketError();
            goto failed;
        }
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
    if (lan->advertisement != BAD_SOCKET) CloseSocket(lan->advertisement);
#ifdef _WIN32
    WSACleanup();
#endif
    free(lan);
}

// Discovery uses a separate UDP socket so probes never occupy the co-op slot.
enum { DISCOVER_QUERY = 0x33444d11, DISCOVER_CLASSIC = 0x33444d12, DISCOVER_INFINITE = 0x33444d13 };

typedef struct NearbyMazeGame {
    MazeLanGame game;
    double lastSeen;
} NearbyMazeGame;

struct MazeLanDiscovery {
    LanSocket socket;
    uint32_t subnet;
    unsigned int next;
    double resumeAt;
    bool broadcast;
    NearbyMazeGame games[MAZE_LAN_GAMES];
};

bool AdvertiseMazeLan(MazeLan *lan, bool infinite)
{
    if (!lan || !lan->host) return false;
    lan->infinite = infinite;
    LanSocket fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd == BAD_SOCKET) { RecordLastSocketError(); return false; }
    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_port = htons(MAZE_LAN_PORT);
    if (!Nonblocking(fd) || bind(fd, (struct sockaddr *)&address, sizeof(address)) != 0)
    {
        RecordLastSocketError();
        CloseSocket(fd);
        return false;
    }
    lan->advertisement = fd;
    return true;
}

static void ReplyToDiscovery(const MazeLan *lan)
{
    if (lan->advertisement == BAD_SOCKET) return;
    for (int i = 0; i < 16; ++i)
    {
        unsigned char data[5];
        struct sockaddr_in sender;
        LanSocklen size = sizeof(sender);
        int bytes = recvfrom(lan->advertisement, (char *)data, sizeof(data), 0,
                             (struct sockaddr *)&sender, &size);
        if (bytes < 0) break;
        uint32_t message;
        if (bytes != sizeof(message)) continue;
        memcpy(&message, data, sizeof(message));
        if (ntohl(message) != DISCOVER_QUERY) continue;
        message = htonl(lan->infinite ? DISCOVER_INFINITE : DISCOVER_CLASSIC);
        sendto(lan->advertisement, (const char *)&message, sizeof(message), 0,
               (struct sockaddr *)&sender, size);
    }
}

MazeLanDiscovery *OpenMazeLanDiscovery(void)
{
    MazeLanDiscovery *discovery = calloc(1, sizeof(*discovery));
    if (!discovery) return NULL;
#ifdef _WIN32
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) { free(discovery); return NULL; }
#endif
    discovery->socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (discovery->socket == BAD_SOCKET || !Nonblocking(discovery->socket))
    {
        RecordLastSocketError();
        CloseMazeLanDiscovery(discovery);
        return NULL;
    }
#ifndef BENNO_IOS
    int enabled = 1;
    discovery->broadcast = setsockopt(discovery->socket, SOL_SOCKET, SO_BROADCAST,
                                      (const char *)&enabled, sizeof(enabled)) == 0;
#endif
    char ip[16];
    FindAddress(ip);
    struct in_addr local;
    if (inet_pton(AF_INET, ip, &local) == 1)
    {
        uint32_t address = ntohl(local.s_addr);
        if ((address >> 24) == 10 || (address >> 24) == 127 || (address >> 20) == 0xac1 ||
            (address >> 16) == 0xc0a8 || (address >> 16) == 0xa9fe)
            discovery->subnet = address & 0xffffff00u;
    }
    discovery->next = 1;
    return discovery;
}

void CloseMazeLanDiscovery(MazeLanDiscovery *discovery)
{
    if (!discovery) return;
    if (discovery->socket != BAD_SOCKET) CloseSocket(discovery->socket);
#ifdef _WIN32
    WSACleanup();
#endif
    free(discovery);
}

static void SendDiscoveryQuery(const MazeLanDiscovery *discovery, uint32_t address)
{
    struct sockaddr_in target = {0};
    target.sin_family = AF_INET;
    target.sin_addr.s_addr = htonl(address);
    target.sin_port = htons(MAZE_LAN_PORT);
    const uint32_t message = htonl(DISCOVER_QUERY);
    if (sendto(discovery->socket, (const char *)&message, sizeof(message), 0,
               (struct sockaddr *)&target, sizeof(target)) < 0 && !WouldBlock()) RecordLastSocketError();
}

void PollMazeLanDiscovery(MazeLanDiscovery *discovery)
{
    double now = GetTime();
    for (int i = 0; i < MAZE_LAN_GAMES; ++i)
        if (now - discovery->games[i].lastSeen > 8) discovery->games[i].game.address[0] = 0;

    if (now >= discovery->resumeAt)
    {
        if (discovery->next == 1)
        {
#ifndef BENNO_IOS
            if (discovery->broadcast) SendDiscoveryQuery(discovery, INADDR_BROADCAST);
#endif
            if (discovery->subnet == 0x7f000000u) SendDiscoveryQuery(discovery, INADDR_LOOPBACK);
        }
        // A bounded unicast sweep also works on iOS without the multicast entitlement.
        for (int i = 0; i < 8 && discovery->next < 255 && !denied; ++i)
            if (discovery->subnet) SendDiscoveryQuery(discovery, discovery->subnet | discovery->next++);
            else discovery->next = 255;
        if (discovery->next == 255)
        {
            discovery->next = 1;
            discovery->resumeAt = now + 1.5;
        }
        else discovery->resumeAt = now + 0.025;
    }

    for (int i = 0; i < 16; ++i)
    {
        unsigned char data[5];
        struct sockaddr_in sender;
        LanSocklen size = sizeof(sender);
        int bytes = recvfrom(discovery->socket, (char *)data, sizeof(data), 0,
                             (struct sockaddr *)&sender, &size);
        if (bytes < 0) break;
        uint32_t message;
        if (bytes != sizeof(message) || ntohs(sender.sin_port) != MAZE_LAN_PORT) continue;
        memcpy(&message, data, sizeof(message));
        message = ntohl(message);
        if (message != DISCOVER_CLASSIC && message != DISCOVER_INFINITE) continue;
        MazeLanGame game = { .infinite = message == DISCOVER_INFINITE };
        char ip[16];
        if (!inet_ntop(AF_INET, &sender.sin_addr, ip, sizeof(ip))) continue;
        snprintf(game.address, sizeof(game.address), "%s:%u", ip, (unsigned int)ntohs(sender.sin_port));
        int slot = -1;
        for (int j = 0; j < MAZE_LAN_GAMES; ++j)
        {
            const char *listed = discovery->games[j].game.address;
            if (strcmp(listed, game.address) == 0) { slot = j; break; }
            if (!*listed && slot < 0) slot = j;
        }
        if (slot >= 0) discovery->games[slot] = (NearbyMazeGame){ game, now };
    }
}

int ListMazeLanGames(const MazeLanDiscovery *discovery, MazeLanGame games[MAZE_LAN_GAMES])
{
    int count = 0;
    if (discovery)
        for (int i = 0; i < MAZE_LAN_GAMES; ++i)
            if (discovery->games[i].game.address[0]) games[count++] = discovery->games[i].game;
    return count;
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
            ReplyToDiscovery(lan);
            lan->peer = accept(lan->listener, NULL, NULL);
            if (lan->peer == BAD_SOCKET)
            {
                lan->failed = !WouldBlock();
                if (lan->failed) RecordLastSocketError();
                return;
            }
            if (!Nonblocking(lan->peer)) { lan->failed = true; return; }
            CloseSocket(lan->listener);
            lan->listener = BAD_SOCKET;
            if (lan->advertisement != BAD_SOCKET)
            {
                CloseSocket(lan->advertisement);
                lan->advertisement = BAD_SOCKET;
            }
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
                RecordSocketError(error);
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
        else if (n == 0 || !WouldBlock())
        {
            if (n < 0) RecordLastSocketError();
            lan->failed = true;
            return;
        }
    }

    int n = recv(lan->peer, (char *)lan->in + lan->readSize, MAX_PACKET - lan->readSize, 0);
    if (n == 0 || (n < 0 && !WouldBlock()))
    {
        if (n < 0) RecordLastSocketError();
        lan->failed = true;
        return;
    }
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
