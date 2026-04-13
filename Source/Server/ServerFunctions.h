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

inline bool SendUdpMessage(SOCKET udpSocket, const char* message, int messageSize, const sockaddr_in& targetAddress)
{
    const sockaddr* destination = reinterpret_cast<const sockaddr*>(&targetAddress);
    return sendto(udpSocket, message, messageSize, 0, destination, sizeof(targetAddress)) != SOCKET_ERROR;
}
