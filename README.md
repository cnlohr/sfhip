# sfhip

SFHIP (Single-File Header IP stack) is writtenin the same vein as ![uip](https://github.com/adamdunkels/uip) but is written in a much more modern single-file-header-library pattern as well as only focusing on optimizations with 32-bit systems in mind.

**NOTE** This stack is still a work in progress, and has not developed a stable API at this time, expect breaking changes.

While still in early stages of development, but as it stands it has:
1. Basic MAC and IPv4 stack, designed for zero-copy operations from DMA descriptors.
2. ARP reply functionality.
3. DHCP client (optional)
4. UDP packet receiption, sending.  (And replying)
5. TCP connections, and connectionless TCP mode.
6. An example connectionless HTTP server (in ![rawtcp/](rawtcp))
7. A basic example with stateful TCP and UDP replies in ![example/](example))

Design Principles:
1. Everything should be two-byte-aligned (because MAC headers are 14 bytes instead of 16).
2. Do not add frills.  Only what's needed for use with other systems.
3. Push as much logic as is feasible to the user layer.  For instance, with TCP, the user layer is responsbile for being able retransmit data when requested by the stack.
4. Be considerate of RISC-V and ARM ABIs surrounding ideally 6 or less parameters being passed in.
5. Perform tail calls wherever possible.
6. Avoid register spill where possible.
7. Assume HTONS, HTONL aren't free.
8. Make the job for the compiler's optimizer easy.

The stack is all designed around operating with either immediate-replies to other packets, by re-writing RX frames to TX, as well as a `tick()` function for sending unsolicited packets.

A basic connectionless TCP implementation takes about 46 bytes of RAM, plus one scratch buffer for TX/RX of whatever MTU size is used, plus about 4kB of text.  For stateful TCP, it takes about 40 bytes per connection.

## Setup

```
sudo apt-get install bridge-utils build-essential gcc-14-riscv64-linux-gnu
```

`gcc-14-riscv64-linux-gnu` is only for size testing.

## License note

This code may be licnsed under the Unlicense, MIT or any of the BSD licenses as you wish, and can be included in open or closed source projects at will.

If you contribute code to this repository, you certify you are granting permission for your code to be licensed under the Unlicense, and agree to the above statement.

