#include "ClientObject.h"

#include "ClientFunctions.h"
#include "../Shared/Debugger.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>

namespace
{
constexpr char kServerAddress[] = "127.0.0.1";
constexpr unsigned short kServerPort = 42000;
constexpr auto kSendInterval = std::chrono::milliseconds(16);
constexpr std::uint32_t kReliableResendDelayMs = 1000;
constexpr std::uint32_t kPositionResendDelayMs = 500;
constexpr size_t kMaxTrackedPositionMessages = 10;
constexpr size_t kMaxInterpolationHistory = 60;

int FindClosestLower(const std::map<std::uint32_t, Tga::Vector2f>& history, const std::uint32_t targetSequence)
{
    const auto iterator = history.lower_bound(targetSequence);
    if (iterator == history.begin())
    {
        return -1;
    }

    return static_cast<int>(std::prev(iterator)->first);
}

int FindClosestHigher(const std::map<std::uint32_t, Tga::Vector2f>& history, const std::uint32_t targetSequence)
{
    const auto iterator = history.upper_bound(targetSequence);
    if (iterator == history.end())
    {
        return -1;
    }

    return static_cast<int>(iterator->first);
}

Tga::Vector2f Interpolate(const std::map<std::uint32_t, Tga::Vector2f>& history, const std::uint32_t lowerBound,
                          const std::uint32_t upperBound, const std::uint32_t targetSequence)
{
    const Tga::Vector2f& lowerPosition = history.at(lowerBound);
    const Tga::Vector2f& upperPosition = history.at(upperBound);
    const float interpolationFactor =
        static_cast<float>(targetSequence - lowerBound) / static_cast<float>(upperBound - lowerBound);

    return lowerPosition + interpolationFactor * (upperPosition - lowerPosition);
}

template <typename EntityType> void ApplyInterpolatedPosition(EntityType& entity, const Message& message)
{
    entity.positionHistory[message.sequenceNumber] = message.position;

    while (entity.positionHistory.size() > kMaxInterpolationHistory)
    {
        entity.positionHistory.erase(entity.positionHistory.begin());
    }

    std::uint64_t sequenceSum = 0;
    int sampleCount = 0;
    for (auto iterator = entity.positionHistory.rbegin(); iterator != entity.positionHistory.rend() && sampleCount < 3;
         ++iterator, ++sampleCount)
    {
        sequenceSum += iterator->first;
    }

    const std::uint32_t averageSequence = sampleCount > 0 ? static_cast<std::uint32_t>(sequenceSum / sampleCount) : 0;
    const int lowerBound = FindClosestLower(entity.positionHistory, averageSequence);
    const int upperBound = FindClosestHigher(entity.positionHistory, averageSequence);

    if (lowerBound >= 0 && upperBound >= 0 && lowerBound != upperBound)
    {
        entity.position = Interpolate(entity.positionHistory, static_cast<std::uint32_t>(lowerBound),
                                      static_cast<std::uint32_t>(upperBound), averageSequence);
    }
    else
    {
        entity.position = entity.lastKnownPosition;
    }

    entity.lastKnownPosition = entity.position;
}

bool CheckCollision(const Tga::Vector2f& point, const Tga::Vector2f& center, const float radius)
{
    const float deltaX = point.x - center.x;
    const float deltaY = point.y - center.y;
    return (deltaX * deltaX) + (deltaY * deltaY) <= radius * radius;
}
} // namespace

ClientHandler& ClientHandler::GetInstance()
{
    static ClientHandler instance;
    return instance;
}

bool ClientHandler::Initialize()
{
    if (networkDebugger == nullptr)
    {
        networkDebugger = new NetworkDebugger();
    }

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

    u_long nonBlocking = 1;
    ioctlsocket(udpSocket, FIONBIO, &nonBlocking);

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(kServerPort);
    InetPtonA(AF_INET, kServerAddress, &serverAddress.sin_addr.s_addr);

    Message joinMessage;
    joinMessage.type = MessageType::Join;
    joinMessage.objectType = MessageObjectType::Enemy;
    joinMessage.position = Tga::Vector2f(1.0F, 1.0F);
    QueueMessage(joinMessage);
    SendBatchedMessages();

    int addressLength = sizeof(clientAddress);
    if (getsockname(udpSocket, reinterpret_cast<sockaddr*>(&clientAddress), &addressLength) == SOCKET_ERROR)
    {
        std::cerr << "Failed to read local UDP endpoint: " << WSAGetLastError() << '\n';
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
                    SendBatchedMessages();
                    lastSendTime = now;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });

    return true;
}

bool ClientHandler::Shutdown()
{
    if (udpSocket != INVALID_SOCKET)
    {
        Message quitMessage;
        quitMessage.type = MessageType::Quit;
        quitMessage.objectType = MessageObjectType::Enemy;
        WriteClientToServerMessageAndSend(quitMessage, udpSocket, serverAddress);
    }

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

    delete networkDebugger;
    networkDebugger = nullptr;

    return true;
}

bool ClientHandler::Update()
{
    if (networkDebugger != nullptr)
    {
        networkDebugger->RefreshStatistics();
    }

    sockaddr_in senderAddress = {};
    int senderAddressSize = sizeof(senderAddress);
    const int receivedBytes = recvfrom(udpSocket, receivedBuffer, NETMESSAGE_SIZE, 0,
                                       reinterpret_cast<sockaddr*>(&senderAddress), &senderAddressSize);

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
        ReadReceivedData(receivedBuffer, receivedBytes, senderAddress);
    }

    return true;
}

void ClientHandler::RenderDebugOverlay() const
{
    if (networkDebugger != nullptr)
    {
        networkDebugger->Render();
    }
}

bool ClientHandler::QueueMessage(Message message)
{
    std::lock_guard<std::mutex> lock(queueMutex);

    message.sequenceNumber = nextSequenceNumber++;
    message.lastSendTime = GetCurrentTimeMs();
    queuedMessages.push_back(message);

    if (message.type == MessageType::Position)
    {
        if (pendingPositionMessages.size() >= kMaxTrackedPositionMessages)
        {
            const auto oldest =
                std::min_element(pendingPositionMessages.begin(), pendingPositionMessages.end(),
                                 [](const auto& left, const auto& right) { return left.first < right.first; });
            pendingPositionMessages.erase(oldest);
        }

        pendingPositionMessages[message.sequenceNumber] = message;
    }
    else
    {
        pendingReliableMessages[message.sequenceNumber] = message;
    }

    return true;
}

bool ClientHandler::ReadReceivedData(const char* data, const int dataSize, const sockaddr_in& senderAddress)
{
    static_cast<void>(senderAddress);
    receivedBufferIndex = 0;

    int offset = 0;
    while (offset + static_cast<int>(sizeof(Message)) <= dataSize)
    {
        Message message;
        std::memcpy(&message, data + offset, sizeof(Message));

        if (networkDebugger != nullptr)
        {
            networkDebugger->UpdateReceivedData(sizeof(Message), message.type);
        }

        if (message.type == MessageType::Ack || message.type == MessageType::PositionAck)
        {
            HandleAck(message);
        }
        else
        {
            HandleReceivedMessage(message);
        }

        offset += static_cast<int>(sizeof(Message));
    }

    return true;
}

void ClientHandler::HandleReceivedMessage(const Message& message)
{
    switch (message.objectType)
    {
    case MessageObjectType::Enemy:
    {
        if (message.type == MessageType::Join)
        {
            if (CompareSockAddr(message.socketAddress, clientAddress))
            {
                return;
            }

            const auto existingEnemy =
                std::find_if(enemies.begin(), enemies.end(), [&](const Enemy& enemy)
                             { return CompareSockAddr(enemy.socketAddress, message.socketAddress); });

            if (existingEnemy == enemies.end())
            {
                Enemy enemy;
                enemy.position = message.position;
                enemy.lastKnownPosition = message.position;
                enemy.socketAddress = message.socketAddress;
                enemies.push_back(enemy);
            }

            return;
        }

        if (message.type == MessageType::Position)
        {
            if (CompareSockAddr(message.socketAddress, clientAddress))
            {
                return;
            }

            auto enemy = std::find_if(enemies.begin(), enemies.end(), [&](const Enemy& currentEnemy)
                                      { return CompareSockAddr(currentEnemy.socketAddress, message.socketAddress); });

            if (enemy == enemies.end())
            {
                Enemy newEnemy;
                newEnemy.socketAddress = message.socketAddress;
                newEnemy.position = message.position;
                newEnemy.lastKnownPosition = message.position;
                enemies.push_back(newEnemy);
                enemy = std::prev(enemies.end());
            }

            ApplyInterpolatedPosition(*enemy, message);
            return;
        }

        if (message.type == MessageType::Quit)
        {
            enemies.erase(std::remove_if(enemies.begin(), enemies.end(), [&](const Enemy& enemy)
                                         { return CompareSockAddr(enemy.socketAddress, message.socketAddress); }),
                          enemies.end());
        }

        return;
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
                object.id = message.id;
                object.position = message.position;
                object.lastKnownPosition = message.position;
                object.socketAddress = message.socketAddress;
                objects.push_back(object);
            }

            return;
        }

        if (message.type == MessageType::Quit)
        {
            objects.erase(std::remove_if(objects.begin(), objects.end(),
                                         [&](const Object& object) { return object.id == message.id; }),
                          objects.end());
        }

        return;
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
                object.id = message.id;
                object.position = message.position;
                object.lastKnownPosition = message.position;
                object.socketAddress = message.socketAddress;
                movingObjects.push_back(object);
            }

            return;
        }

        if (message.type == MessageType::Position)
        {
            auto object = std::find_if(movingObjects.begin(), movingObjects.end(),
                                       [&](const Object& currentObject) { return currentObject.id == message.id; });

            if (object == movingObjects.end())
            {
                Object newObject;
                newObject.id = message.id;
                newObject.position = message.position;
                newObject.lastKnownPosition = message.position;
                movingObjects.push_back(newObject);
                object = std::prev(movingObjects.end());
            }

            ApplyInterpolatedPosition(*object, message);
            return;
        }

        if (message.type == MessageType::Quit)
        {
            movingObjects.erase(std::remove_if(movingObjects.begin(), movingObjects.end(),
                                               [&](const Object& object) { return object.id == message.id; }),
                                movingObjects.end());
        }

        return;
    }
    }
}

void ClientHandler::HandleAck(const Message& message)
{
    std::lock_guard<std::mutex> lock(queueMutex);

    if (networkDebugger != nullptr)
    {
        networkDebugger->RecordAckReceipt();
    }

    if (message.type == MessageType::Ack)
    {
        pendingReliableMessages.erase(message.sequenceNumber);
    }
    else if (message.type == MessageType::PositionAck)
    {
        for (auto iterator = pendingPositionMessages.begin(); iterator != pendingPositionMessages.end();)
        {
            if (iterator->first <= message.sequenceNumber)
            {
                iterator = pendingPositionMessages.erase(iterator);
            }
            else
            {
                ++iterator;
            }
        }
    }

    if (networkDebugger != nullptr && lastAckTime != std::chrono::steady_clock::time_point{})
    {
        const auto rtt =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - lastAckTime);
        networkDebugger->UpdateRtt(rtt);
    }
}

void ClientHandler::SendBatchedMessages()
{
    char sendBuffer[NETMESSAGE_SIZE] = {};
    char* bufferPointer = sendBuffer;
    char* const bufferEnd = sendBuffer + sizeof(sendBuffer);

    std::lock_guard<std::mutex> lock(queueMutex);

    AppendPendingResends(sendBuffer, bufferPointer);

    size_t appendedMessages = 0;
    for (const Message& message : queuedMessages)
    {
        if (!WriteData(bufferPointer, bufferEnd, message))
        {
            break;
        }

        if (networkDebugger != nullptr)
        {
            networkDebugger->UpdateSentData(sizeof(Message), message.type);
        }

        ++appendedMessages;
    }

    if (appendedMessages > 0)
    {
        queuedMessages.erase(queuedMessages.begin(),
                             queuedMessages.begin() +
                                 static_cast<std::vector<Message>::difference_type>(appendedMessages));
    }

    const int bytesToSend = static_cast<int>(bufferPointer - sendBuffer);
    if (bytesToSend <= 0)
    {
        return;
    }

    lastAckTime = std::chrono::steady_clock::now();
    if (!SendUdpMessage(udpSocket, sendBuffer, bytesToSend, serverAddress))
    {
        std::cerr << "Failed to send UDP packet: " << WSAGetLastError() << '\n';
    }
}

void ClientHandler::AppendPendingResends(char* resendBuffer, char*& resendBufferPtr)
{
    char* const resendBufferEnd = resendBuffer + NETMESSAGE_SIZE;
    const std::uint32_t nowMs = GetCurrentTimeMs();

    for (auto& [sequenceNumber, message] : pendingReliableMessages)
    {
        static_cast<void>(sequenceNumber);

        const std::uint32_t elapsedMs = nowMs >= message.lastSendTime ? nowMs - message.lastSendTime : 0;
        if (elapsedMs < kReliableResendDelayMs)
        {
            continue;
        }

        if (!WriteData(resendBufferPtr, resendBufferEnd, message))
        {
            return;
        }

        message.lastSendTime = nowMs;
        if (networkDebugger != nullptr)
        {
            networkDebugger->UpdateSentData(sizeof(Message), message.type);
        }
    }

    if (pendingPositionMessages.empty())
    {
        return;
    }

    auto latestPositionMessage =
        std::max_element(pendingPositionMessages.begin(), pendingPositionMessages.end(),
                         [](const auto& left, const auto& right) { return left.first < right.first; });

    const std::uint32_t elapsedMs =
        nowMs >= latestPositionMessage->second.lastSendTime ? nowMs - latestPositionMessage->second.lastSendTime : 0;
    if (elapsedMs < kPositionResendDelayMs)
    {
        return;
    }

    if (!WriteData(resendBufferPtr, resendBufferEnd, latestPositionMessage->second))
    {
        return;
    }

    latestPositionMessage->second.lastSendTime = nowMs;
    if (networkDebugger != nullptr)
    {
        networkDebugger->UpdateSentData(sizeof(Message), latestPositionMessage->second.type);
    }
}

void ClientHandler::CreateObject(const Tga::Vector2f position)
{
    Message message;
    message.objectType = MessageObjectType::Object;
    message.type = MessageType::Join;
    message.position = position;
    QueueMessage(message);
}

void ClientHandler::CreateMovingObject(const Tga::Vector2f position)
{
    Message message;
    message.objectType = MessageObjectType::ObjectCircleGo;
    message.type = MessageType::Join;
    message.position = position;
    QueueMessage(message);
}

void ClientHandler::RemoveObject(const Tga::Vector2f position)
{
    for (const Object& object : objects)
    {
        if (!CheckCollision(position, object.position, 100.0F))
        {
            continue;
        }

        Message message;
        message.objectType = MessageObjectType::Object;
        message.type = MessageType::Quit;
        message.id = object.id;
        message.position = object.position;
        QueueMessage(message);
    }
}

void ClientHandler::SetWindowHandle(HWND windowHandle)
{
    this->windowHandle = windowHandle;
}

HWND ClientHandler::GetWindowHandle() const
{
    return windowHandle;
}

const std::vector<Enemy>& ClientHandler::GetEnemies() const
{
    return enemies;
}

const std::vector<Object>& ClientHandler::GetObjects() const
{
    return objects;
}

const std::vector<Object>& ClientHandler::GetMovingObjects() const
{
    return movingObjects;
}
