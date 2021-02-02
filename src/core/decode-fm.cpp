//----------------------------------------------------------------------------
//
// File:        decode-fm.cpp
// Date:        21-Jul-2015
// Programmer:  Marc Rousseau
//
// Description: A class to decode an FM encoded disk data sequence
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

#include <list>
#include <stdexcept>
#include "bitstream.hpp"
#include "idisk-sector.hpp"
#include "disk-util.hpp"

constexpr auto expand( int byte ) -> int
{
	return
		(( byte & 0x80 ) ? 0x4000 : 0 ) |
		(( byte & 0x40 ) ? 0x1000 : 0 ) |
		(( byte & 0x20 ) ? 0x0400 : 0 ) |
		(( byte & 0x10 ) ? 0x0100 : 0 ) |
		(( byte & 0x08 ) ? 0x0040 : 0 ) |
		(( byte & 0x04 ) ? 0x0010 : 0 ) |
		(( byte & 0x02 ) ? 0x0004 : 0 ) |
		(( byte & 0x01 ) ? 0x0001 : 0 );
}

constexpr int SYNC_MASK_CLOCK  = expand( 0xEF ) << 1;				// 0b1010'1000'1010'1010
constexpr int SYNC_MASK_DATA   = expand( 0xF8 );					// 0b0101'0101'0100'0000
constexpr int SYNC_MASK        = SYNC_MASK_CLOCK | SYNC_MASK_DATA;	// 0b1111'1101'1110'1010

constexpr int SYNC_TEST_CLOCK  = expand( 0xC7 ) << 1;				// AND of C7 and D7
constexpr int SYNC_TEST_DATA   = SYNC_MASK_DATA;					// AND of all address marks
constexpr int SYNC_TEST        = SYNC_TEST_CLOCK | SYNC_TEST_DATA;

constexpr int SYNC_SWITCH_MASK = ( expand( 0xC7 ^ 0xD7 ) << 1 ) | expand( 0xFF ^ 0xF8 );

static int Collapse( int x )
{
	x &= 0x5555;

	x = ( x | ( x >> 1 )) & 0x3333;
	x = ( x | ( x >> 2 )) & 0x0F0F;
	x = ( x | ( x >> 4 )) & 0x00FF;

	return x;
}

static int GetClock( int reg )
{
	return Collapse( reg >> 1 );
}

static int GetData( int reg )
{
	return Collapse( reg );
}

std::list<sDataFragment> DecodeDataFM( cBitStream &stream )
{
	int reg				{ 0 };

	auto GetBit = [ & ]( )
	{
		auto bit = stream.next( );

		if( bit == -1 )
		{
			throw std::underflow_error( "cBitStream empty" );
		}

		reg = ( reg << 1 ) | bit;

		return bit;
	};

	auto IsSync = [ & ]( )
	{
		if(( reg & SYNC_MASK ) != SYNC_TEST )
		{
			return false;
		}

		switch( reg & SYNC_SWITCH_MASK )
		{
			case expand( 0xF8 ^ MARK_DAM ) :	// Marker: 0xF8  Clock: 0xC7  DAM
			case expand( 0xF8 ^ MARK_DAMx ) :	// Marker: 0xF9  Clock: 0xC7  DAMx
			case expand( 0xF8 ^ MARK_DAMy ) :	// Marker: 0xFA  Clock: 0xC7  DAMx
			case expand( 0xF8 ^ MARK_DDAM ) :	// Marker: 0xFB  Clock: 0xC7  DDAM
			case expand( 0xF8 ^ MARK_IDAM ) :	// Marker: 0xFE  Clock: 0xC7  IDAM
			case expand( 0xF8 ^ MARK_IAM ) | ( expand( 0xC7 ^ 0xD7 ) << 1 ) :	// Marker: 0xFC  Clock: 0xD7  IAM
				break;
			default :
				return false;
		}

		return true;
	};

	auto LostClock = [ & ]( )
	{
		// Make sure the last clock looks correct
		if(( reg & 0x02 ) != 0x02 )
		{
			return true;
		}

		return false;
	};

	auto GetByte = [ & ]( )
	{
		for( int i = 0; i < 8; i++ )
		{
			GetBit( );
			GetBit( );

			if( LostClock( ))
			{
				return false;
			}
		}

		return true;
	};

	auto RecoverFragment = [ & ]( size_t from, size_t to )
	{
		sDataFragment fragment{ };

		if( from < to )
		{
			size_t offset = stream.offset( );

			size_t bytes = ( to - from ) / 16;
			stream.seek( to - bytes * 16 );

			fragment.byteData.reserve( bytes );

			fragment.bitOffsetStart = stream.offset( );
			fragment.clock = -1;

			for( size_t i = 0; ( i < bytes ) && GetByte( ); i++ )
			{
				int byte = GetData( reg );

				fragment.bitOffsetEnd = stream.offset( );
				fragment.byteData.push_back( byte );
			}

			stream.seek( offset );
		}

		return fragment;
	};

	std::list<sDataFragment> list;

	list.push_back( { } );

	GetByte( );

	while( stream.remaining( ) > 0 )
	{
		sDataFragment fragment{ };

		fragment.byteData.reserve( stream.remaining( ) / 16 );

		try
		{
			if( LostClock( ))
			{
				// Searching for clock...
				while( IsSync( ) == false )
				{
					GetBit( );
				}
			}

			fragment.bitOffsetStart = stream.offset( ) - 16;
			fragment.bitOffsetEnd = stream.offset( );
			fragment.clock = IsSync( ) ? GetClock( reg ) : -1;
			fragment.byteData.push_back( GetData( reg ));

			// Make sure we didn't capture part of the clock byte in the last fragment
			if( list.back( ).bitOffsetEnd > fragment.bitOffsetStart )
			{
				list.back( ).bitOffsetEnd -= 16;
				list.back( ).byteData.resize( list.back( ).byteData.size( ) - 1 );
			}

			// See if we can recover any bits after the last fragment using the new clock alignment
			auto recovered = RecoverFragment( list.back( ).bitOffsetEnd, fragment.bitOffsetStart );
			if( !recovered.byteData.empty( ))
			{
				list.push_back( recovered );
			}

			while( GetByte( ))
			{
				int byte = GetData( reg );

				fragment.bitOffsetEnd = stream.offset( );
				fragment.byteData.push_back( byte );
			}
		}
		catch( ... )
		{
		}

		if( !fragment.byteData.empty( ))
		{
			list.push_back( fragment );
		}
	}

	list.pop_front( );

	return list;
}
