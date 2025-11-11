#include <stdio.h>

#define HIP_PHY_HEADER_LENGTH_BYTES 4

#define SFHIP_WARN( x... ) fprintf( stderr, x );

#define SFHIP_IMPLEMENTATION

#define SFHIP_UDP_USER_HANDLER example_udp_user_handler

#include "sfhip.h"

#include <stdint.h>

#define TAP_ADDR "192.168.14.252"
sfhip hip = {
    .ip = HIPIP( 192, 168, 14, 251 ),
    .mask = HIPIP( 255, 255, 255, 0 ),
    .gateway = HIPIP( 192, 168, 14, 1 ),
    .self_mac = { 0xf0, 0x11, 0x22, 0x33, 0x44, 0x55 },
    .hostname = "sfhip_test_linux",
};

uint64_t last_time = 0;
struct timespec monotime;
int64_t runtime = 0;

int linux_send_packet( uint8_t * data, int length );
void linux_got_packet( uint8_t * buf, int length );
int linuxtest( const char * devname_tap, const char * devname_eth );

typedef enum
{
	HTP_START,
	HTP_URL,
	HTP_POSTURL,
	HTP_BACKNEXP,
	HTP_LINEFIRST,
	HTP_LINE,
	HTP_BACKNEXPEND,
	HTP_REQUEST_COMPLETE,
	HTP_HEADER_SENDING,
	HTP_BODY_SENDING,
	HTP_ERROR,
	HTP_TESTING_END,
} httpparsestate;

typedef struct
{
	httpparsestate state : 8;
	int sendno;
} http;

http https[SFHIP_TCP_SOCKETS];

void sfhip_got_dhcp_lease( sfhip * hip, sfhip_address addr )
{
	printf( "DHCP IP: " HIPIPSTR "\n", HIPIPV( addr ) );
}

int sfhip_tcp_accept_connection( sfhip * hip, int sockno, int localport, hipbe32 remote_host )
{
	// return 0 to accept, -1 to abort.
	if ( localport == 80 )
	{
		http * h = https + sockno;
		h->state = HTP_START;
		h->sendno = 0;
		return 1;
	}
	else
	{
		printf( "Invalid port (%d)\n", localport );
		return 0;
	}
}

sfhip_length_or_tcp_code sfhip_tcp_event( sfhip * hip, int sockno, uint8_t * ip_payload, int ip_payload_length, int max_ip_payload, int acked )
{
	http * h = https + sockno;

	uint8_t c;
	uint8_t * p = ip_payload;
	uint8_t * end = p + ip_payload_length;

	httpparsestate s = h->state;
	while ( p != end )
	{
		// TODO: Would be fun to make this a tiny table.
		c = *( p++ );
		switch ( s )
		{
			case HTP_START:
				if ( c == ' ' ) s = HTP_URL;
				break;
			case HTP_URL:
				if ( c == ' ' )
					s = HTP_POSTURL;
				else if ( c == '\r' )
					s = HTP_BACKNEXP;
				else if ( c == '\n' )
					s = HTP_LINEFIRST;
				else
					printf( "%c", c );
				break;
			case HTP_BACKNEXP:
				if ( c == '\n' )
					s = HTP_LINEFIRST;
				else
					s = HTP_ERROR;
				break;
			case HTP_LINEFIRST:
				if ( c == '\r' )
					s = HTP_BACKNEXPEND;
				else if ( c == '\n' )
					s = HTP_REQUEST_COMPLETE;
				else
					s = HTP_LINE;
				break;
			case HTP_POSTURL:
				if ( c == '\r' )
					s = HTP_BACKNEXP;
				else if ( c == '\n' )
					s = HTP_LINEFIRST;
				break;
			case HTP_LINE:
				if ( c == '\r' )
					s = HTP_BACKNEXP;
				else if ( c == '\n' )
					s = HTP_LINEFIRST;
				break;
			case HTP_BACKNEXPEND:
				if ( c == '\n' )
					s = HTP_REQUEST_COMPLETE;
				else
					s = HTP_ERROR;
				break;
			default:
				break;
		}
	}

	int ret = 0;

	// If we can send data back.
	if ( acked )
	{
		if ( s == HTP_HEADER_SENDING )
		{
			s = HTP_BODY_SENDING;
		}
		else if ( s == HTP_BODY_SENDING )
		{
			int sno = h->sendno++;
			if ( sno == 10 )
			{
				s = HTP_TESTING_END;
			}
		}
	}

	if ( s == HTP_REQUEST_COMPLETE && max_ip_payload )
	{
		if ( max_ip_payload )
		{
			// Can send, advance.
			s = HTP_HEADER_SENDING;
		}
		printf( "\nReceived Request\n" );
	}

	h->state = s;

	// If we can send our message, send it.
	if ( !max_ip_payload ) return 0;

	char pl = ( h->sendno % 10 ) + '0';

	// Phase two - send a TCP reply.
	switch ( s )
	{
		case HTP_HEADER_SENDING:
			int r = sprintf( ip_payload, "HTTP/1.0 200 OK\r\nContent-type: text/html\r\n\r\n" );
			return r;
		case HTP_BODY_SENDING:
			memset( ip_payload, pl, 1 ); // 1460 );
			return 1;
		case HTP_TESTING_END:
			return SFHIP_TCP_OUTPUT_FIN;
		default:
			return 0;
	}
	return 0;
}

void sfhip_tcp_socket_closed( sfhip * hip, int sockno )
{
	printf( "Socket Closed\n" );
}

void linux_got_packet( uint8_t * buf, int length )
{
	if ( ( rand() % 10 ) == 0 ) return;

	sfhip_accept_packet( &hip, (sfhip_phy_packet_mtu *)buf, length );
}

int sfhip_send_packet( sfhip * hip, sfhip_phy_packet * data, int length )
{
	if ( ( rand() % 10 ) == 0 ) return 0;

	return linux_send_packet( (uint8_t *)data, length );
}

void linux_tick_callback()
{
	clock_gettime( CLOCK_MONOTONIC_RAW, &monotime );
	uint64_t ms = ( monotime.tv_nsec / 1000000ULL ) + ( monotime.tv_sec * 1000ULL );
	int delta_ms = ms - last_time;

	sfhip_phy_packet_mtu scratch;
	sfhip_tick( &hip, &scratch, delta_ms );

	if ( runtime )
	{
		runtime -= delta_ms;
		if ( runtime <= 0 ) exit( 0 );
	}

	last_time = ms;
}

int example_udp_user_handler( sfhip * hip, sfhip_phy_packet_mtu * pkt, uint8_t * payload, int ulen, int source_port, int destination_port )
{
	if ( destination_port == 9999 )
	{
		printf( "Got 9999 packet\n" );

		sfhip_mac_header * mac = &pkt->mac_header;
		sfhip_ip_header * ip = (sfhip_ip_header *)( mac + 1 );

		int plen = 2;
		payload[0] = 'X';
		payload[1] = '\n';

		sfhip_make_udp_packet( hip, pkt, mac->source, ip->source_address, destination_port, source_port );
		sfhip_send_udp_packet( hip, pkt, plen );
		return 1;
	}

	return 0;
}

int main( int argc, char ** argv )
{
	printf( "Main started\n" );
	printf( "Link Force Symbol: %p\n", &sfhip_accept_packet );
	printf( "Link Force Symbol: %p\n", &sfhip_tick );
	printf( "Link Force Symbol: %p\n", &sfhip_send_packet );

	if ( argc < 3 )
		goto failhelp;

	if ( argc > 3 )
	{
		runtime = atoi( argv[3] ) * 1000ULL;
		printf( "Timing out after %ld ms\n", runtime );
	}

	clock_gettime( CLOCK_MONOTONIC_RAW, &monotime );
	uint64_t ms = ( monotime.tv_nsec / 1000000ULL ) + ( monotime.tv_sec * 1000ULL );
	last_time = ms;

	linuxtest( argv[1], argv[2] );

	return 0;

failhelp:
	SFHIP_WARN( "Usage: [tool] [tunX|tapX|-] [ethernet_dev|-]\n" );
	return -1;
}
