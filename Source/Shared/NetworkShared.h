#pragma once

#include "../Engine/tge/math/Vector2.h"

#include <WinSock2.h>
#include <WS2tcpip.h>

#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

constexpr int NETMESSAGE_SIZE = 512;

using NetworkClock = std::chrono::high_resolution_clock;

struct EntityReplicationData
{
    std::uint32_t lastAcknowledgedSequenceNumber = 0;
    std::uint32_t latestPositionChangedSequenceNumber = 0;
    Tga::Vector2f latestSentPosition = {};
    NetworkClock::time_point lastSendTime = {};
};

enum class MessageType : char
{
    Unknown,
    Join,
    Chat,
    Quit,
    Position,
    Update,
    Ack,
    ServerAck,
    ServerAckType,
    PositionAck,
    ServerACKTYPE = ServerAckType
};

enum class MessageObjectType : char
{
    Enemy,
    Object,
    ObjectCircleGo,
    ObjectCirlceGo = ObjectCircleGo
};

struct Enemy
{
    Tga::Vector2f position = {};
    std::map<std::uint32_t, Tga::Vector2f> positionHistory;
    sockaddr_in socketAddress = {};
    std::uint32_t lastSendTime = 0;
    Tga::Vector2f lastKnownPosition = {};
    NetworkClock::time_point lastUpdateTime = {};
};

struct Object
{
    Tga::Vector2f position = {};
    Tga::Vector2f center = {};
    float angle = 0.0f;
    sockaddr_in socketAddress = {};
    std::map<std::uint32_t, Tga::Vector2f> positionHistory;
    Tga::Vector2f lastKnownPosition = {};
    NetworkClock::time_point lastUpdateTime = {};
    int id = 0;
};

struct Message
{
    MessageType type = MessageType::Unknown;
    MessageObjectType objectType = MessageObjectType::Enemy;
    Tga::Vector2f position = {};
    int id = 0;
    sockaddr_in socketAddress = {};
    std::uint32_t sequenceNumber = 0;
    std::uint32_t lastSendTime = 0;
};

struct MessageWithoutSendTime
{
    MessageType type = MessageType::Unknown;
    MessageObjectType objectType = MessageObjectType::Enemy;
    Tga::Vector2f position = {};
    int id = 0;
    sockaddr_in socketAddress = {};
    std::uint32_t sequenceNumber = 0;
};

struct AckMessage
{
    MessageType type = MessageType::Unknown;
    MessageObjectType objectType = MessageObjectType::Enemy;
    Tga::Vector2f position = {};
    int id = 0;
    sockaddr_in socketAddress = {};
    std::uint32_t sequenceNumber = 0;
};

inline bool CompareSockAddr(const sockaddr_in& left, const sockaddr_in& right)
{
    return left.sin_family == right.sin_family && left.sin_port == right.sin_port &&
           left.sin_addr.S_un.S_addr == right.sin_addr.S_un.S_addr;
}

inline std::uint32_t GetCurrentTimeMs()
{
    return static_cast<std::uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(NetworkClock::now().time_since_epoch()).count());
}

struct User
{
    int id = 0;
    sockaddr_in socketAddress = {};
    std::string password;
    Tga::Vector2f position = {};
    NetworkClock::time_point lastMessageTime = {};
    Tga::Vector2f latestPosition = {};
    NetworkClock::time_point lastPositionUpdateTime = {};
    std::unordered_map<std::uint32_t, Message> pendingReliableMessages;
    std::vector<Message> pendingMessages;
    Message pendingPositionAck = {};
    NetworkClock::time_point lastLateJoinMessageTime = {};
    NetworkClock::time_point lastUpdateTime = {};

    bool operator==(const User& other) const
    {
        return CompareSockAddr(socketAddress, other.socketAddress) && password == other.password;
    }
};
