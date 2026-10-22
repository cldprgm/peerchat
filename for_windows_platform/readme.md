# CLI TCP Chat for Windows

A two-peer terminal chat application written in C. This Windows-specific copy preserves the original project's chat flow, TCP protocol, key exchange, and encrypted message format. It uses Winsock for networking, Windows threads for simultaneous sending and receiving, and the Windows console for keyboard input.

## Build with MSYS2 UCRT64

Install MSYS2, open the **MSYS2 UCRT64** terminal, and install the compiler, GNU Make, and libsodium:

```bash
pacman -S --needed make mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-libsodium
```

From the project directory in that terminal, build with GNU Make:

```bash
make
```

The executable is `build/chat.exe`. The Makefile links libsodium and the MinGW runtime statically, so the executable does not need MSYS2, GCC, or libsodium DLLs at runtime. It still uses Windows system DLLs; use Windows 10 or later, which provides the required Universal C Runtime. To remove build files:

```bash
make clean
```

## Usage

### Start the host

```bash
./build/chat.exe -m host
```

By default, the host listens on port `8082`. Specify another port with `-p`:

```bash
./build/chat.exe -m host -p 9000
```

### Start the client

```bash
./build/chat.exe -m client -ip 127.0.0.1
```

For a custom port:

```bash
./build/chat.exe -m client -ip 127.0.0.1 -p 9000
```

Replace `127.0.0.1` with the host's IPv4 address when connecting from another computer. `127.0.0.1` works only when both processes run on the same machine. Windows Firewall may need to allow the host application to accept connections.