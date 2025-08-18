#pragma once

#include <stdint.h>
#include <string>
#include <vector>

namespace ProtocolKind{
enum Enum : uint8_t{
    UNKNOWN = 0,
    UDPv4,
    UDPv6,
    TCPv4,
    TCPv6
};
}
__attribute__((__unused__)) static const char* ProtocolKind_txt[] = {"UNKNOWN", "UDPv4", "UDPv6", "TCPv4", "TCPv6"};

namespace CastType{
enum Enum : uint8_t{
    UNKNOWN = 0,
    UNICAST,
    MULTICAST
};
}
__attribute__((__unused__)) static const char* CastType_txt[] = {"UNKNOWN", "UNICAST", "MULTICAST"};

namespace EventType{
enum Enum : uint8_t{
    UNKNOWN = 0,
    ADDED,
    REMOVED
};
}
__attribute__((__unused__)) static const char* EventType_txt[] = {"UNKNOWN", "ADDED", "REMOVED"};


struct SubscriberEvent{
    EventType::Enum type;
    std::string topic;
    CastType::Enum cast;
    ProtocolKind::Enum kind;
    uint8_t address[16];
    uint16_t port;
};


class DiscoveryListenerImpl;

class DiscoveryListener{
    DiscoveryListenerImpl* impl;
public:
    DiscoveryListener(size_t domain_id);
    ~DiscoveryListener();
    void add_watched_topic(std::string name);
    void remove_watched_topic(std::string name);
    std::vector<SubscriberEvent> get_subscriber_changes();
};
