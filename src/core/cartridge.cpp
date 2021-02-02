
//----------------------------------------------------------------------------
//
// File:        cartridge.cpp
// Date:        23-Feb-1998
// Programmer:  Marc Rousseau
//
// Description:
//
// Copyright (c) 1998-2004 Marc Rousseau, All Rights Reserved.
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
#include <cstring>
#include <regex>
#include "common.hpp"
#include "logger.hpp"
#include "compress.hpp"
#include "cartridge.hpp"
#include "support.hpp"
#include "decode-lzw.hpp"
#include "encode-lzw.hpp"

#if defined( __GNUC__ )
	#include <unistd.h>
#endif

DBG_REGISTER( __FILE__ );

enum eMemoryRegion
{
	ROM_0,              // 0x0000 - 0x0FFF
	ROM_1,              // 0x1000 - 0x1FFF
	ROM_2,              // 0x2000 - 0x2FFF
	ROM_3,              // 0x3000 - 0x3FFF
	ROM_4,              // 0x4000 - 0x4FFF
	ROM_5,              // 0x5000 - 0x5FFF
	ROM_6,              // 0x6000 - 0x6FFF
	ROM_7,              // 0x7000 - 0x7FFF
	ROM_8,              // 0x8000 - 0x8FFF
	ROM_9,              // 0x9000 - 0x9FFF
	ROM_A,              // 0xA000 - 0xAFFF
	ROM_B,              // 0xB000 - 0xBFFF
	ROM_C,              // 0xC000 - 0xCFFF
	ROM_D,              // 0xD000 - 0xDFFF
	ROM_E,              // 0xE000 - 0xEFFF
	ROM_F,              // 0xF000 - 0xFFFF
	GROM_0,             // 0x0000 - 0x1FFF
	GROM_1,             // 0x2000 - 0x3FFF
	GROM_2,             // 0x4000 - 0x5FFF
	GROM_3,             // 0x6000 - 0x7FFF
	GROM_4,             // 0x8000 - 0x9FFF
	GROM_5,             // 0xA000 - 0xBFFF
	GROM_6,             // 0xC000 - 0xDFFF
	GROM_7              // 0xE000 - 0xFFFF
};

const int FILE_VERSION = 0x20;

static inline void WriteUINT16( UINT16 value, FILE *file )
{
	FUNCTION_ENTRY( nullptr, "WriteUINT16", true );

	fputc( static_cast<UINT8>( value >>  8 ), file );
	fputc( static_cast<UINT8>( value >>  0 ), file );
}

static inline UINT16 ReadUINT16( FILE *file )
{
	FUNCTION_ENTRY( nullptr, "ReadUINT16", true );

	UINT16 retVal = 0;

	retVal = ( retVal << 8 ) | fgetc( file );
	retVal = ( retVal << 8 ) | fgetc( file );

	return retVal;
}

const std::string cCartridge::sm_Banner = "TI-99/4A Module - ";

static int BanksToPower( int x )
{
	int power = 0;

	while( x )
	{
		x /= 2;
		power++;
	}

	return power;
}

static int PowerToBanks( int x )
{
	return x ? 1 << ( x - 1 ) : 0;
}

cCartridge::cCartridge( const std::string &filename ) :
	cBaseObject( "cCartridge" ),
	m_FileName( ),
	m_RamFileName( ),
	m_Title( ),
	m_BaseCRU( 0 ),
	m_Features( )
{
	FUNCTION_ENTRY( this, "cCartridge ctor", true );

	for( auto &memory : m_CpuMemory )
	{
		memory.NumBanks = 0;
		memory.CurBank = memory.Bank;

		for( auto &bank : memory.Bank )
		{
			bank.Type = BANK_ROM;
			bank.Flags = 0;
			bank.Data = nullptr;
		}
	}

	for( auto &memory : m_GromMemory )
	{
		memory.NumBanks = 0;
		memory.CurBank = memory.Bank;

		for( auto &bank : memory.Bank )
		{
			bank.Type = BANK_ROM;
			bank.Flags = 0;
			bank.Data = nullptr;
		}
	}

	if( ! filename.empty () && LoadImage( filename.c_str( )) == false )
	{
		DBG_ERROR( "Cartridge " << filename << " is invalid" );
	}
}

cCartridge::~cCartridge( )
{
	FUNCTION_ENTRY( this, "cCartridge dtor", true );

	if( SaveRAM( ) == false )
	{
		DBG_WARNING( "Non-volatile memory lost" );
	}

	for( auto &memory : m_CpuMemory )
	{
		for( auto &bank : memory.Bank )
		{
			delete [] bank.Data;
		}
	}

	for( auto &memory : m_GromMemory )
	{
		for( auto &bank : memory.Bank )
		{
			delete [] bank.Data;
		}
	}

	memset( m_CpuMemory, 0, sizeof( m_CpuMemory ));
	memset( m_GromMemory, 0, sizeof( m_GromMemory ));
}

cRefPtr<cCartridge> cCartridge::LoadCartridge( const std::string &description, const std::string &folder )
{
	std::cmatch what;

	std::regex pattern( R"((.*) - (.*))", std::regex_constants::ECMAScript );
	if( std::regex_match( description.c_str( ), what, pattern ))
	{
		auto hash = std::string( what[ 1 ].first, what[ 1 ].second );
		auto path = std::string( what[ 2 ].first, what[ 2 ].second );

		if( !std::filesystem::exists( path ))
		{
			path = LocateCartridge( folder, hash );
		}

		if( !path.empty( ))
		{
			return new cCartridge( path );
		}
	}

	return { };
}

//----------------------------------------------------------------------------
// iBaseObject Methods
//----------------------------------------------------------------------------

const void *cCartridge::GetInterface( const std::string &iName ) const
{
	FUNCTION_ENTRY( this, "cCartridge::GetInterface", false );

	if( iName == "iCartridge" )
	{
		return static_cast<const iCartridge *>( this );
	}

	if( iName == "iStateObject" )
	{
		return static_cast< const iStateObject * >( this );
	}

	return cBaseObject::GetInterface( iName );
}

//----------------------------------------------------------------------------
// cCartridge Methods
//----------------------------------------------------------------------------

const char *cCartridge::GetFileName( ) const
{
	FUNCTION_ENTRY( this, "cCartridge::GetFileName", true );

	return m_FileName.empty( ) ? nullptr : m_FileName.c_str( );
}

void cCartridge::SetTitle( const char *title )
{
	FUNCTION_ENTRY( this, "cCartridge::SetTitle", true );

	m_Title = title ? title : "<Unknown>";

	const size_t MAX_TITLE = 80 - 2 - sm_Banner.size( );

	if( m_Title.size( ) > MAX_TITLE )
	{
		m_Title.resize( MAX_TITLE );
	}
}

const char *cCartridge::GetTitle( ) const
{
	FUNCTION_ENTRY( this, "cCartridge::GetTitle", true );

	return m_Title.empty( ) ? nullptr : m_Title.c_str( );
}

void cCartridge::SetCRU( UINT16 cru )
{
	FUNCTION_ENTRY( this, "cCartridge::SetCRU", true );

	m_BaseCRU = cru;
}

UINT16 cCartridge::GetCRU( ) const
{
	FUNCTION_ENTRY( this, "cCartridge::GetCRU", true );

	return m_BaseCRU;
}

std::string cCartridge::sha1( ) const
{
	SHA1Context context;

	auto UpdateContext = [&]( const sMemoryRegion &region, size_t size )
	{
		for( int i = 0; i < region.NumBanks; i++ )
		{
			if( region.Bank[ i ].Type == BANK_ROM )
			{
				context.Update( region.Bank[ i ].Data, size );
			}
		}
	};

	for( auto &region : m_CpuMemory )
	{
		UpdateContext( region, ROM_BANK_SIZE );
	}
	for( auto &region : m_GromMemory )
	{
		UpdateContext( region, GROM_BANK_SIZE );
	}

	return context.Digest( );
}

std::string cCartridge::GetDescriptor( ) const
{
	return sha1( ) + " - " + GetFileName( );
}

void cCartridge::SetFeature( const char *feature, const char *value )
{
	if( *value != '\0' )
	{
		m_Features[ feature ] = value;
	}
}

const char *cCartridge::GetFeature( const char *feature ) const
{
	auto it = m_Features.find( feature );

	return ( it == m_Features.end( )) ? nullptr : it->second.c_str( );
}

std::list<std::string> cCartridge::GetFeatures( ) const
{
	std::list<std::string> list;

	for( auto it : m_Features )
	{
		list.push_back( it.first );
	}

	return list;
}

sMemoryRegion *cCartridge::GetCpuMemory( size_t index )
{
	FUNCTION_ENTRY( this, "cCartridge::GetCpuMemory", true );

	return m_CpuMemory + index;
}

sMemoryRegion *cCartridge::GetGromMemory( size_t index )
{
	FUNCTION_ENTRY( this, "cCartridge::GetGromMemory", true );

	return m_GromMemory + index;
}

bool cCartridge::IsValid( ) const
{
	FUNCTION_ENTRY( this, "cCartridge::IsValid", true );

	for( auto &memory : m_CpuMemory )
	{
		if( memory.NumBanks != 0 )
		{
			return true;
		}
	}

	for( auto &memory : m_GromMemory )
	{
		if( memory.NumBanks != 0 )
		{
			return true;
		}
	}

	return false;
}

void cCartridge::PrintInfo( FILE *file ) const
{
	FUNCTION_ENTRY( nullptr, "cCartridge::PrintInfo", true );

	fprintf( file, "  File: \"%s\"\n", GetFileName( ));
	fprintf( file, " Title: \"%s\"\n", GetTitle( ));

	fprintf( file, "\n" );

	DumpRegion( file, "ROM", m_CpuMemory, SIZE( m_CpuMemory ), ROM_BANK_SIZE, BANK_ROM, true );
	DumpRegion( file, "RAM", m_CpuMemory, SIZE( m_CpuMemory ), ROM_BANK_SIZE, BANK_RAM, false );
	DumpRegion( file, "GROM", m_GromMemory, SIZE( m_GromMemory ), GROM_BANK_SIZE, BANK_ROM, true );
	DumpRegion( file, "GRAM", m_GromMemory, SIZE( m_GromMemory ), GROM_BANK_SIZE, BANK_RAM, false );

	fprintf( file, "\n" );
}

bool cCartridge::LoadImage( const char *filename )
{
	FUNCTION_ENTRY( this, "cCartridge::LoadImage", true );

	DBG_TRACE( "Opening file " << filename );

	FILE *file = filename ? fopen( filename, "rb" ) : nullptr;
	if( file == nullptr )
	{
		DBG_WARNING( "Unable to locate file " << filename );
		return false;
	}

	SetFileName( filename );

	// Make sure this is really a TI-99/4A cartridge file
	char buffer[ 80 ];
	if( fread( buffer, 1, sizeof( buffer ), file ) != sizeof( buffer ))
	{
		DBG_ERROR( "Error reading from file" );
		return false;
	}

	bool retVal = false;

	if( strncmp( buffer, sm_Banner.c_str( ), sm_Banner.length( )) != 0 )
	{
		DBG_WARNING( "Invalid file format" );
	}
	else
	{
		char *ptr = &buffer[ sm_Banner.length( )];
		ptr[ strlen( ptr ) - 2 ] = '\0';
		SetTitle( ptr );

		DBG_EVENT( "Loading module: " << GetTitle( ));

		int version = fgetc( file );
		if(( version & 0x80 ) != 0 )
		{
			if( ungetc( version, file ) != version )
			{
				DBG_ERROR( "Unable to unget character to file" );
			}
			else
			{
				retVal = LoadImageV0( file );
			}
		}
		else
		{
			switch( version & 0xF0 )
			{
				case 0x10 :
					retVal = LoadImageV1( file );
					break;
				case 0x20 :
					retVal = LoadImageV2( file );
					break;
				default :
					DBG_ERROR( "Unrecognized file version" );
					break;
			}
		}

		if( retVal == true )
		{
			retVal = LoadRAM( );
		}
	}

	fclose( file );

	return retVal;
}

bool cCartridge::SaveImage( const char *filename )
{
	FUNCTION_ENTRY( this, "cCartridge::SaveImage", true );

	FILE *file = filename ? fopen( filename, "wb" ) : nullptr;
	if( file == nullptr )
	{
		return false;
	}

	SetFileName( filename );

	std::string banner = sm_Banner + GetTitle( ) + "\n\u001A";
	banner.resize( 80 );
	fwrite( banner.c_str( ), 80, 1, file );

	DBG_EVENT( "Saving module: " << GetTitle( ));

	fputc( FILE_VERSION, file );

	WriteUINT16( m_BaseCRU, file );

	for( auto &memory : m_CpuMemory )
	{
		if( memory.NumBanks != 0 )
		{
			fputc(( UINT8 ) ( ROM_0 + ( &memory - m_CpuMemory )), file );
			fputc( BanksToPower( memory.NumBanks ), file );
			for( int j = 0; j < memory.NumBanks; j++ )
			{
				fputc(( memory.Bank[ j ].Flags & FLAG_BATTERY_BACKED ) ? BANK_BATTERY_BACKED : memory.Bank[ j ].Type, file );
				if( memory.Bank[ j ].Type == BANK_ROM )
				{
					if( SaveBufferLZW( memory.Bank[ j ].Data, ROM_BANK_SIZE, file ) == false )
					{
						fclose( file );
						return false;
					}
				}
			}
		}
	}

	for( auto &memory : m_GromMemory )
	{
		if( memory.NumBanks != 0 )
		{
			fputc(( UINT8 ) ( GROM_0 + ( &memory - m_GromMemory )), file );
			fputc( BanksToPower( memory.NumBanks ), file );
			for( int j = 0; j < memory.NumBanks; j++ )
			{
				fputc(( memory.Bank[ j ].Flags & FLAG_BATTERY_BACKED ) ? BANK_BATTERY_BACKED : memory.Bank[ j ].Type, file );
				if( memory.Bank[ j ].Type == BANK_ROM )
				{
					if( SaveBufferLZW( memory.Bank[ j ].Data, GROM_BANK_SIZE, file ) == false )
					{
						fclose( file );
						return false;
					}
				}
			}
		}
	}

	fclose( file );

	return true;
}

//----------------------------------------------------------------------------
// iStateObject Methods
//----------------------------------------------------------------------------

std::string cCartridge::GetIdentifier( )
{
	FUNCTION_ENTRY( this, "cCartridge::GetIdentifier", true );

	return sha1( );
}

std::optional<sStateSection> cCartridge::SaveState( )
{
	FUNCTION_ENTRY( this, "cCartridge::SaveState", true );

	sStateSection save;
	save.name = GetIdentifier( );

	auto SaveRegion = [&]( std::string name, const sMemoryRegion &region, size_t size )
	{
		sStateSection memory;
		memory.name = name;

		if( int curBank = region.CurBank - &region.Bank[ 0 ]; curBank != 0 )
		{
			memory.store( "CurBank", curBank, SaveFormat::DECIMAL );
		}
		for( int j = 0; j < region.NumBanks; j++ )
		{
			if( region.Bank[ j ].Type == BANK_RAM )
			{
				auto bank = std::string( "BANK" ) + "0123456789ABCDEF"[ j ];
				memory.store( bank, region.Bank[ j ].Data, size );
			}
		}
		if( !memory.data.empty( ) || !memory.subsections.empty( ))
		{
			save.subsections.push_back( memory );
		}
	};

	for( size_t i = 0; i < SIZE( m_CpuMemory ); i++ )
	{
		SaveRegion( std::string( "ROM" ) + "0123456789ABCDEF"[ i ], m_CpuMemory[ i ], ROM_BANK_SIZE );
	}
	for( size_t i = 0; i < SIZE( m_GromMemory ); i++ )
	{
		SaveRegion( std::string( "GROM" ) + "0123456789ABCDEF"[ i ], m_GromMemory[ i ], GROM_BANK_SIZE );
	}

	return save;
}

bool cCartridge::ParseState( const sStateSection &state )
{
	FUNCTION_ENTRY( this, "cCartridge::ParseState", true );

	auto ParseRegion = [&]( std::string name, sMemoryRegion &region, size_t size )
	{
		auto memory = state.getSubsection( name );

		int curBank = 0;
		if( memory.hasValue( "CurBank" ))
		{
			memory.load( "CurBank", curBank, SaveFormat::DECIMAL );
		}
		region.CurBank = &region.Bank[ curBank ];

		for( int j = 0; j < region.NumBanks; j++ )
		{
			if( region.Bank[ j ].Type == BANK_RAM )
			{
				auto bank = std::string( "BANK" ) + "0123456789ABCDEF"[ j ];
				memory.load( bank, region.Bank[ j ].Data, size );
			}
		}
	};

	for( size_t i = 0; i < SIZE( m_CpuMemory ); i++ )
	{
		auto name = std::string( "ROM" ) + "0123456789ABCDEF"[ i ];
		if( state.hasSubsection( name ))
		{
			ParseRegion( name, m_CpuMemory[ i ], ROM_BANK_SIZE );
		}
	}
	for( size_t i = 0; i < SIZE( m_GromMemory ); i++ )
	{
		auto name = std::string( "GROM" ) + "0123456789ABCDEF"[ i ];
		if( state.hasSubsection( name ))
		{
			ParseRegion( name, m_GromMemory[ i ], GROM_BANK_SIZE );
		}
	}

	return false;
}

//----------------------------------------------------------------------------
// Private Methods
//----------------------------------------------------------------------------

void cCartridge::SetFileName( const char *filename )
{
	FUNCTION_ENTRY( this, "cCartridge::SetFileName", true );

	if( filename == nullptr )
	{
		m_FileName.clear( );
		m_RamFileName.clear( );
		return;
	}

	m_FileName = filename;
	m_RamFileName = GetHomePath( ) / m_FileName.stem( );
	m_RamFileName.replace_extension( ".ram" );
}

void cCartridge::DumpRegion( FILE *file, const char *label, const sMemoryRegion *region, size_t count, size_t bankSize, BANK_TYPE_E type, bool forceDisplay ) const
{
	FUNCTION_ENTRY( nullptr, "cCartridge::DumpRegion", true );

	bool foundType = false;
	for( size_t i = 0; i < count; i++ )
	{
		if(( region[ i ].NumBanks > 0 ) && ( region[ i ].Bank[ 0 ].Type == type ))
		{
			foundType = true;
			break;
		}
	}

	if( foundType == false )
	{
		if( forceDisplay == true )
		{
			fprintf( file, "%6.6s: -NONE-\n", label );
		}
		return;
	}

	fprintf( file, "%6.6s:", label );

	for( size_t i = 0; i < count; i++ )
	{
		if(( region[ i ].NumBanks > 0 ) && ( region[ i ].Bank[ 0 ].Type == type ))
		{
			fprintf( file, " %04X", static_cast<unsigned int> ( i * bankSize ));
			if( region[ i ].NumBanks > 1 )
			{
				fprintf( file, "(%d)", region[ i ].NumBanks );
			}
		}
	}

	fprintf( file, "\n" );
}

bool cCartridge::LoadRAM( ) const
{
	FUNCTION_ENTRY( this, "cCartridge::LoadRAM", true );

	if( m_RamFileName.empty( ))
	{
		return true;
	}

	FILE *file = fopen( m_RamFileName.c_str( ), "rb" );

	if( file == nullptr )
	{
		return true;
	}

	DBG_EVENT( "Loading module RAM: " << GetTitle( ));

	for( auto &memory : m_CpuMemory )
	{
		for( int j = 0; j < memory.NumBanks; j++ )
		{
			// If this bank is RAM & Battery backed - update the cartridge
			if( memory.Bank[ j ].Flags & FLAG_BATTERY_BACKED )
			{
				if( LoadBuffer( ROM_BANK_SIZE, memory.Bank[ j ].Data, file ) == false )
				{
					DBG_ERROR( "Unable to load RAM data" );
					return false;
				}
			}
		}
	}

	for( auto &memory : m_GromMemory )
	{
		for( int j = 0; j < memory.NumBanks; j++ )
		{
			// If this bank is RAM & Battery backed - update the cartridge
			if( memory.Bank[ j ].Flags & FLAG_BATTERY_BACKED )
			{
				if( LoadBuffer( GROM_BANK_SIZE, memory.Bank[ j ].Data, file ) == false )
				{
					DBG_ERROR( "Unable to load GROM data" );
					return false;
				}
			}
		}
	}

	fclose( file );

	return true;
}

bool cCartridge::SaveRAM( ) const
{
	FUNCTION_ENTRY( this, "cCartridge::SaveRAM", true );

	// Don't bother creating a .ram file if there is nothing stored in the RAM

	for( auto &memory : m_CpuMemory )
	{
		for( int j = 0; j < memory.NumBanks; j++ )
		{
			if( memory.Bank[ j ].Flags & FLAG_BATTERY_BACKED )
			{
				for( unsigned k = 0; k < ROM_BANK_SIZE; k++ )
				{
					if( memory.Bank[ j ].Data[ k ] != 0 )
					{
						goto save;
					}
				}
			}
		}
	}

	for( auto &memory : m_GromMemory )
	{
		for( int j = 0; j < memory.NumBanks; j++ )
		{
			if( memory.Bank[ j ].Flags & FLAG_BATTERY_BACKED )
			{
				for( unsigned k = 0; k < GROM_BANK_SIZE; k++ )
				{
					if( memory.Bank[ j ].Data[ k ] != 0 )
					{
						goto save;
					}
				}
			}
		}
	}

	if( !m_RamFileName.empty( ))
	{
		unlink( m_RamFileName.c_str( ));
	}

	return true;

save:

	CreateHomePath( );

	FILE *file = !m_RamFileName.empty( ) ? fopen( m_RamFileName.c_str( ), "wb" ) : nullptr;
	if( file == nullptr )
	{
		DBG_ERROR( "Unable to open RAM file" );
		return false;
	}

	DBG_EVENT( "Saving module RAM: " << GetTitle( ));

	for( auto &memory : m_CpuMemory )
	{
		for( int j = 0; j < memory.NumBanks; j++ )
		{
			// If this bank is battery-backed RAM then update the .ram file
			if( memory.Bank[ j ].Flags & FLAG_BATTERY_BACKED )
			{
				if( SaveBuffer( ROM_BANK_SIZE, memory.Bank[ j ].Data, file ) == false )
				{
					DBG_ERROR( "Error writing ROM data" );
					return false;
				}
			}
		}
	}

	for( auto &memory : m_GromMemory )
	{
		for( int j = 0; j < memory.NumBanks; j++ )
		{
			// If this bank is battery-backed RAM then update the .ram file
			if( memory.Bank[ j ].Flags & FLAG_BATTERY_BACKED )
			{
				if( SaveBuffer( GROM_BANK_SIZE, memory.Bank[ j ].Data, file ) == false )
				{
					DBG_ERROR( "Error writing GROM data" );
					return false;
				}
			}
		}
	}

	fclose( file );

	return true;
}

bool cCartridge::EncodeCallback( void *, size_t size, void *ptr )
{
	FUNCTION_ENTRY( nullptr, "cCartridge::EncodeCallback", false );

	*( size_t * ) ptr = size;

	return true;
}

bool cCartridge::DecodeCallback( void *, size_t, void * )
{
	FUNCTION_ENTRY( nullptr, "cCartridge::DecodeCallback", false );

	return true;
}

bool cCartridge::SaveBufferLZW( const void *buffer, size_t size, FILE *file )
{
	FUNCTION_ENTRY( nullptr, "cCartridge::SaveBufferLZW", false );

	DBG_ASSERT( size < 0x8000 );

	if( buffer == nullptr )
	{
		DBG_ERROR( "Internal error - buffer is NULL" );
		return false;
	}

	bool retVal = false;

	size_t outSize   = 0;
	UINT8 *outBuffer = new UINT8[ 2 * size ];

	cEncodeLZW encoder( 15 );

	encoder.SetWriteCallback( EncodeCallback, outBuffer, 2 * size, &outSize );

	if( encoder.EncodeBuffer( buffer, size ) != 1 )
	{
		DBG_ERROR( "Error compressing data" );
	}
	else
	{
		const void *ptr = outBuffer;
		// Make sure we didn't make things worse
		if( outSize > size )
		{
			outSize = 0x8000 | size;
			ptr = buffer;
		}
		WriteUINT16( outSize & 0xFFFF, file );
		if( fwrite( ptr, 1, outSize & 0x7FFF, file ) != ( outSize & 0x7FFF ))
		{
			DBG_ERROR( "Error writing to file" );
		}
		else
		{
			retVal = true;
		}
	}

	delete [] outBuffer;

	return retVal;
}

bool cCartridge::LoadBufferLZW( void *buffer, size_t size, FILE *file )
{
	FUNCTION_ENTRY( nullptr, "cCartridge::LoadBufferLZW", false );

	UINT16 inSize = ReadUINT16( file );

	bool retVal = false;

	if( inSize & 0x8000 )
	{
		if( fread( buffer, 1, inSize & 0x7FFF, file ) != ( inSize & 0x7FFF ))
		{
			DBG_ERROR( "Error reading from file" );
		}
		else
		{
			retVal = true;
		}
	}
	else
	{
		cDecodeLZW decoder( 15 );

		decoder.SetWriteCallback( DecodeCallback, buffer, size, nullptr );

		UINT8 *inBuffer = new UINT8[ inSize ];

		if( fread( inBuffer, 1, inSize, file ) != inSize )
		{
			DBG_ERROR( "Error reading from file" );
		}
		else
		{
			if( decoder.ParseBuffer( inBuffer, inSize ) != 1 )
			{
				DBG_ERROR( "Invalid LZW data" );
			}
			else
			{
				retVal = true;
			}
		}

		delete [] inBuffer;
	}

	return retVal;
}

bool cCartridge::LoadBufferRLE( void *, size_t, FILE * )
{
	FUNCTION_ENTRY( nullptr, "cCartridge::LoadBufferRLE", false );

	return true;
}

//----------------------------------------------------------------------------
//
// Version 0:
//
//   Offset Size  Contents
//     0000   80  Banner - "TI-99/4A Module - <NAME>"
//      0000   1  tag
//     [0001   2  CRU base] only if marked as DSR
//      0001   1  type
//      0002   1  # banks
//      0003   4  bank sizes
//      [0007 ##  RLE compressed data] (only if type == ROM)
//
//
// Version 1:
//
//   Offset Size  Contents
//     0000   80  Banner - "TI-99/4A Module - <NAME>"
//     0050    1  Version marker (0x10)
//     0051    2  CRU base
//      0000   1  index
//      0001   1  # banks
//       0000  1  bank 1 type
//      [0001 ##  RLE compressed data] (only if type == ROM)
//
//
// Version 2:
//
//   Offset Size  Contents
//     0000   80  Banner - "TI-99/4A Module - <NAME>"
//     0050    1  Version marker (0x20)
//
// Manufacturer
// Copyright/date
// Catalog Number
// Icon/Image
//
//     0051    2  CRU base
//      0000   1  index
//      0001   1  # banks
//       0000  1  bank 1 type
//      [0000  2  Size of data] (only if type == ROM)
//      [0002 ##  LZW compressed data] (only if type == ROM)
//
//----------------------------------------------------------------------------

bool cCartridge::LoadImageV0( FILE *file )
{
	FUNCTION_ENTRY( this, "cCartridge::LoadImageV0", true );

	UINT8 tag = ( UINT8 ) fgetc( file );

	while( !feof( file ))
	{
		bool dsr = ( tag & 0x40 ) ? true : false;
		UINT8 index = ( UINT8 ) ( tag & 0x3F );

		sMemoryRegion *memory = nullptr;
		UINT16 size = 0;

		if( index < GROM_0 )
		{
			memory = &m_CpuMemory[ index ];
			size   = ROM_BANK_SIZE;
		}
		else
		{
			index -= GROM_0;
			memory = &m_GromMemory[ index ];
			size   = GROM_BANK_SIZE;
		}

		if( dsr )
		{
			m_BaseCRU = ( UINT16 ) fgetc( file );
			m_BaseCRU = ( UINT16 ) ( m_BaseCRU | (( UINT8 ) fgetc( file ) << 8 ));
			DBG_EVENT( "  CRU Base: " << hex << m_BaseCRU );
		}

		BANK_TYPE_E type = ( BANK_TYPE_E ) ( fgetc( file ) + 1 );

		memory->NumBanks = ( UINT16 ) fgetc( file );
		UINT16 NumBytes[ 4 ];
		if( fread( NumBytes, 1, sizeof( NumBytes ), file ) != sizeof( NumBytes ))
		{
			DBG_ERROR( "Error reading from file" );
			return false;
		}

		DBG_EVENT( "  " << (( size != GROM_BANK_SIZE ) ? " RAM" : "GROM" ) << " @ " << hex << ( UINT16 ) ( index * size ));

		for( int i = 0; i < memory->NumBanks; i++ )
		{
			memory->Bank[ i ].Type = ( type != BANK_BATTERY_BACKED ) ? type : BANK_RAM;
			memory->Bank[ i ].Flags = ( type == BANK_BATTERY_BACKED ) ? FLAG_BATTERY_BACKED : 0;
			memory->Bank[ i ].Data = new UINT8[ size ];
			memset( memory->Bank[ i ].Data, 0, size );
			if( type == BANK_ROM )
			{
				memory->Bank[ i ].Flags |= FLAG_READ_ONLY;
				if( LoadBuffer( NumBytes[ i ], memory->Bank[ i ].Data, file ) == false )
				{
					DBG_ERROR( "Error reading data from file" );
					return false;
				}
			}
		}
		memory->CurBank = &memory->Bank[ 0 ];

		tag = ( UINT8 ) fgetc( file );
	}

	return true;
}

bool cCartridge::LoadImageV1( FILE *file )
{
	FUNCTION_ENTRY( this, "cCartridge::LoadImageV1", true );

	m_BaseCRU = ReadUINT16( file );

	UINT8 index = ( UINT8 ) fgetc( file );

	while( !feof( file ))
	{
		sMemoryRegion *memory = nullptr;
		UINT16 size = 0;

		if( index < GROM_0 )
		{
			memory = &m_CpuMemory[ index ];
			size   = ROM_BANK_SIZE;
		}
		else
		{
			index -= GROM_0;
			memory = &m_GromMemory[ index ];
			size   = GROM_BANK_SIZE;
		}

		memory->NumBanks = ( int ) fgetc( file );

		DBG_EVENT( "  " << (( size != GROM_BANK_SIZE ) ? " RAM" : "GROM" ) << " @ " << hex << ( UINT16 ) ( index * size ));

		for( int i = 0; i < memory->NumBanks; i++ )
		{
			BANK_TYPE_E type = ( BANK_TYPE_E ) fgetc( file );
			memory->Bank[ i ].Type = ( type != BANK_BATTERY_BACKED ) ? type : BANK_RAM;
			memory->Bank[ i ].Flags = ( type == BANK_BATTERY_BACKED ) ? FLAG_BATTERY_BACKED : 0;
			memory->Bank[ i ].Data = new UINT8[ size ];
			memset( memory->Bank[ i ].Data, 0, size );
			if( memory->Bank[ i ].Type == BANK_ROM )
			{
				memory->Bank[ i ].Flags |= FLAG_READ_ONLY;
				if( LoadBuffer( size, memory->Bank[ i ].Data, file ) == false )
				{
					DBG_ERROR( "Error reading data from file" );
					return false;
				}
			}
		}
		memory->CurBank = &memory->Bank[ 0 ];

		index = ( UINT8 ) fgetc( file );
	}

	return true;
}

bool cCartridge::LoadImageV2( FILE *file )
{
	FUNCTION_ENTRY( this, "cCartridge::LoadImageV2", true );

	m_BaseCRU = ReadUINT16( file );

	UINT8 index = ( UINT8 ) fgetc( file );

	while( !feof( file ))
	{
		sMemoryRegion *memory = nullptr;
		size_t size = 0;

		if( index < GROM_0 )
		{
			memory = &m_CpuMemory[ index ];
			size   = ROM_BANK_SIZE;
		}
		else
		{
			index -= GROM_0;
			memory = &m_GromMemory[ index ];
			size   = GROM_BANK_SIZE;
		}

		memory->NumBanks = PowerToBanks( fgetc( file ));

		for( int i = 0; i < memory->NumBanks; i++ )
		{
			BANK_TYPE_E type = ( BANK_TYPE_E ) fgetc( file );
			memory->Bank[ i ].Type = ( type != BANK_BATTERY_BACKED ) ? type : BANK_RAM;
			memory->Bank[ i ].Flags = ( type == BANK_BATTERY_BACKED ) ? FLAG_BATTERY_BACKED : 0;
			memory->Bank[ i ].Data = new UINT8[ size ];
			memset( memory->Bank[ i ].Data, 0, size );
			if( memory->Bank[ i ].Type == BANK_ROM )
			{
				memory->Bank[ i ].Flags |= FLAG_READ_ONLY;
				if( LoadBufferLZW( memory->Bank[ i ].Data, size, file ) == false )
				{
					return false;
				}
			}
		}

		memory->CurBank = &memory->Bank[ 0 ];

		DBG_EVENT( "  " << (( size != GROM_BANK_SIZE ) ?
		                    memory->CurBank->Type == BANK_ROM ? "ROM" : "RAM" :
		                    memory->CurBank->Type == BANK_ROM ? "GROM" : "GRAM" ) << " @ " << hex << ( UINT16 ) ( index * size ));

		index = ( UINT8 ) fgetc( file );
	}

	return true;
}
