//----------------------------------------------------------------------------
//
// File:        mkspch.cpp
// Date:        06-May-2001
// Programmer:  Marc Rousseau
//
// Description:
//
// Copyright (c) 2001-2004 Marc Rousseau, All Rights Reserved.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307, USA.
//
// Revision History:
//
//----------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "common.hpp"
#include "logger.hpp"
#include "option.hpp"

DBG_REGISTER( __FILE__ );

constexpr int ROM_SIZE      = 0x8000;
constexpr int MIN_NODE_SIZE = 10;		// [ phrase length (1), previous (2), next (2), ? (1), offset (2), data length (2) ] + [ data (0) ]

struct sNode
{
	sNode       *prev;
	sNode       *next;
	size_t       phrase_offset;
	std::string  phrase;
	size_t       data_offset;
	size_t       data_length;
	const UINT8 *data;
	bool copy;

	sNode( ) :
		prev( nullptr ),
		next( nullptr ),
		phrase_offset( 0 ),
		phrase( ),
		data_offset( 0 ),
		data_length( 0 ),
		data( nullptr ),
		copy( false )
	{
	}
};

static int line = 0;

const char *ReadFile( FILE *file )
{
	static char buffer[ 0x8000 * 3 + 128 ];

	const char *ptr;

	// Read the next line from the file - ignore blank or comment lines
	do
	{
		line++;
		if( fgets( buffer, sizeof( buffer ), file ) == nullptr )
		{
			return nullptr;
		}
		ptr = buffer;
		while( isspace( *ptr ))
		{
			ptr++;
		}
	}
	while(( *ptr == '#' ) || ( *ptr == '\0' ));

	// Remove the trailing '\n'
	buffer[ strlen( buffer ) - 1 ] = '\0';

	return ptr;
}

const char *ReadPhrase( const char *ptr, sNode *node )
{
	FUNCTION_ENTRY( nullptr, "ReadPhrase", false );

	if( *ptr == '"' )
	{
		const char *end = ptr + 1;
		while(( *end != '\0' ) && ( *end != '"' ))
		{
			end++;
		}
		int length = end - ptr - 1;
		node->phrase = std::string( ptr + 1, length );
		ptr = ( *end == '"' ) ? end + 1 : end;
	}
	else
	{
		const char *end = ptr + 1;
		while(( *end != '\0' ) && !isspace( *end ))
		{
			end++;
		}
		int length = end - ptr;
		node->phrase = std::string( ptr, length );
		ptr = end;
	}

	while( isspace( *ptr ))
	{
		ptr++;
	}

	return ptr;
}

void ReadHexData( const char *ptr, sNode *node )
{
	FUNCTION_ENTRY( nullptr, "ReadHexData", false );

	UINT8 buffer[ 0x8000 ];
	int data_length = 0;

	while( *ptr )
	{
		unsigned int byte  = 0;
		int count = sscanf( ptr, "%02X", &byte );

		if( count != 1 )
		{
			fprintf( stderr, "bad data!\n" );
			return;
		}

		buffer[ data_length++ ] = ( UINT8 ) byte;

		ptr += 2;
		while( isspace( *ptr ))
		{
			ptr++;
		}
	}

	UINT8 *data = new UINT8[ data_length ];
	memcpy( data, buffer, data_length );

	node->data = data;
	node->data_length = data_length;
}

static UINT8 *vsmDataPtr;
static int vsmBitsLeft;
static int vsmData;

void WriteBits( int data, int count )
{
	FUNCTION_ENTRY( nullptr, "WriteBits", false );

	int mask = 1 << ( count - 1 );

	if( data >= ( 1 << count ))
	{
		DBG_WARNING( "data (" << data << ") contains more than " << count << " bits" );
	}

	while( count-- )
	{
		vsmBitsLeft--;
		vsmData <<= 1;

		if( data & mask )
		{
			vsmData |= 1;
		}

		mask >>= 1;

		if( vsmBitsLeft == 0 )
		{
			*vsmDataPtr++ = ( UINT8 ) vsmData;
			vsmBitsLeft   = 8;
		}
	}
}

int ReadCoefficient( const char *&ptr, char type, int bits )
{
	FUNCTION_ENTRY( nullptr, "ReadCoefficient", false );

	if(( ptr[0] != type ) || ( ptr[1] != ':' ))
	{
		throw "Syntax error";
	}

	ptr += 2;

	char *end = nullptr;

	int coefficient = std::strtol( ptr, &end, 2 );

	if( end - ptr != bits )
	{
		throw "Invalid coefficient";
	}

	WriteBits( coefficient, bits );

	while( isspace( *end ))
	{
		end++;
	}

	ptr = end;

	return coefficient;
}

bool ParseFrame( FILE *file )
{
	FUNCTION_ENTRY( nullptr, "ParseFrame", false );

	const char *ptr = ReadFile( file );

	int energy = ReadCoefficient( ptr, 'E', 4 );

	if( energy == 15 )
	{
		return false;
	}

	if( energy != 0x00 )
	{
		int repeat = ReadCoefficient( ptr, 'R', 1 );
		int pitch  = ReadCoefficient( ptr, 'P', 6 );

		if( repeat == 0 )
		{
				ReadCoefficient( ptr, 'K', 5 );
				ReadCoefficient( ptr, 'K', 5 );
				ReadCoefficient( ptr, 'K', 4 );
				ReadCoefficient( ptr, 'K', 4 );

			if( pitch != 0 )
			{
				ReadCoefficient( ptr, 'K', 4 );
				ReadCoefficient( ptr, 'K', 4 );
				ReadCoefficient( ptr, 'K', 4 );
				ReadCoefficient( ptr, 'K', 3 );
				ReadCoefficient( ptr, 'K', 3 );
				ReadCoefficient( ptr, 'K', 3 );
			}
		}
	}

	return true;
}

void ReadParsedData( sNode *node, FILE *file )
{
	FUNCTION_ENTRY( nullptr, "ReadParsedData", false );

	try
	{
		UINT8 buffer[ 0x8000 ];

		vsmDataPtr  = buffer;
		vsmBitsLeft = 8;
		vsmData     = 0;

		while( ParseFrame( file ))
		{
		}

		if( vsmBitsLeft != 8 )
		{
			*vsmDataPtr++ = ( UINT8 ) ( vsmData << vsmBitsLeft );
		}

		int data_length = vsmDataPtr - buffer;

		UINT8 *data = new UINT8[ data_length ];
		memcpy( data, buffer, data_length );

		node->data        = data;
		node->data_length = data_length;
	}
	catch( const char *msg )
	{
		fprintf( stderr, "Phrase: \"%s\" - %s\n", node->phrase.c_str( ), msg );
	}

	vsmDataPtr = nullptr;
}

void ReadData( const char *ptr, sNode *node, FILE *file )
{
	FUNCTION_ENTRY( nullptr, "ReadData", false );

	// We'll figure this out later
	node->copy        = false;
	node->data        = nullptr;
	node->data_length = 0;

	if( strncmp( ptr, "<null>", 6 ) == 0 )
	{
		UINT8 *data = new UINT8[ 1 ];

		data[ 0 ] = 0xF0;

		node->data        = data;
		node->data_length = 1;
	}
	else if( *ptr == '-' )
	{
		ptr += 1;
		while( isspace( *ptr ))
		{
			ptr++;
		}
		if( isxdigit( *ptr ))
		{
			ReadHexData( ptr, node );
		}
	}
	else if( *ptr == '\0' )
	{
		ReadParsedData( node, file );
	}
}

sNode *ReadNode( FILE *file )
{
	FUNCTION_ENTRY( nullptr, "ReadNode", true );

	sNode *node = nullptr;

	if( const char *ptr = ReadFile( file ))
	{
		node = new sNode;

		ptr = ReadPhrase( ptr, node );

		if( node->phrase.empty( ))
		{
			fprintf( stdout, "Invalid phrase on line %d\n", line );
			exit( -1 );
		}

		ReadData( ptr, node, file );

		if( node->data == nullptr )
		{
			fprintf( stdout, "Invalid speech data on line %d\n", line );
			exit( -1 );
		}

		if( node->data_length > 255 )
		{
			fprintf( stdout, "Phrase \"%s\" contains too much speech data on line %d\n", node->phrase.c_str( ), line );
			exit( -1 );
		}
	}

	return node;
}

int sort_node( const void *ptr1, const void *ptr2 )
{
	FUNCTION_ENTRY( nullptr, "sort_node", false );

	const sNode *node1 = * ( const sNode * const * ) ptr1;
	const sNode *node2 = * ( const sNode * const * ) ptr2;

	return node1->phrase.compare( node2->phrase );
}

sNode *SplitList( sNode **list, int size )
{
	FUNCTION_ENTRY( nullptr, "SplitList", true );

	if( size == 0 )
	{
		return nullptr;
	}

	int rootIndex = size / 2;
	sNode *root = list[ rootIndex ];

	root->prev = SplitList( list, rootIndex );
	root->next = SplitList( list + rootIndex + 1, size - rootIndex - 1 );

	return root;
}

size_t CalculateOffsets( sNode *root, sNode **list, int count )
{
	FUNCTION_ENTRY( nullptr, "CalculateOffsets", true );

	// Calculate the phrase offsets
	size_t offset = 1;
	root->phrase_offset = offset;
	offset += 1 + root->phrase.length( ) + 2 + 2 + 4;
	for( int i = 0; i < count; i++ )
	{
		sNode *node = list[ i ];
		if( node == root )
		{
			continue;
		}
		node->phrase_offset = offset;
		offset += 1 + node->phrase.length( ) + 2 + 2 + 4;
	}

	// Calculate the data offsets
	for( int i = 0; i < count; i++ )
	{
		sNode *node = list[ i ];
		// Look for a match in an earlier phrase
		for( int j = 0; j < i; j++ )
		{
			if( list[ j ]->data_length >= node->data_length )
			{
				// Look for this phrase at the end of a previous one
				size_t size_offset = list[ j ]->data_length - node->data_length;
				if( memcmp(( const char * ) list[ j ]->data + size_offset, node->data, node->data_length ) == 0 )
				{
					node->copy = true;
					node->data_offset = list[ j ]->data_offset;
				}
			}
		}
		if( node->copy == false )
		{
			node->data_offset = offset;
			offset += node->data_length;
		}
	}

	return offset;
}

UINT8 *StoreNode( const sNode *node, UINT8 *ptr )
{
	FUNCTION_ENTRY( nullptr, "StoreNode", true );

	*ptr++ = ( UINT8 ) ( node->phrase.size( ));
	ptr += sprintf(( char * ) ptr, "%s", node->phrase.c_str( ));

	size_t offset;

	offset = node->prev ? node->prev->phrase_offset : 0;
	*ptr++ = ( UINT8 ) ( offset >> 8 );
	*ptr++ = ( UINT8 ) ( offset & 0xFF );

	offset = node->next ? node->next->phrase_offset : 0;
	*ptr++ = ( UINT8 ) ( offset >> 8 );
	*ptr++ = ( UINT8 ) ( offset & 0xFF );

	offset = node->data_offset;
	*ptr++ = ( UINT8 ) ( '\0' );              // Dunno what this is yet
	*ptr++ = ( UINT8 ) ( offset >> 8 );
	*ptr++ = ( UINT8 ) ( offset & 0xFF );
	*ptr++ = ( UINT8 ) ( node->data_length );

	return ptr;
}

void MakeROM( const sNode *root, sNode **list, int count, UINT8 *rom )
{
	FUNCTION_ENTRY( nullptr, "MakeROM", true );

	UINT8 *ptr = rom;

	*ptr++ = 0xAA;

	// Store the phrases
	ptr = StoreNode( root, ptr );
	for( int i = 0; i < count; i++ )
	{
		const sNode *node = list[ i ];
		if( node == root )
		{
			continue;
		}
		ptr = StoreNode( node, ptr );
	}

	// Store the data
	for( int i = 0; i < count; i++ )
	{
		const sNode *node = list[ i ];
		if( node->copy == false )
		{
			memcpy( ptr, node->data, node->data_length );
			ptr += node->data_length;
		}
	}
}

bool ParseFileName( const char *arg, void *filename )
{
	FUNCTION_ENTRY( nullptr, "ParseFileName", true );

	const char *ptr = strchr( arg, '=' );

	// Handle special case of extract all
	if( ptr == nullptr )
	{
		fprintf( stderr, "A filename needs to be specified: '%s'\n", arg );
		return false;
	}

	strcpy(( char * ) filename, ptr + 1 );

	return true;
}

void PrintUsage( )
{
	FUNCTION_ENTRY( nullptr, "PrintUsage", true );

	fprintf( stdout, "Usage: mkspch [options] file\n" );
	fprintf( stdout, "\n" );
}

size_t vsmBytesLeft;

int ReadBits( int count, const char *text, FILE *file )
{
	FUNCTION_ENTRY( nullptr, "ReadBits", false );

	UINT8 data = 0;

	if( file != nullptr )
	{
		fprintf( file, "%s", text );
	}

	while( count-- )
	{
		if( vsmBitsLeft == 0 )
		{
			if( vsmBytesLeft == 0 )
			{
				throw "End of speech data reached with no STOP CODE frame";
			}
			vsmBytesLeft--;
			vsmData      = *vsmDataPtr++;
			vsmBitsLeft  = 8;
		}

		data <<= 1;
		if( vsmData & 0x80 )
		{
			data |= 1;
		}

		if( file != nullptr )
		{
			fprintf( file, "%c", ( data & 1 ) ? '1' : '0' );
		}

		vsmData <<= 1;
		vsmBitsLeft--;
	}

	return data;
}

bool DumpFrame( FILE *file )
{
	FUNCTION_ENTRY( nullptr, "DumpFrame", false );

	if( file != nullptr )
	{
		fprintf( file, "\t" );
	}

	int energy = ReadBits( 4, "E:", file );

	if( energy == 15 )
	{
		if( file != nullptr )
		{
			fprintf( file, "\n" );
		}
		return false;
	}

	if( energy != 0x00 )
	{
		int repeat = ReadBits( 1, " R:", file );
		int pitch  = ReadBits( 6, " P:", file );

		if( repeat == 0 )
		{
			// Voiced/Unvoiced frame
			ReadBits( 5, " K:", file );
			ReadBits( 5, " K:", file );
			ReadBits( 4, " K:", file );
			ReadBits( 4, " K:", file );

			if( pitch != 0 )
			{
				// Voiced frame
				ReadBits( 4, " K:", file );
				ReadBits( 4, " K:", file );
				ReadBits( 4, " K:", file );
				ReadBits( 3, " K:", file );
				ReadBits( 3, " K:", file );
				ReadBits( 3, " K:", file );
			}
		}
	}

	if( file != nullptr )
	{
		fprintf( file, "\n" );
	}

	return true;
}

size_t DumpSpeechData( FILE *file, sNode *node )
{
	FUNCTION_ENTRY( nullptr, "DumpSpeechData", true );

	vsmBytesLeft = node->data_length;
	vsmDataPtr   = const_cast<UINT8 *>( node->data );
	vsmBitsLeft  = 0;

	try
	{
		while( DumpFrame( file ))
		{
		}
	}
	catch( const char *msg )
	{
		fprintf( stderr, "Phrase: \"%s\" - %s\n", node->phrase.c_str( ), msg );
	}

	return vsmBytesLeft;
}

size_t CheckData( FILE *file, sNode *node, int *phrases, int *unique, size_t *data_used, UINT8 *flags )
{
	FUNCTION_ENTRY( nullptr, "CheckData", true );

	size_t wasted = 0;

	if( node->prev )
	{
		wasted += CheckData( file, node->prev, phrases, unique, data_used, flags );
	}

	*phrases   += 1;
	*data_used += 1 + node->phrase.size( ) + 6;

	if( file != nullptr )
	{
		fprintf( file, "Phrase: \"%s\"\n", node->phrase.c_str( ));
	}

	size_t extra = DumpSpeechData( file, node );

	if(( vsmBytesLeft > 0 ) && ( verbose != 0 ))
	{
		fprintf( stderr, "%d bytes left processing phrase %s\n", ( int ) vsmBytesLeft, node->phrase.c_str( ));
	}

	size_t offset = node->data_offset;

	if( flags[ offset ] == 0 )
	{
		*unique    += 1;
		*data_used += node->data_length;
		wasted     += extra;
		memset( flags + offset, 1, node->data_length );
	}

	if( node->next )
	{
		wasted += CheckData( file, node->next, phrases, unique, data_used, flags );
	}

	return wasted;
}

void PrintStats( sNode *root )
{
	UINT8 flags[ ROM_SIZE ];
	memset( flags, 0, sizeof( flags ));

	int phrases      = 0;
	int unique       = 0;
	size_t data_used = 1;

	size_t data_wasted = CheckData(( verbose >= 1 ) ? stdout : nullptr, root, &phrases, &unique, &data_used, flags );

	if(( data_wasted > 0 ) && ( verbose >= 1 ))
	{
		fprintf( stdout, "\n" );
	}

	fprintf( stdout, "%7d Phrases (%d unique)\n", phrases, unique );
	fprintf( stdout, "%7d Bytes used (%d bytes excess)\n", ( int ) data_used, ( int ) data_wasted );
	fprintf( stdout, "%7d Bytes free (potentially %d bytes)\n", ( int ) ( ROM_SIZE - data_used ), ( int ) ( ROM_SIZE - data_used + data_wasted ));
	fprintf( stdout, "\n" );
}

void FreeTree( sNode *node )
{
	if( node->prev != nullptr )
	{
		FreeTree( node->prev );
	}
	if( node->next != nullptr )
	{
		FreeTree( node->next );
	}

	delete [] const_cast<UINT8 *>( node->data );
	delete node;
}

int main( int argc, char *argv[] )
{
	FUNCTION_ENTRY( nullptr, "main", true );

	char outputFile[ 256 ] = "spchrom.bin";

	sOption optList[ ] =
	{
		{ 'o', "output=*<filename>",  OPT_NONE,                      true, outputFile,    ParseFileName,  "Create output file <filename>" },
		{ 'v', "verbose",             OPT_VALUE_SET | OPT_SIZE_BOOL, true, &verbose,      nullptr,        "Display extra information" }
	};

	if( argc == 1 )
	{
		PrintHelp( SIZE( optList ), optList );
		return 0;
	}

	printf( "TI-99/4A Speech ROM Utility\n" );

	int index = 1;
	index = ParseArgs( index, argc, argv, SIZE( optList ), optList );

	if( index >= argc )
	{
		fprintf( stderr, "No input file specified\n" );
		return -1;
	}

	FILE *datFile = fopen( argv[ index ], "rt" );
	if( datFile == nullptr )
	{
		fprintf( stderr, "Unable to open input file \"%s\"\n", argv[ index ] );
		return -1;
	}

	sNode *list[ ROM_SIZE / MIN_NODE_SIZE ];
	int count = 0;

	// Read in the basic phrases and their speech data
	for( EVER )
	{
		sNode *node = ReadNode( datFile );
		if( node == nullptr )
		{
			break;
		}
		if( count >= ROM_SIZE / MIN_NODE_SIZE )
		{
			fprintf( stderr, "There are too many phrases in the file \"%s\"\n", argv[ index ] );
			break;
		}
		list[ count++ ] = node;
	}

	fclose( datFile );

	// Put them in alphabetical order
	qsort( list, count, sizeof( sNode * ), sort_node );

	// Build the binary tree
	sNode *root = SplitList( list, count );

	size_t size = CalculateOffsets( root, list, count );

	if( size > ROM_SIZE )
	{
		fprintf( stderr, "There is too much data in the file (over by %d bytes)\n", ( int ) ( size - ROM_SIZE ));
		return -1;
	}

	fprintf( stdout, "\n" );
	fprintf( stdout, "%5d Phrases read\n", count );
	fprintf( stdout, "%7d Bytes used\n", ( int ) size );
	fprintf( stdout, "\n" );

	UINT8 ROM[ ROM_SIZE ];
	memset( ROM, 0, sizeof( ROM ));

	MakeROM( root, list, count, ROM );

	FILE *romFile = fopen( outputFile, "wb" );
	if( romFile == nullptr )
	{
		fprintf( stderr, "Unable to open output file \"%s\"\n", outputFile );
		FreeTree( root );
		return -1;
	}

	fwrite( ROM, sizeof( ROM ), 1, romFile );

	fclose( romFile );

	PrintStats( root );

	FreeTree( root );

	return 0;
}
