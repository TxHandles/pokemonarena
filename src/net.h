#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Online co-op connection (ENet over UDP). One host, one guest. Kept free of raylib so the
// Windows socket headers don't clash with it.
namespace net {

constexpr int DEFAULT_PORT = 47777;

enum class State { Off, Waiting, Connecting, Connected, Failed };

// Open the connection point. The host listens on 'port'; a guest uses any free port. Either way
// it starts finding this computer's internet connection code.
bool Open(bool host, int port, std::string& err);
bool Connect(const std::string& address, std::string& err);  // guest: "1.2.3.4", "1.2.3.4:5000", a hostname, or a code
bool IsHost();
void Close();                                              // disconnect and shut down
void Poll();                                               // call every frame
State GetState();
const std::string& StatusText();
int PingMs();

void Send(const std::string& msg);                         // reliable, in order
bool Receive(std::string& msg);                            // next message, if any

// Addresses this computer can be reached at on its own network / VPNs (Radmin, ZeroTier).
std::vector<std::string> LocalAddresses();
// Internet connection code (public address found through a STUN server), "" until known.
const std::string& MyCode();
bool CodeLooksValid(const std::string& code);
// Host side: the friend's code, so the host's router lets their connection in (hole punching).
void PunchTo(const std::string& friendCode);

} // namespace net
