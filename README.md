*This project has been created as part of the 42 curriculum by nakbas, etorun and makpolat.*

# ft_irc

## Description

`ft_irc` is an IRC (Internet Relay Chat) server written from scratch in **C++98**, following
the RFC 1459 / RFC 2812 message format. It accepts connections from real IRC clients over
TCP/IP, authenticates them with a server password, and lets them set a nickname, register a
user, join channels, exchange public channel messages and private messages, and administrate
channels as operators.

The goal of the project is to understand how a standard network protocol works end to end:
socket setup, non-blocking I/O multiplexed by a single `poll()` loop, reassembling TCP
byte streams into complete protocol messages, and replying with the exact numeric replies a
real client expects.

Key design points:

- **A single `poll()` call** drives everything — listening, reading, and writing. No forking,
  no threads, no per-client blocking calls.
- **All sockets are non-blocking** (`fcntl(fd, F_SETFL, O_NONBLOCK)`).
- **Every client owns an input and an output buffer.** Incoming data is appended to the input
  buffer and only parsed once a complete `\n`-terminated line is available, so a command split
  across several TCP packets is rebuilt correctly. Outgoing data is queued and written only
  when `poll()` reports the socket as writable, so a slow reader can never block the server.
- **Disconnections are deferred.** A client is only marked for removal while commands are being
  processed; the actual cleanup happens at the end of the poll iteration, and only after its
  pending output (for example `ERROR :Closing link`) has been flushed. This avoids dangling
  pointers and use-after-free.
- No server-to-server communication and no client are implemented — that is out of scope.

**Reference client: `irssi`.**

## Instructions

### Build

```bash
make            # build ./ircserv
make clean      # remove object files
make fclean     # remove object files and the binary
make re         # rebuild from scratch
```

Compiled with `c++ -Wall -Wextra -Werror -std=c++98`.

### Run

```bash
./ircserv <port> <password>
```

- `port` — the TCP port the server listens on (1–65535).
- `password` — the connection password every client must send with `PASS`.

Example:

```bash
./ircserv 6667 secret
```

### Connect with irssi

```bash
irssi
/connect 127.0.0.1 6667 secret
/join #general
/msg #general hello
```

Or with `nc` for raw protocol testing:

```bash
nc -C 127.0.0.1 6667
PASS secret
NICK bob
USER bob 0 * :Bob
JOIN #general
PRIVMSG #general :hello
```

### Tests

An automated test suite that drives the server the way `irssi` does is included (not part of the
graded deliverable):

```bash
make test              # runs tests/run_tests.sh
./tests/irssi_manual.sh # helper for manual testing with irssi
```

## Features

### Implemented commands

| Command | Purpose |
|---------|---------|
| `PASS`  | Send the connection password |
| `NICK`  | Set or change the nickname |
| `USER`  | Register the username and real name |
| `CAP`   | IRCv3 capability negotiation (empty list; needed so irssi completes registration) |
| `PING` / `PONG` | Keep-alive / lag measurement |
| `QUIT`  | Disconnect, announced to shared channels |
| `JOIN`  | Join one or more channels (`JOIN 0` leaves all) |
| `PART`  | Leave one or more channels |
| `PRIVMSG` / `NOTICE` | Message a channel or a user (`NOTICE` never generates error replies) |
| `TOPIC` | View or change the channel topic |
| `KICK`  | Eject a client from a channel *(operator)* |
| `INVITE`| Invite a client to a channel *(operator on `+i` channels)* |
| `MODE`  | Change channel or user modes |
| `NAMES` / `WHO` / `WHOIS` / `MOTD` | Informational queries |

### Channel modes

| Mode | Meaning |
|------|---------|
| `+i` / `-i` | Invite-only channel |
| `+t` / `-t` | Restrict `TOPIC` to channel operators |
| `+k <key>` / `-k` | Set / remove the channel key (password) |
| `+o <nick>` / `-o <nick>` | Give / take channel operator privilege |
| `+l <limit>` / `-l` | Set / remove the user limit |

The client that creates a channel automatically becomes its operator.

## Technical choices

- **`poll()`** was chosen over `select()` (no `FD_SETSIZE` limit, cleaner per-fd event handling)
  and over `kqueue()`/`epoll()` (portable between macOS and Linux).
- **`POLLOUT` is only requested when a client actually has queued output**, so the server sleeps
  in `poll()` instead of spinning while idle.
- **The return value of `recv`/`send` alone decides what happens next.** `errno` is never
  inspected after a `recv`/`send` to decide whether to retry: since both are called only after
  `poll()` reported the socket ready, a return of `0` or `-1` is treated as the end of the
  connection.
- **Case-insensitive nicknames and channel names** follow the RFC 1459 casemapping (`[]\^` are
  the uppercase forms of `{}|~`).
- **Input is bounded** (`MAX_INPUT_SIZE`) so a client that never sends a line terminator cannot
  make the server allocate without limit.

### Source layout

```
include/    Server, Client, Channel, Parser, Utils headers + Replies.hpp (numeric codes)
src/
  main.cpp             argument validation, signal handlers
  Server.cpp           socket setup, the poll() loop, connection lifecycle
  ServerUtils.cpp      lookup helpers, reply helpers, registration, command dispatch
  Client.cpp           per-client state and input/output buffers
  Channel.cpp          members, operators, invites, modes, broadcasting
  Parser.cpp           RFC 1459 message parsing (prefix, command, params, trailing)
  Utils.cpp            string helpers, nickname/channel name validation
  Commands*.cpp        command implementations, grouped by area
tests/                 automated and manual test scripts
```

## Resources

- [RFC 1459 — Internet Relay Chat Protocol](https://datatracker.ietf.org/doc/html/rfc1459)
- [RFC 2812 — IRC Client Protocol](https://datatracker.ietf.org/doc/html/rfc2812)
- [Modern IRC client protocol documentation](https://modern.ircdocs.horse/)
- [IRC numeric replies list](https://defs.ircdocs.horse/defs/numerics.html)
- Beej's Guide to Network Programming — sockets, `poll()`, and non-blocking I/O
- `man poll`, `man 2 recv`, `man 2 send`, `man fcntl`
- `irssi` documentation, used as the reference client

### Use of AI

AI assistance (Claude) was used for the following tasks, and every generated part was read,
tested and adjusted by hand before being kept:

- **Protocol research and reply formats** — checking which numeric replies `irssi` expects during
  registration (`001`–`005`, `375`/`372`/`376`) and after `JOIN` (`353`/`366`, `352`, `324`), and
  why `CAP LS`/`CAP END` has to be answered.
- **Reviewing the poll loop against the subject's constraints** — in particular removing the
  `errno`-based retry logic after `recv`/`send` and making sure no `send` happens outside the
  `poll()` loop, by deferring a client's removal until its output buffer is flushed.
- **Writing the automated test script** (`tests/run_tests.sh`), which replays realistic IRC
  sessions against the server and checks the replies.
- **Drafting this README.**

The architecture (class split, buffering strategy, deferred disconnect design) and the command
implementations were reviewed line by line, and the behaviour of each part can be explained and
defended.
