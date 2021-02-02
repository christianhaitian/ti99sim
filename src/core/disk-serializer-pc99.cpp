//----------------------------------------------------------------------------
//
// File:        disk-serializer-pc99.cpp
// Date:        06-July-2015
// Programmer:  Marc Rousseau
//
// Description: A class to load/save PC99 disk images
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

#include <algorithm>
#include <cstring>
#include <set>
#include "common.hpp"
#include "logger.hpp"
#include "disk-media.hpp"
#include "disk-serializer-pc99.hpp"
#include "idisk-track.hpp"

DBG_REGISTER( __FILE__ );

static void ZapCRC( sDataBuffer &buffer, std::vector<size_t> clocks, track::Format format )
{
	size_t offset = ( format == track::Format::MFM ) ? 1 : 0;

	size_t lastSize = 0;

	// Revert CRCs back to F7F7
	for( auto clock : clocks )
	{
		if( clock + offset < buffer.size( ))
		{
			UINT8 *ptr = &buffer[ clock + offset ];
			switch( *ptr )
			{
				case MARK_IDAM :
					if( clock + offset + 4 < buffer.size( ))
					{
						lastSize = 128u << ( ptr[ 4 ] & 0x03 );
						ptr[ 5 ] = 0xF7;
						ptr[ 6 ] = 0xF7;
					}
					break;
				case MARK_DDAM :
				case MARK_DAMx :
				case MARK_DAMy :
				case MARK_DAM :
					if( clock + offset + lastSize + 2 < buffer.size( ))
					{
						ptr[ lastSize + 1 ] = 0xF7;
						ptr[ lastSize + 2 ] = 0xF7;
					}
					break;
				default:
					break;
			}
		}
	}
}

cDiskSerializerPC99::cDiskSerializerPC99( ) :
	cBaseObject( "cDiskSerializerPC99" )
{
}

cDiskSerializerPC99::~cDiskSerializerPC99( )
{
}

bool cDiskSerializerPC99::MatchesFormat( FILE *file )
{
	FUNCTION_ENTRY( nullptr, "cDiskSerializerPC99::MatchesFormat", true );

	fseek( file, 0L, SEEK_SET );

	UINT8 buffer[ 64 ];

	if( fread( buffer, sizeof( buffer ), 1, file ) != 1 )
	{
		return false;
	}

	track::Format format = track::Format::Unknown;

	// Look at the index GAP character (should be 0xFF or 0x4E but PC99 uses 0x00 for FM disks)
	switch( buffer[ 0 ] )
	{
		case 0x00 :
		case 0xFF :
			format = track::Format::FM;
			break;
		case 0x4E :
			format = track::Format::MFM;
			break;
		default :
			return false;
	}

	// See if we can find a proper address mark
	if( FindAddressMark( 0xFC, 0xFC, format, buffer, buffer + 64 ) == nullptr )
	{
		return false;
	}

	return true;
}

bool cDiskSerializerPC99::SupportsFeatures( const cDiskImage &image )
{
	FUNCTION_ENTRY( this, "cDiskSerializerPC99::SupportsFeatures", true );

	// TODO

	return true;
}

eDiskFormat cDiskSerializerPC99::GetFormat( ) const
{
	FUNCTION_ENTRY( this, "cDiskSerializerPC99::GetFormat", true );

	return FORMAT_RAW_TRACK;
}

//----------------------------------------------------------------------------
//
// Read disk files that contain raw track data.  This is the format used by
// PC99.  This code attempts to correctly read disk images that may contain
// unusual formatting.
//
//----------------------------------------------------------------------------

bool cDiskSerializerPC99::ReadFile( FILE *file, cDiskImage *image )
{
	FUNCTION_ENTRY( this, "cDiskSerializerPC99::ReadFile", true );

	UINT8 buffer[ 64 ];

	if( fread( buffer, sizeof( buffer ), 1, file ) != 1 )
	{
		DBG_ERROR( "Error reading from file" );
		return false;
	}

	// Look at the index GAP character (should be 0xFF or 0x4E but PC99 uses 0x00 for FM disks)
	track::Format format = ( buffer[ 0 ] != 0x4E ) ? track::Format::FM : track::Format::MFM;

	size_t maxTrackSize = 120 * (( format == track::Format::FM ) ? TRACK_SIZE_FM : TRACK_SIZE_MFM ) / 100;

	if( FindAddressMark( 0xFC, 0xFC, format, buffer, buffer + 64 ) == nullptr )
	{
		return false;
	}

	auto trackData = ReadTrackData( file, format, maxTrackSize );

	size_t maxTrack = 0;
	size_t maxHeads = 0;

	std::tie( maxTrack, maxHeads ) = DetermineSize( trackData, format );

	// Clear any old data & prepare for a new image
	image->AllocateTracks( maxTrack + 1, maxHeads + 1 );

	size_t index = 0;
	for( size_t h = 0; h <= maxHeads; h++ )
	{
		for( size_t t = 0; t <= maxTrack; t++ )
		{
			if(( index < trackData.size( )) && ( trackData[ index ].data.size( ) > 0 ))
			{
				auto track = dynamic_cast<cDiskTrack*>( image->GetTrack( t, h ));

				track->RawWrite( format, trackData[ index ].clock, trackData[ index ].data );

				// Update the CRC values read in (PC99 stores them as F7F7)
				for( auto &sector : track->GetSectors( ))
				{
					track->DataModified( sector->GetID( ) - 1, 4 );
					track->DataModified( sector->GetData( ) - 1, sector->Size( ));
				}

				index++;
			}
		}
	}

	DBG_EVENT( "Disk loaded" );

	return true;
}

bool cDiskSerializerPC99::WriteFile( const cDiskImage &image, FILE *file )
{
	FUNCTION_ENTRY( this, "cDiskSerializerPC99::WriteFile", true );

	for( size_t h = 0; h < image.GetNumHeads( ); h++ )
	{
		for( size_t t = 0; t < image.GetNumTracks( ); t++ )
		{
			auto track = image.GetTrack( t, h );

			auto buffer = track->Read( );

			if( !buffer.empty( ))
			{
				ZapCRC( buffer, track->GetClockLocations( ), track->GetFormat( ));

				if( fwrite( buffer.data( ), buffer.size( ), 1, file ) != 1 )
				{
					DBG_ERROR( "Error writing to file" );
					return false;
				}
			}
		}
	}

	return true;
}

void cDiskSerializerPC99::AddClockLocations( sTrackInfo &track, track::Format format, size_t offset )
{
	FUNCTION_ENTRY( nullptr, "cDiskSerializerPC99::AddClockLocations", false );

	if( format == track::Format::MFM )
	{
		for( int i = 3; i > 0; i-- )
		{
			track.clock.push_back( static_cast<size_t>( offset - i ));
		}
	}
	else
	{
		track.clock.push_back( static_cast<size_t>( offset ));
	}
}

const UINT8 *cDiskSerializerPC99::FindAddressMark( UINT8 mask, UINT8 mark, track::Format format, const UINT8 *ptr, const UINT8 *max )
{
	FUNCTION_ENTRY( nullptr, "cDiskSerializerPC99::FindAddressMark", false );

restart:

	if( ptr >= max )
	{
		return nullptr;
	}

	// Find start of sync bytes
	while( *ptr != '\x00' )
	{
		if( ++ptr >= max )
		{
			return nullptr;
		}
	}

	while( *ptr == '\x00' )
	{
		if( ++ptr >= max )
		{
			return nullptr;
		}
	}

	// Look for clock pattern for MFM tracks
	if( format == track::Format::MFM )
	{
		for( int i = 0; i < 3; i++ )
		{
			if( *ptr != 0xA1 )
			{
				goto restart;
			}
			if( ++ptr >= max )
			{
				return nullptr;
			}
		}
	}

	if(( *ptr++ & mask ) != mark )
	{
		goto restart;
	}

	return ( ptr < max ) ? ptr : nullptr;
}

cDiskSerializerPC99::sTrackInfo cDiskSerializerPC99::FindTrack( track::Format format, const UINT8 *start, const UINT8 *max )
{
	FUNCTION_ENTRY( nullptr, "cDiskSerializerPC99::FindTrack", true );

	std::set<UINT8> sectors;

	sTrackInfo track;

	const UINT8 *ptr = start;

	// The track should start with either an Index (0xFC) or ID (0xFE) Address Mark
	const UINT8 *markID = FindAddressMark( 0xFC, 0xFC, format, ptr, max );

	if( markID != nullptr )
	{
		// Remember the number of bytes from the beginning of the track to the 1st Address Mark
		int indexGapSize = markID - ptr - 1;

		DBG_ASSERT( indexGapSize > 0 );

		// Handle an Index Address Mark
		if( markID[ -1 ] == MARK_IAM )
		{
			AddClockLocations( track, format, static_cast<size_t>( markID - start - 1 ));

			ptr = markID;
		}

		while( ptr < max )
		{
			markID = FindAddressMark( 0xFC, 0xFC, format, ptr, max );

			if( markID == nullptr )
			{
				break;
			}

			int gapSize = markID - ptr - 1;
			UINT8 sectorId = markID[ 2 ];

			DBG_ASSERT( gapSize > 0 );

			// See if we have passed the EOT filler (or already seen this sector)
			if(( gapSize > (( format == track::Format::FM ) ? 220 : 736 )) || sectors.count( sectorId ))
			{
				ptr = markID - indexGapSize - 1;
				break;
			}

			if( markID[ -1 ] != MARK_IDAM )
			{
				break;
			}

			sectors.insert( sectorId );

			// NOTE: WD1771 controllers can use 0xF8 0xF9 0xFA or 0xFB
			// NOTE: PC controllers cannot detect 0xF9 or 0xFA marks
			const UINT8 *markData = FindAddressMark( 0xFC, 0xF8, format, markID + 4 + 2, max );

			if( markData == nullptr )
			{
				break;
			}

			int dataSize = 128 << markID[ 3 ];

			if(( dataSize <= 0 ) || ( markData + dataSize + 2 >= max ))
			{
				break;
			}

			AddClockLocations( track, format, static_cast<size_t>( markID - start - 1 ));
			AddClockLocations( track, format, static_cast<size_t>( markData - start - 1 ));

			ptr = markData + dataSize + 2;
		}
	}

	track.data = { start, ptr };

	return track;
}

std::pair<size_t, size_t> cDiskSerializerPC99::DetermineSize( const std::vector<cDiskSerializerPC99::sTrackInfo> &trackData, track::Format format )
{
	size_t maxTrack = 0;
	size_t maxHeads = 0;

	cDiskTrack track;

	for( auto &data : trackData )
	{
		track.RawWrite( format, data.clock, data.data );

		if( auto sector = track.GetSector( -1, -1, -1 ))
		{
			maxTrack = std::max( maxHeads, static_cast<size_t> ( sector->LogicalCylinder( )));
			maxHeads = std::max( maxHeads, static_cast<size_t> ( sector->LogicalHead( )));
		}
	}

	return std::pair<size_t, size_t>( maxTrack, maxHeads );
}

std::vector<cDiskSerializerPC99::sTrackInfo> cDiskSerializerPC99::ReadTrackData( FILE *file, track::Format format, size_t maxTrackSize )
{
	FUNCTION_ENTRY( nullptr, "cDiskSerializerPC99::ReadTrackData", true );

	fseek( file, 0, SEEK_SET );

	std::vector<sTrackInfo> trackData;

	size_t count = 0;

	UINT8 trackBuffer[ 2 * MAX_TRACK_SIZE ];

	size_t read = fread( trackBuffer + count, 1, sizeof( trackBuffer ) - count, file );

	while( read != 0 )
	{
		count += read;

		// trackBuffer should now contain enough data for at least 1 track
		while( count >= maxTrackSize )
		{
			auto track = FindTrack( format, trackBuffer, trackBuffer + count );
			if( track.data.empty( ))
			{
				break;
			}
			trackData.push_back( track );

			DBG_ASSERT( count > track.data.size( ));

			count -= track.data.size( );

			memmove( trackBuffer, trackBuffer + track.data.size( ), count );
		}

		read = fread( trackBuffer + count, 1, sizeof( trackBuffer ) - count, file );
	}

	if( count > 0 )
	{
		auto track = FindTrack( format, trackBuffer, trackBuffer + count );
		if( !track.data.empty( ))
		{
			trackData.push_back( track );

			DBG_ASSERT( count > track.data.size( ));

			count -= track.data.size( );

			memmove( trackBuffer, trackBuffer + track.data.size( ), count );
		}
	}

	if(( count > 0 ) && ( trackData.empty( ) == false ))
	{
		cDiskSerializerPC99::sTrackInfo &lastTrack = *trackData.rbegin( );

		lastTrack.data.insert( lastTrack.data.end( ), trackBuffer, trackBuffer + count );
	}

	return trackData;
}
