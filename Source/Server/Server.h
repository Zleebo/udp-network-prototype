#pragma once

#include "../Shared/Network-Shared.h"

#include <WinSock2.h>
#include <WS2tcpip.h>

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

class ServerHandler
{
  public:
    static ServerHandler& GetInstance();

    bool Initialize();
    bool Shutdown();
    bool Update();

  private:
    ServerHandler() = default;
    ServerHandler(const ServerHandler&) = delete;
    ServerHandler& operator=(const ServerHandler&) = delete;

    bool ReceiveMessage();
    void UpdateMovingObjects();
    void SendMessages();
    void SendWorldState(User& user);
    void QueueLateJoinState(User& user);
    void QueuePositionAck(const Message& message, const sockaddr_in& clientAddress);
    void QueueAck(const Message& message, const sockaddr_in& clientAddress);
    void HandleAck(const Message& message, const sockaddr_in& clientAddress);
    void HandleReceivedMessage(const Message& message, const sockaddr_in& clientAddress);
    void ProcessReceivedData(const char* data, int dataSize, const sockaddr_in& clientAddress);
    void CheckInactiveUsers();

    std::thread sendThread;
    std::mutex usersMutex;
    std::atomic<bool> isRunning = true;

    SOCKET udpSocket = INVALID_SOCKET;
    WSADATA winsockData = {};
    sockaddr_in serverAddress = {};

    std::uint32_t nextSequenceNumber = 1;
    int nextObjectId = 53433;
    char receivedBuffer[NETMESSAGE_SIZE] = {};
    int receivedBufferIndex = 0;

    std::vector<User> users;
    std::vector<Object> objects;
    std::vector<Object> movingObjects;
    std::vector<Message> broadcastMessages;
};
