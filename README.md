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

A basic connectionless TCP implementation takes about 46 bytes of RAM, plus one scratch buffer for TX/RX of whatever MTU size is used, plus about 4kB of flash.  For stateful TCP, it takes about 40 bytes per connection.

## Usage


1. Configure and include the library in one file.  You can include it in multiple files, but, only one file can define `SFHIP_IMPLEMENTATION`. The below example is from the ![example](example/):
```c
#define HIP_PHY_HEADER_LENGTH_BYTES 4
#define SFHIP_WARN( x... ) fprintf( stderr, x );
#define SFHIP_IMPLEMENTATION
#define SFHIP_UDP_USER_HANDLER example_udp_user_handler
#include "sfhip.h"
```
3. Create your sfhip object.  You can have multiple sfhip objects per application for multiple network interfaces.
```c
  sfhip hip = {
    .ip =      HIPIP( 192, 168,  14, 251 ),
    .mask =    HIPIP( 255, 255, 255, 0   ),
    .gateway = HIPIP( 192, 168,  14, 1   ),
    .self_mac = { 0xf0, 0x11, 0x22, 0x33, 0x44, 0x55 },
    .hostname = "sfhip_test_linux", // Only used for DHCP client, if enabled.
    .opaque = <whatever you want, so you can identify this connection>;
  };
```
3. When you get a packet on your interface, call this functions:
```c
int sfhip_accept_packet( sfhip * hip, sfhip_phy_packet_mtu * data, int length );
```
4. Call this function periodically, it's ok if milliseconds is zero, but, make sure that it captures the sum of the time that has been spent accurately.
```c
int sfhip_tick( sfhip * hip, sfhip_phy_packet_mtu * scratch, int milliseconds );
```
5. You will get callbacks for any of the features you have enabled.  Some callbacks are:
```c
// Called when you get a new IP address from DHCP.
void sfhip_got_dhcp_lease( sfhip * hip, sfhip_address addr );

// Called on UDP packet reception
int example_udp_user_handler(
    sfhip * hip,
    sfhip_phy_packet_mtu * pkt,
    uint8_t * payload,
    int ulen,
    int source_port,
    int destination_port );

//Called when you get a TCP connection request.
int sfhip_tcp_accept_connection( sfhip * hip, int sockno, int localport, hipbe32 remote_host );

//Called when there is new TCP data, acks or you need to retransmit:
sfhip_length_or_tcp_code sfhip_tcp_event( sfhip * hip, int sockno,
    uint8_t * ip_payload, int ip_payload_length, int max_out_payload,
    int acked );

//Called on TCP closure
void sfhip_tcp_socket_closed( sfhip * hip, int sockno );
```

## Setup

```
sudo apt-get install bridge-utils build-essential gcc-14-riscv64-linux-gnu
```

`gcc-14-riscv64-linux-gnu` is only for size testing.

## Future work

More servers and features!  Who knows, maybe some day IPv6.

## License note

This code may be licnsed under the Unlicense, MIT or any of the BSD licenses as you wish, and can be included in open or closed source projects at will.

If you contribute code to this repository, you certify you are granting permission for your code to be licensed under the Unlicense, and agree to the above statement.

