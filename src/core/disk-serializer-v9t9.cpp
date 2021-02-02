//----------------------------------------------------------------------------
//
// File:        disk-serializer-v9t9.cpp
// Date:        04-May-2015
// Programmer:  Marc Rousseau
//
// Description: A class to load/save v9t9 disk images
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

#include <cstring>
#include "common.hpp"
#include "logger.hpp"
#include "disk-media.hpp"
#include "disk-serializer-v9t9.hpp"
#include "idisk-track.hpp"
#include "file-system-disk.hpp"

DBG_REGISTER( __FILE__ );

cDiskSerializerV9T9::cDiskSerializerV9T9( ) :
	cBaseObject( "cDiskSerializerV9T9" )
{
}

cDiskSerializerV9T9::~cDiskSerializerV9T9( )
{
}

constexpr int DiskSize( int tracks, int sides, int sectors )
{
	return tracks * sides * sectors * DEFAULT_SECTOR_SIZE;
}

bool cDiskSerializerV9T9::MatchesFormat( FILE *file )
{
	FUNCTION_ENTRY( nullptr, "cDiskSerializerV9T9::MatchesFormat", true );

	fseek( file, 0L, SEEK_END );

	switch( ftell( file ))
	{
		case DiskSize( 35, 1,  9 ) :
		case DiskSize( 40, 1,  9 ) :	//   90K : SSSD
		case DiskSize( 40, 2,  9 ) :	//  180K : DSSD (or 18-sector-per-track SSDD)
		case DiskSize( 40, 1, 16 ) :	//  160K : SSDD
		case DiskSize( 40, 2, 16 ) :	//  320K : 16-sector-per-track DSDD
		case DiskSize( 40, 2, 18 ) :	//  360K : DSDD
		case DiskSize( 40, 2, 20 ) :	//  400K : CF7+
		case DiskSize( 80, 2, 16 ) :	//  640K : 16-sector-per-track 80-track DSDD (Myarc only)
		case DiskSize( 80, 2, 18 ) :	//  720K : 18-sector-per-track 80-track DSDD (Myarc only)
		case DiskSize( 80, 2, 36 ) :	// 1.44M : DSHD (Myarc only)
			fseek( file, 13, SEEK_SET );
			if( fgetc( file ) != 'D' ) return false;
			if( fgetc( file ) != 'S' ) return false;
			if( fgetc( file ) != 'K' ) return false;
			return true;
		default :
			break;
	}

	return false;
}

bool cDiskSerializerV9T9::SupportsFeatures( const cDiskImage &image )
{
	FUNCTION_ENTRY( this, "cDiskSerializerV9T9::SupportsFeatures", true );

	// Look for features that will get lost/destroyed if saved in raw sector format
	for( size_t h = 0; h < image.GetNumHeads( ); h++ )
	{
		for( size_t t = 0; t < image.GetNumTracks( ); t++ )
		{
			size_t c = ( h == 0 ) ? t : image.GetNumTracks( ) - ( t + 1 );

			const iDiskTrack *track = image.GetTrack( c, h );

			size_t sectors = 0;

			for( size_t s = 0; s < MAX_SECTORS; s++ )
			{
				const iDiskSector *sector = track->GetSector( c, h, s );
				if( sector == nullptr )
				{
					break;
				}
				sectors++;
				// Look for non-standard data address marks
				if(( sector->LogicalSize( ) != 1 ) || ( sector->DataMark( ) != MARK_DAM ))
				{
					return false;
				}
			}

			// Look for raw track without valid index or data address marks or extra/missing sectors
			if(( sectors == 0 ) && ( sectors != track->GetSectors( ).size( )))
			{
				return false;
			}
		}
	}

	return true;
}

eDiskFormat cDiskSerializerV9T9::GetFormat( ) const
{
	FUNCTION_ENTRY( this, "cDiskSerializerV9T9::GetFormat", true );

	return FORMAT_RAW_SECTOR;
}

//----------------------------------------------------------------------------
//
// Read disk files that contain raw sector data.  This is the format used by
// v9t9.  This code assumes that these files are 'well behaved'.  This means
// that the assumption is made that they are standard disks with no irregular
// properties (i.e. it contains 9/16/18 256 byte sectors per track).
//
//----------------------------------------------------------------------------

bool cDiskSerializerV9T9::ReadFile( FILE *file, cDiskImage *image )
{
	FUNCTION_ENTRY( this, "cDiskSerializerV9T9::ReadFile", true );

	fseek( file, 0L, SEEK_END );
	UINT32 size = ftell( file );
	fseek( file, 0L, SEEK_SET );

	size_t totalSectors = size / DEFAULT_SECTOR_SIZE;

	char buffer[ DEFAULT_SECTOR_SIZE ];

	// Read the 1st sector from the disk to get the VIB
	if( fread( buffer, sizeof( buffer ), 1, file ) != 1 )
	{
		DBG_ERROR( "Error reading from file" );
		return false;
	}

	VIB *vib = reinterpret_cast<VIB *>( buffer );

	size_t noSectors = ( vib->SectorsPerTrack != 0 ) ? vib->SectorsPerTrack : 9;
	size_t noTracks  = totalSectors / noSectors;

	// Some TI disks don't have accurate # sides & format
	size_t noSides       = ( noTracks > MAX_TRACKS_LO ) ? 2 : 1;
	track::Format format = ( noSectors == 9 ) ? track::Format::FM : track::Format::MFM;

	// Correct the number of tracks
	noTracks /= noSides;

	// Clear any old data & prepare for a new image
	image->FormatDisk( noTracks, noSides, noSectors, format );

	fseek( file, 0L, SEEK_SET );

	for( size_t h = 0; h < noSides; h++ )
	{
		for( size_t t = 0; t < noTracks; t++ )
		{
			size_t c = ( h == 0 ) ? t : noTracks - ( t + 1 );

			iDiskTrack *track = image->GetTrack( c, h );

			for( size_t s = 0; s < noSectors; s++ )
			{
				if( fread( buffer, sizeof( buffer ), 1, file ) != 1 )
				{
					DBG_ERROR( "Error reading from file" );
					return false;
				}

				iDiskSector *sector = track->GetSector( c, h, s );

				sector->Write( { buffer, buffer + DEFAULT_SECTOR_SIZE } );
			}
		}
	}

	DBG_EVENT( "Disk loaded" );

	return true;
}

bool cDiskSerializerV9T9::WriteFile( const cDiskImage &image, FILE *file )
{
	FUNCTION_ENTRY( this, "cDiskSerializerV9T9::WriteFile", true );

	for( size_t h = 0; h < image.GetNumHeads( ); h++ )
	{
		for( size_t t = 0; t < image.GetNumTracks( ); t++ )
		{
			size_t c = ( h == 0 ) ? t : image.GetNumTracks( ) - ( t + 1 );

			auto track = image.GetTrack( c, h );

			for( size_t s = 0; s < MAX_SECTORS; s++ )
			{
				const iSector *sector = track->GetSector( -1, -1, s );
				if( sector == nullptr )
				{
					continue;
				}
				if( sector->GetData( ) == nullptr )
				{
					break;
				}
					if( fwrite( sector->GetData( ), sector->Size( ), 1, file ) != 1 )
				{
					DBG_ERROR( "Error writing to file" );
					return false;
				}
			}
		}
	}

	return true;
}
