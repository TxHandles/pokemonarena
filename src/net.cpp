// Online co-op transport: ENet (reliable, ordered messages over UDP), plus a STUN lookup to make
// a "connection code" (this computer's internet address) and UDP hole punching so friends can
// connect without forwarding ports on most home routers.
#include "net.h"
#include <enet/enet.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <deque>
#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#endif

namespace net {
namespace {

bool gInit = false;
ENetHost* gHost = nullptr;
ENetPeer* gPeer = nullptr;
bool gIsHost = false;
bool gReplaced = false;
State gState = State::Off;
std::string gStatus;
std::deque<std::string> gInbox;
std::string gCode;

// STUN
struct StunServer { const char* name; int port; };
const StunServer kStun[] = {{"stun.l.google.com", 19302}, {"stun1.l.google.com", 19302}, {"stun.cloudflare.com", 3478}};
std::vector<ENetAddress> gStunAddrs;
unsigned char gStunTx[12];
double gStunNext = 0;
int gStunTries = 0;

// Hole punching
bool gPunch = false;
ENetAddress gPunchAddr{};
double gPunchNext = 0;
const char kPunch[] = "ACPUNCH";

double Now() { return enet_time_get() / 1000.0; }

// Testing aid: ARENA_NET_LAG_MS=100 holds every outgoing message that long (fake distance).
struct Delayed { double at; std::string data; };
std::deque<Delayed> gOutbox;
int LagMs() {
    static int lag = -1;
    if (lag < 0) { const char* e = std::getenv("ARENA_NET_LAG_MS"); lag = e ? std::max(0, std::atoi(e)) : 0; }
    return lag;
}
void SendNow(const std::string& msg);

const char kB32[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

std::string MakeCode(uint32_t hostBE, uint16_t port) {
    // 32-bit address (network order) + 16-bit port = 48 bits -> 10 base-32 characters.
    uint8_t ip[4];
    std::memcpy(ip, &hostBE, 4);
    uint64_t v = ((uint64_t)ip[0] << 40) | ((uint64_t)ip[1] << 32) | ((uint64_t)ip[2] << 24) | ((uint64_t)ip[3] << 16) | port;
    std::string s;
    for (int i = 9; i >= 0; i--) s += kB32[(v >> (i * 5)) & 31];
    return "AC-" + s.substr(0, 5) + "-" + s.substr(5);
}

bool ParseCode(const std::string& in, ENetAddress& out) {
    std::string c;
    for (char ch : in) {
        char u = (ch >= 'a' && ch <= 'z') ? (char)(ch - 32) : ch;
        if (u == 'O') u = '0';
        if (u == 'I' || u == 'L') u = '1';
        if (std::strchr(kB32, u) && u) c += u;
    }
    if (c.size() == 12 && c.compare(0, 2, "AC") == 0) c = c.substr(2);
    if (c.size() != 10) return false;
    uint64_t v = 0;
    for (char ch : c) v = (v << 5) | (uint64_t)(std::strchr(kB32, ch) - kB32);
    uint8_t ip[4] = {(uint8_t)(v >> 40), (uint8_t)(v >> 32), (uint8_t)(v >> 24), (uint8_t)(v >> 16)};
    uint32_t be;
    std::memcpy(&be, ip, 4);
    if (be == 0) return false;
    out.host = be;
    out.port = (uint16_t)(v & 0xFFFF);
    return true;
}

void SendRaw(const ENetAddress& to, const void* data, size_t n) {
    if (!gHost) return;
    ENetBuffer b;
    b.data = (void*)data;
    b.dataLength = n;
    enet_socket_send(gHost->socket, &to, &b, 1);
}

void SendStun() {
    for (int i = 0; i < 12; i++) gStunTx[i] = (unsigned char)(enet_time_get() * 31 + i * 97 + gStunTries * 13);
    unsigned char req[20] = {0x00, 0x01, 0x00, 0x00, 0x21, 0x12, 0xA4, 0x42};
    std::memcpy(req + 8, gStunTx, 12);
    for (const auto& a : gStunAddrs) SendRaw(a, req, sizeof req);
}

// Raw packets that aren't ENet's: STUN answers and punch packets.
int ENET_CALLBACK Intercept(ENetHost* host, ENetEvent*) {
    const unsigned char* d = host->receivedData;
    size_t n = host->receivedDataLength;
    if (n >= sizeof kPunch - 1 && std::memcmp(d, kPunch, sizeof kPunch - 1) == 0) return 1;
    if (n >= 20 && d[0] == 0x01 && d[1] == 0x01 && d[4] == 0x21 && d[5] == 0x12 && d[6] == 0xA4 && d[7] == 0x42) {
        if (std::memcmp(d + 8, gStunTx, 12) != 0) return 1;
        size_t len = ((size_t)d[2] << 8) | d[3];
        size_t p = 20, end = std::min(n, 20 + len);
        while (p + 4 <= end) {
            int type = (d[p] << 8) | d[p + 1];
            size_t alen = ((size_t)d[p + 2] << 8) | d[p + 3];
            const unsigned char* a = d + p + 4;
            if ((type == 0x0020 || type == 0x0001) && alen >= 8 && a[1] == 0x01 && p + 4 + alen <= end) {
                uint16_t port = (uint16_t)((a[2] << 8) | a[3]);
                uint8_t ip[4] = {a[4], a[5], a[6], a[7]};
                if (type == 0x0020) {
                    port ^= 0x2112;
                    ip[0] ^= 0x21; ip[1] ^= 0x12; ip[2] ^= 0xA4; ip[3] ^= 0x42;
                }
                uint32_t be;
                std::memcpy(&be, ip, 4);
                gCode = MakeCode(be, port);
                if (type == 0x0020) break;
            }
            p += 4 + ((alen + 3) & ~(size_t)3);
        }
        return 1;
    }
    return 0;
}

bool InitOnce(std::string& err) {
    if (gInit) return true;
    if (enet_initialize() != 0) { err = "Couldn't start networking."; return false; }
    gInit = true;
    return true;
}

} // namespace

bool Open(bool host, int port, std::string& err) {
    Close();
    if (!InitOnce(err)) return false;
    ENetAddress addr;
    addr.host = ENET_HOST_ANY;
    addr.port = (uint16_t)(host ? port : 0);
    gHost = enet_host_create(&addr, host ? 2 : 1, 2, 0, 0);   // host: room for a reconnect while the old link times out
    if (!gHost) {
        err = host ? "Couldn't open port " + std::to_string(port) + " (is another copy of the game hosting?)" : "Couldn't open a network port.";
        return false;
    }
    gHost->intercept = Intercept;
    gIsHost = host;
    gState = host ? State::Waiting : State::Off;
    gStatus = host ? "Waiting for your friend to join..." : "";
    gCode.clear();
    gStunAddrs.clear();
    for (const auto& s : kStun) {
        ENetAddress a;
        if (enet_address_set_host(&a, s.name) == 0) {
            a.port = (uint16_t)s.port;
            gStunAddrs.push_back(a);
        }
    }
    gStunTries = 0;
    gStunNext = 0;
    return true;
}

bool Connect(const std::string& address, std::string& err) {
    if (!gHost) { err = "Not ready."; return false; }
    ENetAddress a;
    std::string s = address;
    while (!s.empty() && s.back() == ' ') s.pop_back();
    while (!s.empty() && s.front() == ' ') s.erase(s.begin());
    bool isCode = ParseCode(s, a) && (s.find('.') == std::string::npos);
    if (!isCode) {
        int port = DEFAULT_PORT;
        size_t c = s.rfind(':');
        if (c != std::string::npos) {
            port = std::atoi(s.c_str() + c + 1);
            s = s.substr(0, c);
        }
        if (s.empty() || enet_address_set_host(&a, s.c_str()) != 0) { err = "Couldn't find that address."; return false; }
        a.port = (uint16_t)port;
    }
    // Punch toward the host too, so our router expects its replies.
    gPunch = true;
    gPunchAddr = a;
    gPunchNext = 0;
    gPeer = enet_host_connect(gHost, &a, 2, 0);
    if (!gPeer) { err = "Couldn't start connecting."; return false; }
    enet_peer_timeout(gPeer, 0, 8000, 15000);
    gState = State::Connecting;
    gStatus = "Connecting...";
    return true;
}

bool IsHost() { return gIsHost; }
bool HasSocket() { return gHost != nullptr; }
bool TakeReplaced() { bool r = gReplaced; gReplaced = false; return r; }

void Rearm() {
    if (!gHost) return;
    if (gPeer) { enet_peer_reset(gPeer); gPeer = nullptr; }
    gInbox.clear();
    gOutbox.clear();
    gReplaced = false;
    gState = gIsHost ? State::Waiting : State::Off;
    gStatus = gIsHost ? "Waiting for your friend to come back..." : "";
}

void DropForTest() {
    if (gPeer) { enet_peer_reset(gPeer); gPeer = nullptr; }
    gInbox.clear();
    gOutbox.clear();
    gState = State::Failed;
    gStatus = "The connection was lost.";
}

void PunchTo(const std::string& friendCode) {
    ENetAddress a;
    if (!ParseCode(friendCode, a)) return;
    gPunch = true;
    gPunchAddr = a;
    gPunchNext = 0;
}

void Close() {
    if (gPeer && gHost) {
        enet_peer_disconnect_now(gPeer, 0);
        enet_host_flush(gHost);
    }
    gPeer = nullptr;
    if (gHost) enet_host_destroy(gHost);
    gHost = nullptr;
    gState = State::Off;
    gStatus.clear();
    gInbox.clear();
    gOutbox.clear();
    gPunch = false;
    gCode.clear();
    gReplaced = false;
}

void Poll() {
    if (!gHost) return;
    double now = Now();
    // Ask the STUN servers what our internet address looks like (a few tries).
    if (gCode.empty() && !gStunAddrs.empty() && gStunTries < 6 && now >= gStunNext) {
        SendStun();
        gStunTries++;
        gStunNext = now + 1.0;
    }
    if (gPunch && gState != State::Connected && now >= gPunchNext) {
        SendRaw(gPunchAddr, kPunch, sizeof kPunch - 1);
        gPunchNext = now + 0.25;
    }
    while (!gOutbox.empty() && gOutbox.front().at <= now) {
        SendNow(gOutbox.front().data);
        gOutbox.pop_front();
    }
    ENetEvent ev;
    while (gHost && enet_host_service(gHost, &ev, 0) > 0) {
        switch (ev.type) {
            case ENET_EVENT_TYPE_CONNECT:
                if (gPeer && gPeer != ev.peer && gState == State::Connected) {
                    if (!gIsHost) { enet_peer_disconnect_now(ev.peer, 0); break; }
                    // Only one friend: a new connection replaces the old one (they came back after a drop).
                    enet_peer_reset(gPeer);
                    gInbox.clear();
                    gOutbox.clear();
                    gReplaced = true;
                }
                gPeer = ev.peer;
                enet_peer_timeout(gPeer, 0, 8000, 15000);
                gState = State::Connected;
                gStatus = "Connected!";
                break;
            case ENET_EVENT_TYPE_RECEIVE:
                gInbox.emplace_back((const char*)ev.packet->data, ev.packet->dataLength);
                enet_packet_destroy(ev.packet);
                break;
            case ENET_EVENT_TYPE_DISCONNECT:
                if (ev.peer == gPeer && gPeer) {
                    gPeer = nullptr;
                    if (gState == State::Connecting) { gState = State::Failed; gStatus = "Couldn't reach your friend's game."; }
                    else if (gIsHost && gState != State::Connected) { gState = State::Waiting; }
                    else { gState = State::Failed; gStatus = "The connection was lost."; }
                }
                break;
            default: break;
        }
    }
}

State GetState() { return gState; }
const std::string& StatusText() { return gStatus; }
int PingMs() { return gPeer ? (int)gPeer->roundTripTime : 0; }

void Send(const std::string& msg) {
    if (!gPeer || gState != State::Connected) return;
    if (LagMs() > 0) { gOutbox.push_back({Now() + LagMs() / 1000.0, msg}); return; }
    SendNow(msg);
}

namespace {
void SendNow(const std::string& msg) {
    if (!gPeer || gState != State::Connected) return;
    ENetPacket* p = enet_packet_create(msg.data(), msg.size(), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(gPeer, 0, p);
    enet_host_flush(gHost);
}
} // namespace

bool Receive(std::string& msg) {
    if (gInbox.empty()) return false;
    msg = std::move(gInbox.front());
    gInbox.pop_front();
    return true;
}

const std::string& MyCode() { return gCode; }
bool CodeLooksValid(const std::string& code) {
    ENetAddress a;
    return ParseCode(code, a);
}

std::vector<std::string> LocalAddresses() {
    std::vector<std::string> out;
#ifdef _WIN32
    std::string err;
    if (!InitOnce(err)) return out;
    char name[256];
    if (gethostname(name, sizeof name) != 0) return out;
    addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET;
    if (getaddrinfo(name, nullptr, &hints, &res) != 0) return out;
    for (addrinfo* p = res; p; p = p->ai_next) {
        char buf[64];
        inet_ntop(AF_INET, &((sockaddr_in*)p->ai_addr)->sin_addr, buf, sizeof buf);
        std::string s = buf;
        if (s.rfind("127.", 0) != 0 && std::find(out.begin(), out.end(), s) == out.end()) out.push_back(s);
    }
    freeaddrinfo(res);
#else
    ifaddrs* ifs = nullptr;
    if (getifaddrs(&ifs) != 0) return out;
    for (ifaddrs* i = ifs; i; i = i->ifa_next) {
        if (!i->ifa_addr || i->ifa_addr->sa_family != AF_INET) continue;
        char buf[64];
        inet_ntop(AF_INET, &((sockaddr_in*)i->ifa_addr)->sin_addr, buf, sizeof buf);
        std::string s = buf;
        if (s.rfind("127.", 0) != 0) out.push_back(s);
    }
    freeifaddrs(ifs);
#endif
    return out;
}

} // namespace net
