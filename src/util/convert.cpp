//----------------------------------------------------------------------------
//
// File:        convert.cpp
// Date:        09-May-1995
// Programmer:  Marc Rousseau
//
// Description: This programs will convert a .lst or .dat file to a .ctg file
//
// Copyright (c) 1995-2004 Marc Rousseau, All Rights Reserved.
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

#if !defined( _MSC_VER )
	#include <dirent.h>
#endif
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "common.hpp"
#include "logger.hpp"
#include "cartridge.hpp"
#include "option.hpp"
#include "disk-media.hpp"
#include "file-system-disk.hpp"
#include "fileio.hpp"
#include "support.hpp"

DBG_REGISTER( __FILE__ );

#if defined( OS_OS2 ) || defined( OS_WINDOWS )
	const char MARKER_CHAR = '\x10';
#else
	const char MARKER_CHAR = '*';
#endif

enum eFileType
{
	LST,		// The original: A text listing with either a hex dump or (new) explicit filenames
	ROM,		// Console ROMS (ticpu.bin+tigpl.bin or 994arom.bin+994agrom.bin)
	HEX,		// Generic binary files (fileG.bin, fileC.bin, fileD.bin)
	GK,			// Gram Kracker file(s)
	CTG,		// Existing ti99sim cartridge file
	PIF,		// Windows PIF file
	LS379		// 74LS379 bank switch ROM file
};

struct sGromHeader
{
	// This is only valid AFTER bytes have been swapped !!!
	UINT8    Valid;
	UINT8    Version;
	UINT8    NoApplications;
	UINT8    reserved1;
	UINT16   PowerUpHeader;
	UINT16   ApplicationHeader;
	UINT16   DSR_Header;
	UINT16   SubprogramHeader;
	UINT16   InterruptHeader;
	UINT16   reserved2;

	sGromHeader( UINT8 *ptr );
};

static inline UINT16 GetUINT16( const void *_ptr )
{
	FUNCTION_ENTRY( nullptr, "GetUINT16", true );

	const UINT8 *ptr = ( const UINT8 * ) _ptr;
	return ( UINT16 ) (( ptr[ 0 ] << 8 ) | ptr[ 1 ] );
}

static bool IsMultipleOf( int x, int base )
{
	return x / base * base == x;
}

static bool IsPowerOf2( int x )
{
	return ( x != 0 ) && (( x & ( ~x + 1)) == x );
}

static int NextPowerOf2( int x )
{
	if( IsPowerOf2( x ))
	{
		return x;
	}

	int power = 0;

	while( x > 0 )
	{
		x /= 2;
		power += 1;
	}

	return 1 << power;
}

sGromHeader::sGromHeader( UINT8 *ptr ) :
	Valid( 0 ),
	Version( 0 ),
	NoApplications( 0 ),
	reserved1( 0 ),
	PowerUpHeader( 0 ),
	ApplicationHeader( 0 ),
	DSR_Header( 0 ),
	SubprogramHeader( 0 ),
	InterruptHeader( 0 ),
	reserved2( 0 )
{
	FUNCTION_ENTRY( this, "sGromHeader ctor", true );

	sGromHeader *rawHdr = reinterpret_cast<sGromHeader *>( ptr );

	Valid             = rawHdr->Valid;
	Version           = rawHdr->Version;
	NoApplications    = rawHdr->NoApplications;
	reserved1         = rawHdr->reserved1;

	PowerUpHeader     = GetUINT16( &rawHdr->PowerUpHeader );
	ApplicationHeader = GetUINT16( &rawHdr->ApplicationHeader );
	DSR_Header        = GetUINT16( &rawHdr->DSR_Header );
	SubprogramHeader  = GetUINT16( &rawHdr->SubprogramHeader );
	InterruptHeader   = GetUINT16( &rawHdr->InterruptHeader );
	reserved2         = GetUINT16( &rawHdr->reserved2 );
}

void FindName( cCartridge *cartridge )
{
	FUNCTION_ENTRY( nullptr, "FindName", true );

	if( cartridge->GetTitle( ) != nullptr )
	{
		return;
	}

	printf( "Found the following names:\n" );

	char *title = nullptr;

	// Search GROM for headers
	for( unsigned i = 0; i < NUM_GROM_BANKS; i++ )
	{
		sMemoryRegion *memory = cartridge->GetGromMemory( i );
		for( int j = 0; j < memory->NumBanks; j++ )
		{
			UINT8 *data = memory->Bank[ 0 ].Data;
			sGromHeader hdr( data );
			if( hdr.Valid != 0xAA )
			{
				continue;
			}
			int addr = i * GROM_BANK_SIZE;
			if( hdr.ApplicationHeader == 0 )
			{
				continue;
			}
			int appHdr = hdr.ApplicationHeader;
			while( appHdr )
			{
				appHdr -= addr;
				if(( appHdr < 0 ) || ( appHdr > GROM_BANK_SIZE ))
				{
					break;
				}
				UINT8 length = data[ appHdr + 4 ];
				UINT8 *name = &data[ appHdr + 5 ];
				printf( " %c %*.*s\n", title ? ' ' : MARKER_CHAR, length, length, name );
				appHdr = ( data[ appHdr ] << 8 ) + data[ appHdr + 1 ];
				if( title == nullptr )
				{
					title = ( char * ) malloc( length + 1 );
					sprintf( title, "%*.*s", length, length, name );
				}
			}
		}
	}

	// Search ROM for headers
	for( unsigned i = 0; i < NUM_ROM_BANKS; i++ )
	{
		sMemoryRegion *memory = cartridge->GetCpuMemory( i );
		for( int j = 0; j < memory->NumBanks; j++ )
		{
			UINT8 *data = memory->Bank[ 0 ].Data;
			if( data == nullptr )
			{
				DBG_ERROR( "Invalid memory - bank " << j << " is missing @>" << hex << ( UINT16 ) ROM_BANK_SIZE );
				continue;
			}
			sGromHeader hdr( data );
			if( hdr.Valid != 0xAA )
			{
				continue;
			}
			int addr = i * ROM_BANK_SIZE;
			if( hdr.ApplicationHeader == 0 )
			{
				continue;
			}
			int appHdr = hdr.ApplicationHeader;
			while( appHdr )
			{
				appHdr -= addr;
				if(( appHdr < 0 ) || ( appHdr >= ROM_BANK_SIZE ))
				{
					break;
				}
				UINT8 length = data[ appHdr + 4 ];
				UINT8 *name = &data[ appHdr + 5 ];
				printf( " %c %*.*s\n", title ? ' ' : MARKER_CHAR, length, length, name );
				appHdr = ( data[ appHdr ] << 8 ) + data[ appHdr + 1 ];
				if( title == nullptr )
				{
					title = ( char * ) malloc( length + 1 );
					sprintf( title, "%*.*s", length, length, name );
				}
			}
		}
	}

	cartridge->SetTitle( title );

	if( title != nullptr )
	{
		free( title );
	}

	printf( "\n" );
}

void ResizeGROM( cCartridge *cartridge )
{
	for( unsigned i = 0; i < NUM_GROM_BANKS; i++ )
	{
		sMemoryRegion *memory = cartridge->GetGromMemory( i );
		for( int j = 0; j < memory->NumBanks; j++ )
		{
			if( memory->Bank[ j ].Type == BANK_ROM )
			{
				UINT8 *data = memory->Bank[ j ].Data;
				for( int x = 0; x < 0x0800; x++ )
				{
					data[ 0x1800 + x ] = data[ 0x0800 + x ] | data[ 0x1000 + x ];
				}
			}
		}
	}
}

void ShowSummary( cCartridge *cartridge )
{
	FUNCTION_ENTRY( nullptr, "ShowSummary", true );

	printf( "\nModule Summary:\n" );

	printf( "  Title: %s\n", cartridge->GetTitle( ));

	UINT16 cru = cartridge->GetCRU( );
	if( cru != 0 )
	{
		printf( "    CRU: %04X\n", cru );
	}

	printf( "  GROMS: " );
	int groms = 0;
	for( unsigned i = 0; i < NUM_GROM_BANKS; i++ )
	{
		if( cartridge->GetGromMemory( i )->NumBanks > 0 )
		{
			printf( "%d ", i );
			groms++;
		}
	}
	printf( groms ? "\n" : "None\n" );

	if( cartridge->GetCpuMemory( 0 )->NumBanks > 0 )
	{
		printf( "  Operating System ROM\n" );
	}

	for( unsigned i = 2; i < NUM_ROM_BANKS; i++ )
	{
		if( cartridge->GetCpuMemory( i )->NumBanks > 0 )
		{
			int banks = cartridge->GetCpuMemory( i )->NumBanks;
			int type  = cartridge->GetCpuMemory( i )->Bank[ 0 ].Type;
			printf( "  %d bank%s of %s at %04X\n", banks, ( banks > 1 ) ? "s" : "", ( type == BANK_ROM ) ? "ROM" : "RAM", i * 0x1000 );
		}
	}

	printf( "\n" );
}

//----------------------------------------------------------------------------
//
// Create a module from a .lst/.dat listing file
//
// Valid Regions:
//
//    ROM   - CPU memory
//    GROM  - GROM memory
//
// The types of sections that can be specified are:
//
//    ROM   - Read-Only memory
//    RAM   - Read/Write memory
//    RAMB  - Battery backed Read/Write memory
//
// ROM/RAM banks are 4096 (0x1000) bytes
// GROM/GRAM banks are 8192 (0x2000) bytes
//
//----------------------------------------------------------------------------

int ConvertDigit( char digit )
{
	FUNCTION_ENTRY( nullptr, "ConvertDigit", true );

	int number = toupper( digit ) - '0';
	if( number > 9 )
	{
		number -= ( 'A' - '0' ) - 10;
	}
	return number;
}

char *GetAddress( char *ptr, UINT32 *address )
{
	FUNCTION_ENTRY( nullptr, "GetAddress", true );

	*address = 0;

	if( ptr == nullptr )
	{
		return nullptr;
	}

	while( isxdigit( *ptr ))
	{
		*address <<= 4;
		*address |= ConvertDigit( *ptr++ );
	}

	while(( *ptr != '\0' ) && !isspace( *ptr ))
	{
		ptr++;
	}

	return ( *ptr != '\0' ) ? ptr : nullptr;
}

static int LineNumber = 0;
static const char *FileName;

bool ReadLine( FILE *file, char *buffer, int length )
{
	FUNCTION_ENTRY( nullptr, "ReadLine", true );

	*buffer = '\0';

	do
	{
		if( fgets( buffer, length, file ) == nullptr )
		{
			return false;
		}
		LineNumber++;
		while(( *buffer != '\0' ) && ( isspace( *buffer )))
		{
			buffer++;
		}
	}
	while( *buffer == '*' );

	return true;
}

void ReadBank( FILE *file, UINT8 *buffer, unsigned int size )
{
	FUNCTION_ENTRY( nullptr, "ReadBank", true );

	char line[ 256 ];

	size_t start = ftell( file );

	if( ReadLine( file, line, sizeof( line )) == false )
	{
		fprintf( stderr, "\n%s:%d: Unexpected end of file\n", FileName, LineNumber );
		exit( -1 );
	}

	if( strncmp( line, "; FILE ", 7 ) == 0 )
	{
		char filename[ 256 ];
		unsigned int offset, length;
		const char *format = ( line[ 7 ] == '"' ) ? "; FILE \"%255[^\"]\" %8x %8x\n" : "; FILE %255s %8x %8x\n" ;
		if( sscanf( line, format, filename, &offset, &length ) == 3 )
		{
			FILE *dataFile = fopen( filename, "rb" );
			if( dataFile == nullptr )
			{
				fprintf( stderr, "\n%s:%d: Unable to open data file '%s'\n", FileName, LineNumber, filename );
				exit( -1 );
			}

			fseek( dataFile, 0, SEEK_END );
			size_t end = ftell( dataFile );

			if( offset >= end )
			{
				fprintf( stderr, "\n%s:%d: Specified file offset (%04X) is invalid\n", FileName, LineNumber, offset );
			}
			else
			{
				if( offset + length > end )
				{
					fprintf( stderr, "\n%s:%d: Not enough data (%d bytes specified, only %d bytes available)\n", FileName, LineNumber, length, ( int )( end - offset ));
					length = end - offset;
				}

				fseek( dataFile, offset, SEEK_SET );
				if( fread( buffer, std::min( length, size ), 1, dataFile ) != 1 )
				{
					fprintf( stderr, "\n%s:%d: Error reading from file '%s'\n", FileName, LineNumber, filename );
					exit( -1 );
				}

				if( length < size )
				{
					memset( buffer + length, 0, size - length );

					if(( size == GROM_BANK_SIZE ) && ( length == 0x1800 ))
					{
						for( int i = 0; i < 0x0800; i++ )
						{
							buffer[ 0x1800 + i ] = buffer[ 0x0800 + i ] | buffer[ 0x1000 + i ];
						}
					}
				}
			}

			fclose( dataFile );
		}
		else
		{
			fprintf( stderr, "\n%s:%d: Syntax error\n", FileName, LineNumber );
		}
		return;
	}

	fseek( file, start, SEEK_SET );

	int mask = size - 1;

	unsigned max = ( unsigned ) -1;

	for( unsigned int j = 0; j < size; )
	{
		start = ftell( file );

		if( ReadLine( file, line, sizeof( line )) == false )
		{
			break;
		}

		if( line[ 0 ] == ';' )
		{
			fseek( file, start, SEEK_SET );
			LineNumber--;
			break;
		}

		UINT32 address;
		char *src = GetAddress( line, &address );
		if( src == nullptr )
		{
			break;
		}

		fprintf( stdout, "%04X", address );
		fflush( stdout );

		UINT8 *dest = &buffer[ address & mask ];

		int x = 0;
		while(( *src != '\0' ) && (( unsigned ) x < max ))
		{
			while(( *src == ' ' ) || ( *src == '-' ))
			{
				src++;
			}
			if( isxdigit( src[ 0 ] ) && isxdigit( src[ 1 ] ))
			{
				int hi = ConvertDigit( *src++ );
				int lo = ConvertDigit( *src++ );
				if( isxdigit( *src ))
				{
					break;
				}
				*dest++ = ( UINT8 ) (( hi << 4 ) | lo );
				if( ++j == size )
				{
					break;
				}
				x++;
			}
			else
			{
				break;
			}
		}
		if(( max == ( unsigned ) -1 ) && ( x > 0 ))
		{
			max = x;
		}
	}

	fprintf( stdout, "    " );
	fflush( stdout );
}

void ReadFile( const char *filename, cCartridge *cartridge )
{
	FUNCTION_ENTRY( nullptr, "ReadFile", true );

	FILE *file = fopen( filename, "rt" );
	if( file == nullptr )
	{
		fprintf( stderr, "ERROR: Unable to open file \"%s\"\n", filename );
		exit( -1 );
	}

	FileName = filename;
	LineNumber = 0;

	char line[ 256 ], temp[ 10 ];

	if( ReadLine( file, line, sizeof( line )) == false )
	{
		fprintf( stderr, "ERROR: %s:%d: File is empty!\n", FileName, LineNumber );
		exit( -1 );
	}

	// Look for a module title (optional)
	if( line[ 0 ] == '[' )
	{
		line[ strlen( line ) - 2 ] = '\0';
		cartridge->SetTitle( &line[ 1 ] );
		if( ReadLine( file, line, sizeof( line )) == false )
		{
			fprintf( stderr, "ERROR:  %s:%d: File is invalid - must have at least one bank of ROM/GROM\n", FileName, LineNumber );
			exit( -1 );
		}
	}

	// Look for a CRU address (optional)
	if( strnicmp( &line[ 2 ], "CRU", 3 ) == 0 )
	{
		unsigned int base = 0;
		if(( sscanf( &line[ 2 ], "%9s = %x", temp, &base ) == 2 ) && ( base != 0 ))
		{
			cartridge->SetCRU( base );
		}
		if( ReadLine( file, line, sizeof( line )) == false )
		{
			fprintf( stderr, "ERROR: %s:%d: File is invalid - must have at least one bank of ROM/GROM\n", FileName, LineNumber );
			exit( -1 );
		}
	}

	int errors = 0;

	for( EVER )
	{
		if( line[ 0 ] != ';' )
		{
			errors += 1;
			fprintf( stderr, "WARNING: %s:%d: Expected line beginning with ';'.\n", FileName, LineNumber );
			if( ReadLine( file, line, sizeof( line )) == false )
			{
				break;
			}
			continue;
		}

		int index;
		char type[ 10 ];
		if( sscanf( &line[ 2 ], "%9s %d", type, &index ) != 2 )
		{
			errors += 1;
			fprintf(stderr, "WARNING: %s:%d: Error reading memory type.\n", FileName, LineNumber );
			continue;
		}

		// Get the next memory type (ROM or GROM) and index
		if(( strcmp( type, "ROM" ) != 0 ) && ( strcmp( type, "GROM" ) != 0 ))
		{
			errors += 1;
			fprintf( stderr, "WARNING: %s:%d: Invalid memory index '%s' - expected either 'ROM' or 'GROM'.\n", FileName, LineNumber, type );
			if( ReadLine( file, line, sizeof( line )) == false )
			{
				break;
			}
			continue;
		}

		int size = ( type[ 0 ] == 'G' ) ? GROM_BANK_SIZE : ROM_BANK_SIZE;
		int maxIndex = ( type[ 0 ] == 'G' ) ? NUM_GROM_BANKS : NUM_ROM_BANKS;
		if( index > maxIndex )
		{
			errors += 1;
			fprintf( stderr, "WARNING: %s:%d: Invalid %s index (%d) indicated.\n", FileName, LineNumber, type, index );
			if( ReadLine( file, line, sizeof( line )) == false )
			{
				break;
			}
			continue;
		}
		sMemoryRegion *memory = ( type[ 0 ] == 'G' ) ? cartridge->GetGromMemory( index ) : cartridge->GetCpuMemory( index );

		if( ReadLine( file, line, sizeof( line )) == false )
		{
			break;
		}

READ_BANK:

		if( line[ 0 ] != ';' )
		{
			errors += 1;
			fprintf( stderr, "%s:%d: Expected line beginning with ';'.\n", FileName, LineNumber );
			if( ReadLine( file, line, sizeof( line )) == false )
			{
				break;
			}
			continue;
		}

		int bank;
		if( sscanf( &line[ 2 ], "%9s %d - %s", temp, &bank, type ) != 3 )
		{
			errors += 1;
			fprintf( stderr, "WARNING: %s:%d: Syntax error.\n", FileName, LineNumber );
			if( ReadLine( file, line, sizeof( line )) == false )
			{
				break;
			}
			continue;
		}

		if( strcmp( temp, "BANK" ) != 0 )
		{
			errors += 1;
			fprintf( stderr, "WARNING: %s:%d: Syntax error - expected BANK statement.\n", FileName, LineNumber );
			if( ReadLine( file, line, sizeof( line )) == false )
			{
				break;
			}
			continue;
		}

		if(( strcmp( type, "ROM" ) != 0 ) && ( strcmp( type, "RAM" ) != 0 ) && ( strcmp( type, "RAMB" ) != 0 ))
		{
			errors += 1;
			fprintf( stderr, "WARNING: %s:%d: Invalid memory type '%s' - expected either 'ROM', 'RAM', or 'RAMB'.\n", FileName, LineNumber, type );
			if( ReadLine( file, line, sizeof( line )) == false )
			{
				break;
			}
			continue;
		}

		BANK_TYPE_E memoryType = ( type[ 3 ] == 'B' ) ? BANK_BATTERY_BACKED :
		                         ( type[ 1 ] == 'O' ) ? BANK_ROM : BANK_RAM;

		if( memory->NumBanks <= bank )
		{
			memory->NumBanks = bank + 1;
		}

		UINT8 *ptr = new UINT8[ size ];
		memset( ptr, 0, size );

		memory->Bank[ bank ].Type = memoryType;
		memory->Bank[ bank ].Data = ptr;

		if( memoryType == BANK_ROM )
		{
			fprintf( stdout, "\nReading %-4s %2d bank %d:     ", ( size == ROM_BANK_SIZE ) ? "ROM" : "GROM", index, bank );
			fflush( stdout );
			ReadBank( file, memory->Bank[ bank ].Data, size );
		}

		if( ReadLine( file, line, sizeof( line )) == false )
		{
			break;
		}

		if( strnicmp( &line[ 2 ], "BANK", 4 ) == 0 )
		{
			goto READ_BANK;
		}
	}

	printf( "\n" );

	fclose( file );

	if( errors > 0 )
	{
		exit( -1 );
	}
}

//----------------------------------------------------------------------------
//
// Read raw binary file(s)
//
//----------------------------------------------------------------------------
void ReadHex( const char *gromName, const char *romName1, const char *romName2, cCartridge *cartridge, bool isDSR )
{
	FUNCTION_ENTRY( nullptr, "ReadHex", true );

	// Read GROM
	cRefPtr<cFile> file = cFile::Open( gromName, "disks" );
	if( file != nullptr )
	{
		int bytesLeft = file->FileSize( );
		int region    = 3;

		do
		{
			int bytesRead = 0;

			sMemoryRegion *memory = cartridge->GetGromMemory( region++ );
			memory->NumBanks       = 1;
			memory->Bank[ 0 ].Type = BANK_ROM;
			memory->Bank[ 0 ].Data = new UINT8[ GROM_BANK_SIZE ];
			for( int offset = 0; offset < GROM_BANK_SIZE; offset += DEFAULT_SECTOR_SIZE )
			{
				int count = file->ReadRecord( memory->Bank[ 0 ].Data + offset, DEFAULT_SECTOR_SIZE );
				bytesRead += count;
			}

			if( verbose )
			{
				fprintf( stdout, "GROM %04X - read %04X bytes from '%s'\n", ( region - 1 ) * GROM_BANK_SIZE, bytesRead, gromName );
			}

			bytesLeft -= bytesRead;
		}
		while( bytesLeft > 0 );
	}

	// Read RAM 0
	file = cFile::Open( romName1, "disks" );
	if( file != nullptr )
	{
		int baseIndex = ( isDSR == true ) ? 4 : 6;

		for( int i = 0; i < 2; i++ )
		{
			int bytesRead = 0;

			sMemoryRegion *memory = cartridge->GetCpuMemory( baseIndex + i );
			memory->NumBanks       = 1;
			memory->Bank[ 0 ].Type = BANK_ROM;
			memory->Bank[ 0 ].Data = new UINT8[ ROM_BANK_SIZE ];
			for( int offset = 0; offset < ROM_BANK_SIZE; offset += DEFAULT_SECTOR_SIZE )
			{
				int count = file->ReadRecord( memory->Bank[ 0 ].Data + offset, DEFAULT_SECTOR_SIZE );
				bytesRead += count;
			}

			if( verbose )
			{
				fprintf( stdout, "ROM (bank 0) - read %04X bytes from '%s'\n", bytesRead, romName1 );
			}
		}

		// Read RAM 1
		file = cFile::Open( romName2, "disks" );
		if( file != nullptr )
		{
			for( int i = 0; i < 2; i++ )
			{
				int bytesRead = 0;

				sMemoryRegion *memory = cartridge->GetCpuMemory( baseIndex + i );
				memory->NumBanks       = 2;
				memory->Bank[ 1 ].Type = BANK_ROM;
				memory->Bank[ 1 ].Data = new UINT8[ ROM_BANK_SIZE ];
				for( int offset = 0; offset < ROM_BANK_SIZE; offset += DEFAULT_SECTOR_SIZE )
				{
					int count = file->ReadRecord( memory->Bank[ 0 ].Data + offset, DEFAULT_SECTOR_SIZE );
					bytesRead += count;
				}

				if( verbose )
				{
					fprintf( stdout, "ROM (bank 1) - read %04X bytes from '%s'\n", bytesRead, romName2 );
				}
			}
		}
	}
}

void Read379( const char *fileName, cCartridge *cartridge, bool inverted )
{
	FUNCTION_ENTRY( nullptr, "Read379", true );

	// Read the cartridge ROM
	FILE *file = fopen( fileName, "rb" );
	if( file == nullptr )
	{
		return;
	}

	fseek( file, 0, SEEK_END );
	size_t size = ftell( file );
	fseek( file, 0, SEEK_SET );

	if( ! IsMultipleOf( size, 2 * ROM_BANK_SIZE ))
	{
		fprintf( stderr, "ERROR: File \"%s\" is not a multiple of %d bytes\n", fileName, 2 * ROM_BANK_SIZE );
		fclose( file );
		return;
	}

	int numBanks = size / ( 2 * ROM_BANK_SIZE );

	if( ! IsPowerOf2 ( numBanks ))
	{
		fprintf( stderr, "ERROR: File \"%s\" only contains data for %d banks (need %d more)\n", fileName, numBanks, NextPowerOf2( numBanks ) - numBanks );
		fclose( file );
		return;
	}

	constexpr int MAX_BANKS = sizeof( sMemoryRegion::Bank ) / sizeof( sMemoryRegion::Bank[ 0 ] );

	if( numBanks > MAX_BANKS )
	{
		fprintf( stderr, "ERROR: This version of TI-99/Sim only supports %d banks, \"%s\" contains %d - the last %d will not be included\n", MAX_BANKS, fileName, numBanks, numBanks - MAX_BANKS );
		numBanks = MAX_BANKS;
	}

	for( int i = 0; i < 2 * numBanks; i++ )
	{
		int bank = inverted ? ( numBanks - 1 ) - ( i / 2 ) : ( i / 2 );

		sMemoryRegion *memory = cartridge->GetCpuMemory( 6 + ( i % 2 ));

		memory->NumBanks = numBanks;
		memory->Bank[ bank ].Type = BANK_ROM;
		memory->Bank[ bank ].Data = new UINT8[ ROM_BANK_SIZE ];

		int read = fread( memory->Bank[ bank ].Data, 1, ROM_BANK_SIZE, file );

		if( read != ROM_BANK_SIZE )
		{
			if( read > 0 )
			{
				fprintf( stderr, "WARNING: Error reading from 379 ROM file \"%s\" (read %d bytes, expecting %d bytes - bank %d)\n", fileName, read, ROM_BANK_SIZE, bank );
				memory->NumBanks = 0;
			}
			break;
		}

		if( verbose )
		{
			fprintf( stdout, "ROM %04X (bank %d) - read %04X bytes from '%s'\n", ( 6 + ( i % 2 )) * ROM_BANK_SIZE, bank, read, fileName );
		}
	}

	fclose( file );
}

//----------------------------------------------------------------------------
//
// Read V9T9 style .hex/.bin files given a 'base' file name.
//
// A search is done first for V6.0 style names using the specified extension
//   i.e. Given foo.bin -> foog.bin, fooc.bin, food.bin
// If no match is found, a search for earlier naming conventions is made
//   i.e. Given foo.hex -> foo(g).hex, fooc0.hex, fooc1.hex
//
//----------------------------------------------------------------------------
void ReadHex( const char *fileName, cCartridge *cartridge, bool isDSR )
{
	FUNCTION_ENTRY( nullptr, "ReadHex", true );

	static const char *gromNames[ ] =
	{
		"%sg.%s", "%sG.%s", "%s.%s"
	};
	static const char *romNames1[ ] =
	{
		"%sc.%s", "%sC.%s", "%sc0.%s", "%sC0.%s"
	};
	static const char *romNames2[ ] =
	{
		"%sd.%s", "%sD.%s", "%sc1.%s", "%sC1.%s"
	};

	int warnings = 0;

	char filename[ 512 ], base[ 512 ], ext[ 4 ];

	auto ReadMemoryRegion = [&]( bool isGROM, int index, int bank, FILE *file )
	{
		size_t size = isGROM ? GROM_BANK_SIZE : ROM_BANK_SIZE;

		UINT8 *data = new UINT8[ size ];

		size_t read = fread( data, 1, size, file );

		if( verbose )
		{
			if( isGROM )
			{
				fprintf( stdout, "GROM %04X - read %04X bytes from '%s'\n", ( int ) ( index * size ), ( int ) read, filename );
			}
			else
			{
				fprintf( stdout, "ROM %04X (bank %d) - read %04X bytes from '%s'\n", ( int ) ( index * size ), bank, ( int ) read, filename );
			}
		}

		// Fill out 6K GROM banks
		if( isGROM && ( read == 0x1800 ))
		{
			for( int i = 0; i < 0x0800; i++ )
			{
				data[ 0x1800 + i ] = data[ 0x0800 + i ] | data[ 0x1000 + i ];
			}
			read = 0x2000;
		}

		sMemoryRegion *memory = isGROM ? cartridge->GetGromMemory( index ) : cartridge->GetCpuMemory( index );

		memory->NumBanks = bank + 1;
		memory->Bank[ bank ].Type = BANK_ROM;
		memory->Bank[ bank ].Data = data;

		if( read != size )
		{
			if( warnings++ == 0 )
			{
				fprintf( stderr, "\n" );
			}
			fprintf( stderr, "WARNING: Error reading from %s%s file \"%s\" (read %d bytes, expecting %d bytes) \n", isDSR ? "DSR " : "", isGROM ? "GROM" : "ROM", filename, ( int ) read, ROM_BANK_SIZE );
			memset( data + read, 0, size - read );
		}
	};

	strcpy( filename, fileName );

	if( isDSR == true )
	{
		// Read DSR ROM
		if( FILE *file = fopen( fileName, "rb" ))
		{
			ReadMemoryRegion( false, 4, 0, file );
			ReadMemoryRegion( false, 5, 0, file );
			fclose( file );
		}

		return;
	}

	// Truncate the filename to the 'base' filename
	size_t len = strlen( filename );
	strcpy( ext, &filename[ len-3 ] );
	strcpy( base, filename );
	base[ len-4 ] = '\0';

	// Read GROM
	for( auto &pattern : gromNames )
	{
		sprintf( filename, pattern, base, ext );
		if( FILE *file = fopen( filename, "rb" ))
		{
			int region = 3;
			int ch = getc( file );
			while( !feof( file ))
			{
				ungetc( ch, file );
				ReadMemoryRegion( true, region++, 0, file );
				ch = getc( file );
			}
			fclose( file );
			break;
		}
	}

	// Read RAM 0
	for( auto &pattern : romNames1 )
	{
		sprintf( filename, pattern, base, ext );
		if( FILE *file = fopen( filename, "rb" ))
		{
			ReadMemoryRegion( false, 6, 0, file );
			ReadMemoryRegion( false, 7, 0, file );
			fclose( file );
			break;
		}
	}

	// Read RAM 1
	for( auto &pattern : romNames2 )
	{
		sprintf( filename, pattern, base, ext );
		if( FILE *file = fopen( filename, "rb" ))
		{
			ReadMemoryRegion( false, 6, 1, file );
			ReadMemoryRegion( false, 7, 1, file );
			fclose( file );
			break;
		}
	}

	if( warnings != 0 )
	{
		fprintf( stderr, "\n" );
	}
}

bool FindFile( char *filename )
{
#if defined( _MSC_VER )
	FILE *file =  fopen( filename, "r+" );
	if( file != nullptr )
	{
		fclose( file );
		return true;
	}
#endif

#if defined( OS_LINUX ) || defined( OS_MACOSX )
	DIR *dir = opendir( "." );
	dirent *dp = readdir( dir );
	while( dp != nullptr )
	{
		if( !( dp->d_type & DT_DIR ) && !stricmp( filename, dp->d_name ))
		{
			strcpy( filename, dp->d_name );
			closedir( dir );
			return true;
		}
		dp = readdir( dir );
	}

	closedir( dir );
#endif

	return false;
}

//----------------------------------------------------------------------------
//  Read Microsoft Program Information Files
//----------------------------------------------------------------------------
void ReadProgramInformation( const char *fileName, cCartridge *cartridge )
{
	FILE *file = fopen( fileName, "rb" );
	if( file == nullptr )
	{
		fprintf( stderr, "ERROR: Unable to open file \"%s\"\n", fileName );
		return;
	}
	char buffer[ 4096 ];
	// Jump to the Parameters string of the Basic section
	fseek( file, 0xA5, SEEK_SET );
	if( fgets( buffer, sizeof( buffer ), file ) == nullptr )
	{
		fclose( file );
		return;
	}
	fclose( file );

	char romName[ 4096 ];

	char *ptr = strstr( buffer, "-cart " );
	while( ptr != nullptr )
	{
		char *name = ptr + strlen( "-cart " );
		while( *name && isspace( *name ))
		{
			name++;
		}
		char *end = name;
		while( *end && !isspace( *end ))
		{
			end++;
		}
		int length = end - name;
		snprintf( romName, 4096, "%*.*s", length, length, name );
		// Find and correct the case of the filename
		if( !FindFile( romName ))
		{
			return;
		}
		if( stricmp( romName, "blankg.bin" ) != 0 )
		{
			char *ext = strchr( romName, '.' );
			memcpy( ext - 1, ext, strlen( ext ) + 1 );
			ReadHex( romName, cartridge, false );
			return;
		}
		ptr = strstr( end, "-cart " );
	}
}

//----------------------------------------------------------------------------
//  Read V9T9 style CPU & GPL console ROMS and make a 'special' cartridge
//----------------------------------------------------------------------------
void ReadConsole( const char *fileName, cCartridge *cartridge )
{
	FUNCTION_ENTRY( nullptr, "ReadConsole", true );

	static const char *romNames[ ] =
	{
		"", "%sticpu.%s", "%s994arom.%s"
	};
	static const char *gromNames[ ] =
	{
		"", "%stigpl.%s", "%s994agrom.%s"
	};

	char filename[ 512 ], name[ 512 ];

	strcpy( filename, fileName );

	const char *base = "";

	// Truncate the filename to the 'base' filename
	size_t type = 0, len = strlen( filename ), max = len;
	const char *ext = &filename[ len-3 ];

	if(( len >= 6 ) && ( strnicmp( filename + len - 6, "ti.", 3 ) == 0 ))
	{
		max = 6;
		type = 1;
	}
	if(( len >= 8 ) && ( strnicmp( filename + len - 8, "994a.", 5 ) == 0 ))
	{
		max = 8;
		type = 2;
	}

	if( len > max )
	{
		base = filename;
		filename[ len - max ] = '\0';
	}

	sprintf( name, romNames[ type ], base, ext );
	ReadHex( name, cartridge, true );

	// Move ROMS 4+5 to ROMs 0+1
	*cartridge->GetCpuMemory( 0 ) = *cartridge->GetCpuMemory( 4 );
	*cartridge->GetCpuMemory( 1 ) = *cartridge->GetCpuMemory( 5 );

	// Use ROM 2 to zero out GROMs 4+5
	sMemoryRegion *memory = cartridge->GetCpuMemory( 2 );
	*cartridge->GetCpuMemory( 4 ) = *memory;
	*cartridge->GetCpuMemory( 5 ) = *memory;

	sprintf( name, gromNames[ type ], base, ext );
	ReadHex( name, cartridge, false );

	// Move GROMs 3+4+5 to GROMs 0+1+2 and
	*cartridge->GetGromMemory( 0 ) = *cartridge->GetGromMemory( 3 );
	*cartridge->GetGromMemory( 1 ) = *cartridge->GetGromMemory( 4 );
	*cartridge->GetGromMemory( 2 ) = *cartridge->GetGromMemory( 5 );

	// Use GROM 6 to zero out GROMs 3+4+5
	memory = cartridge->GetGromMemory( 6 );
	*cartridge->GetGromMemory( 3 ) = *memory;
	*cartridge->GetGromMemory( 4 ) = *memory;
	*cartridge->GetGromMemory( 5 ) = *memory;

	// Create banks for 32K Memory Expansion
	int ramBanks[ ] = { 0x02, 0x03, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F };

	for( unsigned i = 0; i < SIZE( ramBanks ); i++ )
	{
		cartridge->GetCpuMemory( ramBanks[ i ] )->NumBanks       = 1;
		cartridge->GetCpuMemory( ramBanks[ i ] )->Bank[ 0 ].Type = BANK_RAM;
	}

	// Set the name and save it to the expected filename
	cartridge->SetTitle( "TI-99/4A Console" );
	cartridge->SaveImage( "TI-994A.ctg" );

	ShowSummary( cartridge );
}

//----------------------------------------------------------------------------
//
//    Gram Kracker: Loader/Saver Order and Header bytes
//
//    Byte        Description
//     00         More to load flag
//                  FF = More to load
//                  80 = Load UTIL Option next
//                  00 = Last file to load
//     01         What Gram Chip or Ram Bank
//                  01 = Grom/Gram 0 g0000
//                  02 = Grom/Gram 1 g2000
//                  03 = Grom/Gram 2 g4000
//                  04 = Grom/Gram 3 g6000
//                  05 = Grom/Gram 4 g8000
//                  06 = Grom/Gram 5 gA000
//                  07 = Grom/Gram 6 gC000
//                  08 = Grom/Gram 7 gE000
//                  09 = Rom/Ram Bank 1 c6000
//                  0A = Rom/Ram Bank 2 c6000
//            00 or FF = Program Image - load to Memory Expansion
//     02         0000 = Number of bytes to load
//     04         0000 = Address to start loading at
//
//----------------------------------------------------------------------------

bool ReadGK( const char *baseFilename, cCartridge *cartridge )
{
	FUNCTION_ENTRY( nullptr, "ReadGK", true );

	auto path = std::filesystem::path( baseFilename );
	auto ext = path.extension( ).native( );
	auto name = path.replace_extension( "" );

	char filename[ 128 ];
	strcpy( filename, baseFilename );

	int more = 0xFF;
	int nextIndex = 1;

	while( more == 0xFF )
	{
		cRefPtr<cFile> file = cFile::Open( filename, "disks" );
		if( file == nullptr )
		{
			fprintf( stderr, "Unable to open file \"%s\"\n", filename );
			return false;
		}

		if( file->IsProgram( ) == false )
		{
			fprintf( stderr, "File \"%s\" is not a Gram Kracker file\n", filename );
			return false;
		}

		if( verbose >= 1 )
		{
			fprintf( stdout, "Reading file \"%s\"\n", filename );
		}

		UINT8 fileBuffer[ DEFAULT_SECTOR_SIZE ];

		int count = file->ReadRecord( fileBuffer, DEFAULT_SECTOR_SIZE );

		if( count < 6 )
		{
			fprintf( stderr, "Error reading from file \"%s\"\n", filename );
			return false;
		}

		more = fileBuffer[ 0 ];

		int bank    = fileBuffer[ 1 ];
		int length  = GetUINT16( fileBuffer + 2 );
		int address = GetUINT16( fileBuffer + 4 );

		if( length > GROM_BANK_SIZE )
		{
			fprintf( stderr, "Invalid length indicated in file \"%s\"\n", filename );
			return false;
		}

		UINT8 *buffer = new UINT8[ GROM_BANK_SIZE ];
		memset( buffer, 0, GROM_BANK_SIZE );

		UINT8 *src = fileBuffer + 6;
		UINT8 *dst = buffer;

		count -= 6;

		while(( length > 0 ) && ( count > 0 ))
		{
			memcpy( dst, src, count );

			length -= count;
			dst += count;

			count = file->ReadRecord( fileBuffer, DEFAULT_SECTOR_SIZE );
			src = fileBuffer;
		}

		sMemoryRegion *region;

		if(( bank > 0 ) && ( bank < 9 ))
		{
			region = cartridge->GetGromMemory( address >> 13 );
			region->NumBanks = 1;
			region->Bank[ 0 ].Data = buffer;
		}
		else if( bank == 9 )
		{
			region = cartridge->GetCpuMemory( address >> 12 );
			if( region->NumBanks == 0 )
			{
				region->NumBanks = 1;
			}
			region->Bank[ 0 ].Data = buffer;
			region++;
			UINT8 *newBuffer = new UINT8[ ROM_BANK_SIZE ];
			memcpy( newBuffer, buffer + ROM_BANK_SIZE, ROM_BANK_SIZE );
			if( region->NumBanks == 0 )
			{
				region->NumBanks = 1;
			}
			region->Bank[ 0 ].Data = newBuffer;
		}
		else if( bank == 10 )
		{
			region = cartridge->GetCpuMemory( address >> 12 );
			region->NumBanks = 2;
			region->Bank[ 1 ].Data = buffer;
			region++;
			UINT8 *newBuffer = new UINT8[ ROM_BANK_SIZE ];
			memcpy( newBuffer, buffer + ROM_BANK_SIZE, ROM_BANK_SIZE );
			region->NumBanks = 2;
			region->Bank[ 1 ].Data = newBuffer;
		}

		sprintf( filename, "%s%d%s", name.c_str( ), nextIndex++, ext.c_str( ));
	}

	return true;
}

void HexDump( FILE *file, int base, const UINT8 *pData, int length )
{
	FUNCTION_ENTRY( nullptr, "HexDump", true );

	while( length > 0 )
	{
		int count = std::min( length, 16 );

		char bufferHex[ 16 * 3 + 3 ], *ptrHex = bufferHex;
		char bufferASCII[ 16 + 1 ], *ptrASCII = bufferASCII;
		char bufferBASIC[ 16 + 1 ], *ptrBASIC = bufferBASIC;

		for( int i = 0; i < count; i++ )
		{
			if( i == 8 ) ptrHex += sprintf( ptrHex, "- " );
			ptrHex   += sprintf( ptrHex, "%02X ", pData[ i ] );
			ptrASCII += sprintf( ptrASCII, "%c", isprint( pData[ i ] ) ? pData[ i ] : '.' );
			ptrBASIC += sprintf( ptrBASIC, "%c", isprint( pData[ i ] + 0x60 ) ? pData[ i ] + 0x60 : '.' );
		}

		fprintf( file, "%04X %-50.50s'%s' %*s'%s'\n", base, bufferHex, bufferASCII, 16 - count, "", bufferBASIC );

		pData  += count;
		base   += count;
		length -= count;
	}
}

void DumpCartridge( cCartridge *cartridge, const std::string &defaultName )
{
	FUNCTION_ENTRY( nullptr, "DumpCartridge", true );

	const char *types[ ] = { "??", "RAM", "ROM", "RAMB" };

	auto path = std::filesystem::path{ cartridge->GetFileName( ) ? cartridge->GetFileName( ) : defaultName };

	std::string filename = path.stem( ).native( ) + ".dat";

	FILE *file = fopen( filename.c_str( ), "wt" );
	if( file == nullptr )
	{
		fprintf( stderr, "Unable to open file \"%s\"\n", filename.c_str( ));
		return;
	}

	fprintf( file, "[ %s ]\n", cartridge->GetTitle( ));

	if( cartridge->GetCRU( ) != 0 )
	{
		fprintf( file, "; CRU = %04X\n", cartridge->GetCRU( ));
	}

	for( unsigned i = 0; i < NUM_ROM_BANKS; i++ )
	{
		const sMemoryRegion *ptr = cartridge->GetCpuMemory( i );
		if( ptr->NumBanks == 0 )
		{
			continue;
		}
		fprintf( file, "; ROM %d\n", i );
		for( int j = 0; j < ptr->NumBanks; j++ )
		{
			int type = ( ptr->Bank[ j ].Flags & FLAG_BATTERY_BACKED ) ? BANK_BATTERY_BACKED : ptr->Bank[ j ].Type;
			fprintf( file, "; BANK %d - %s\n", j, types[ type ] );
			if( ptr->Bank[ j ].Type == BANK_ROM )
			{
				HexDump( file, i * ROM_BANK_SIZE, ptr->Bank[ j ].Data, ROM_BANK_SIZE );
			}
		}
	}

	for( unsigned i = 0; i < NUM_GROM_BANKS; i++ )
	{
		const sMemoryRegion *ptr = cartridge->GetGromMemory( i );
		if( ptr->NumBanks == 0 )
		{
			continue;
		}
		fprintf( file, "; GROM %d\n", i );
		for( int j = 0; j < ptr->NumBanks; j++ )
		{
			int type = ( ptr->Bank[ j ].Flags & FLAG_BATTERY_BACKED ) ? BANK_BATTERY_BACKED : ptr->Bank[ j ].Type;
			fprintf( file, "; BANK %d - %s\n", j, types[ type ] );
			if( ptr->Bank[ j ].Type == BANK_ROM )
			{
				int size = Is6K( ptr->Bank[ j ].Data, 0x2000 ) ? 0x1800 : 0x2000;
				HexDump( file, i * GROM_BANK_SIZE, ptr->Bank[ j ].Data, size );
			}
		}
	}

	fclose( file );
}

bool isProgramInformation( const char *fileName )
{
	FUNCTION_ENTRY( nullptr, "isProgramInformation", true );

	size_t len = strlen( fileName );
	return (( len > 4 ) && ( stricmp( &fileName[ len-4 ], ".pif" ) == 0 )) ? true : false;
}

bool isCartridge( const char *fileName )
{
	FUNCTION_ENTRY( nullptr, "isCartridge", true );

	size_t len = strlen( fileName );
	return (( len > 4 ) && ( stricmp( &fileName[ len-4 ], ".ctg" ) == 0 )) ? true : false;
}

bool isHexFile( const char *fileName )
{
	FUNCTION_ENTRY( nullptr, "isHexFile", true );

	size_t len = strlen( fileName );
	if(( len > 4 ) && ( stricmp( &fileName[ len-4 ], ".hex" ) == 0 ))
	{
		return true;
	}
	if(( len > 4 ) && ( stricmp( &fileName[ len-4 ], ".bin" ) == 0 ))
	{
		return true;
	}

	FILE *file = fopen( fileName, "rb" );
	if( file != nullptr )
	{
		fseek( file, 0, SEEK_END );
		size_t size = ftell( file );
		fclose( file );
		if( size % 4096 == 0 )
		{
			return true;
		}
	}

	return false;
}

bool isRomFile( const char *fileName )
{
	FUNCTION_ENTRY( nullptr, "isRomFile", true );

	if( isHexFile( fileName ) == false )
	{
		return false;
	}
	size_t len = strlen( fileName );
	if(( len >= 6 ) && ( strnicmp( fileName + len - 6, "ti.", 3 ) == 0 ))
	{
		return true;
	}
	if(( len >= 8 ) && ( strnicmp( fileName + len - 8, "994a.", 5 ) == 0 ))
	{
		return true;
	}
	return false;
}

bool isListing( const char *fileName )
{
	FUNCTION_ENTRY( nullptr, "isListing", true );

	size_t len = strlen( fileName );
	if(( len > 4 ) && ( stricmp( &fileName[ len-4 ], ".lst" ) == 0 ))
	{
		return true;
	}
	if(( len > 4 ) && ( stricmp( &fileName[ len-4 ], ".dat" ) == 0 ))
	{
		return true;
	}
	return false;
}

eFileType WhichType( const char *fileName )
{
	FUNCTION_ENTRY( nullptr, "WhichType", true );

	if( isProgramInformation( fileName ))
	{
		return PIF;
	}
	if( isCartridge( fileName ))
	{
		return CTG;
	}
	if( isRomFile( fileName ))
	{
		return ROM;
	}
	if( isHexFile( fileName ))
	{
		return HEX;
	}
	if( isListing( fileName ))
	{
		return LST;
	}
	return GK;
}

void PrintUsage( )
{
	FUNCTION_ENTRY( nullptr, "PrintUsage", true );

	fprintf( stdout, "Usage: convert-ctg [options] file [title]\n" );
	fprintf( stdout, "\n" );
}

bool ParseCRU( const char *arg, void *ptr )
{
	FUNCTION_ENTRY( nullptr, "ParseCRU", true );

	arg += strlen( "cru=" );

	if( sscanf( arg, "%x", ( unsigned int * ) ptr ) != 1 )
	{
		fprintf( stderr, "Invalid CRU address specified: '%s'\n", arg );
		return false;
	}

	return true;
}

int main( int argc, char *argv[] )
{
	FUNCTION_ENTRY( nullptr, "main", true );

	int baseCRU = -1;
	bool dumpCartridge = false;
	bool force6K = false;
	bool is378 = false;
	bool is379 = false;

	sOption optList[ ] =
	{
		{ '8', "378",        OPT_VALUE_SET | OPT_SIZE_BOOL, true, &is378,         nullptr,  "Create a ROM cartridge using 74LS378 bank switching" },
		{ '9', "379",        OPT_VALUE_SET | OPT_SIZE_BOOL, true, &is379,         nullptr,  "Create a ROM cartridge using 74LS379 inverted bank switching" },
		{ '6', "force6K",    OPT_VALUE_SET | OPT_SIZE_BOOL, true, &force6K,       nullptr,  "Treat all GROM banks as 6K when creating new cartridges" },
		{  0,  "cru*=base",  OPT_NONE,                      0,    &baseCRU,       ParseCRU, "Create a DSR cartridge at the indicated CRU address" },
		{ 'd', "dump",       OPT_VALUE_SET | OPT_SIZE_BOOL, true, &dumpCartridge, nullptr,  "Create a hex dump of the cartridge" },
		{ 'v', "verbose*=n", OPT_VALUE_PARSE_INT,           1,    &verbose,       nullptr,  "Display extra information" }
	};

	if( argc == 1 )
	{
		PrintHelp( SIZE( optList ), optList );
		return 0;
	}

	printf( "TI-99/Sim .ctg file converter\n\n" );

	int index = 1;
	index = ParseArgs( index, argc, argv, SIZE( optList ), optList );

	if( index >= argc )
	{
		fprintf( stderr, "No input file specified\n" );
		return -1;
	}

	cRefPtr<cCartridge> cartridge = new cCartridge( "" );

	std::string srcFilename = argv[ index++ ];
	std::string dstFilename;

	switch( WhichType( srcFilename.c_str( )))
	{
		case PIF :
			ReadProgramInformation( srcFilename.c_str( ), cartridge );
			break;
		case CTG :
			dstFilename = srcFilename;
			if((( srcFilename = LocateFile( "cartridges", dstFilename )).empty( ) == true ) &&
			   (( srcFilename = LocateFile( "console", dstFilename )).empty( ) == true ))
			{
				fprintf( stderr, "Unable to load cartridge \"%s\"\n", dstFilename.c_str( ));
				return -1;
			}
			if( cartridge->LoadImage( srcFilename.c_str( )) == false )
			{
				fprintf( stderr, "The file \"%s\" does not appear to be a proper ROM cartridge\n", dstFilename.c_str( ));
				return -1;
			}
			break;
		case ROM :
			ReadConsole( srcFilename.c_str( ), cartridge );
			return 0;
		case HEX :
			if( is378 || is379 )
			{
				Read379( srcFilename.c_str( ), cartridge, is379 );
			}
			else
			{
				ReadHex( srcFilename.c_str( ), cartridge, ( baseCRU > 0 ) ? true : false );
			}
			break;
		case LST :
			ReadFile( srcFilename.c_str( ), cartridge );
			break;
		case GK :
			ReadGK( srcFilename.c_str( ), cartridge );
			break;
		default :
			fprintf( stderr, "Unable to determine the file format for \"%s\"\n", srcFilename.c_str( ));
			return -1;
	}

	if( !cartridge->IsValid( ))
	{
		fprintf( stderr, "Failed to create cartridge from \"%s\"\n", srcFilename.c_str( ));
		return -1;
	}

	if( baseCRU != -1 )
	{
		cartridge->SetCRU( baseCRU );
	}

	if( cartridge->GetTitle( ) == nullptr )
	{
		if( argv[ index ] != nullptr )
		{
			cartridge->SetTitle( argv[ index ] );
		}
		else
		{
			FindName( cartridge );
		}
	}

	if( force6K )
	{
		ResizeGROM( cartridge );
	}

	ShowSummary( cartridge );

	if( dumpCartridge == true )
	{
		DumpCartridge( cartridge, dstFilename.empty( ) ? srcFilename : dstFilename );
	}
	else
	{
		auto filename = std::filesystem::path{ dstFilename.empty( ) ? srcFilename : dstFilename }.filename( );

		filename.replace_extension( ".ctg" );

		cartridge->SaveImage( filename.c_str( ));
	}

	return 0;
}
