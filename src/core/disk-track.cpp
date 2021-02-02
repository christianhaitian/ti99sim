//----------------------------------------------------------------------------
//
// File:		disk-track.cpp
// Date:		01-Jul-2015
// Programmer:	Marc Rousseau
//
// Description: A class to manipulate disk tracks
//
// Copyright (c) 2015 Marc Rousseau, All Rights Reserved.
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

#include "common.hpp"
#include "logger.hpp"
#include "disk-track.hpp"
#include "disk-sector.hpp"

DBG_REGISTER( __FILE__ );

extern UINT16 crctable[ 256 ];

UINT16 crctable[ 256 ];					// TODO

static UINT16 GenerateCRC( UINT16 data, UINT16 generator )
{
	UINT16 crc = 0;

	for( int bit = 0; bit < 8; bit++ )
	{
		if((( crc ^ data ) & 0x8000 ) == 0 )
		{
			crc = crc << 1;
		}
		else
		{
			crc = ( crc << 1 ) ^ generator;
		}
		data <<= 1;
	}

	return crc;
}

static bool Generate( UINT16 generator )
{
	for( int i = 0; i < 256; i++ )
	{
		crctable[ i ] = GenerateCRC(( UINT16 ) i << 8, generator );
	}

	return true;
}

static bool init = Generate( 0x1021 );

//----------------------------------------------------------------------------
// cDiskTrack
//----------------------------------------------------------------------------

cDiskTrack::cDiskTrack( ) :
	Dirty( true ),
	Format( track::Format::Unknown ),
	Clock( ),
	Data( ),
	Sector( )
{
}

bool cDiskTrack::HasChanged( ) const
{
	return Dirty;
}

void cDiskTrack::ClearChanged( )
{
	Dirty = false;
}

track::Format cDiskTrack::GetFormat( ) const
{
	return Format;
}

sDataBuffer cDiskTrack::Read( ) const
{
	return Data;
}

bool cDiskTrack::Write( track::Format newFormat, std::vector<UINT8> newData )
{
	FUNCTION_ENTRY( this, "cDiskTrack::Write", true );

	Format = newFormat;

	Clock.clear( );
	Data.clear( );
	Sector.clear( );
	Data.reserve( newData.size( ) + 36 * 2 );

	UINT16 crc = 0;

	auto writeByte = [&]( UINT8 byte )
	{
		Data.push_back( byte );
		crc = ( crc << 8 ) ^ crctable[ (( crc >> 8 ) ^ byte ) & 0xFF ];
	};

	for( size_t i = 0; i < newData.size( ); i++ )
	{
		auto byte = newData[ i ];

		if( byte >= 0xF5 )
		{
			if( byte == 0xF7 )
			{
				int crcHi = ( crc >> 8 ) & 0xFF;
				int crcLo = crc & 0xFF;
				writeByte( crcHi );
				writeByte( crcLo );
				continue;
			}

			if( Format == track::Format::FM )
			{
				switch( byte )
				{
					case 0xF5 :
					case 0xF6 :
						// Not allowed
						continue;
					case 0xF8 :
					case 0xF9 :
					case 0xFA :
					case 0xFB :
					case 0xFE :
						crc = 0xFFFF;
						// Fall through
					case 0xFC :
						Clock.push_back( Data.size( ));
						break;
					case 0xFD :
					case 0xFF :
						break;
					default :
						// Should never get here
						break;
				}
			}
			else
			{
				switch( byte )
				{
					case 0xF5 :
						Clock.push_back( Data.size( ));
						writeByte( 0xA1 );
						crc = 0xCDB4;
						continue;
					case 0xF6 :
						Clock.push_back( Data.size( ));
						byte = 0xC2;
						break;
					default :
						break;
				}
			}
		}

		writeByte( byte );
	}

	LocateSectors( );

	Dirty = true;

	return true;
}

bool cDiskTrack::RawWrite( track::Format newFormat, std::vector<size_t> newClock, std::vector<UINT8> newData )
{
	FUNCTION_ENTRY( this, "cDiskTrack::RawWrite", true );

	Format = newFormat;

	Clock = newClock;
	Data  = newData;

	LocateSectors( );

	Dirty = true;

	return true;
}

void cDiskTrack::Erase( )
{
	if( ! IsEmpty( ))
	{
		Format = track::Format::Unknown;
		Clock.clear( );
		Data.clear( );
		Sector.clear( );

		Dirty = true;
	}
}

bool cDiskTrack::IsEmpty( ) const
{
	return Data.empty( );
}

bool cDiskTrack::VerifyID( const UINT8 *id ) const
{
	FUNCTION_ENTRY( this, "cDiskTrack::VerifyID", true );

	UINT16 crc = ( Format == track::Format::FM ) ? 0xFFFF : 0xCDB4;
	const UINT8 *ptr = id;

	auto readByte = [&]( )
	{
		UINT8 byte = *ptr++;
		crc = ( crc << 8 ) ^ crctable[ (( crc >> 8 ) ^ byte ) & 0xFF ];
		return byte;
	};

	readByte( );
	for( int i = 4; i > 0; i-- )
	{
		readByte( );
	}

	return crc == (( ptr[ 0 ] << 8 ) | ptr[ 1 ] );
}

bool cDiskTrack::VerifyData( const UINT8 *data, size_t size ) const
{
	FUNCTION_ENTRY( this, "cDiskTrack::VerifyData", true );

	UINT16 crc = ( Format == track::Format::FM ) ? 0xFFFF : 0xCDB4;
	const UINT8 *ptr = data;

	auto readByte = [&]( )
	{
		UINT8 byte = *ptr++;
		crc = ( crc << 8 ) ^ crctable[ (( crc >> 8 ) ^ byte ) & 0xFF ];
		return byte;
	};

	readByte( );
	for( size_t i = 0; i < size; i++ )
	{
		readByte( );
	}

	return crc == (( ptr[ 0 ] << 8 ) | ptr[ 1 ] );
}

void cDiskTrack::DataModified( const UINT8 *data, size_t size )
{
	FUNCTION_ENTRY( this, "cDiskTrack::DataModified", true );

	Dirty = true;

	UINT16 crc = ( Format == track::Format::FM ) ? 0xFFFF : 0xCDB4;
	const UINT8 *ptr = data;

	auto readByte = [&]( )
	{
		UINT8 byte = *ptr++;
		crc = ( crc << 8 ) ^ crctable[ (( crc >> 8 ) ^ byte ) & 0xFF ];
		return byte;
	};

	readByte( );

	for( size_t i = 0; i < size; i++ )
	{
		readByte( );
	}

	UINT8 *updatePtr = const_cast<UINT8 *>( ptr );

	*updatePtr++ = ( crc >> 8 );
	*updatePtr++ = crc & 0xFF;
}

void cDiskTrack::LocateSectors( )
{
	FUNCTION_ENTRY( this, "cDiskTrack::LocateSectors", true );

	auto FindAddressMark = [&]( size_t index )
	{
		UINT8 byte = Data[ index ];

		// Return the next byte for MFM clock bytes
		if(( byte == 0xA1 ) || ( byte == 0xC2 ))
		{
			if( index + 1 < Data.size( ))
			{
				index = index + 1;
			}
		}

		return &Data[ index ];
	};

	UINT8 *lastID = nullptr;

	Sector.clear( );

	for( size_t i = 0; i < Clock.size( ); i++ )
	{
		UINT8 *mark = FindAddressMark( Clock[ i ] );

		switch( *mark )
		{
			case 0xF8 :
			case 0xF9 :
			case 0xFA :
			case 0xFB :
				if(( mark - lastID ) < (( Format == track::Format::FM ) ? 33 : 45 )) // TODO - ??? 30 : 43 ))
				{
					Sector.push_back( { this, lastID, mark } );
				}
				break;
			case 0xFE :
				lastID = mark;
				break;
			default :
				break;
		}
	}
}

std::vector<size_t> cDiskTrack::GetClockLocations( ) const
{
	return Clock;
}

std::vector<iDiskSector*> cDiskTrack::GetSectors( ) const
{
	std::vector<iDiskSector*> sectors;

	for( auto &sector : Sector )
	{
		sectors.push_back( const_cast<cDiskSector*>( &sector ));
	}

	return sectors;
}

iDiskSector *cDiskTrack::GetSector( int logicalCylinder, int logicalHead, int logicalSector )
{
	FUNCTION_ENTRY( this, "cDiskTrack::GetSector", true );

	for( auto &sector : Sector )
	{
		if( sector.Matches( logicalCylinder, logicalHead, logicalSector ))
		{
			return &sector;
		}
	}

	return nullptr;
}

const iDiskSector *cDiskTrack::GetSector( int logicalCylinder, int logicalHead, int logicalSector ) const
{
	FUNCTION_ENTRY( this, "cDiskTrack::GetSector", true );

	for( auto &sector : Sector )
	{
		if( sector.Matches( logicalCylinder, logicalHead, logicalSector ))
		{
			return &sector;
		}
	}

	return nullptr;
}
