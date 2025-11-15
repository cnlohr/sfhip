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


#include "httptable.h"


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

	uint32_t seq_num = HIPHTONL( tcp->ackno );
	uint32_t ack_num = HIPHTONL( tcp->seqno );
	int local_port = tcp->destination_port;
	int remote_port = tcp->source_port;
	hipmac  remote_mac = mac->source;


	uint16_t flags = HIPNTOHS( tcp->flags );

	int also_fin = false;
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

	int reply_type_and_len = 0;



	if( flags & SFHIP_TCP_SOCKETS_FLAG_ACK )
	{
		if( seq_num > 2 && !(flags & SFHIP_TCP_SOCKETS_FLAG_FIN) )
		{
			int fileid = (1024-seq_num)&0x3ff;
			int bank = (seq_num>>10);

			int sent_first_time = 1022-fileid;
			int bsofar = sent_first_time + (bank)*1024;
			//printf( "Ack: %d FID: %d  Bank: %d  Send first: %d  bsofar: %d\n", seq_num, fileid, bank, sent_first_time, bsofar );
			if( fileid < nrFileOffsets ) 
			{
				uint32_t ofs = httpoffsets[fileid*2+0] + bsofar;
				uint32_t tlen = httpoffsets[fileid*2+1] - bsofar;

				int max_send = 1024;
				int dosend = tlen;
				if( tlen > max_send )
				{
					dosend = max_send;
				}
				else
				{
					also_fin = true;
				}
				//printf( "DOS: %d  %d  %d\n", dosend, tlen, tlen+bsofar );
				memcpy( ip_payload, httpoutputtable + ofs, dosend );
				reply_type_and_len = dosend;
			}
		}
	}


	if( flags & SFHIP_TCP_SOCKETS_FLAG_SYN )
	{
		reply_type_and_len = SFHIP_TCP_OUTPUT_SYNACK;
		seq_num = 1;
		ack_num++;
	}
	else if( flags & SFHIP_TCP_SOCKETS_FLAG_PSH )
	{
		ack_num += ip_payload_length;

		//printf( "FIRST 4:%c%c%c%c\n", ((uint8_t*)ip_payload)[0], ((uint8_t*)ip_payload)[1], ((uint8_t*)ip_payload)[2], ((uint8_t*)ip_payload)[3] );

		int emit = 0; // 0 = 404 not found.

		const uint8_t * cur = httpstatetable;
		int pldidx = 0;
		do
		{
			//printf( "CUR: %02x\n", cur-httpstatetable );
			uint32_t fail = ((hipunalignedu32*)cur)->v;
			//printf( "CFIRST (fail): %02x\n", fail);
			cur += 4;

			if( fail & 0x80000000 )
			{
				//printf( "EMIT: %08x\n", fail );
				emit = fail & 0x7fffffff;
				cur = httpstatetable + ((hipunalignedu32*)cur)->v;
				continue;
			}

			do
			{
				char c = ((uint8_t*)ip_payload)[pldidx++];
				char comp = *(cur++);
				//printf( "COMP: %d %02x\n", comp, cur - httpstatetable );
				if( !comp || comp == 1 )
				{
					if( comp == 0 ) pldidx--;
					else cur++; // because \x01 is followed by x00
					cur = httpstatetable + ((hipunalignedu32*)cur)->v;
					//printf( "! %d %d (%c %c) JUMP TO: %02x\n", c, comp, c, comp, cur -httpstatetable);
					break;
				}
				else if( c != comp )
				{
					//printf( "!= %d %d (%c %c) FAIL TO: %08x\n", c, comp, c, comp, fail );
					if( fail & 0x40000000 )
						fail &= 0x3fffffff;// Continue
					else
						pldidx--;
					cur = httpstatetable + fail;
					break;
				}
				else
				{
					//printf( "KEEP GOING: %d %d\n", c, comp );
					// Keep going
				}
			}while(pldidx < ip_payload_length );
		} while( pldidx < ip_payload_length );

		uint32_t ofs = httpoffsets[emit*2+0];
		uint32_t tlen = httpoffsets[emit*2+1];

		//printf( "EMIT: %d  %d %d\n", emit, ofs, tlen );
		int max_send = 1022 - emit;
		int dosend = tlen;
		if( tlen > max_send )
		{
			dosend = max_send;
		}
		else
		{
			also_fin = true;
		}
		//printf( "INIT SEND: %d\n", dosend );

		memcpy( ip_payload, httpoutputtable + ofs, dosend );
		reply_type_and_len = dosend;
	}
	else if ( flags & SFHIP_TCP_SOCKETS_FLAG_FIN  )
	{
		// Override ack.
		reply_type_and_len = SFHIP_TCP_OUTPUT_ACK;
		ack_num++;
	}





	sfhip_make_ip_packet( hip, pkt, remote_mac, sender );

	ip->destination_address = sender;

	int optionadd = 0;
	int rflags = 0;
	int seqsub = 0;

	switch ( reply_type_and_len )
	{
		case 0:
			return 0;
		default:
			if ( reply_type_and_len > 0 )
			{
				rflags = SFHIP_TCP_SOCKETS_FLAG_PSH;
				//sock->pending_send_size = payload_length;
				break;
			}
		case SFHIP_TCP_OUTPUT_ACK:
			reply_type_and_len = 0;
			break;
		case SFHIP_TCP_OUTPUT_RESET:
			rflags = SFHIP_TCP_SOCKETS_FLAG_RESET;
			//sock->remote_address = 0;
			//sock->seq_num = HIPHTONL( tcp->ackno );
			reply_type_and_len = 0;
			break;
		case SFHIP_TCP_OUTPUT_SYNACK:
			rflags = SFHIP_TCP_SOCKETS_FLAG_SYN;
			//sock->pending_send_size = 1;
			reply_type_and_len = 0;
			break;
		case SFHIP_TCP_OUTPUT_FIN:
			rflags = SFHIP_TCP_SOCKETS_FLAG_FIN;
			//sock->mode = SFHIP_TCP_MODE_CLOSING_WAIT;
			//sock->pending_send_size = 1;
			reply_type_and_len = 0;
			break;
		case SFHIP_TCP_OUTPUT_KEEPALIVE:
			rflags = SFHIP_TCP_SOCKETS_FLAG_PSH;
			reply_type_and_len = 0;
			seqsub = 1; // one less sequence numbers is how TCP handles keepalive.
			break;
	}

	if( also_fin )
		rflags |= SFHIP_TCP_SOCKETS_FLAG_FIN;

	rflags |= SFHIP_TCP_SOCKETS_FLAG_ACK;

	// uip does this... not sure why.
	if ( rflags & SFHIP_TCP_SOCKETS_FLAG_SYN )
	{
		reply_type_and_len = 4;
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

	ip->length = HIPHTONS( sizeof( sfhip_ip_header ) + sizeof( sfhip_tcp_header ) + reply_type_and_len );

	// Build and compute checksum on TCP pseudo-header in-place.
	uint16_t * csumstart = ( (void *)tcp ) - 12;
	csumstart[0] = SFHIP_IPPROTO_TCP << 8;
	csumstart[1] = HIPHTONS( sizeof( sfhip_tcp_header ) + reply_type_and_len );

		#if SFHIP_EMIT_TCP_CHECKSUM
	uint16_t csum = sfhip_internet_checksum( (uint16_t *)csumstart, reply_type_and_len + sizeof( sfhip_tcp_header ) + 12 );
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

	int packlen = reply_type_and_len + HIP_PHY_HEADER_LENGTH_BYTES +
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
