#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

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
};

typedef struct tabletoken_t tabletoken;


tabletoken continuematch[nrFiles*3+3];

void CommonRange( int start, int filestart, int fileend )
{
	int i;
	int cno = start;
	for( ; ; cno++ )
	{
		int matchingchar = -1;
		for( i = filestart; i < fileend; i++ )
		{
			char c = fileList[i][cno];
			if( matchingchar == -1 )
				matchingchar = c;
			else if( matchingchar != c )
			{
				CommonRange
				// pull off top and bottom from here.
				return;
			}
		}
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


	tabletoken start = { };
	tabletoken geturl = { };
	tabletoken do404 = { }; // TODO.


	start.matching_string = " ";
	start.pass = &start;
	start.fail = &geturl;

	geturl.matching_string = " /";
	geturl.pass = continuematch;
	geturl.fail = &do404;

	CommonRange( 0, 0, nrFiles );



	// Table definition:
	// [abort jump]
	// matching chars
	// [all matched jump]

	// jumps are 




	// Table Language:
	// output / flag
	// match char / jump to
	// match char / jump to
	// null  / jump to (exeption)



	return 0;
}


