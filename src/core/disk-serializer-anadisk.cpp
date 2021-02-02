//----------------------------------------------------------------------------
//
// File:        disk-serializer-anadisk.cpp
// Date:        04-May-2015
// Programmer:  Marc Rousseau
//
// Description: A class to load/save AnaDisk disk images
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
#include "common.hpp"
#include "logger.hpp"
#include "disk-media.hpp"
#include "disk-serializer-anadisk.hpp"

DBG_REGISTER( __FILE__ );

//----------------------------------------------------------------------------
//	   +------+------+------+------+------+------+----------+
//	   | ACYL | ASID | LCYL | LSID | LSEC | LLEN | COUNT    |
//	   +------+------+------+------+------+------+----------+
//
//		ACYL	Actual cylinder, 1 byte
//		ASID	Actual side, 1 byte
//		LCYL	Logical cylinder; cylinder as read, 1 byte
//		LSID	Logical side; or side as read, 1 byte
//		LSEC	Sector number as read, 1 byte
//		LLEN	Length code as read, 1 byte
//		COUNT	Byte count of data to follow, 2 bytes.  If zero,
//				no data is contained in this sector.
//
// All sectors occurring on a side will be grouped together;
// however, they will appear in the same order as they occurred on
// the diskette.  Therefore, if an 8 sector-per-track diskette were
// scanned which had a physical interleave of 2:1, the sectors might
// appear in the order 1,5,2,6,3,7,4,8 in the DOS dump file.
//
//----------------------------------------------------------------------------

struct sAnadiskHeader
{
	UINT8          ActualCylinder;
	UINT8          ActualSide;
	UINT8          LogicalCylinder;
	UINT8          LogicalSide;
	UINT8          LogicalSector;
	UINT8          Length;
	UINT16         DataCount;
};

static inline void SetUINT16_LE( void *_ptr, UINT16 value )
{
	FUNCTION_ENTRY( nullptr, "SetUINT16", true );

	UINT8 *ptr = ( UINT8 * ) _ptr;

	ptr[ 0 ] = value & 0x0FF;
	ptr[ 1 ] = value >> 8;
}

static inline UINT16 GetUINT16_LE( const void *_ptr )
{
	FUNCTION_ENTRY( nullptr, "GetUINT16_LE", true );

	const UINT8 *ptr = ( const UINT8 * ) _ptr;

	return ( UINT16 ) (( ptr[ 1 ] << 8 ) | ptr[ 0 ] );
}

struct sTrackInfo
{
	int         noSectors;
	int         totalSize;
	sSectorInfo info[ 18 ];
	sDataBuffer data[ 18 ];
};


cDiskSerializerAnaDisk::cDiskSerializerAnaDisk( ) :
	cBaseObject( "cDiskSerializerAnaDisk" )
{
}

cDiskSerializerAnaDisk::~cDiskSerializerAnaDisk( )
{
}

bool cDiskSerializerAnaDisk::MatchesFormat( FILE *file )
{
	FUNCTION_ENTRY( nullptr, "cDiskSerializerAnaDisk::MatchesFormat", true );

	fseek( file, 0L, SEEK_SET );

	int noSectors = 0;
	int noHeads   = -1;
	int noTracks  = -1;

	for( EVER )
	{
		sAnadiskHeader header;
		int read = fread( &header, 1, sizeof( sAnadiskHeader ), file );
		if( read != sizeof( sAnadiskHeader ))
		{
			return (( read == 0 ) && ( feof( file ))) ? true : false;
		}

		if(( header.ActualSide >= 2 ) || ( header.ActualCylinder >= MAX_TRACKS ))
		{
			break;
		}

		noHeads  = std::max( noHeads, ( int ) header.ActualSide );
		noTracks = std::max( noTracks, ( int ) header.ActualCylinder );

		// Make sure we handle this properly on big-endian machines
		size_t size = GetUINT16_LE( &header.DataCount );

		char buffer[ 4096 ];
		if(( size > sizeof( buffer )) || ( fread( buffer, size, 1, file ) != 1 ))
		{
			break;
		}

		if( size == DEFAULT_SECTOR_SIZE )
		{
			noSectors += 1;
		}
	}

	return ( noTracks >= 34 ) && ( noSectors > 0 );
}

bool cDiskSerializerAnaDisk::SupportsFeatures( const cDiskImage &image )
{
	FUNCTION_ENTRY( this, "cDiskSerializerAnaDisk::SupportsFeatures", true );

	return true;
}

eDiskFormat cDiskSerializerAnaDisk::GetFormat( ) const
{
	FUNCTION_ENTRY( this, "cDiskSerializerAnaDisk::GetFormat", true );

	return FORMAT_ANADISK;
}

//----------------------------------------------------------------------------
//
// This routine is designed to read disk files created by AnaDisk that contain
// an eight byte header before each sector.  Disks using this format may have
// irregular sized sectors, 'incorrect' sector numbering schemes, etc.  Using
// this format, it is possible to handle 'copy-protected' disks that utilize
// non-standard disk formatting properties.
//
//----------------------------------------------------------------------------

bool cDiskSerializerAnaDisk::ReadFile( FILE *file, cDiskImage *image )
{
	FUNCTION_ENTRY( this, "cDiskSerializerAnaDisk::ReadFile", true );

	// Make a 1st pass to see what the disk looks like
	fseek( file, 0L, SEEK_SET );

	size_t noHeads  = 0;
	size_t noTracks = 0;

	sTrackInfo info[ 2 ][ MAX_TRACKS ];
	memset( info, 0, sizeof( info ));

	for( EVER )
	{
		sAnadiskHeader header;
		if( fread( &header, sizeof( sAnadiskHeader ), 1, file ) != 1 )
		{
			break;
		}

		if(( header.ActualSide >= 2 ) || ( header.ActualCylinder >= MAX_TRACKS ))
		{
			DBG_WARNING( "Bad header at offset " << hex << static_cast<size_t>( ftell( file )) - sizeof( sAnadiskHeader ));
			if(( noHeads == 0 ) || ( noTracks == 0 ))
			{
				return false;
			}
			break;
		}

		if( header.ActualSide > noHeads )
		{
			noHeads = header.ActualSide;
		}
		if( header.ActualCylinder > noTracks )
		{
			noTracks = header.ActualCylinder;
		}

		// Make sure we handle this properly on big-endian machines
		UINT16 size = GetUINT16_LE( &header.DataCount );

		// Save the information we need to construct the disk image
		sTrackInfo *track = &info[ header.ActualSide ][ header.ActualCylinder ];

		sSectorInfo *sector = &track->info[ track->noSectors ];
		sector->LogicalCylinder = header.LogicalCylinder;
		sector->LogicalHead     = header.LogicalSide;
		sector->LogicalSector   = header.LogicalSector;
		sector->Size            = header.Length;
		sector->DataMark        = 0xFB;

		if( size > 0 )
		{
			track->data[ track->noSectors ].resize( size );
			if( fread( &track->data[ track->noSectors ][ 0 ], size, 1, file ) != 1 )
			{
				DBG_ERROR( "Unable to read sector data" );
				return false;
			}
		}

		track->noSectors++;
		track->totalSize += size;
	}

	// Clear any old data & prepare for a new image
	image->AllocateTracks((( noTracks + 1 ) > MAX_TRACKS_LO ) ? MAX_TRACKS_HI : MAX_TRACKS_LO, noHeads + 1 );

	// Now, copy the data we read to the proper place on the disk
	for( size_t h = 0; h <= noHeads; h++ )
	{
		for( size_t t = 0; t <= noTracks; t++ )
		{
			if( info[ h ][ t ].noSectors > 0 )
			{
				sTrackInfo *tinfo = &info[ h ][ t ];

				track::Format format = ( tinfo->totalSize > TRACK_SIZE_FM ) ? track::Format::MFM : track::Format::FM;

				const std::vector<sSectorInfo> sectorInfo{ tinfo->info, tinfo->info + tinfo->noSectors };

				iDiskTrack *track = image->GetTrack( t, h );

				auto trackImage = cDiskImage::FormatTrack( format, sectorInfo );

				track->Write( format, trackImage );

				for( int s = 0; s < tinfo->noSectors; s++ )
				{
					iDiskSector *sector = track->GetSector( tinfo->info[ s ].LogicalCylinder, tinfo->info[ s ].LogicalHead, tinfo->info[ s ].LogicalSector );
					sector->Write( tinfo->info[ s ].DataMark, tinfo->data[ s ] );
				}
			}
		}
	}

	DBG_EVENT( "Disk loaded" );

	return true;
}

bool cDiskSerializerAnaDisk::WriteFile( const cDiskImage &image, FILE *file )
{
	FUNCTION_ENTRY( this, "cDiskSerializerAnaDisk::WriteFile", true );

	for( size_t h = 0; h < image.GetNumHeads( ); h++ )
	{
		for( size_t t = 0; t < image.GetNumTracks( ); t++ )
		{
			sAnadiskHeader header;
			header.ActualCylinder  = ( UINT8 ) t;
			header.ActualSide      = ( UINT8 ) h;

			for( auto &sector : image.GetTrack( t, h )->GetSectors( ))
			{
				header.LogicalCylinder = ( UINT8 ) sector->LogicalCylinder( );
				header.LogicalSide     = ( UINT8 ) sector->LogicalHead( );
				header.LogicalSector   = ( UINT8 ) sector->LogicalSector( );
				header.Length          = ( UINT8 ) sector->LogicalSize( );

				// Encode any non-standard Data Address Marks
				header.Length |= ( UINT8 ) (( 0xFB - sector->DataMark( )) << 6 );

				SetUINT16_LE( &header.DataCount, sector->Size( ));

				if( fwrite( &header, sizeof( header ), 1, file ) != 1 )
				{
					DBG_ERROR( "Error writing to file" );
					return false;
				}

				if( sector->GetData( ) != nullptr )
				{
					if( fwrite( sector->GetData( ), sector->Size( ), 1, file ) != 1 )
					{
						DBG_ERROR( "Error writing to file" );
						return false;
					}

				}
			}
		}
	}

	return true;
}
