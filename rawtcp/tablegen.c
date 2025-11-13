#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdint.h>

#define MAX_SIZE (1024*1024)
#define MAX_TOKENS 1024

static uint8_t data_image[MAX_SIZE];
int endpointer = 0;

char ** fileList;
int nrFiles;

void enumerate( const char * dirName )
{
	DIR* dir = opendir( dirName );
	struct dirent *pent = NULL;
	while (pent = readdir (dir)) 
	{
		if ( pent->d_name[0] == '.' )
			continue;

		char * fileName = (char*)malloc( strlen(dirName) + strlen(pent->d_name) + 2 );
		strcpy( fileName, dirName );
		strcat( fileName, "/" );
		strcat( fileName, pent->d_name );

		struct stat st;
		stat(fileName, &st);

		if( S_ISDIR( st.st_mode ) )
		{
			enumerate( fileName );
		}
		else
		{
			fileList = realloc( fileList, (nrFiles + 1) * sizeof( char * ) );
			fileList[nrFiles++] = fileName;
		}
	}
}

struct tabletoken_t
{
	struct tabletoken_t * fail;
	struct tabletoken_t * pass;
	int    emit_token;
	char * matching_string;

	int    stream_location;
};

typedef struct tabletoken_t tabletoken;

tabletoken * emittableTokens[MAX_TOKENS];
int numEmittableTokens;

tabletoken * GenToken()
{
	tabletoken * ret = calloc( sizeof( tabletoken ), 1 );
	emittableTokens[numEmittableTokens++] = ret;
	return ret;
}

tabletoken * lineend;
tabletoken * continuematch;

tabletoken * CommonRange( int start, int filestart, int fileend )
{
	fprintf( stderr, "Common Rage: %d %d %d\n", start, filestart, fileend );

	int i;
	int cno = start;
	for( ; ; cno++ )
	{
		int matchingchar = -1;
		for( i = filestart; i < fileend; i++ )
		{
			char c = fileList[i][cno];
			if( c == 0 && fileend - filestart == 1 )
			{
				tabletoken * fail = GenToken();
				fail->emit_token = -1;
				fail->pass = lineend;

				tabletoken * pass = GenToken();
				pass->emit_token = filestart + 1;
				pass->pass = lineend;

				tabletoken * ret = GenToken();
				ret->emit_token = 0;
				ret->pass = pass;
				ret->fail = fail;
				ret->matching_string = malloc( cno - start + 2 );
				memcpy( ret->matching_string, fileList[filestart] + start, cno-start );
				ret->matching_string[cno-start] = ' ';
				ret->matching_string[cno-start+1] = 0;
				fprintf( stderr, "FinalStr: \"%s\"\n", ret->matching_string );
				return ret;

			}

			if( matchingchar == -1 )
				matchingchar = c;
			else if( matchingchar != c )
			{
				fprintf( stderr, "Disagree %c %c (%d)\n", matchingchar, c, i );
				tabletoken * pass = CommonRange( cno, filestart, i ); 
				tabletoken * fail = CommonRange( cno - 1, i, fileend );

				tabletoken * ret = GenToken();
				ret->emit_token = 0;
				ret->pass = pass;
				ret->fail = fail;
				ret->matching_string = malloc( cno - start + 1 );
				memcpy( ret->matching_string, fileList[filestart] + start, cno-start );
				ret->matching_string[cno-start] = 0;
				return ret;
			}
		}
	}
}

int AppendData( uint8_t * data, int len )
{
	fprintf( stderr, "Appending: %d, %d\n", endpointer,len );
	memcpy( data_image + endpointer, data, len );
	endpointer += len;
	return endpointer;
}


void PrintTree( tabletoken * search, int depth )
{
	if( search->emit_token )
		fprintf( stderr, "%*c%d\n", depth+1, ' ', search->emit_token );
	else
	{
		fprintf( stderr, "%*c%s\n", depth+1, ' ', search->matching_string );
		PrintTree( search->pass, depth + 1 );
		PrintTree( search->fail, depth + 1 );
	}
}

int main()
{
	printf( "#include <stdint.h>\n" );

	enumerate( "web" );
	fprintf( stderr, "%d\n", nrFiles );

	qsort( fileList, nrFiles, sizeof(char*), (__compar_fn_t)strcmp );

	int i;
	int maxLength = 0;
	for( i = 0; i < nrFiles; i++ )
	{
		fprintf( stderr, "%s\n", fileList[i] );
		int len = strlen( fileList[i] );
		if( len > maxLength ) maxLength = len;
	}

	continuematch = calloc( sizeof( tabletoken ), nrFiles*3+3 );

	tabletoken * start = GenToken();
	tabletoken * geturl = GenToken();
	tabletoken * do404 = GenToken(); // todo

	start->matching_string = " ";
	start->pass = start;
	start->fail = geturl;

	geturl->matching_string = " /";
	geturl->pass = continuematch;
	geturl->fail = do404;

	do404->emit_token = -1;

	tabletoken * search = CommonRange( 0, 0, nrFiles );

//	tabletoken lineend_final = { .place_in_stream = -1; }

	// Could do more header parsing here.
	lineend = GenToken();
	tabletoken * lineend_final = GenToken();
	lineend->matching_string = "\r";
	lineend->pass = lineend_final;
	lineend->fail = lineend;
	lineend_final->matching_string = "\n";
	lineend_final->pass = lineend;
	lineend_final->fail = lineend_final;

	PrintTree( search, 0 );

	for( int i = 0; i < numEmittableTokens; i++ )
	{
		tabletoken * e = emittableTokens[i];
		e->stream_location = endpointer;
		if( !e->emit_token )
		{
			uint32_t holder = 0xffffffff;
			AppendData( (uint8_t*)&holder, 4 );
			AppendData( (uint8_t*)&holder, 4 ); // pass
			fprintf( stderr, "%d\n", e->matching_string );
			fprintf( stderr, ":%s:\n",  e->matching_string );
			AppendData( e->matching_string, strlen( e->matching_string ) );
			AppendData( (uint8_t*)&holder, 4 );
		}
		else
		{
			if( e->emit_token < 0 )
			{
				// 404.
				char header[960];
				int nhdr;
				nhdr = snprintf( header, sizeof(header), "HTTP/1.1 404 Not Found\r\n\r\n" );
				AppendData( (uint8_t*)&nhdr, 4 );
				AppendData( header, nhdr );
			}
			else
			{
				const char * fname = fileList[e->emit_token-1];
				struct stat st;
				stat(fname, &st);

				int len = st.st_size;

				char header[960];

				char * ext = strrchr( fname, '.' );
				int nhdr = 0;
				if( !ext )
					nhdr = snprintf( header, sizeof(header), "HTTP/1.1 200 Ok\r\nContent-Type: application/octet-stream\r\n\r\n" );
				else if( strcmp( ext, ".png" ) == 0 )
					nhdr = snprintf( header, sizeof(header), "HTTP/1.1 200 Ok\r\nContent-Type: image/png\r\n\r\n" );
				else if( strcmp( ext, ".gif" ) == 0 )
					nhdr = snprintf( header, sizeof(header), "HTTP/1.1 200 Ok\r\nContent-Type: image/gif\r\n\r\n" );
				else if( strcmp( ext, ".txt" ) == 0 )
					nhdr = snprintf( header, sizeof(header), "HTTP/1.1 200 Ok\r\nContent-Type: text/plain\r\n\r\n" );
				else if( strcmp( ext, ".htm" ) == 0 || strcmp( ext, ".html" ) == 0 )
					nhdr = snprintf( header, sizeof(header), "HTTP/1.1 200 Ok\r\nContent-Type: text/html\r\n\r\n" );
				else
					nhdr = snprintf( header, sizeof(header), "HTTP/1.1 200 Ok\r\nContent-Type: application/octet-stream\r\n\r\n" );

				int lenplus = (len + nhdr) | 0x80000000;
				AppendData( e->pass->stream_location ); // TODO: do we know all continuations here?
				AppendData( (uint8_t*)&lenplus, 4 );
				AppendData( header, nhdr );

				FILE * f = fopen( fname, "rb" );
				int r = fread( data_image + endpointer, len, 1, f );
				fclose( f );
				if( r != 1 )
				{
					fprintf( stderr, "error: could not open %s\n", fname );
				}
				endpointer += len;

				// TODO: support multi-outputs
				// TODO: support gzip.

			}
		}
	}

	for( int i = 0; i < numEmittableTokens; i++ )
	{
		tabletoken * e = emittableTokens[i];
		if( !e->emit_token )
		{
			uint32_t loca = e->stream_location;

			*(uint32_t*)((data_image) + loca) = e->fail->stream_location;
			*(uint32_t*)((data_image) + loca + 4 + strlen( e->matching_string )) = e->pass->stream_location;
		}
	}

	printf( "#ifndef _HTTP_TABLE_H\n" );
	printf( "#define _HTTP_TABLE_H\n\n" );
	printf( "#include <stdint.h>\n\n" );
	printf( "uint8_t httptable[] = { ");
	for( int i = 0; i < endpointer; i++ )
	{
		if( ( i & 15 ) == 0 )
		{
			printf( "\n\t" );
		}

		printf( "0x%02x, ", data_image[i] );
	}
	printf( "};\n\n#endif\n" );
	return 0;
}


