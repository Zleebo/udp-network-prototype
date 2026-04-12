# UDP Network Prototype

Small client/server sandbox I built to work through the fundamentals of UDP game networking. The interesting part isn't the gameplay but the transport layer: batched packets, sequence numbers, cumulative ACKs, selective resends for non-position messages, and client-side interpolation to smooth out gaps between updates.

There's also an in-game debug overlay showing RTT, throughput, packet loss, and per-message-type stats, which made tuning a lot easier.

## Building

Open `Server.sln` in Visual Studio 2022 (v143 toolset, Windows 10 SDK), pick `Debug | x64`, and build. The client binary ends up in `Bin/` alongside the required DLLs and shader files.

## Running

Start the server project first, then the client. The client connects to `127.0.0.1:42000` automatically.

Controls:
- `WASD` — move the local player
- `B` — spawn an object
- `C` — remove a nearby object
- `V` — spawn a moving circular object

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
