//----------------------------------------------------------------------------
//
// File:        disk-serializer-hfe.cpp
// Date:        04-May-2015
// Programmer:  Marc Rousseau
//
// Description: A class to load/save hfe disk images
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

#include <memory.h>
#include "common.hpp"
#include "logger.hpp"
#include "disk-media.hpp"
#include "disk-serializer-hfe.hpp"
#include "file-system-disk.hpp"
#include "disk-util.hpp"
#include "bitstream.hpp"
#include "support.hpp"

DBG_REGISTER( __FILE__ );

constexpr size_t RoundToMultiple( size_t number, size_t modulus )
{
	size_t remainder = number % modulus;
	return ( remainder == 0 ) ? number : number + modulus - remainder;
}

static bool BuildTrack( const std::list<sDataFragment> &fragments, std::vector<size_t> *clock, std::vector<UINT8> *data )
{
	clock->clear( );
	data->clear( );

	size_t lastBitOffset = 0;

	for( auto &fragment : fragments )
	{
		int lostBits = fragment.bitOffsetStart - lastBitOffset;

		while( lostBits > 16 )
		{
			lostBits -= 16;
			data->push_back( 0x00 );
		}

		if( fragment.clock != -1 )
		{
			clock->push_back( data->size( ));
		}

		data->insert( data->end( ), fragment.byteData.begin( ), fragment.byteData.end( ));

		lastBitOffset = fragment.bitOffsetEnd;
	}

	return true;
}

static bool DecodeFM( const std::vector<UINT8> &encodedData, std::vector<size_t> *clock, std::vector<UINT8> *data )
{
	cBitStreamLSB bitstream{ encodedData, encodedData.size( ) * 8, true };

	auto fragments = DecodeDataFM( bitstream );

	return BuildTrack( fragments, clock, data );
}

static bool DecodeMFM( const std::vector<UINT8> &encodedData, std::vector<size_t> *clock, std::vector<UINT8> *data )
{
	cBitStreamLSB bitstream{ encodedData, encodedData.size( ) * 8, false };

	auto fragments = DecodeDataMFM( bitstream );

	return BuildTrack( fragments, clock, data );
}

static std::list<sDataFragment> BuildFragments( const std::vector<size_t> &clock, const std::vector<UINT8> &data, int (*getClock)( int ))
{
	std::list<sDataFragment> fragments;

	if( !data.empty( ))
	{
		auto start = data.begin( );

		size_t lastClock = 0;

		if( !clock.empty( ))
		{
			if( clock.front( ) != lastClock )
			{
				size_t size = clock.front( );

				fragments.push_back( { lastClock * 16, ( lastClock + size ) * 16, -1, { start, start + size }} );
				start += size;

				lastClock = clock.front( );
			}

			for( size_t i = 0; i + 1 < clock.size( ); i++ )
			{
				size_t nextClock = clock[ i + 1 ];
				size_t size = nextClock - lastClock;

				fragments.push_back( { lastClock * 16, ( lastClock + size ) * 16, getClock( *start ), { start, start + size }} );
				start += size;

				lastClock = nextClock;
			}
		}

		if( lastClock != data.size( ))
		{
			size_t size = data.size( ) - lastClock;

			fragments.push_back( { lastClock * 16, ( lastClock + size ) * 16, getClock( *start ), { start, start + size }} );
		}
	}

	return fragments;
}

static std::vector<UINT8> EncodeFM( const std::vector<size_t> &clock, const std::vector<UINT8> &data )
{
	auto fragments = BuildFragments( clock, data, []( int clockByte ){ return ( clockByte == 0xFC ) ? 0xD7 : 0xC7; } );

	return EncodeDataFM( fragments, true );
}

static std::vector<UINT8> EncodeMFM( const std::vector<size_t> &clock, const std::vector<UINT8> &data )
{
	auto fragments = BuildFragments( clock, data, []( int clockByte ){ return ( clockByte == 0xA1 ) ? 0x0A: 0x14; } );

	return EncodeDataMFM( fragments, true );
}

cDiskSerializerHFE::cDiskSerializerHFE( ) :
	cBaseObject( "cDiskSerializerHFE" ),
	cDiskSerializer( ),
	FileBuffer( )
{
}

cDiskSerializerHFE::~cDiskSerializerHFE( )
{
}

bool cDiskSerializerHFE::MatchesFormat( FILE *file )
{
	FUNCTION_ENTRY( nullptr, "cDiskSerializerHFE::MatchesFormat", true );

	fseek( file, 0, SEEK_SET );

	HxC::FileHeader header;
	if( fread( &header, sizeof( header ), 1, file ) != 1 )
	{
		return false;
	}

	// Look for the a proper HxC signature
	if( memcmp( header.signature, HxC::HEADER_SIGNATURE, 8 ) == 0 )
	{
		// Last sanity check
		if(( header.numTracks <= 80 ) && ( header.numSides <= 2 ))
		{
			return true;
		}
	}

	return false;
}

bool cDiskSerializerHFE::SupportsFeatures( const cDiskImage &image )
{
	FUNCTION_ENTRY( this, "cDiskSerializerHFE::SupportsFeatures", true );

	// TODO

	return true;
}

eDiskFormat cDiskSerializerHFE::GetFormat( ) const
{
	FUNCTION_ENTRY( this, "cDiskSerializerHFE::GetFormat", true );

	return FORMAT_HFE;
}

bool cDiskSerializerHFE::LoadTrack( size_t cylinder, size_t head, iDiskTrack *track )
{
	FUNCTION_ENTRY( this, "cDiskSerializerHFE::LoadTrack", true );

	auto header = reinterpret_cast<const HxC::FileHeader *>( FileBuffer.data( ));

	auto trackLUT = reinterpret_cast<const HxC::pictrack *>( FileBuffer.data( ) + 0x200 );

	if( trackLUT[ cylinder ].trackLength > 0 )
	{
		size_t chunks = ( trackLUT[ cylinder ].trackLength + 511 ) / 512;

		std::vector<UINT8> trackData;

		trackData.reserve( chunks * 256 );

		for( size_t i = 0; i < chunks; i++ )
		{
			const UINT8 *start = FileBuffer.data( ) + ( trackLUT[ cylinder ].offset + i ) * 512 + head * 256;
			trackData.insert( trackData.end( ), start, start + 256 );
		}

		trackData.resize( trackLUT[ cylinder ].trackLength / 2 );

		HxC::ENCODING encoding = static_cast<HxC::ENCODING>( header->trackEncoding );

		track::Format format = ( encoding != HxC::ENCODING::ISOIBM_MFM ) ? track::Format::FM : track::Format::MFM;

		auto decode = ( encoding != HxC::ENCODING::ISOIBM_MFM ) ? DecodeFM : DecodeMFM;

		std::vector<size_t> clock;
		std::vector<UINT8> data;

		decode( trackData, &clock, &data );

		track->RawWrite( format, clock, data );
	}

	return true;
}

void cDiskSerializerHFE::LoadComplete( )
{
	FUNCTION_ENTRY( this, "cDiskSerializerHFE::LoadComplete", true );

	FileBuffer.clear( );
}

bool cDiskSerializerHFE::ReadFile( FILE *file, cDiskImage *image )
{
	FUNCTION_ENTRY( this, "cDiskSerializerHFE::ReadFile", true );

	fseek( file, 0, SEEK_END );
	auto size = ftell( file );
	fseek( file, 0, SEEK_SET );

	if( size <= 0 )
	{
		return false;
	}

	FileBuffer.resize( static_cast<size_t>( size ));
	if( fread( FileBuffer.data( ), FileBuffer.size( ), 1, file ) != 1 )
	{
		FileBuffer.clear( );
		return false;
	}

	auto header = reinterpret_cast<const HxC::FileHeader *>( FileBuffer.data( ));

	// Double check the signature
	if( memcmp( header->signature, HxC::HEADER_SIGNATURE, 8 ) == 0 )
	{
		// Clear any old data & prepare for a new image
		image->AllocateTracks( header->numTracks, header->numSides );
		image->SetLoadOnDemand( this );

		DBG_EVENT( "Disk loaded" );

		return true;
	}

	return false;
}

std::vector<UINT8> GetTrackData( const iDiskTrack *track )
{
	if( track == nullptr )
	{
		return std::vector<UINT8>( );
	}

	auto encode = ( track->GetFormat( ) == track::Format::FM ) ? EncodeFM : EncodeMFM;

	return encode( track->GetClockLocations( ), track->Read( ));
}

bool cDiskSerializerHFE::WriteFile( const cDiskImage &image, FILE *file )
{
	FUNCTION_ENTRY( this, "cDiskSerializerHFE::WriteFile", true );

	std::vector<UINT8> buffer( 1024, 0xFF );

	auto header = reinterpret_cast<HxC::FileHeader *>( buffer.data( ));

	auto trackLUT = reinterpret_cast<HxC::pictrack *>( buffer.data( ) + 0x200 );

	auto track0 = image.GetTrack( 0, 0 );

	memcpy( header->signature, HxC::HEADER_SIGNATURE, 8 );
	header->revision			= 0;
	header->numTracks			= image.GetNumTracks( );
	header->numSides			= image.GetNumHeads( );
	header->trackEncoding		= static_cast<UINT8>(( track0->GetFormat( ) == track::Format::MFM ) ? HxC::ENCODING::ISOIBM_MFM : HxC::ENCODING::ISOIBM_FM );
	header->bitRate				= ( track0->GetFormat( ) == track::Format::MFM ) ? 250 : 300;
	header->floppyRPM			= ( track0->GetFormat( ) == track::Format::MFM ) ? 300 : 360;
	header->floppyInterfaceMode	= static_cast<UINT8>(( image.GetNumTracks( ) == 80 ) ? HxC::FLOPPYMODE::IBMPC_HD : HxC::FLOPPYMODE::GENERIC_SHUGGART_DD );
	header->trackListOffset		= RoundToMultiple( sizeof( HxC::FileHeader ), 512 ) / 512;
	header->writeAllowed		= true;
	header->singleStep			= ( header->numTracks > 40 ) ? 0xFF : 0x00;

	fseek( file, RoundToMultiple( buffer.size( ), 512 ), SEEK_SET );

	for( size_t cylinder = 0; cylinder < image.GetNumTracks( ); cylinder++ )
	{
		auto side0 = GetTrackData( image.GetTrack( cylinder, 0 ));
		auto side1 = GetTrackData( image.GetTrack( cylinder, 1 ));

		size_t size = std::max( side0.size( ), side1.size( ));

		trackLUT[ cylinder ].offset = ftell( file ) / 512;
		trackLUT[ cylinder ].trackLength = size * 2;

		size = RoundToMultiple( size, 256 );

		side0.insert( side0.end( ), size - side0.size( ), 0xFF );
		side1.insert( side1.end( ), size - side1.size( ), 0xFF );

		size_t chunks = size / 256;

		for( size_t i = 0; i < chunks; i++ )
		{
			fwrite( side0.data( ) + ( i * 256 ), 1, 256, file );
			fwrite( side1.data( ) + ( i * 256 ), 1, 256, file );
		}
	}

	fseek( file, 0, SEEK_SET );
	fwrite( buffer.data( ), 1, buffer.size( ), file );

	return true;
}
