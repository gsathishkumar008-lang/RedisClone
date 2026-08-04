# Redis-Inspired In-Memory Server (C++ / Windows IOCP)

A learning project that implements a small Redis-inspired, in-memory key-value server in C++. It uses Windows I/O Completion Ports (IOCP) to serve concurrent TCP clients and uses the Redis Serialization Protocol (RESP) for requests and replies.

> This is not a complete or production-ready Redis replacement. See [Current limitations](#current-limitations).

## Features

- Windows asynchronous TCP server built with IOCP
- RESP command parsing and RESP-formatted responses
- Per-connection request buffering for fragmented and pipelined TCP data
- String commands: `PING`, `SET`, `GET`, `DEL`, `EXISTS`, `KEYS`
- Expiration commands: `EXPIRE`, `TTL`, `PERSIST`
- List commands: `LPUSH`, `RPOP`
- Append-only-file (AOF) persistence and replay on startup
- Thread-safe in-memory database access
- Interactive command-line client

## Supported commands

| Command | Example | Description |
| --- | --- | --- |
| `PING` | `PING` | Returns `PONG`. |
| `SET` | `SET name Sathish` | Stores a string value. A normal `SET` removes an existing TTL. |
| `GET` | `GET name` | Returns a stored string, or a RESP null value when missing. |
| `DEL` | `DEL name` | Deletes one key. |
| `EXISTS` | `EXISTS name` | Returns `1` when the key exists, otherwise `0`. |
| `KEYS` | `KEYS` | Returns all stored keys. |
| `EXPIRE` | `EXPIRE name 30` | Sets an expiry in seconds. |
| `TTL` | `TTL name` | Returns remaining expiry time, `-1` for no expiry, and `-2` if missing. |
| `PERSIST` | `PERSIST name` | Removes a key's expiry. |
| `LPUSH` | `LPUSH tasks study` | Pushes a value to the left of a list. |
| `RPOP` | `RPOP tasks` | Pops a value from the right of a list. |
| `CLEAR` | `CLEAR` | Clears the database. This is a project-specific command, not standard Redis. |

## Requirements

- Windows
- A C++17 compiler; the included VS Code task uses MSYS2 UCRT64 `g++`
- WinSock2 (`ws2_32`), included with Windows

## Build

Open PowerShell in the project directory:

```powershell
cd C:\Users\sathi\Documents\CODES\RedisClone
```

Build the server:

```powershell
C:\msys64\ucrt64\bin\g++.exe -std=c++17 -Wall -Wextra server.cpp Database.cpp CommandHandler.cpp AOF.cpp IOCPServer.cpp -o server.exe -lws2_32
```

Build the interactive client:

```powershell
C:\msys64\ucrt64\bin\g++.exe -std=c++17 client.cpp -o client.exe -lws2_32
```

Alternatively, in VS Code press `Ctrl` + `Shift` + `B` and select **Build Redis Server**.

## Run

Start the server in one terminal:

```powershell
.\server.exe
```

Start the client in a second terminal:

```powershell
.\client.exe
```

Example session:

```text
redis> PING
+PONG

redis> SET name Sathish
+OK

redis> GET name
$7
Sathish

redis> EXPIRE name 10
:1

redis> TTL name
:9
```

The server listens on port `6379` by default. If startup reports a bind error, another process is already using port `6379`; stop that process or change the port in both `server.cpp` and `client.cpp`.

## Persistence

Successful mutating commands are written to `appendonly.aof`. On startup, the server replays that file to rebuild its in-memory state.

To start with an empty database, stop the server and delete `appendonly.aof`.

## Stress test

The repository includes `stress_test.cpp`, which creates multiple concurrent clients. Build and run it while the server is running:

```powershell
C:\msys64\ucrt64\bin\g++.exe -std=c++17 stress_test.cpp -o stress_test.exe -lws2_32
.\stress_test.exe
```

## Current limitations

- Windows only: networking is implemented with IOCP.
- Implements a small command subset, not the full Redis command set.
- No authentication, TLS, replication, clustering, transactions, Lua scripting, pub/sub, eviction policy, or memory limits.
- AOF is the only persistence mechanism; there are no RDB snapshots or AOF rewrite/compaction.
- The included client is intentionally simple and prints raw RESP replies.

## Resume description

**Redis-inspired in-memory key-value server (C++ / Windows IOCP)** — Built a concurrent TCP key-value server with RESP parsing, TTL expiration, append-only-file recovery, and basic string/list commands.
