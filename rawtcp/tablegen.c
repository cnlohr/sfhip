#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdint.h>

#define MAX_SIZE (1024*1024*128)
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
	int failcont;
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
	ret->emit_token = -1;
	ret->matching_string = "*";
	emittableTokens[numEmittableTokens++] = ret;
	return ret;
}

tabletoken * do404;
tabletoken * lineend;
tabletoken * continuematch;
tabletoken * terminal;

tabletoken * CommonRange( int start, int filestart, int fileend, tabletoken * failif )
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
				fprintf( stderr, "Inner fail %d %d %d\n", c, fileend, filestart );
				tabletoken * pass = GenToken();
				pass->emit_token = filestart + 1; // file ID
				pass->pass = lineend;
				pass->matching_string = "T";

				tabletoken * ret = GenToken();
				ret->pass = pass;
				ret->fail = failif;
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
				fprintf( stderr, "Disagree %c %c (%d)  Splitting [%d %d %d]\n", matchingchar, c, i, filestart, i, fileend );
				tabletoken * fail = CommonRange( cno, i, fileend, terminal );
				tabletoken * pass = CommonRange( cno, filestart, i, fail ); 

				tabletoken * ret = GenToken();
				ret->pass = pass;
				ret->fail = failif;
				ret->matching_string = malloc( cno - start + 1 );
				memcpy( ret->matching_string, fileList[filestart] + start, cno-start );
				fprintf( stderr, "MATCHING_STR in non-match:\"%s\"\n", ret->matching_string );
				ret->matching_string[cno-start] = 0;
				return ret;
			}
		}
	}
}

int AppendData( uint8_t * data, int len )
{
	memcpy( data_image + endpointer, data, len );
	endpointer += len;
	return endpointer;
}


void PrintTree( tabletoken * search, int depth )
{
	if( search == terminal )
	{
		fprintf( stderr, "%*cTERMINAL\n", depth+1, ' ' );
		return;
	}
	if( search->emit_token >= 0 )
		fprintf( stderr, "%*c%d (%02x) (%02x %02x)\n", depth+1, ' ', search->emit_token, search->stream_location, search->pass?search->pass->stream_location:0, search->fail?search->fail->stream_location:0 );
	else
	{
		fprintf( stderr, "%*c%s (%02x) (%02x %02x)\n", depth+1, ' ', search->matching_string, search->stream_location, search->pass?search->pass->stream_location:0, search->fail?search->fail->stream_location:0 );
		PrintTree( search->pass, depth + 1 );
		PrintTree( search->fail, depth + 1 );
	}
}

int main()
{
	int i;
	printf( "#include <stdint.h>\n" );

	const char * rootWeb = "web";

	enumerate( rootWeb );
	fprintf( stderr, "%d\n", nrFiles );

	qsort( fileList, nrFiles, sizeof(char*), (__compar_fn_t)strcmp );

	const char * fileListOrig[nrFiles];

	for( i = 0; i < nrFiles; i++ )
	{
		fileListOrig[i] = fileList[i];
		fileList[i] += strlen(rootWeb); // Pull off "web"
	}

	int maxLength = 0;

	for( i = 0; i < nrFiles; i++ )
	{
		fprintf( stderr, "%s\n", fileList[i] );
		int len = strlen( fileList[i] );
		if( len > maxLength ) maxLength = len;
	}

	tabletoken * start = GenToken();
	do404 = GenToken(); // todo

	lineend = GenToken();
	tabletoken * lineend_final = GenToken();
	terminal = GenToken();

	tabletoken * search = CommonRange( 0, 0, nrFiles, do404 );

	start->matching_string = "GET ";
	start->pass = search;
	start->fail = do404;

//	tabletoken lineend_final = { .place_in_stream = -1; }

	// Could do more header parsing here.


	lineend->matching_string = "\r";
	lineend->pass = lineend_final;
	lineend->fail = lineend;
	lineend->failcont = 1;
	lineend_final->matching_string = "\n";
	lineend_final->pass = lineend;
	lineend_final->fail = lineend_final;
	lineend_final->failcont = 1;

	do404->emit_token = 0;
	do404->pass = terminal;
	do404->matching_string = "";


	terminal->matching_string = "\x01"; // Tricky: code 1 = consume, always.
	terminal->fail = terminal;
	terminal->failcont = 1;
	terminal->pass = terminal;

	for( int i = 0; i < numEmittableTokens; i++ )
	{
		tabletoken * e = emittableTokens[i];
		e->stream_location = endpointer;
		if( e->emit_token < 0 )
		{
			uint32_t holder = 0xffffffff;
			AppendData( (uint8_t*)&holder, 4 ); // fail
			AppendData( e->matching_string, strlen( e->matching_string )+1 ); // Need null
			AppendData( (uint8_t*)&holder, 4 ); // pass
		}
		else
		{
			uint32_t holder = 0xffffffff;
			AppendData( (uint8_t*)&holder, 4 ); // value
			AppendData( (uint8_t*)&holder, 4 ); // pass
		}
	}

	for( int i = 0; i < numEmittableTokens; i++ )
	{
		tabletoken * e = emittableTokens[i];
		uint32_t ep = e->stream_location;

		uint8_t * ee = data_image + ep;

			fprintf( stderr, "%d %d %p [%p %p %s]\n", i, e->emit_token, e->matching_string, e->pass, e, e->matching_string );

		if( e->emit_token < 0 )
		{
			uint32_t pass = e->pass->stream_location;
			uint32_t fail = e->fail->stream_location;
			(*((uint32_t*)ee)) = fail | (e->failcont<<30);
			AppendData( e->matching_string, strlen( e->matching_string )+1 ); // Need null
			(*((uint32_t*) (ee+4+strlen( e->matching_string )+1))) = pass;
		}
		else
		{
			uint32_t value = ((uint32_t)e->emit_token) | ((uint32_t)0x80000000);
			uint32_t pass = e->pass->stream_location;
			(*((uint32_t*)ee)) = value;
			(*((uint32_t*)(ee+4))) = pass;
		}
	}

	fprintf( stderr, "Important Elements:\n" );
	fprintf( stderr, "do404 = %02x\n" , do404->stream_location );
	fprintf( stderr, "lineend = %02x\n" , lineend->stream_location );
	fprintf( stderr, "start = %02x\n" , start->stream_location );
	fprintf( stderr, "terminal = %02x\n" , terminal->stream_location );

	PrintTree( search, 0 );


	printf( "#ifndef _HTTP_TABLE_H\n" );
	printf( "#define _HTTP_TABLE_H\n\n" );
	printf( "#include <stdint.h>\n\n" );
	printf( "const uint8_t httpstatetable[%d] = { ", endpointer);
	for( int i = 0; i < endpointer; i++ )
	{
		if( ( i & 15 ) == 0 )
		{
			printf( "\n\t" );
		}

		printf( "0x%02x, ", data_image[i] );
	}
	printf( "};\n\n" );

	int outFilePointers[nrFiles + 1];
	int outFileLens[nrFiles + 1];
	endpointer = 0;

	for( int i = 0; i < nrFiles + 1; i++ )
	{
		outFilePointers[i] = endpointer;
		if( i == 0 )
		{
			// 404.
			char header[960];
			int nhdr;
			nhdr = snprintf( header, sizeof(header), "HTTP/1.1 404 Not Found\r\n\r\n404 Not Found" );
			AppendData( header, nhdr );
		}
		else
		{
			const char * fname = fileListOrig[i-1];
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

			memcpy( data_image + endpointer, header, nhdr );
			endpointer += nhdr;
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

		int flen = outFileLens[i] = endpointer - outFilePointers[i];
	}

	
	printf( "#define nrFileOffsets %d\n", nrFiles + 1 );

	printf( "const uint32_t httpoffsets[nrFileOffsets*2] = {\n");
	for( int i = 0; i < nrFiles + 1; i++ )
	{
		printf( "\t%d, %d,\n", outFilePointers[i], outFileLens[i] );
	}
	printf( "};\n\n" );
	printf( "const uint8_t httpoutputtable[%d] = {",endpointer);
	for( int i = 0; i < endpointer; i++ )
	{
		if( ( i & 15 ) == 0 )
		{
			printf( "\n\t" );
		}

		printf( "0x%02x, ", data_image[i] );
	}
	printf( "};\n\n" );
	printf( "\n#endif\n" );
	return 0;
}


