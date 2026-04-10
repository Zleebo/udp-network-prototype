#include "pch.h"
#include "Debugger.h"

#include <algorithm>
#include <numeric>

namespace
{
template <typename T> void TrimHistory(std::vector<T>& history, const size_t maxSize)
{
    if (history.size() > maxSize)
    {
        history.erase(history.begin(), history.begin() + (history.size() - maxSize));
    }
}

void TrimTypeHistory(std::map<int, std::vector<int>>& history, const size_t maxSize)
{
    for (auto& [messageType, samples] : history)
    {
        static_cast<void>(messageType);
        TrimHistory(samples, maxSize);
    }
}
} // namespace

std::string MessageTypeToString(const MessageType messageType)
{
    switch (messageType)
    {
    case MessageType::Unknown:
        return "Unknown";
    case MessageType::Join:
        return "Join";
    case MessageType::Quit:
        return "Quit";
    case MessageType::Position:
        return "Position";
    case MessageType::Update:
        return "Update";
    case MessageType::Ack:
        return "Ack";
    case MessageType::ServerAck:
        return "ServerAck";
    case MessageType::PositionAck:
        return "PositionAck";
    default:
        return "Other";
    }
}

void NetworkStatisticsWindow::Update(const float newRttMs, const float newSentDataPerFrame,
                                     const float newReceivedDataPerFrame, const int newSentMessagesPerSecond,
                                     const int newReceivedMessagesPerSecond, const float newPacketLossPercentage)
{
    rttMs = newRttMs;
    sentDataPerFrame = newSentDataPerFrame;
    receivedDataPerFrame = newReceivedDataPerFrame;
    sentMessagesPerSecond = newSentMessagesPerSecond;
    receivedMessagesPerSecond = newReceivedMessagesPerSecond;
    packetLossPercentage = newPacketLossPercentage;
}

void NetworkStatisticsWindow::UpdateMessageTypeStats(const std::map<int, int>& newAverageSentBytes,
                                                     const std::map<int, int>& newAverageReceivedBytes,
                                                     const std::map<int, int>& newMessageCounts)
{
    averageSentBytes = newAverageSentBytes;
    averageReceivedBytes = newAverageReceivedBytes;
    messageCounts = newMessageCounts;
}

void NetworkStatisticsWindow::Render() const
{
    ImGui::Begin("Network Statistics");
    ImGui::Text("RTT: %.2f ms", rttMs);
    ImGui::Text("Sent data: %.2f bytes/frame", sentDataPerFrame);
    ImGui::Text("Received data: %.2f bytes/frame", receivedDataPerFrame);
    ImGui::Text("Sent messages: %d / sec", sentMessagesPerSecond);
    ImGui::Text("Received messages: %d / sec", receivedMessagesPerSecond);
    ImGui::Text("Packet loss: %.2f%%", packetLossPercentage);
    ImGui::End();
}

void NetworkStatisticsWindow::RenderMessageTypeStats() const
{
    ImGui::Begin("Message Type Statistics");

    for (const auto& [messageType, count] : messageCounts)
    {
        const int sentBytes =
            averageSentBytes.find(messageType) != averageSentBytes.end() ? averageSentBytes.at(messageType) : 0;
        const int receivedBytes = averageReceivedBytes.find(messageType) != averageReceivedBytes.end()
                                      ? averageReceivedBytes.at(messageType)
                                      : 0;
        const std::string typeName = MessageTypeToString(static_cast<MessageType>(messageType));

        ImGui::Text("%s: sent %d bytes, received %d bytes, %d msg/s", typeName.c_str(), sentBytes, receivedBytes,
                    count);
    }

    ImGui::End();
}

NetworkDebugger::NetworkDebugger()
    : lastFrameSampleTime(std::chrono::steady_clock::now()), lastMessageSampleTime(std::chrono::steady_clock::now()),
      lastAverageTime(std::chrono::steady_clock::now())
{
}

void NetworkDebugger::UpdateSentData(const int bytesSent, const MessageType messageType)
{
    totalSentBytes += bytesSent;
    ++totalSentMessages;
    sentDataPerType[static_cast<int>(messageType)] += bytesSent;
    messageTypeCounts[static_cast<int>(messageType)]++;
}

void NetworkDebugger::UpdateReceivedData(const int bytesReceived, const MessageType messageType)
{
    totalReceivedBytes += bytesReceived;
    ++totalReceivedMessages;
    receivedDataPerType[static_cast<int>(messageType)] += bytesReceived;
    messageTypeCounts[static_cast<int>(messageType)]++;
}

void NetworkDebugger::UpdateRtt(const std::chrono::milliseconds rtt)
{
    currentRttMs = static_cast<float>(rtt.count());
}

void NetworkDebugger::RecordAckReceipt()
{
    ++totalAckReceipts;
}

void NetworkDebugger::RefreshStatistics()
{
    RecordFrameData();
    UpdateAverages();
}

void NetworkDebugger::Render() const
{
    statsWindow.Render();
    statsWindow.RenderMessageTypeStats();
}

void NetworkDebugger::CalculatePacketLoss()
{
    if (totalSentMessages <= 0)
    {
        packetLossPercentage = 0.0f;
        return;
    }

    packetLossPercentage =
        (1.0f - static_cast<float>(totalAckReceipts) / static_cast<float>(totalSentMessages)) * 100.0f;
    packetLossPercentage = (std::max)(packetLossPercentage, 0.0f);
}

void NetworkDebugger::RecordFrameData()
{
    constexpr size_t maxHistorySamples = 600;
    const auto now = std::chrono::steady_clock::now();

    CalculatePacketLoss();

    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFrameSampleTime).count() >= 16)
    {
        sentDataHistory.push_back(totalSentBytes);
        receivedDataHistory.push_back(totalReceivedBytes);
        rttHistory.push_back(currentRttMs);

        totalSentBytes = 0;
        totalReceivedBytes = 0;
        currentRttMs = 0.0f;

        for (auto& [messageType, bytes] : sentDataPerType)
        {
            sentDataHistoryPerType[messageType].push_back(bytes);
            bytes = 0;
        }

        for (auto& [messageType, bytes] : receivedDataPerType)
        {
            receivedDataHistoryPerType[messageType].push_back(bytes);
            bytes = 0;
        }

        lastFrameSampleTime = now;
    }

    if (std::chrono::duration_cast<std::chrono::seconds>(now - lastMessageSampleTime).count() >= 1)
    {
        sentMessagesHistory.push_back(totalSentMessages);
        receivedMessagesHistory.push_back(totalReceivedMessages);
        totalSentMessages = 0;
        totalReceivedMessages = 0;
        totalAckReceipts = 0;
        lastMessageSampleTime = now;
    }

    TrimHistory(sentDataHistory, maxHistorySamples);
    TrimHistory(receivedDataHistory, maxHistorySamples);
    TrimHistory(sentMessagesHistory, maxHistorySamples);
    TrimHistory(receivedMessagesHistory, maxHistorySamples);
    TrimHistory(rttHistory, maxHistorySamples);
    TrimTypeHistory(sentDataHistoryPerType, maxHistorySamples);
    TrimTypeHistory(receivedDataHistoryPerType, maxHistorySamples);
}

void NetworkDebugger::UpdateAverages()
{
    const auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - lastAverageTime).count() < 1)
    {
        return;
    }

    std::map<int, int> averageSentBytes;
    std::map<int, int> averageReceivedBytes;
    std::map<int, int> averageMessageCounts;

    for (const auto& [messageType, samples] : sentDataHistoryPerType)
    {
        if (!samples.empty())
        {
            averageSentBytes[messageType] = std::accumulate(samples.begin(), samples.end(), 0) / samples.size();
        }
    }

    for (const auto& [messageType, samples] : receivedDataHistoryPerType)
    {
        if (!samples.empty())
        {
            averageReceivedBytes[messageType] = std::accumulate(samples.begin(), samples.end(), 0) / samples.size();
        }
    }

    for (auto& [messageType, count] : messageTypeCounts)
    {
        averageMessageCounts[messageType] = count;
        count = 0;
    }

    if (!sentDataHistory.empty() && !receivedDataHistory.empty() && !sentMessagesHistory.empty() &&
        !receivedMessagesHistory.empty() && !rttHistory.empty())
    {
        const float averageSentData =
            static_cast<float>(std::accumulate(sentDataHistory.begin(), sentDataHistory.end(), 0)) /
            sentDataHistory.size();
        const float averageReceivedData =
            static_cast<float>(std::accumulate(receivedDataHistory.begin(), receivedDataHistory.end(), 0)) /
            receivedDataHistory.size();
        const int averageSentMessages =
            std::accumulate(sentMessagesHistory.begin(), sentMessagesHistory.end(), 0) / sentMessagesHistory.size();
        const int averageReceivedMessages =
            std::accumulate(receivedMessagesHistory.begin(), receivedMessagesHistory.end(), 0) /
            receivedMessagesHistory.size();
        const float averageRtt =
            std::accumulate(rttHistory.begin(), rttHistory.end(), 0.0f) / static_cast<float>(rttHistory.size());

        statsWindow.Update(averageRtt, averageSentData, averageReceivedData, averageSentMessages,
                           averageReceivedMessages, packetLossPercentage);
    }

    statsWindow.UpdateMessageTypeStats(averageSentBytes, averageReceivedBytes, averageMessageCounts);

    sentDataHistory.clear();
    receivedDataHistory.clear();
    sentMessagesHistory.clear();
    receivedMessagesHistory.clear();
    rttHistory.clear();

    for (auto& [messageType, samples] : sentDataHistoryPerType)
    {
        static_cast<void>(messageType);
        samples.clear();
    }

    for (auto& [messageType, samples] : receivedDataHistoryPerType)
    {
        static_cast<void>(messageType);
        samples.clear();
    }

    lastAverageTime = now;
}
