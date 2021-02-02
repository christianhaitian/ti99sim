//----------------------------------------------------------------------------
//
// File:        bitstream.cpp
// Date:        21-July-2015
// Programmer:  Marc Rousseau
//
// Description: A set of classes to extract individual bits from a byte array
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
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
//
// Revision History:
//
//----------------------------------------------------------------------------

#include "bitstream.hpp"

cBitStream::cBitStream( const std::vector<UINT8> &data_, size_t size_, bool skip_ ) :
	data( data_ ),
	totalBits( size_ ),
	ptr( data.data( )),
	skip( skip_ )
{
}

cBitStream::~cBitStream( )
{
}

size_t cBitStream::offset( ) const
{
	return skip ? bitOffset / 2 : bitOffset;
}

size_t cBitStream::size( ) const
{
	auto bits = totalBits;

	return skip ? bits / 2 : bits;
}

size_t cBitStream::remaining( ) const
{
	if( bitOffset >= totalBits )
	{
		return 0;
	}

	auto bits = totalBits - bitOffset;

	return skip ? bits / 2 : bits;
}

bool cBitStream::seek( size_t newOffset )
{
	newOffset = skip ? newOffset * 2 : newOffset;

	if( newOffset >= totalBits )
	{
		bitOffset = totalBits;
		return false;
	}

	ptr = data.data( ) + newOffset / 8;

	bitOffset = newOffset;

	if( bitOffset % 8 != 0 )
	{
		byte = *ptr++;
	}

	return true;
}

cBitStreamLSB::cBitStreamLSB( const std::vector<UINT8> &data_, size_t size_, bool skip_ ) :
	cBitStream( data_, size_, skip_ ),
	mask( skip ? 0x03 : 0x01 )
{
}

int cBitStreamLSB::next( )
{
	if( bitOffset >= totalBits )
	{
		return -1;
	}

	if( bitOffset % 8 == 0 )
	{
		byte = *ptr++;
	}

	int index = bitOffset % 8;

	bitOffset += skip ? 2 : 1;

	return ( byte & ( mask << index )) ? 1 : 0;
}

cBitStreamMSB::cBitStreamMSB( const std::vector<UINT8> &data_, size_t size_, bool skip_ ) :
	cBitStream( data_, size_, skip_ ),
	mask( skip ? 0xC0 : 0x80 )
{
}

int cBitStreamMSB::next( )
{
	if( bitOffset >= totalBits )
	{
		return -1;
	}

	if( bitOffset % 8 == 0 )
	{
		byte = *ptr++;
	}

	int index = bitOffset % 8;

	bitOffset += skip ? 2 : 1;

	return ( byte & ( mask >> index )) ? 1 : 0;
}
