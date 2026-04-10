#include "Server.h"

#include "ServerFunctions.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>

namespace
{
constexpr unsigned short kListenPort = 42000;
constexpr auto kSendInterval = std::chrono::milliseconds(16);
constexpr auto kLateJoinRefreshInterval = std::chrono::milliseconds(200);
constexpr auto kObjectUpdateInterval = std::chrono::milliseconds(100);
constexpr auto kPlayerUpdateInterval = std::chrono::milliseconds(80);
constexpr auto kInactiveTimeout = std::chrono::seconds(5);
constexpr float kRelevantDistance = 800.0F;
constexpr float kImmediateDistance = 400.0F;
constexpr float kCircleRadius = 150.0F;
constexpr float kCircleStepDegrees = 20.0F * 0.16F;
constexpr float kPi = 3.14159265359F;
constexpr float kFullCircleDegrees = 360.0F;
constexpr float kDegreesToRadians = kPi / 180.0F;

float Distance(const Tga::Vector2f& left, const Tga::Vector2f& right)
{
    const float deltaX = left.x - right.x;
    const float deltaY = left.y - right.y;
    return std::sqrt((deltaX * deltaX) + (deltaY * deltaY));
}

template <typename Collection>
bool AppendMessages(char*& bufferPointer, char* bufferEnd, Collection& messages, const bool clearAllOnSuccess)
{
    size_t appendedCount = 0;
    for (const Message& message : messages)
    {
        if (!WriteData(bufferPointer, bufferEnd, message))
        {
            break;
        }

        ++appendedCount;
    }

    if (clearAllOnSuccess && appendedCount == messages.size())
    {
        messages.clear();
        return appendedCount > 0;
    }

    if (appendedCount > 0)
    {
        messages.erase(messages.begin(), messages.begin() + appendedCount);
    }

    return appendedCount > 0;
}
} // namespace

ServerHandler& ServerHandler::GetInstance()
{
    static ServerHandler instance;
    return instance;
}

bool ServerHandler::Initialize()
{
    std::cout << "Starting Winsock...";
    if (WSAStartup(MAKEWORD(2, 2), &winsockData) != 0)
    {
        std::cerr << " failed with error " << WSAGetLastError() << ".\n";
        return false;
    }
    std::cout << " OK!\n";

    udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udpSocket == INVALID_SOCKET)
    {
        std::cerr << "Failed to create UDP socket: " << WSAGetLastError() << '\n';
        return false;
    }

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(kListenPort);

    if (bind(udpSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) == SOCKET_ERROR)
    {
        std::cerr << "Failed to bind UDP socket: " << WSAGetLastError() << '\n';
        return false;
    }

    isRunning = true;
    sendThread = std::thread(
        [this]()
        {
            auto lastSendTime = std::chrono::steady_clock::now();

            while (isRunning)
            {
                const auto now = std::chrono::steady_clock::now();
                if (now - lastSendTime >= kSendInterval)
                {
                    SendMessages();
                    lastSendTime = now;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });

    return true;
}

bool ServerHandler::Shutdown()
{
    isRunning = false;

    if (sendThread.joinable())
    {
        sendThread.join();
    }

    if (udpSocket != INVALID_SOCKET)
    {
        closesocket(udpSocket);
        udpSocket = INVALID_SOCKET;
    }

    WSACleanup();
    return true;
}

bool ServerHandler::Update()
{
    UpdateMovingObjects();
    return ReceiveMessage();
}

bool ServerHandler::ReceiveMessage()
{
    sockaddr_in clientAddress = {};
    int clientAddressSize = sizeof(clientAddress);
    const int receivedBytes = recvfrom(udpSocket, receivedBuffer, sizeof(receivedBuffer), 0,
                                       reinterpret_cast<sockaddr*>(&clientAddress), &clientAddressSize);

    if (receivedBytes == SOCKET_ERROR)
    {
        const int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK)
        {
            std::cerr << "recvfrom failed with error " << error << '\n';
        }

        return true;
    }

    if (receivedBytes > 0)
    {
        ProcessReceivedData(receivedBuffer, receivedBytes, clientAddress);
    }

    return true;
}

void ServerHandler::UpdateMovingObjects()
{
    std::lock_guard<std::mutex> lock(usersMutex);

    for (Object& object : movingObjects)
    {
        object.angle += kCircleStepDegrees;
        object.angle = std::fmod(object.angle, kFullCircleDegrees);

        const float angleRadians = object.angle * kDegreesToRadians;
        object.position.x = object.center.x + kCircleRadius * std::cos(angleRadians);
        object.position.y = object.center.y + kCircleRadius * std::sin(angleRadians);
    }
}

void ServerHandler::SendMessages()
{
    std::lock_guard<std::mutex> lock(usersMutex);
    const auto now = std::chrono::steady_clock::now();

    for (User& user : users)
    {
        char sendBuffer[NETMESSAGE_SIZE] = {};
        char* bufferPointer = sendBuffer;
        char* const bufferEnd = sendBuffer + sizeof(sendBuffer);

        QueueLateJoinState(user);
        SendWorldState(user);

        if (user.pendingPositionAck.type != MessageType::Unknown)
        {
            user.pendingMessages.push_back(user.pendingPositionAck);
            user.pendingPositionAck = {};
        }

        for (const Message& message : broadcastMessages)
        {
            if (!WriteData(bufferPointer, bufferEnd, message))
            {
                break;
            }
        }

        AppendMessages(bufferPointer, bufferEnd, user.pendingMessages, false);

        const int bytesToSend = static_cast<int>(bufferPointer - sendBuffer);
        if (bytesToSend > 0 && !SendUdpMessage(udpSocket, sendBuffer, bytesToSend, user.socketAddress))
        {
            std::cerr << "Failed to send packet to client " << user.id << ": " << WSAGetLastError() << '\n';
        }

        user.lastUpdateTime = now;
    }

    broadcastMessages.clear();
    CheckInactiveUsers();
}

void ServerHandler::QueueLateJoinState(User& user)
{
    const auto now = std::chrono::steady_clock::now();
    if (now - user.lastLateJoinMessageTime < kLateJoinRefreshInterval)
    {
        return;
    }

    for (const User& otherUser : users)
    {
        if (CompareSockAddr(otherUser.socketAddress, user.socketAddress))
        {
            continue;
        }

        Message message;
        message.type = MessageType::Join;
        message.objectType = MessageObjectType::Enemy;
        message.position = otherUser.position;
        message.socketAddress = otherUser.socketAddress;
        user.pendingMessages.push_back(message);
    }

    for (const Object& object : objects)
    {
        Message message;
        message.type = MessageType::Join;
        message.objectType = MessageObjectType::Object;
        message.position = object.position;
        message.id = object.id;
        user.pendingMessages.push_back(message);
    }

    for (const Object& object : movingObjects)
    {
        Message message;
        message.type = MessageType::Join;
        message.objectType = MessageObjectType::ObjectCircleGo;
        message.position = object.position;
        message.id = object.id;
        user.pendingMessages.push_back(message);
    }

    user.lastLateJoinMessageTime = now;
}

void ServerHandler::SendWorldState(User& user)
{
    const auto now = std::chrono::steady_clock::now();

    for (Object& object : movingObjects)
    {
        const float distance = Distance(user.position, object.position);
        if (distance > kRelevantDistance)
        {
            continue;
        }

        const bool sendImmediately = distance <= kImmediateDistance;
        const bool sendOnTimer = now - object.lastUpdateTime > kObjectUpdateInterval;
        if (!sendImmediately && !sendOnTimer)
        {
            continue;
        }

        Message message;
        message.type = MessageType::Position;
        message.objectType = MessageObjectType::ObjectCircleGo;
        message.position = object.position;
        message.id = object.id;
        message.sequenceNumber = nextSequenceNumber++;
        user.pendingMessages.push_back(message);
        object.lastUpdateTime = now;
    }

    for (User& otherUser : users)
    {
        if (CompareSockAddr(otherUser.socketAddress, user.socketAddress))
        {
            continue;
        }

        const float distance = Distance(user.position, otherUser.position);
        if (distance > kRelevantDistance)
        {
            continue;
        }

        const bool sendImmediately = distance <= kImmediateDistance;
        const bool sendOnTimer = now - otherUser.lastUpdateTime > kPlayerUpdateInterval;
        if (!sendImmediately && !sendOnTimer)
        {
            continue;
        }

        Message message;
        message.type = MessageType::Position;
        message.objectType = MessageObjectType::Enemy;
        message.position = otherUser.position;
        message.socketAddress = otherUser.socketAddress;
        message.sequenceNumber = nextSequenceNumber++;
        user.pendingMessages.push_back(message);
        otherUser.lastUpdateTime = now;
    }
}

void ServerHandler::QueuePositionAck(const Message& message, const sockaddr_in& clientAddress)
{
    auto user = std::find_if(users.begin(), users.end(), [&](const User& currentUser)
                             { return CompareSockAddr(currentUser.socketAddress, clientAddress); });

    if (user == users.end())
    {
        return;
    }

    Message ackMessage;
    ackMessage.type = MessageType::PositionAck;
    ackMessage.sequenceNumber = message.sequenceNumber;
    user->pendingPositionAck = ackMessage;
}

void ServerHandler::QueueAck(const Message& message, const sockaddr_in& clientAddress)
{
    auto user = std::find_if(users.begin(), users.end(), [&](const User& currentUser)
                             { return CompareSockAddr(currentUser.socketAddress, clientAddress); });

    if (user == users.end())
    {
        return;
    }

    Message ackMessage;
    ackMessage.type = MessageType::Ack;
    ackMessage.sequenceNumber = message.sequenceNumber;
    user->pendingMessages.push_back(ackMessage);
}

void ServerHandler::HandleAck(const Message& message, const sockaddr_in& clientAddress)
{
    auto user = std::find_if(users.begin(), users.end(), [&](const User& currentUser)
                             { return CompareSockAddr(currentUser.socketAddress, clientAddress); });

    if (user == users.end() || message.type != MessageType::ServerAck)
    {
        return;
    }

    user->pendingReliableMessages.erase(message.sequenceNumber);
}

void ServerHandler::HandleReceivedMessage(const Message& message, const sockaddr_in& clientAddress)
{
    std::lock_guard<std::mutex> lock(usersMutex);

    if (message.type == MessageType::ServerAck)
    {
        HandleAck(message, clientAddress);
        return;
    }

    switch (message.objectType)
    {
    case MessageObjectType::Enemy:
    {
        if (message.type == MessageType::Join)
        {
            const auto existingUser = std::find_if(users.begin(), users.end(), [&](const User& user)
                                                   { return CompareSockAddr(user.socketAddress, clientAddress); });

            if (existingUser == users.end())
            {
                User user;
                user.id = nextObjectId++;
                user.socketAddress = clientAddress;
                user.position = message.position;
                user.latestPosition = message.position;
                user.lastMessageTime = std::chrono::steady_clock::now();
                users.push_back(user);
                std::cout << "User " << user.id << " joined.\n";
            }
        }
        else if (message.type == MessageType::Quit)
        {
            Message quitMessage;
            quitMessage.type = MessageType::Quit;
            quitMessage.objectType = MessageObjectType::Enemy;
            quitMessage.position = message.position;
            quitMessage.socketAddress = clientAddress;
            broadcastMessages.push_back(quitMessage);

            users.erase(std::remove_if(users.begin(), users.end(), [&](const User& user)
                                       { return CompareSockAddr(user.socketAddress, clientAddress); }),
                        users.end());
        }
        else if (message.type == MessageType::Position)
        {
            QueuePositionAck(message, clientAddress);

            const auto user = std::find_if(users.begin(), users.end(), [&](const User& currentUser)
                                           { return CompareSockAddr(currentUser.socketAddress, clientAddress); });

            if (user != users.end())
            {
                user->position = message.position;
                user->latestPosition = message.position;
                user->lastPositionUpdateTime = std::chrono::steady_clock::now();
            }
        }

        break;
    }

    case MessageObjectType::Object:
    {
        if (message.type == MessageType::Join)
        {
            const auto existingObject = std::find_if(objects.begin(), objects.end(),
                                                     [&](const Object& object) { return object.id == message.id; });

            if (existingObject == objects.end())
            {
                Object object;
                object.id = nextObjectId++;
                object.position = message.position;
                object.lastKnownPosition = message.position;
                object.socketAddress = clientAddress;
                objects.push_back(object);
            }
        }
        else if (message.type == MessageType::Quit)
        {
            Message quitMessage;
            quitMessage.type = MessageType::Quit;
            quitMessage.objectType = MessageObjectType::Object;
            quitMessage.position = message.position;
            quitMessage.id = message.id;
            broadcastMessages.push_back(quitMessage);

            objects.erase(std::remove_if(objects.begin(), objects.end(),
                                         [&](const Object& object) { return object.id == message.id; }),
                          objects.end());
        }

        break;
    }

    case MessageObjectType::ObjectCircleGo:
    {
        if (message.type == MessageType::Join)
        {
            const auto existingObject = std::find_if(movingObjects.begin(), movingObjects.end(),
                                                     [&](const Object& object) { return object.id == message.id; });

            if (existingObject == movingObjects.end())
            {
                Object object;
                object.id = nextObjectId++;
                object.position = message.position;
                object.center = message.position;
                object.lastKnownPosition = message.position;
                object.socketAddress = clientAddress;
                movingObjects.push_back(object);
            }
        }
        else if (message.type == MessageType::Quit)
        {
            Message quitMessage;
            quitMessage.type = MessageType::Quit;
            quitMessage.objectType = MessageObjectType::ObjectCircleGo;
            quitMessage.position = message.position;
            quitMessage.id = message.id;
            broadcastMessages.push_back(quitMessage);

            movingObjects.erase(std::remove_if(movingObjects.begin(), movingObjects.end(),
                                               [&](const Object& object) { return object.id == message.id; }),
                                movingObjects.end());
        }

        break;
    }
    }

    if (message.type != MessageType::Position)
    {
        QueueAck(message, clientAddress);
    }
}

void ServerHandler::ProcessReceivedData(const char* data, const int dataSize, const sockaddr_in& clientAddress)
{
    receivedBufferIndex = 0;

    int offset = 0;
    while (offset + static_cast<int>(sizeof(Message)) <= dataSize)
    {
        Message message;
        std::memcpy(&message, data + offset, sizeof(Message));
        HandleReceivedMessage(message, clientAddress);
        offset += static_cast<int>(sizeof(Message));
    }

    std::lock_guard<std::mutex> lock(usersMutex);
    const auto user = std::find_if(users.begin(), users.end(), [&](const User& currentUser)
                                   { return CompareSockAddr(currentUser.socketAddress, clientAddress); });

    if (user != users.end())
    {
        user->lastMessageTime = std::chrono::steady_clock::now();
    }
}

void ServerHandler::CheckInactiveUsers()
{
    const auto now = std::chrono::steady_clock::now();

    for (auto iterator = users.begin(); iterator != users.end();)
    {
        if (now - iterator->lastMessageTime <= kInactiveTimeout)
        {
            ++iterator;
            continue;
        }

        Message quitMessage;
        quitMessage.type = MessageType::Quit;
        quitMessage.objectType = MessageObjectType::Enemy;
        quitMessage.position = iterator->position;
        quitMessage.socketAddress = iterator->socketAddress;
        broadcastMessages.push_back(quitMessage);

        std::cout << "Client " << iterator->id << " removed because of inactivity.\n";
        iterator = users.erase(iterator);
    }
}
