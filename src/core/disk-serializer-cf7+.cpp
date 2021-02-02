//----------------------------------------------------------------------------
//
// File:        disk-serializer-cf7.cpp
// Date:        04-May-2015
// Programmer:  Marc Rousseau
//
// Description: A class to load/save CF7+ disk images
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
#include <memory.h>
#include "common.hpp"
#include "logger.hpp"
#include "disk-media.hpp"
#include "disk-serializer-cf7+.hpp"
#include "file-system-disk.hpp"
#include "support.hpp"

DBG_REGISTER( __FILE__ );

constexpr size_t CF7_SECTOR_COUNT = 1600;
constexpr size_t CF7_DISK_SIZE    = CF7_SECTOR_COUNT * 512;

#define DENSITY_DOUBLE 2

static inline void SetUINT16( void *_ptr, UINT16 value )
{
	FUNCTION_ENTRY( nullptr, "SetUINT16", true );

	UINT8 *ptr = ( UINT8 * ) _ptr;

	ptr[ 0 ] = value >> 8;
	ptr[ 1 ] = value & 0x0FF;
}

static inline UINT16 GetUINT16( const void *_ptr )
{
	FUNCTION_ENTRY( nullptr, "GetUINT16", true );

	const UINT8 *ptr = ( const UINT8 * ) _ptr;
	return ( UINT16 ) (( ptr[ 0 ] << 8 ) | ptr[ 1 ] );
}

static size_t freadcf7( void *restrict ptr, size_t size, size_t nitems, FILE *restrict file )
{
	char *buffer = new char[ size * nitems * 2 ];

	// Read the raw sector from the disk
	size_t read = fread( buffer, 2 * size, nitems, file );
	if( read == nitems )
	{
		for( size_t i = 0; i < size; i++ )
		{
			((char*)ptr) [ i ] = buffer[ 2*i ];
		}
	}

	delete [] buffer;

	return read;
}

static size_t fwritecf7( const void *restrict ptr, size_t size, size_t nitems, FILE *restrict file )
{
	char *buffer = new char[ size * nitems * 2 ];

	memset( buffer, 0xE5, size * nitems * 2 );

	for( size_t i = 0; i < size; i++ )
	{
		buffer[ 2*i ] = ((const char*)ptr) [ i ];
	}

	size_t written = fwrite( buffer, 2 * size, nitems, file );

	delete [] buffer;

	return written;
}

cDiskSerializerCF7::cDiskSerializerCF7( ) :
	cBaseObject( "cDiskSerializerCF7" ),
	cDiskSerializer( ),
	m_VolumeIndex( 0 ),
	m_MaxVolumeIndex( 0 )
{
}

cDiskSerializerCF7::~cDiskSerializerCF7( )
{
}

std::string cDiskSerializerCF7::GetRawFileName( const std::string &fileName )
{
	auto pos = fileName.find_last_of( '#' );

	return fileName.substr( 0, pos );
}

size_t cDiskSerializerCF7::Volume( const std::string &fileName )
{
	auto pos = fileName.find_last_of( '#' );

	return ( pos != std::string::npos ) ? static_cast<size_t>( std::stoi( fileName.substr( pos + 1 ))) : 0;
}

bool cDiskSerializerCF7::MatchesFormat( FILE *file, const char *filename )
{
	FUNCTION_ENTRY( nullptr, "cDiskSerializerCF7::MatchesFormat", true );

	fseek( file, 0L, SEEK_END );

	long size = ftell( file );

	size_t volume = std::max( Volume( filename ), static_cast<size_t>( 1 ));

	if( static_cast<size_t>( size ) < volume * CF7_DISK_SIZE )
	{
		return false;
	}

	return true;
}

bool cDiskSerializerCF7::SupportsFeatures( const cDiskImage &image )
{
	FUNCTION_ENTRY( this, "cDiskSerializerCF7::SupportsFeatures", true );

	// TODO
/*
	int sectors = 0;

	for( size_t h = 0; h < m_NumHeads; h++ )
	{
		for( size_t t = 0; t < m_NumTracks; t++ )
		{
			size_t track = ( h == 0 ) ? t : m_NumTracks - ( t + 1 );
			for( size_t s = 0; s < MAX_SECTORS; s++ )
			{
				const sSector *sector = GetSector( track, h, s );
				if( sector == nullptr )
				{
					continue;
				}
				if( sector->Data == nullptr )
				{
					break;
				}
				if( sectors > CF7_SECTOR_COUNT )
				{
					DBG_ERROR( "Disk image is too big for a CF7+ volume" );
					return false;
				}
				if( fwritecf7( sector->Data, DEFAULT_SECTOR_SIZE, 1, file ) != 1 )
				{
					DBG_ERROR( "Error writing to file" );
					return false;
				}
				sectors += 1;
*/
	return true;
}

eDiskFormat cDiskSerializerCF7::GetFormat( ) const
{
	FUNCTION_ENTRY( this, "cDiskSerializerCF7::GetFormat", true );

	return FORMAT_CF7;
}

size_t cDiskSerializerCF7::GetVolume( ) const
{
	return m_VolumeIndex;
}

size_t cDiskSerializerCF7::MaxVolume( ) const
{
	return m_MaxVolumeIndex;
}

std::string cDiskSerializerCF7::RawFileName( const char *filename ) const
{
	return GetRawFileName( std::string{ filename } );
}

bool cDiskSerializerCF7::LoadFile( const char *filename, cDiskImage *image )
{
	m_VolumeIndex = Volume( filename );
	m_MaxVolumeIndex = GetMaxVolume( filename );

	return cDiskSerializer::LoadFile( filename, image );
}

bool cDiskSerializerCF7::SaveFile( const cDiskImage &image, const char *filename )
{
	m_VolumeIndex = Volume( filename );
	m_MaxVolumeIndex = GetMaxVolume( filename );

	return cDiskSerializer::SaveFile( image, filename );
}

bool cDiskSerializerCF7::LoadTrack( size_t tIndex, size_t hIndex, iDiskTrack *track )
{
	FUNCTION_ENTRY( this, "cDiskSerializerCF7::LoadTrack", true );

	size_t noSectors = 20;

	auto trackImage = cDiskImage::FormatTrack( track::Format::MFM, tIndex, hIndex, noSectors, 1 );

	track->Write( track::Format::MFM, trackImage );

	long fstart = m_VolumeIndex ? static_cast<long>(( m_VolumeIndex - 1 ) * CF7_DISK_SIZE ) : 0;

	tIndex = ( hIndex == 0 ) ? tIndex : 40 - ( tIndex + 1 );

	fstart += ( tIndex * noSectors + hIndex * ( CF7_SECTOR_COUNT / 2 )) * 512;

	fseek( m_DemandLoadFile, fstart, SEEK_SET );

	char buffer[ DEFAULT_SECTOR_SIZE ];

	for( size_t s = 0; s < noSectors; s++ )
	{
		if( freadcf7( buffer, sizeof( buffer ), 1, m_DemandLoadFile ) != 1 )
		{
			DBG_ERROR( "Error reading from file" );
			return false;
		}

		iDiskSector *sector = track->GetSector( -1, -1, s );

		sector->Write( { buffer, buffer + DEFAULT_SECTOR_SIZE } );
	}

	return true;
}

FILE *cDiskSerializerCF7::OpenForWrite( const std::string &name )
{
	FUNCTION_ENTRY( this, "cDiskSerializerCF7::OpenForWrite", true );

	if( FILE *file = fopen( name.c_str( ), "r+b" ))
	{
		return file;
	}

	return cDiskSerializer::OpenForWrite( name );
}

bool cDiskSerializerCF7::ReadFile( FILE *file, cDiskImage *image )
{
	FUNCTION_ENTRY( this, "cDiskSerializerCF7::ReadFile", true );

	// Force the disk configuration
	size_t noTracks        = 40;
	size_t noSides         = 2;
	size_t noSectors       = 20;

	// Clear any old data & prepare for a new image

	image->AllocateTracks( noTracks, noSides );
	image->SetLoadOnDemand( this );
	m_DemandLoadFile = file;

	// Update the disk configuration in the VIB
	if( iSector *sector = image->GetTrack( 0, 0 )->GetSector( 0, 0, 0 ))
	{
		VIB vib;
		memcpy( &vib, sector->GetData( ), sizeof( vib ));

		vib.TracksPerSide   = noTracks;
		vib.Sides           = noSides;
		vib.SectorsPerTrack = noSectors;
		vib.Density         = DENSITY_DOUBLE;

		sector->Write( { (UINT8*) &vib, (UINT8*)( &vib + 1 ) } );

		DBG_EVENT( "Disk loaded" );

		return true;
	}

	return false;
}

bool cDiskSerializerCF7::WriteFile( const cDiskImage &image, FILE *file )
{
	FUNCTION_ENTRY( this, "cDiskSerializerCF7::WriteFile", true );

	long fstart = m_VolumeIndex ? static_cast<long>(( m_VolumeIndex - 1 ) * CF7_DISK_SIZE ) : 0;

	fseek( file, fstart, SEEK_SET );

	size_t sectors = 0;

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
				if( fwritecf7( sector->GetData( ), DEFAULT_SECTOR_SIZE, 1, file ) != 1 )
				{
					DBG_ERROR( "Error writing to file" );
					return false;
				}
				sectors += 1;
			}
		}
	}

	char empty[ DEFAULT_SECTOR_SIZE ];
	memset( empty, 0xE5, DEFAULT_SECTOR_SIZE );

	// Make sure the file is padded out to the full size of a CF7+ volume
	while( sectors < CF7_SECTOR_COUNT )
	{
		if( fwritecf7( empty, DEFAULT_SECTOR_SIZE, 1, file ) != 1 )
		{
			DBG_ERROR( "Error writing to file" );
			return false;
		}
		sectors += 1;
	}

	// Update the disk configuration in the VIB
	auto sector = image.GetTrack( 0, 0 )->GetSector( 0, 0, 0 );

	VIB vib;
	memcpy( &vib, sector->GetData( ), sizeof( vib ));

	UINT16 offset = GetUINT16( &vib.FormattedSectors );
	if( offset < CF7_SECTOR_COUNT )
	{
		vib.TracksPerSide   = 40;
		vib.Sides           = 2;
		vib.SectorsPerTrack = 20;
		vib.Density         = DENSITY_DOUBLE;

		SetUINT16( &vib.FormattedSectors, CF7_SECTOR_COUNT );

		memset( &vib.AllocationMap[ offset / 8 ], 0, ( CF7_SECTOR_COUNT - offset ) / 8 );

		fseek( file, fstart, SEEK_SET );
		if( fwritecf7( &vib, DEFAULT_SECTOR_SIZE, 1, file ) != 1 )
		{
			DBG_ERROR( "Error writing to file" );
			return false;
		}
	}

	return true;
}

size_t cDiskSerializerCF7::GetMaxVolume( const char *filename )
{
	std::string validName = LocateFile( "disks", RawFileName( filename ));

	size_t maxVolume = 1;

	if( !validName.empty( ))
	{
		if( FILE *file = fopen( validName.c_str( ), "rb" ))
		{
			fseek( file, 0, SEEK_END );

			size_t size = static_cast<size_t>( ftell( file ));

			maxVolume = size / CF7_DISK_SIZE;

			fclose( file );
		}
	}

	return maxVolume;
}
