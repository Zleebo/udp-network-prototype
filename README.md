# UDP Network Prototype

Prototype focused on a common multiplayer problem: keeping gameplay responsive over UDP while still recovering from packet loss and out-of-order delivery. The transport layer handles batched packets, sequence numbers, cumulative ACKs, selective resends for non-position messages, and client-side interpolation to smooth out gaps between updates.

There's also an in-game debug overlay showing RTT, throughput, packet loss, and per-message-type stats, which made tuning a lot easier.

## Highlights

- Sequence-numbered UDP messages for ordering and loss detection
- Cumulative ACKs to avoid redundant acknowledgement traffic
- Client-side interpolation to smooth remote movement between updates
- In-game debug overlay for RTT, throughput, packet loss, and per-message-type stats

## Building

Open `Server.sln` in Visual Studio 2022 (v143 toolset, Windows 10 SDK), pick `Debug | x64`, and build. The client binary ends up in `Bin/` alongside the required DLLs and shader files.

## Running

Start the server project first, then the client. The client connects to `127.0.0.1:42000` automatically.

Controls:
- `WASD` - move the local player
- `B` - spawn an object
- `C` - remove a nearby object
- `V` - spawn a moving circular object

## Where To Look First

- `Source/Shared/NetworkShared.h` for the packet/message layouts, sequence numbers, and ACK-related shared state
- `Source/Server/Server.cpp` for batching, resend handling, cumulative ACK processing, and world-state replication
- `Source/Game/source/ClientObject.cpp` for client-side interpolation and remote entity updates
- `Source/Shared/Debugger.cpp` for the in-game networking debug overlay

## Layout

| Folder | Contents |
|--------|----------|
| `Source/Game/` | Client runtime and gameplay loop |
| `Source/Server/` | UDP server |
| `Source/Shared/` | Network data structures and debug overlay |
| `Source/Engine/` | Engine support code |
| `Source/External/` | Third-party dependencies |

`Game.sln` builds just the client. `Server.sln` builds both.

Settings are loaded from `Bin/settings/` and engine assets from `EngineAssets/`.
