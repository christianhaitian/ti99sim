//----------------------------------------------------------------------------
//
// File:        bitstream.hpp
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

#ifndef BITSTREAM_HPP_
#define BITSTREAM_HPP_

#include "common.hpp"
#include <vector>

class cBitStream
{
protected:

	const std::vector<UINT8>          &data;
	size_t                             totalBits { 0 };
	size_t                             bitOffset { 0 };
	const UINT8                       *ptr       { nullptr };
	UINT8                              byte      { 0 };
	bool                               skip      { false };

public:

	cBitStream( const std::vector<UINT8> &data, size_t size, bool skip );
	virtual ~cBitStream( );

	virtual auto offset( ) const -> size_t;
	virtual auto size( ) const -> size_t;
	virtual auto remaining( ) const -> size_t;
	virtual auto seek( size_t offset ) -> bool;
	virtual auto next( ) -> int = 0;
};

class cBitStreamLSB :
	public cBitStream
{

	UINT8                              mask { 0x01 };

public:

	cBitStreamLSB( const std::vector<UINT8> &data, size_t size, bool skip );

	virtual auto next( ) -> int;
};

class cBitStreamMSB :
	public cBitStream
{
	UINT8                              mask { 0x01 };

public:

	cBitStreamMSB( const std::vector<UINT8> &data, size_t size, bool skip );

	virtual auto next( ) -> int;
};

#endif
