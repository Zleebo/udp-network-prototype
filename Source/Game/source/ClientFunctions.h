#pragma once

#include "../Shared/NetworkShared.h"

#include <cstring>
#include <type_traits>

template <typename T> bool WriteData(char*& buffer, char* bufferEnd, const T& data)
{
    static_assert(std::is_standard_layout_v<T>, "Only serialization of standard layout types is supported.");

    if (buffer + sizeof(T) > bufferEnd)
    {
        return false;
    }

    std::memcpy(buffer, &data, sizeof(T));
    buffer += sizeof(T);
    return true;
}

template <typename T> bool WriteData(char*& buffer, char* bufferEnd, const T* data, size_t count)
{
    static_assert(std::is_standard_layout_v<T>, "Only serialization of standard layout types is supported.");

    const size_t bytesToWrite = sizeof(T) * count;
    if (buffer + bytesToWrite > bufferEnd)
    {
        return false;
    }

    std::memcpy(buffer, data, bytesToWrite);
    buffer += bytesToWrite;
    return true;
}

inline bool SendUdpMessage(SOCKET udpSocket, const char* message, int messageSize, const sockaddr_in& serverAddress)
{
    const sockaddr* destination = reinterpret_cast<const sockaddr*>(&serverAddress);
    return sendto(udpSocket, message, messageSize, 0, destination, sizeof(serverAddress)) != SOCKET_ERROR;
}

inline bool WriteClientToServerMessageAndSend(const Message& message, SOCKET udpSocket,
                                              const sockaddr_in& serverAddress)
{
    return SendUdpMessage(udpSocket, reinterpret_cast<const char*>(&message), sizeof(Message), serverAddress);
}
