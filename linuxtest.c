#include <stdio.h>
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
#include <time.h>

#include <poll.h>

#include "example.c"

#define FAIL( x... )          \
	{                         \
		fprintf( stderr, x ); \
		exit( -5 );           \
	}

int fdtap, fddev;

uint8_t mymac_dev[6];
uint8_t mymac_tap[6];

int linux_send_packet( uint8_t * data, int length )
{
	if ( length < 18 ) return -1;
	uint8_t * dpl = (uint8_t *)data;
	dpl[2] = dpl[16];
	dpl[3] = dpl[17];

#if 0
	printf( "TX %4d: ", length );
	for( int i = 0; i < length; i++ )
	{
		if( (i & 0xf) == 0 && i ) printf( "\n         " );
		printf( "%02x ", dpl[i] );
	}
	printf( "\n" );
#endif

	int usetaptun = 0;
	int offset = 0;
	int fd = fdtap;

	// Determine if the target MAC is the host's TAP device.
	if ( memcmp( mymac_tap, dpl + 4, 6 ) == 0 )
	{
		usetaptun = 1;
	}

	// For if we only have one device.
	if ( !fdtap ) usetaptun = 0;
	if ( !fddev ) usetaptun = 1;

	if ( !usetaptun )
	{
		fd = fddev;
		offset = 4;
	}

	int sendlen = length - offset;
	int r = write( fd, dpl + offset, sendlen );
	if ( r != sendlen )
	{
		fprintf( stderr, "Warning: write() failed with error %s\n", strerror( errno ) );
	}

	return 0;
}

int linuxtest( const char * devname_tap, const char * devname_eth )
{

	fflush( stdout );

	{
		const char * devname = devname_tap;

		if ( strcmp( devname, "-" ) == 0 )
		{
			fdtap = 0;
		}
		else
		{
			int istap = strncmp( devname, "tap", 3 ) == 0;
			int istun = strncmp( devname, "tun", 3 ) == 0;

			fdtap = open( "/dev/net/tun", O_RDWR );
			if ( fdtap == -1 )
				FAIL( "Can't open dev net tap/tun\n" );

			struct ifreq ifr = { .ifr_flags = ( istap ? IFF_TAP : IFF_TUN ) };

			strncpy( ifr.ifr_name, devname, IFNAMSIZ );

			if ( ( ioctl( fdtap, TUNSETIFF, (void *)&ifr ) ) == -1 )
				FAIL( "TUNSETIFF failed %s\n", strerror( errno ) );

			int sockup = socket( AF_INET, SOCK_DGRAM, IPPROTO_IP );

			struct ifreq ifr_ifup = { 0 };
			ifr.ifr_flags = 0;
			( (struct sockaddr_in *)&ifr.ifr_addr )->sin_family = AF_INET;
			( (struct sockaddr_in *)&ifr.ifr_addr )->sin_addr.s_addr = inet_addr( TAP_ADDR );
			if ( ( ioctl( sockup, SIOCSIFADDR, (void *)&ifr ) ) == -1 )
				FAIL( "SIOCSIFADDR failed %s\n", strerror( errno ) );

			ifr_ifup.ifr_flags = ( istap ? IFF_TAP : IFF_TUN );
			strncpy( ifr_ifup.ifr_name, devname, IFNAMSIZ );
			ifr_ifup.ifr_flags = IFF_UP | IFF_BROADCAST | IFF_RUNNING | IFF_MULTICAST;
			if ( ( ioctl( sockup, SIOCSIFFLAGS, (void *)&ifr_ifup ) ) == -1 )
				FAIL( "SIOCSIFFLAGS failed %s\n", strerror( errno ) );
			if ( ( ioctl( sockup, SIOCSIFFLAGS, (void *)&ifr_ifup ) ) == -1 )
				FAIL( "SIOCSIFFLAGS failed %s\n", strerror( errno ) );

			// Also need to configrue its IP

			close( sockup );

			struct ifreq ifr_mac = { 0 };
			if ( ioctl( fdtap, SIOCGIFHWADDR, &ifr_mac ) < 0 )
			{
				FAIL( "Error: Couldn't get self-MAC\n" );
			}

			memcpy( mymac_tap, (char *)ifr_mac.ifr_hwaddr.sa_data, 6 );
		}
	}

	{
		const char * devname = devname_eth;

		if ( strcmp( devname, "-" ) == 0 )
		{
			fddev = 0;
		}
		else
		{
			fddev = socket( PF_PACKET, SOCK_RAW, htons( ETH_P_ALL ) );

			struct ifreq if_idx;
			memset( &if_idx, 0, sizeof( struct ifreq ) );
			strncpy( if_idx.ifr_name, devname, IFNAMSIZ - 1 );
			if ( ioctl( fddev, SIOCGIFINDEX, &if_idx ) < 0 )
			{
				fprintf( stderr, "Error: can't get index to interface [%s]\n", devname );
				return -1;
			}

			struct sockaddr_ll socket_address = {
			    .sll_ifindex = if_idx.ifr_ifindex,
			    .sll_halen = 0,
			    .sll_family = AF_PACKET,
			    .sll_protocol = htons( ETH_P_ALL ),
			    .sll_hatype = 0,
			    .sll_pkttype = PACKET_BROADCAST,
			};

			if ( bind( fddev, (const struct sockaddr *)&socket_address, sizeof( socket_address ) ) < 0 )
				FAIL( "Cannot bind to %s\n", devname );

			// https://stackoverflow.com/questions/10070008/reading-from-pf-packet-sock-raw-with-read-misses-packets MAYBE?
			struct packet_mreq mr = {
			    .mr_ifindex = socket_address.sll_ifindex,
			    .mr_type = PACKET_MR_PROMISC,
			};

			if ( setsockopt( fddev, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mr, sizeof( mr ) ) < 0 )
			{
				FAIL( "Error setting packet membership\n" );
			}

			struct ifreq ifr = { 0 };

			strncpy( ifr.ifr_name, devname, IFNAMSIZ - 1 );
			if ( ioctl( fddev, SIOCGIFHWADDR, &ifr ) < 0 )
			{
				FAIL( "Error: Couldn't get self-MAC\n" );
			}

			memcpy( mymac_dev, (char *)ifr.ifr_hwaddr.sa_data, 6 );
			// setsockopt( fddev, SOL_SOCKET, SO_BINDTODEVICE, devname, strlen(devname) );
		}
	}

	do
	{
		uint8_t buf[2048 + 4] HIPALIGN16 = { 0 };

		int nr_fds = ( !!fddev ) + ( !!fdtap );

		struct pollfd fds[2] = { 0 };
		int i = 0;
		if ( fddev )
		{
			fds[i].fd = fddev;
			fds[i].events = POLLIN;
			i++;
		}
		if ( fdtap )
		{
			fds[i].fd = fdtap;
			fds[i].events = POLLIN;
			i++;
		}

		int r = poll( fds, nr_fds, 10 );
		if ( r < 0 ) FAIL( "Fail on poll" );

		for ( i = 0; i < nr_fds; i++ )
		{
			if ( fds[i].fd && ( fds[i].revents & POLLIN ) )
			{
				int offset = ( ( fds[i].fd == fddev ) ? 4 : 0 );
				// If not a taptun device, add 4 bytes to the offset
				int r = read( fds[i].fd, buf + offset, 2048 );
				if ( r < 0 )
					FAIL( "read() failed %s\n", strerror( errno ) );
				linux_got_packet( buf, r + offset );
			}
		}

		linux_tick_callback();

#if 0
		printf( "%4d: ", r );
		for( int i = 0; i < r; i++ )
		{
			if( (i & 0xf) == 0 && i ) printf( "\n      " );
			printf( "%02x ", buf[i] );
		}
		printf( "\n" );
#endif

	} while ( 1 );

	return 0;
}
