# CLI TCP Chat in C

A simple two-peer terminal chat application written in C.

The project uses TCP sockets for communication and POSIX threads to handle sending and receiving messages concurrently. One peer starts the application in **host** mode and waits for a connection, while the other starts it in **client** mode and connects to the host.

## Features

* TCP-based communication
* Two-peer terminal chat
* Simultaneous sending and receiving of messages
* POSIX threads for concurrent I/O
* Separate host and client modes

## Requirements

* POSIX-compatible system
* GCC

## Build

Clone the repository and build the project:

```bash
git clone https://github.com/cldprgm/peerchat.git
cd peerchat
make
```

The executable will be created as:

```text
./chat
```

To remove build artifacts:

```bash
make clean
```

To perform a clean rebuild:

```bash
make re
```

## Usage

### Start the host

The host listens for an incoming TCP connection:

```bash
./chat -m host
```

By default, port `8082` is used.

A custom port can be specified with `-p`:

```bash
./chat -m host -p 9000
```

### Start the client

The client connects to the host using its IPv4 address:

```bash
./chat -m client -ip 127.0.0.1
```

A custom port can also be specified:

```bash
./chat -m client -ip 127.0.0.1 -p 9000
```

### Command-line options

| Option        | Description                   |
| ------------- | ----------------------------- |
| `-m host`     | Start in host mode            |
| `-m client`   | Start in client mode          |
| `-ip ADDRESS` | IPv4 address of the host      |
| `-p PORT`     | TCP port, from `1` to `65535` |

## Example

Start the host in one terminal:

```bash
./chat -m host
```

Then start the client in another terminal:

```bash
./chat -m client -ip 127.0.0.1
```

Once connected, both peers can exchange messages through the terminal.


### Modules

**`args.c`**
Parses and validates command-line arguments.

**`network.c`**
Handles TCP sockets, connections, sending, and receiving messages.

**`chat.c`**
Implements the chat logic, terminal input, and background threads.

**`main.c`**
Initializes the application and selects the host or client mode.

## Technologies

* POSIX sockets
* TCP/IP
* POSIX threads (`pthread`)
* termios

## Limitations

This project is intentionally simple and currently supports:

* one connection per host
* two peers only
* IPv4 addresses
* terminal-based communication

It does not currently provide encryption, multiple simultaneous clients, message history, or a graphical interface.

## License

This project is available under the MIT License.
