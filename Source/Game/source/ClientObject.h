#pragma once

#include "../Shared/Network-Shared.h"

#include <WinSock2.h>
#include <WS2tcpip.h>

#include <atomic>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

class NetworkDebugger;

class ClientHandler
{
  public:
    static ClientHandler& GetInstance();

    bool Initialize();
    bool Shutdown();
    bool Update();
    void RenderDebugOverlay() const;

    bool QueueMessage(Message message);
    void CreateObject(Tga::Vector2f position);
    void CreateMovingObject(Tga::Vector2f position);
    void RemoveObject(Tga::Vector2f position);

    void SetWindowHandle(HWND windowHandle);
    HWND GetWindowHandle() const;

    const std::vector<Enemy>& GetEnemies() const;
    const std::vector<Object>& GetObjects() const;
    const std::vector<Object>& GetMovingObjects() const;

  private:
    ClientHandler() = default;
    ClientHandler(const ClientHandler&) = delete;
    ClientHandler& operator=(const ClientHandler&) = delete;

    bool ReadReceivedData(const char* data, int dataSize, const sockaddr_in& senderAddress);
    void HandleReceivedMessage(const Message& message);
    void HandleAck(const Message& message);
    void SendBatchedMessages();
    void AppendPendingResends(char* resendBuffer, char*& resendBufferPtr);

    std::thread sendThread;
    mutable std::mutex queueMutex;
    std::atomic<bool> isRunning = true;

    SOCKET udpSocket = INVALID_SOCKET;
    WSADATA winsockData = {};
    sockaddr_in clientAddress = {};
    sockaddr_in serverAddress = {};
    HWND windowHandle = nullptr;

    std::vector<Message> queuedMessages;
    std::unordered_map<std::uint32_t, Message> pendingReliableMessages;
    std::unordered_map<std::uint32_t, Message> pendingPositionMessages;
    std::chrono::steady_clock::time_point lastAckTime = {};
    std::uint32_t nextSequenceNumber = 1;

    char receivedBuffer[NETMESSAGE_SIZE] = {};
    int receivedBufferIndex = 0;

    std::vector<Enemy> enemies;
    std::vector<Object> objects;
    std::vector<Object> movingObjects;
    NetworkDebugger* networkDebugger = nullptr;
};
