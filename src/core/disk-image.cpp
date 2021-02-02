//----------------------------------------------------------------------------
//
// File:        disk-image.cpp
// Date:        06-May-2015
// Programmer:  Marc Rousseau
//
// Description: A class to manipulate disk images
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
#include "disk-image.hpp"

DBG_REGISTER( __FILE__ );

/*
  IBM System 3740 - Single Density

  GAP4a SYNC IAM GAP1 SYNC IDAM C H S N CC GAP2 SYNC DAM          CC
   40x   6x       26x  6x       Y D E O RR  11x  6x   FB    Data  RR  GAP3  GAP4b
   FF    00   FC  FF   00   FE  L   C   CC  FF   00   F8          CC

const sDiskGap GAP_FM_3740[ 5 ] =
{
	{ {  40, 0xFF }, {   6, 0x00 }, {   0, 0x00 } },
	{ {  26, 0xFF }, {   6, 0x00 }, {   0, 0x00 } },
	{ {  11, 0xFF }, {   6, 0x00 }, {   0, 0x00 } },
	{ {   ?, 0xFF }, {   6, 0x00 }, {   0, 0x00 } },
	{ {   ?, 0xFF }, {   0, 0x00 }, {   0, 0x00 } }
};

*/

/*
  IBM System 34 - Double Density

  GAP4a SYNC   IAM GAP1 SYNC    IDAM C H S N CC GAP2 SYNC DAM           CC
   80x   12x 3x     50x 12x 3x       Y D E O RR  22x  12x 3x FB   Data  RR  GAP3  GAP4b
   4E    00  C2 FC  4E   00  A1   FE L   C   CC  4e   00  A1 F8         CC


const sDiskGap GAP_MFM_3740[ 5 ] =
{
	{ {  80, 0x4E }, {  12, 0x00 }, {   3, 0xC2 } },
	{ {  50, 0x4E }, {  12, 0x00 }, {   3, 0xA1 } },
	{ {  22, 0x4E }, {  12, 0x00 }, {   3, 0xA1 } },
	{ {   ?, 0x4E }, {  12, 0x00 }, {   3, 0xA1 } },
	{ {   ?, 0x4E }, {   0, 0x00 }, {   0, 0x00 } }
};

*/

/*
  TI-DISK ROM Defaults

  GAP1  SYNC IDAM C H S N CC GAP2 SYNC DAM         CC  GAP3  GAP4b
   16x   6x       Y D E O RR  11x  6x         Data RR   45x   231x
   00    00   FE  L   C   CC  FF   00   FB         CC   FF    FF
*/

const sDiskGap GAP_FM[ 5 ] =
{
	{ {   0, 0x00 }, {   0, 0x00 }, {   0, 0x00 } },	// 0xFC
	{ {  16, 0x00 }, {   6, 0x00 }, {   0, 0x00 } },	// 0xFE
	{ {  11, 0xFF }, {   6, 0x00 }, {   0, 0x00 } },	// 0xFB (0xF8)
	{ {  45, 0xFF }, {   6, 0x00 }, {   0, 0x00 } },	// 0xFE
	{ { 225, 0xFF }, {   0, 0x00 }, {   0, 0x00 } }
};

// 0xF5 -> 0xA1
// 0xF6 -> 0xC2

const sDiskGap GAP_MFM[ 5 ] =
{
	{ {   0, 0x4E }, {   0, 0x00 }, {   0, 0x00 } },	// 0xFC
	{ {  40, 0x4E }, {  10, 0x00 }, {   3, 0xF5 } },	// 0xFE
	{ {  22, 0x4E }, {  12, 0x00 }, {   3, 0xF5 } },	// 0xFB (0xF8)
	{ {  24, 0x4E }, {  10, 0x00 }, {   3, 0xF5 } },	// 0xFE
	{ { 736, 0x4E }, {   0, 0x00 }, {   0, 0x00 } }
};

//----------------------------------------------------------------------------
//  cDiskImage
//----------------------------------------------------------------------------

//
// Create a buffer to simulate a WriteTrack buffer from a WD1771 controller
//

std::vector<UINT8> cDiskImage::FormatTrack( track::Format format, size_t tIndex, size_t hIndex, size_t numSectors, size_t interleave )
{
	std::vector<sSectorInfo> sectorInfo( numSectors );

	for( size_t s = 0; s < numSectors; s++ )
	{
		sectorInfo[ s ].LogicalCylinder = tIndex;
		sectorInfo[ s ].LogicalHead     = hIndex;
		sectorInfo[ s ].LogicalSector   = s;
		sectorInfo[ s ].Size            = 1;
		sectorInfo[ s ].DataMark        = 0xFB;
	}

	return FormatTrack( format, sectorInfo );
}

std::vector<UINT8> cDiskImage::FormatTrack( track::Format format, const std::vector<sSectorInfo> &sectorInfo )
{
	FUNCTION_ENTRY( nullptr, "cDiskImage::FormatTrack", false );

	return FormatTrack(( format == track::Format::FM ) ? GAP_FM : GAP_MFM, sectorInfo );
}

std::vector<UINT8> cDiskImage::FormatTrack( const sDiskGap gapInfo[ 5 ], const std::vector<sSectorInfo> &sectorInfo )
{
	FUNCTION_ENTRY( nullptr, "cDiskImage::FormatTrack", false );

	auto WriteGap = []( std::vector<UINT8> &data, const sDiskGap &gap )
	{
		// Filler (usually 0x00, 0xFF, or 0x4E)
		for( int i = 0; i < gap.filler.count; i++ )
		{
			data.push_back( gap.filler.byte );
		}

		// Sync bytes (always 0x00)
		for( int i = 0; i < gap.sync.count; i++ )
		{
			data.push_back( gap.sync.byte );
		}

		// Clock bytes (MFM only: 0xA1 or 0xC2 - clock is embedded in address mark for FM)
		for( int i = 0; i < gap.clock.count; i++ )
		{
			data.push_back( gap.clock.byte );
		}
	};

	std::vector<UINT8> data;

	if( gapInfo[ 0 ].sync.count > 0 )
	{
		WriteGap( data, gapInfo[ 0 ] );
		data.push_back( 0xFC );											// Index Address Mark
	}

	sDiskGap idGap = gapInfo[ 1 ];

	for( auto &sector : sectorInfo )
	{
		WriteGap( data, idGap );

		data.push_back( 0xFE );											// ID Address Mark

		data.push_back(( UINT8 ) sector.LogicalCylinder );
		data.push_back(( UINT8 ) sector.LogicalHead );
		data.push_back(( UINT8 ) sector.LogicalSector );
		data.push_back(( UINT8 ) sector.Size );

		data.push_back( 0xF7 );											// 2-byte Checksum

		WriteGap( data, gapInfo[ 2 ] );

		data.push_back(( UINT8 ) ( UINT8 ) sector.DataMark );			// Data Address Mark

		data.insert( data.end( ), 128u << sector.Size, 0xE5 );

		data.push_back( 0xF7 );											// 2-byte Checksum

		idGap = gapInfo[ 3 ];
	}

	WriteGap( data, gapInfo[ 4 ] );

	return data;
}

size_t cDiskImage::GetNumHeads( ) const
{
	return NumHeads;
}

size_t cDiskImage::GetNumTracks( ) const
{
	return NumTracks;
}

bool cDiskImage::AllocateTracks( size_t numTracks, size_t numHeads )
{
	FUNCTION_ENTRY( this, "cDiskImage::AllocateTracks", true );

	DBG_ASSERT( numHeads > 0 );
	DBG_ASSERT( numTracks > 0 );

	NumTracks = numTracks;
	NumHeads  = numHeads;

	Track.clear( );

	Track.resize( NumHeads, { } );

	for( auto &track : Track )
	{
		track.resize( numTracks, { } );
	}

	return true;
}

//----------------------------------------------------------------------------
//
// Format a 'standard' disk using the given parameters
//
//----------------------------------------------------------------------------

bool cDiskImage::FormatDisk( size_t numTracks, size_t numSides, size_t numSectors, track::Format format )
{
	FUNCTION_ENTRY( this, "cDiskImage::FormatDisk", true );

	DBG_ASSERT( numSectors > 0 );

	if( ! AllocateTracks( numTracks, numSides ))
	{
		return false;
	}

	for( size_t h = 0; h < numSides; h++ )
	{
		for( size_t t = 0; t < numTracks; t++ )
		{
			auto trackImage = FormatTrack( format, t, h, numSectors, 1 );

			WriteTrack( t, h, format, trackImage );
		}
	}

	return true;
}

bool cDiskImage::WriteTrack( size_t tIndex, size_t hIndex, track::Format format, std::vector<UINT8> data )
{
	FUNCTION_ENTRY( this, "cDiskImage::WriteTrack", true );

	DBG_ASSERT( tIndex < NumTracks );
	DBG_ASSERT( hIndex < NumHeads );

	return Track[ hIndex ][ tIndex ].Write( format, data );
}

iDiskTrack *cDiskImage::GetTrack( size_t tIndex, size_t hIndex )
{
	FUNCTION_ENTRY( this, "cDiskImage::GetTrack", true );

	if( tIndex >= NumTracks )
	{
		return nullptr;
	}
	if( hIndex >= NumHeads )
	{
		return nullptr;
	}

	if(( Track[ hIndex ][ tIndex ].IsEmpty( )) && ( Serializer != nullptr ))
	{
		if( !Serializer->LoadTrack( tIndex, hIndex, &Track[ hIndex ][ tIndex ] ))
		{
			Track[ hIndex ][ tIndex ].Erase( );
		}
		Track[ hIndex ][ tIndex ].ClearChanged( );
	}

	return &Track[ hIndex ][ tIndex ];
}

const iDiskTrack *cDiskImage::GetTrack( size_t tIndex, size_t hIndex ) const
{
	FUNCTION_ENTRY( this, "cDiskImage::GetTrack", true );

	if( tIndex >= NumTracks )
	{
		return nullptr;
	}
	if( hIndex >= NumHeads )
	{
		return nullptr;
	}

	return &Track[ hIndex ][ tIndex ];
}

bool cDiskImage::HasChanged( ) const
{
	for( size_t h = 0; h < NumHeads; h++ )
	{
		for( auto &track : Track[ h ] )
		{
			if( track.HasChanged( ))
			{
				return true;
			}
		}
	}

	return false;
}

void cDiskImage::ClearChanged( )
{
	for( size_t h = 0; h < NumHeads; h++ )
	{
		for( auto &track : Track[ h ] )
		{
			track.ClearChanged( );
		}
	}
}

void cDiskImage::SetLoadOnDemand( iDiskSerializer *serializer )
{
	Serializer = serializer;
}

void cDiskImage::CompleteLoad( ) const
{
	if( Serializer != nullptr )
	{
		for( size_t h = 0; h < NumHeads; h++ )
		{
			for( size_t t = 0; t < NumTracks; t++ )
			{
				cDiskTrack *track = const_cast<cDiskTrack*>( &Track[ h ][ t ] );

				if( track->IsEmpty( ))
				{
					Serializer->LoadTrack( t, h, track );
				}
			}
		}

		const_cast<cRefPtr<iDiskSerializer>&>( Serializer )->LoadComplete( );
	}
}
