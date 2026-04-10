# UDP Network Prototype

This repository contains a Windows/Visual Studio UDP gameplay networking prototype built around a small client/server sandbox. It focuses on message batching, acknowledgements, resend logic, position replication, interpolation, and a lightweight in-game network debug overlay.

## Highlights

- UDP client/server transport on localhost
- Message types for join, quit, position, object updates, and acknowledgements
- Batched packet writes with fixed-size message serialization
- Reliable resend tracking for non-position gameplay messages
- Position acknowledgement handling for high-frequency movement updates
- Client-side interpolation for replicated actors and moving objects
- Optional packet-loss and latency simulation hooks in the networking helpers
- Debug overlay for RTT, throughput, packet loss, and message type statistics

## Project Layout

- `Game.sln` builds the playable client
- `Server.sln` builds the server and client together for local testing
- `Source/Game/` contains the client runtime and gameplay loop
- `Source/Server/` contains the UDP server
- `Source/Shared/` contains shared network data and the debug overlay
- `Source/Engine/` and `Source/External/` contain the engine/runtime support code required by the prototype

## Requirements

- Windows
- Visual Studio 2022 with the `v143` toolset
- Windows 10 SDK

## Build

1. Open `Server.sln` in Visual Studio 2022.
2. Select `Debug | x64`.
3. Build the solution.

The client executable is written to `Bin/`. Required runtime DLLs and shader binaries are already included in this repository.

## Run

1. Start the server project.
2. Start the game/client project.
3. Move the local player with `W`, `A`, `S`, and `D`.

Controls:

- `WASD` moves the local player
- `B` creates an object
- `C` removes a nearby object
- `V` creates a moving circular object

## Notes

- The prototype targets `127.0.0.1:42000`.
- Project settings live in `Bin/settings/`.
- Engine assets are loaded from `EngineAssets/`.
- Vendored third-party code and licenses are preserved where required.

