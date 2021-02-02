//----------------------------------------------------------------------------
//
// File:        decode-mfm.cpp
// Date:        21-Jul-2015
// Programmer:  Marc Rousseau
//
// Description: A class to decode an MFM encoded disk data sequence
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

std::list<sDataFragment> DecodeDataMFM( cBitStream &stream )
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
		// Look for the clock pattern:
		switch( reg & 0x7FFF )
		{
			case 0x4489 : // x100 0100 1000 1001 => 0xA1 with missing clock
			case 0x5224 : // x101 0010 0010 0100 => 0xC2 with missing clock
				return true;
		}

		return false;
	};

	auto LostClock = [ & ]( )
	{
		// Make sure the last clock looks correct
		switch( reg & 0x07 )
		{
			case 0x02 :	// Data 0 0 -> 0 1 0
			case 0x01 :	// Data 0 1 -> 0 0 1
			case 0x04 :	// Data 1 0 -> 1 0 0
			case 0x05 :	// Data 1 1 -> 1 0 1
				break;
			default :
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
