# c-struct-over-the-wire

A minimal C demo of sending a message over TCP using a **C struct as the packet
header**. The server sends one length-prefixed text message, and the client reads the
header, learns how many bytes follow, then reads and prints the body.

It is a learning example: about 100 lines of C, no dependencies beyond a POSIX system.
The [Known issues](#known-issues) section lists what you would need to fix before using
the pattern for real traffic.

```
 server (0.0.0.0:8080)                          client
   |  accept() one connection                     |  connect(<ip>, <port>)
   |  send( [header 8 B][content 97 B] ) -------> |  recv(header, 8 B)
   |  close, exit                                 |  recv(content, length B), print
```

## Requirements

- Linux or another POSIX system (BSD sockets, `<netinet/in.h>`, `<arpa/inet.h>`)
- `gcc`
- `make`

## Build and run

```bash
make            # builds bin/server and bin/client
make clean      # removes bin/ and build/
```

Start the server in one terminal:

```bash
./bin/server
```

```
listening ...
connected to 127.0.0.1:50246
```

Run the client in another:

```bash
./bin/client 127.0.0.1 8080
```

```
connecting ...
msg[97]: hello mate, I'm just calling to check on you man. You've been quiet of late what's going on man?
```

Usage: `./bin/client <ip> <port>`. The server port is fixed at **8080** in `src/server.c`.
The server handles **one** client and then exits. After it exits, wait about a minute
before starting it again (see [Known issues](#known-issues), item 8).

## Project layout

```
include/packet.h   the header struct and packet struct shared by both programs
src/server.c       listens, accepts one client, sends one packet
src/client.c       connects, receives one packet, prints it
makefile           builds into build/ (objects) and bin/ (executables)
```

## The wire format

`include/packet.h` defines:

```c
typedef struct {
    uint8_t  type;
    uint32_t length;
} header;

typedef struct {
    header p_header;
    char   content[];      // flexible array member: the message body follows the header
} packet;
```

On x86-64 Linux the compiler pads the 1-byte `type` so that `length` sits on a 4-byte
boundary, so the header is **8 bytes**, not 5. The bytes on the wire (captured from the
running server) are:

| Offset | Size | Field | Contents |
|---|---|---|---|
| 0 | 1 | `type` | **never assigned by the server**; the client ignores it |
| 1 | 3 | padding | compiler-inserted, **uninitialized** |
| 4 | 4 | `length` | body size as a `uint32_t` in **network byte order** (`htonl`) |
| 8 | `length` | `content` | the message, including its trailing `\0` |

The hard-coded greeting is 97 bytes including the terminator, so the server sends
`8 + 97 = 105` bytes per packet.

Points worth understanding:

- **Why `sizeof(packet)` is 8.** A flexible array member contributes no size. To build a
  packet, allocate `sizeof(header) + body_size` bytes, as `server.c` does with `malloc`.
- **Why `length` goes through `htonl`/`ntohl`.** Different machines store integers in
  different byte orders; the network order is big-endian. `type` is one byte, so it needs no
  conversion.
- **TCP has no message boundaries.** It is a byte stream. That is the reason for the length
  field: the receiver reads the fixed-size header first, then exactly `length` more bytes.
- **Sending the struct directly only works while both ends agree on its memory layout.**
  Here the server and client are built from the same header by the same compiler on the same
  architecture. See [Making the format portable](#making-the-format-portable).

## How the code works

### `src/server.c`

1. Creates a TCP socket, binds `INADDR_ANY:8080` and calls `listen(fd, 3)`.
2. `accept()`s one connection (the `ACCEPT:` label only retries when `accept` fails) and
   prints the peer address with `inet_ntop`.
3. Allocates a packet of `sizeof(header) + sizeof(msg)` bytes, sets `length`
   to `htonl(sizeof(msg))`, copies the message in and `send()`s the whole buffer.
4. Frees the buffer, closes both sockets and returns.

### `src/client.c`

1. Parses `<ip> <port>`, fills a `sockaddr_in` and creates a socket.
2. Calls `connect()` in a loop (`CONNECT:` label) until it succeeds.
3. `recv()`s `sizeof(header)` bytes, converts `length` with `ntohl`, allocates `length`
   bytes and `recv()`s the body.
4. Prints `msg[<length>]: <text>`, frees the buffer and closes the sockets.

### `makefile`

Compiles each `src/*.c` to `build/*.o` with `-Iinclude -Wall -Wextra -std=c99`, then links
`bin/server` with `gcc` and `bin/client` with `g++`. A `build/%.o: src/%.cpp` rule exists
but nothing uses it.

## Making the format portable

To stop the compiler from choosing the layout, pin it down and check it:

```c
#pragma once
#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint8_t  type;
    uint32_t length;       // always stored in network byte order
} header;

_Static_assert(sizeof(header) == 5, "wire header must be 5 bytes");
```

This removes the padding (the header becomes 5 bytes, so rebuild both programs).
`__attribute__` is a GCC/Clang extension. A fully portable alternative is to serialize each
field by hand into a byte buffer (`buf[0] = type;` then write `length` with `htonl`) and
never send the struct itself.

## Known issues

I built the project and checked each of these by running it. Items 1, 2, 3, 5, 6, 7, 8, 9 and
10 are reproducible with the commands given in the last column. The rest I found by reading
the code.

| # | Where | Problem | Effect and how to see it |
|---|---|---|---|
| 1 | `server.c` | `type` is never set, and `malloc` does not zero the struct's padding | The server sends 4 bytes of uninitialized heap memory with every packet. Run `MALLOC_PERTURB_=165 ./bin/server` and capture the stream: header begins `5a 5a 5a 5a`. Fix: use `calloc` or `memset`, and assign `type`. |
| 2 | `packet.h` | Does not include `<stdint.h>` | The header is not self-contained: `echo '#include <packet.h>' > t.c && gcc -Iinclude -fsyntax-only t.c` fails with `unknown type name 'uint8_t'`. It only compiles today because `<netinet/in.h>` is included first. |
| 3 | `client.c` | `server_fd = connect(...)` stores connect's return value (0 or -1), then `close(server_fd)` | On success it **closes file descriptor 0 (stdin)**, plus the real socket. Remove the variable. |
| 4 | both | One `send()` and one `recv()` per field, return values unchecked | TCP may deliver or accept fewer bytes than requested. Loop until all bytes are transferred (or use `MSG_WAITALL` on the client) and check for errors and EOF. It works on loopback in practice. |
| 5 | `client.c` | Body is not NUL-terminated by the client, and `length` is trusted | If a server sends text without the terminator, `printf("%s")` reads past the buffer (AddressSanitizer reports a heap-buffer-overflow). A hostile `length` also makes the client `malloc` up to 4 GiB. Fix: cap `length`, allocate `length + 1` and set the last byte to 0. |
| 6 | `client.c` | Failed `connect()` retries immediately in a tight `goto` loop | With no server listening, it printed about 270,000 error lines in 3 seconds. It also retries on the same socket, which POSIX leaves unspecified after a failure. Fix: sleep, and close and recreate the socket per attempt. |
| 7 | `client.c` | Usage message contains `\v` | It prints a vertical tab instead of a newline: `E: wrong usage^K./bin/client <ip> <port>`. Use `\n`. |
| 8 | `server.c` | No `SO_REUSEADDR` before `bind` | Restarting the server within about a minute fails: `bind: Address already in use` (exit code 1). The server closes first, so its port lingers in `TIME_WAIT`. Fix: `setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, ...)`. |
| 9 | `makefile` | The line `PHONY: all clean` is missing its leading dot | `.PHONY` is what marks targets as always-run. As written, `touch clean && make clean` prints `'clean' is up to date` and does nothing. |
| 10 | `makefile` | Object files do not depend on `include/packet.h` | After `touch include/packet.h`, `make` says `Nothing to be done`. Changing the struct leaves stale binaries, which matters in a project about struct layout. Add `-MMD -MP` and `-include $(wildcard build/*.d)`. |
| 11 | `client.c` | `printf("%d", len)` with a `uint32_t` | Use `%u` (or `PRIu32`); lengths above 2^31 print as negative. |
| 12 | both | `sockaddr_in` is not zeroed, and `socket()`, `inet_pton()` and `send()` results are unchecked | Add `memset(&addr, 0, sizeof addr)` and check return values. Ignore `SIGPIPE` or use `MSG_NOSIGNAL` on `send`. |
| 13 | `server.c` | `accept` failure path loops forever without a delay, and `s_len` is not reset | A persistent error (for example running out of file descriptors) would spin the CPU. |
| 14 | `makefile` | Client is linked with `g++ ... -std=c++17` | Unnecessary for C code and requires `g++` to be installed. Link with `gcc`. |

## Extending it

- **Use the `type` field.** Define constants (`MSG_TEXT = 1`, ...), set the field in
  `server.c`, and `switch` on it in `client.c`.
- **Serve more than one client.** Wrap `accept()` in a loop and handle each connection in a
  thread, a child process or an event loop (`poll`/`epoll`).
- **Add a magic number or version** to the header so the receiver can detect a desynchronised
  stream or a mismatched protocol version.

