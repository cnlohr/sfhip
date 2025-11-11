#include <stdio.h>

#define HIP_PHY_HEADER_LENGTH_BYTES 4

#define SFHIP_IMPLEMENTATION
#include "sfhip.h"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/socket.h>
#include <sys/types.h>

#define FAIL( x... )          \
	{                         \
		fprintf( stderr, x ); \
		exit( -5 );           \
	}

int ifindex;
int fd, fdprime;
int usetaptun;
uint8_t mymac[6];

int sfhip_send_packet( struct sfhip * hip, struct sfhip_phy_packet * data, int length )
{
	if ( length < 18 ) return -1;
	uint8_t * dpl = (uint8_t *)data;
	dpl[2] = dpl[16];
	dpl[3] = dpl[17];

#if 1
	printf( "TX %4d: ", length );
	int i;
	for ( i = 0; i < length; i++ )
	{
		if ( ( i & 0xf ) == 0 && i ) printf( "\n         " );
		printf( "%02x ", dpl[i] );
	}
	printf( "\n" );
#endif

	int sendlen = length - ( usetaptun ? 0 : 4 );
	int r = write( fd, dpl + ( usetaptun ? 0 : 4 ), sendlen );
	if ( r != sendlen )
	{
		fprintf( stderr, "Warning: write() failed with error %s\n", strerror( errno ) );
	}

	struct sockaddr_ll socket_address = {
	    .sll_ifindex = ifindex,
	    .sll_halen = 0,
	    .sll_family = AF_PACKET,
	    .sll_protocol = htons( ETH_P_ALL ), // htons(ETH_P_802_3),
	    .sll_hatype = 0,
	    .sll_pkttype = PACKET_HOST, // PACKET_OUTGOING, //PACKET_LOOPBACK,
	};

	//	memcpy( socket_address.sll_addr, dpl + 4, 6 );
	// printf( "%02x:%02x:%02x:%02x:%02x:%02x\n", dpl[4], dpl[5],dpl[6],dpl[7],dpl[8], dpl[9] );

	r = sendto( fdprime, dpl + ( usetaptun ? 0 : 4 ), sendlen, MSG_NOSIGNAL,
	            (struct sockaddr *)&socket_address, sizeof( socket_address ) );
	if ( r != sendlen )
	{
		fprintf( stderr, "Warning: write() failed with error %s\n", strerror( errno ) );
	}

	return 0;
}

int main( int argc, char ** argv )
{
	if ( argc < 2 )
		goto failhelp;

	struct sfhip hip = {
	    .ip = HIPIP( 192, 168, 13, 251 ),
	    .mask = HIPIP( 255, 255, 255, 0 ),
	    .gateway = HIPIP( 192, 168, 13, 1 ),
	    .self_mac = { 0xf0, 0x11, 0x22, 0x33, 0x44, 0x55 },
	};

	{
		const char * devname = argv[1];

		int istap = strncmp( devname, "tap", 3 ) == 0;
		int istun = strncmp( devname, "tun", 3 ) == 0;

		if ( istun || istap )
		{
			usetaptun = 1;
			fd = open( "/dev/net/tun", O_RDWR );
			if ( fd == -1 )
				FAIL( "Can't open dev net tap/tun\n" );

			struct ifreq ifr = { .ifr_flags = istap ? IFF_TAP : IFF_TUN };
			strncpy( ifr.ifr_name, devname, IFNAMSIZ );
			if ( ( ioctl( fd, TUNSETIFF, (void *)&ifr ) ) == -1 )
				FAIL( "TUNSETIFF failed %s\n", strerror( errno ) );
		}
		else
		{
			fd = socket( PF_PACKET, SOCK_RAW, htons( ETH_P_ALL ) );        // or AF_PACKET/IPPROTO_RAW / PF_PACKET/htons(ETH_P_ALL)
			fdprime = socket( PF_PACKET, SOCK_RAW, htons( ETH_P_802_3 ) ); // or AF_PACKET/IPPROTO_RAW / PF_PACKET/htons(ETH_P_ALL)

			struct ifreq if_idx;
			memset( &if_idx, 0, sizeof( struct ifreq ) );
			strncpy( if_idx.ifr_name, devname, IFNAMSIZ - 1 );
			if ( ioctl( fd, SIOCGIFINDEX, &if_idx ) < 0 )
			{
				fprintf( stderr, "Error: can't get index to interface [%s]\n", devname );
				return -1;
			}

			ifindex = if_idx.ifr_ifindex;
#if 1
			struct sockaddr_ll socket_address = {
			    .sll_ifindex = ifindex,
			    .sll_halen = 0,
			    .sll_family = AF_PACKET,
			    .sll_protocol = htons( ETH_P_ALL ),
			    .sll_hatype = 0,
			    .sll_pkttype = PACKET_BROADCAST,
			};

			if ( bind( fd, (const struct sockaddr *)&socket_address, sizeof( socket_address ) ) < 0 )
			{
				fprintf( stderr, "Cannot bind to %s\n", devname );
				close( fd );
				exit( -5 );
			}
#endif

#if 1
			// https://stackoverflow.com/questions/10070008/reading-from-pf-packet-sock-raw-with-read-misses-packets MAYBE?
			struct packet_mreq mr = {
			    .mr_ifindex = socket_address.sll_ifindex,
			    .mr_type = PACKET_MR_PROMISC,
			};

			if ( setsockopt( fd, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mr, sizeof( mr ) ) < 0 )
			{
				FAIL( "Error setting packet membership\n" );
			}
#endif

#if 1
			struct ifreq ifr = { 0 };

			strncpy( ifr.ifr_name, devname, IFNAMSIZ - 1 );
			if ( ioctl( fd, SIOCGIFHWADDR, &ifr ) < 0 )
			{
				FAIL( "Error: Couldn't get self-MAC\n" );
			}

			memcpy( mymac, (char *)ifr.ifr_hwaddr.sa_data, 6 );
			int rv = setsockopt( fd, SOL_SOCKET, SO_BINDTODEVICE, devname, strlen( devname ) );
			printf( "SO_BINDTODEVICE: %d / dev index: %d / MAC: \n", rv, socket_address.sll_ifindex );
#endif
			//			struct ifreq ifrprom = { 0 };
			//			ifrprom.ifr_flags |= IFF_PROMISC;
			//			ioctl( fd, SIOCSIFFLAGS, &ifrprom);
		}
	}

	do
	{
		uint8_t buf[2048 + 4] HIPALIGN32 = { 0 };

		// If not a taptun device, add 4 bytes to the offset
		int r = read( fd, buf + ( usetaptun ? 0 : 4 ), 2048 );
		if ( r < 0 )
		{
			fprintf( stderr, "read() failed %s\n", strerror( errno ) );
			break;
		}

#if 0
		printf( "%4d: ", r );
		int i;
		for( i = 0; i < r; i++ )
		{
			if( (i & 0xf) == 0 && i ) printf( "\n      " );
			printf( "%02x ", buf[i] );
		}
		printf( "\n" );
#endif
		sfhip_accept_packet( &hip, (struct sfhip_phy_packet *)buf, r + ( usetaptun ? 0 : 4 ) );

	} while ( 1 );

	close( fd );
	return 0;
failhelp:
	FAIL( "Usage: [tool] [tunX|tapX]\n" );
}
