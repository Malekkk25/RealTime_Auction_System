# Real-Time Auction System

A real-time auction application built in C using TCP sockets and POSIX threads.  
It includes a server and a terminal client that communicate in real time to manage live bidding sessions.

## Features

### Server
- Accepts multiple client connections.
- Starts auctions automatically when at least one client is connected.
- Broadcasts auction updates to all clients.
- Handles bids, timers, and auction results.
- Logs events with timestamps and colors.

### Client
- Connects to the auction server.
- Lets users join with a nickname.
- Displays live auction information in the terminal.
- Sends bids during an active auction.
- Shows accepted and rejected bid notifications.
- Displays the winner when the auction ends.

## Technologies
- C language
- TCP sockets
- POSIX threads (`pthread`)
- ANSI terminal colors

## Project Structure
- `server.c` — auction server.
- `client.c` — terminal client.
- `auction.h` — shared constants and declarations.
- `Makefile` — build automation.
- `bin/` — compiled executables.

## Requirements
- GCC
- GNU Make
- Linux or another POSIX-compatible system
- `pthread` support

## Build

Make sure the `bin/` directory exists before compiling:

```bash
mkdir -p bin
make
```

This will generate:
- `bin/server`
- `bin/client`

## Run

### Start the server
```bash
make run-server
```

### Start a client
```bash
make run-client
```

### Start a client on another machine
```bash
make run-client-remote
```

You can also run the client manually:

```bash
./bin/client 127.0.0.1
```


## Commands
- `bid <amount>` — place a bid.
- `quit` — exit the client.

### Example
```bash
bid 150
```

## Network Protocol
The server and client exchange text messages such as:
- `JOIN <name>`
- `WELCOME <id>`
- `NEW_AUCTION <item>|<price>`
- `NEW_BID <name>|<amount>|<time_left>`
- `TIME <seconds>`
- `REJECT <reason>`
- `END <winner>|<price>`
- `PLAYERS <count>`

## Notes
- A bid must be strictly higher than the current price.
- If a bid is placed near the end, the auction timer is extended.
- The server ignores `SIGPIPE` to avoid crashes when a client disconnects unexpectedly.

## Author
- MALEK AYED
