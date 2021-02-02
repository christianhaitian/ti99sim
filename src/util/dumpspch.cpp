//----------------------------------------------------------------------------
//
// File:        dumpspch.cpp
// Date:        31-Jul-2002
// Programmer:  Marc Rousseau
//
// Description:
//
// Copyright (c) 2002-2004 Marc Rousseau, All Rights Reserved.
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

static int dataFormat;

static const UINT8 *vsmDataPtr;
static int vsmBytesLeft;
static int vsmBitsLeft;
static int vsmData;

static inline UINT16 GetUINT16( const void *_ptr )
{
	FUNCTION_ENTRY( nullptr, "GetUINT16", true );

	const UINT8 *ptr = ( const UINT8 * ) _ptr;
	return ( UINT16 ) (( ptr[ 0 ] << 8 ) | ptr[ 1 ] );
}

sNode *ReadNode( const UINT8 *rom, int offset )
{
	FUNCTION_ENTRY( nullptr, "ReadNode", true );

	sNode *node = new sNode;

	size_t length = rom[ offset ];

	const UINT8 *ptr = &rom[ offset + length + 1 ];

	int prevOffset = GetUINT16( ptr );
	int nextOffset = GetUINT16( ptr + 2 );
	int dataOffset = GetUINT16( ptr + 5 );

	node->phrase      = { ( const char * ) rom + offset + 1, length };
	node->data        = rom + dataOffset;
	node->data_offset = dataOffset;
	node->data_length = ptr[ 7 ];
	node->copy        = false;

	if( prevOffset )
	{
		node->prev = ReadNode( rom, prevOffset );
	}
	if( nextOffset )
	{
		node->next = ReadNode( rom, nextOffset );
	}

	return node;
}

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
	vsmDataPtr   = node->data;
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

void DumpPhrase( sNode *node, FILE *spchFile )
{
	FUNCTION_ENTRY( nullptr, "DumpPhrase", true );

	if( node->prev )
	{
		DumpPhrase( node->prev, spchFile );
	}

	if( dataFormat == 0 )
	{
		int space = 20 - ( int ) node->phrase.size( );
		fprintf( spchFile, "\"%s\"%*.*s -", node->phrase.c_str( ), space, space, "" );

		for( size_t i = 0; i < node->data_length; i++ )
		{
			fprintf( spchFile, " %02X", node->data[ i ] );
		}
		fprintf( spchFile, "\n" );
	}
	else if( dataFormat == 1 )
	{
		fprintf( spchFile, "\"%s\"\n", node->phrase.c_str( ));

		DumpSpeechData( spchFile, node );
	}
	else
	{
		DBG_ERROR( "Unrecognized data format (" << dataFormat << ")" );
	}

	if( node->next )
	{
		DumpPhrase( node->next, spchFile );
	}
}

void DumpROM( sNode *root, FILE *spchFile )
{
	FUNCTION_ENTRY( nullptr, "DumpROM", true );

	fprintf( spchFile, "# TMS5220 Speech ROM data file\n" );
	fprintf( spchFile, "\n" );

	DumpPhrase( root, spchFile );
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

	delete node;
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

	fprintf( stdout, "Usage: dumpspch [options] file\n" );
	fprintf( stdout, "\n" );
}

int main( int argc, char *argv[] )
{
	FUNCTION_ENTRY( nullptr, "main", true );

	char outputFile[ 256 ] = "spchrom.dat";

	sOption optList[ ] =
	{
		{  0,  "format=hex",          OPT_VALUE_SET | OPT_SIZE_INT,  0,    &dataFormat,   nullptr,        "Speech data listed in hexadecimal" },
		{  0,  "format=spch",         OPT_VALUE_SET | OPT_SIZE_INT,  1,    &dataFormat,   nullptr,        "Decoded speech data" },
		{ 'o', "output=*<filename>",  OPT_NONE,                      true, outputFile,    ParseFileName,  "Create output file <filename>" },
		{ 'v', "verbose",             OPT_VALUE_SET | OPT_SIZE_BOOL, true, &verbose,      nullptr,        "Display extra information" }
	};

	if( argc == 1 )
	{
		PrintHelp( SIZE( optList ), optList );
		return 0;
	}

	printf( "TI-99/4A Speech ROM Dump Utility\n" );

	int index = 1;
	index = ParseArgs( index, argc, argv, SIZE( optList ), optList );

	if( index >= argc )
	{
		fprintf( stderr, "No input file specified\n" );
		return -1;
	}

	FILE *romFile = fopen( argv[ index ], "rb" );
	if( romFile == nullptr )
	{
		fprintf( stderr, "Unable to open input file \"%s\"\n", argv[ index ] );
		return -1;
	}

	UINT8 ROM[ ROM_SIZE ];
	memset( ROM, 0, sizeof( ROM ));

	if( fread( ROM, sizeof( ROM ), 1, romFile ) != 1 )
	{
		fprintf( stderr, "Error reading from file \"%s\"\n", argv[ index ] );
		fclose( romFile );
		return -1;
	}

	fclose( romFile );

	sNode *root = ReadNode( ROM, 1 );

	FILE *datFile = fopen( outputFile, "wt" );
	if( datFile == nullptr )
	{
		fprintf( stderr, "Unable to open output file \"%s\"\n", outputFile );
		FreeTree( root );
		return -1;
	}

	DumpROM( root, datFile );

	fclose( datFile );

	fprintf( stdout, "\n" );

	PrintStats( root );

	FreeTree( root );

	return 0;
}
