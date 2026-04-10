#pragma once

#include "../../Source/External/imgui/imgui.h"
#include "Network-Shared.h"

#include <chrono>
#include <map>
#include <string>
#include <vector>

class NetworkStatisticsWindow
{
  public:
    void Update(float rttMs, float sentDataPerFrame, float receivedDataPerFrame, int sentMessagesPerSecond,
                int receivedMessagesPerSecond, float packetLossPercentage);
    void UpdateMessageTypeStats(const std::map<int, int>& averageSentBytes,
                                const std::map<int, int>& averageReceivedBytes,
                                const std::map<int, int>& messageCounts);
    void Render() const;
    void RenderMessageTypeStats() const;

  private:
    float rttMs = 0.0f;
    float sentDataPerFrame = 0.0f;
    float receivedDataPerFrame = 0.0f;
    int sentMessagesPerSecond = 0;
    int receivedMessagesPerSecond = 0;
    float packetLossPercentage = 0.0f;
    std::map<int, int> averageSentBytes;
    std::map<int, int> averageReceivedBytes;
    std::map<int, int> messageCounts;
};

class NetworkDebugger
{
  public:
    NetworkDebugger();

    void UpdateSentData(int bytesSent, MessageType messageType);
    void UpdateReceivedData(int bytesReceived, MessageType messageType);
    void UpdateRtt(std::chrono::milliseconds rtt);
    void RecordAckReceipt();
    void RefreshStatistics();
    void Render() const;

  private:
    void CalculatePacketLoss();
    void RecordFrameData();
    void UpdateAverages();

    NetworkStatisticsWindow statsWindow;
    std::chrono::steady_clock::time_point lastFrameSampleTime;
    std::chrono::steady_clock::time_point lastMessageSampleTime;
    std::chrono::steady_clock::time_point lastAverageTime;

    float currentRttMs = 0.0f;
    int totalSentBytes = 0;
    int totalReceivedBytes = 0;
    int totalSentMessages = 0;
    int totalReceivedMessages = 0;
    int totalAckReceipts = 0;
    float packetLossPercentage = 0.0f;

    std::map<int, int> messageTypeCounts;
    std::map<int, int> sentDataPerType;
    std::map<int, int> receivedDataPerType;
    std::map<int, std::vector<int>> sentDataHistoryPerType;
    std::map<int, std::vector<int>> receivedDataHistoryPerType;

    std::vector<int> sentDataHistory;
    std::vector<int> receivedDataHistory;
    std::vector<float> rttHistory;
    std::vector<int> sentMessagesHistory;
    std::vector<int> receivedMessagesHistory;
};

std::string MessageTypeToString(MessageType messageType);
