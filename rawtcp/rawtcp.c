#include <stdio.h>

#define HIP_PHY_HEADER_LENGTH_BYTES 4

#define SFHIP_WARN( x... ) fprintf( stderr, x );

#define SFHIP_IMPLEMENTATION

#define SFHIP_UDP_USER_HANDLER example_udp_user_handler

#define SFHIP_TCP_SOCKETS 0

#define SFHIP_DHCP_CLIENT 0

#define SFHIP_TCP_OVERRIDE_HANDLER tcp_override

#include "sfhip.h"

#include <stdint.h>


#define TAP_ADDR "192.168.13.252"
sfhip hip = {
    .ip = HIPIP( 192, 168, 13, 251 ),
    .mask = HIPIP( 255, 255, 255, 0 ),
    .gateway = HIPIP( 192, 168, 13, 1 ),
    .self_mac = { 0xf0, 0x11, 0x22, 0x33, 0x44, 0x55 },
//    .hostname = "sfhip_test_linux",
};

uint64_t last_time = 0;
struct timespec monotime;
int64_t runtime = 0;

int linux_send_packet( uint8_t * data, int length );
void linux_got_packet( uint8_t * buf, int length );
int linuxtest( const char * devname_tap, const char * devname_eth );

#include "../linuxtest.c"

void linux_got_packet( uint8_t * buf, int length )
{
	//	if ( ( rand() % 10 ) == 0 ) return;

	sfhip_accept_packet( &hip, (sfhip_phy_packet_mtu *)buf, length );
}

int sfhip_send_packet( sfhip * hip, sfhip_phy_packet * data, int length )
{
	//	if ( ( rand() % 10 ) == 0 ) return 0;

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

		return sfhip_send_udp_packet( hip, pkt, mac->source, ip->source_address,
		                              destination_port, source_port, plen );
	}

	return 0;
}

int tcp_override( sfhip * hip,
                      sfhip_phy_packet_mtu * pkt,
                      void * ip_payload,
                      int ip_payload_length )
{
	sfhip_mac_header * mac = &pkt->mac_header;
	sfhip_ip_header * ip = (sfhip_ip_header *)( mac + 1 );
	sfhip_tcp_header * tcp = (sfhip_tcp_header *)( ip + 1 );

	uint32_t seq_num = tcp->ackno;
	uint32_t ack_num = tcp->seqno;
	int local_port = tcp->destination_port;
	int remote_port = tcp->source_port;
	hipmac  remote_mac = mac->source;


	uint16_t flags = HIPNTOHS( tcp->flags );

	int hlen = ( flags >> 12 ) << 2;

	// TCP packet size does not match, or runt packet.
	if ( ip_payload_length < 0 )
		return -1;

		#if SFHIP_CHECK_TCP_CHECKSUM || SFHIP_CHECK_UDP_CHECKSUM
	sfhip_address sender = ( (sfhip_address *)ip_payload )[-2];
		#else
	sfhip_address sender = ( (sfhip_ip_header *)( data->payload ) )->source_address;
		#endif

		#if SFHIP_CHECK_TCP_CHECKSUM
	// Build pseudo-header for checksum.
	uint16_t ccsum =
	    sfhip_internet_checksum( ip_payload - 12, ip_payload_length + 12 );

	if ( ccsum )
		return -1;
		#endif

	ip_payload_length -= hlen;
	ip_payload += hlen;

	if( flags & SFHIP_TCP_SOCKETS_FLAG_SYN )
	{
		printf( "Flags: %d\n", flags );
	}
	else
	{
		ip_payload_length = 0;
	}

	sfhip_make_ip_packet( hip, pkt, remote_mac, sender );

	ip->destination_address = sender;

	int optionadd = 0;
	int rflags = 0;
	int seqsub = 0;

	switch ( ip_payload_length )
	{
		case 0:
			return 0;
		default:
			if ( ip_payload_length > 0 )
			{
				rflags = SFHIP_TCP_SOCKETS_FLAG_PSH;
				//sock->pending_send_size = payload_length;
				break;
			}
		case SFHIP_TCP_OUTPUT_ACK:
			ip_payload_length = 0;
			break;
		case SFHIP_TCP_OUTPUT_RESET:
			rflags = SFHIP_TCP_SOCKETS_FLAG_RESET;
			//sock->remote_address = 0;
			//sock->seq_num = HIPHTONL( tcp->ackno );
			ip_payload_length = 0;
			break;
		case SFHIP_TCP_OUTPUT_SYNACK:
			rflags = SFHIP_TCP_SOCKETS_FLAG_SYN;
			//sock->pending_send_size = 1;
			ip_payload_length = 0;
			break;
		case SFHIP_TCP_OUTPUT_FIN:
			rflags = SFHIP_TCP_SOCKETS_FLAG_FIN;
			//sock->mode = SFHIP_TCP_MODE_CLOSING_WAIT;
			//sock->pending_send_size = 1;
			ip_payload_length = 0;
			break;
		case SFHIP_TCP_OUTPUT_KEEPALIVE:
			rflags = SFHIP_TCP_SOCKETS_FLAG_PSH;
			ip_payload_length = 0;
			seqsub = 1; // one less sequence numbers is how TCP handles keepalive.
			break;
	}

	rflags |= SFHIP_TCP_SOCKETS_FLAG_ACK;

	// uip does this... not sure why.
	if ( rflags & SFHIP_TCP_SOCKETS_FLAG_SYN )
	{
		ip_payload_length = 4;
		optionadd = 4;
		( (hipbe32 *)( tcp + 1 ) )[0] = HIPHTONL(
		    0x02040000 |
		    ( SFHIP_MTU - sizeof( sfhip_tcp_header ) - sizeof( sfhip_ip_header ) -
		      sizeof( sfhip_phy_packet ) - 18 /* to just make it a smoler */ ) );
	}

	tcp->source_port = local_port;
	tcp->destination_port = remote_port;
	tcp->seqno = HIPHTONL( seq_num - seqsub );
	tcp->ackno = HIPHTONL( ack_num );
	tcp->window = HIPHTONS( SFHIP_MTU - sizeof( sfhip_tcp_header ) -
	                        sizeof( sfhip_ip_header ) - sizeof( sfhip_phy_packet ) );
	tcp->checksum = 0;
	tcp->urgent = 0;

	tcp->flags = HIPHTONS( ( (uint8_t)rflags ) | ( ( ( sizeof( sfhip_tcp_header ) + optionadd ) >> 2 ) << 12 ) );

	ip->length = HIPHTONS( sizeof( sfhip_ip_header ) + sizeof( sfhip_tcp_header ) + ip_payload_length );

	// Build and compute checksum on TCP pseudo-header in-place.
	uint16_t * csumstart = ( (void *)tcp ) - 12;
	csumstart[0] = SFHIP_IPPROTO_TCP << 8;
	csumstart[1] = HIPHTONS( sizeof( sfhip_tcp_header ) + ip_payload_length );

		#if SFHIP_EMIT_TCP_CHECKSUM
	uint16_t csum = sfhip_internet_checksum( (uint16_t *)csumstart, ip_payload_length + sizeof( sfhip_tcp_header ) + 12 );
	// No 0x0000 option for payload (maybe) TODO checkme.
	// if( udpcsum == 0x0000 ) udpcsum = 0xffff;
	tcp->checksum = csum;
		#endif

	// Fixup overwritten pseudo header. Note these fields are never
	// initialized, so we have to initialize them here!!
	ip->ttl = 64;
	ip->protocol = SFHIP_IPPROTO_TCP;
	ip->header_checksum = 0;

	uint16_t hs =
	    sfhip_internet_checksum( (uint16_t *)ip, sizeof( sfhip_ip_header ) );
	ip->header_checksum = hs;

	int packlen = ip_payload_length + HIP_PHY_HEADER_LENGTH_BYTES +
	              sizeof( sfhip_mac_header ) + sizeof( sfhip_ip_header ) +
	              sizeof( sfhip_tcp_header );
	return sfhip_send_packet( hip, (sfhip_phy_packet *)pkt, packlen );
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
