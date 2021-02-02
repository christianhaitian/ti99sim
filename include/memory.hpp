//----------------------------------------------------------------------------
//
// File:		memory.hpp
// Date:		17-Mar-2016
// Programmer:	Marc Rousseau
//
// Description:
//
// Copyright (c) 1998-2004 Marc Rousseau, All Rights Reserved.
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

#ifndef MEMORY_HPP_
#define MEMORY_HPP_

#include <algorithm>
#include "common.hpp"

template <int page_size> class cMemoryManager
{
	static constexpr int PAGE_SIZE  = page_size;
	static constexpr int PAGE_COUNT = 0x10000 / PAGE_SIZE;

	struct sMemoryPage
	{
		bool        isROM;
		UINT8      *data;
	};

	UINT8           blankPage[ PAGE_SIZE ];
	sMemoryPage     memory[ PAGE_COUNT ];

public:

	cMemoryManager( ) :
		blankPage{ }
	{
		SetMemory( 0x0000, 0x10000, nullptr, true );
	}

	void SetMemory( UINT16 address, int size, UINT8 *data, bool isROM )
	{
		int base = address / PAGE_SIZE;

		for( int i = 0; i * PAGE_SIZE < size; i++ )
		{
			memory[ base + i ].isROM = data ? isROM : true;
			memory[ base + i ].data  = data ? data + ( i * PAGE_SIZE ) : blankPage;
		}
	}

	void Read( UINT16 address, int size, UINT8 *data )
	{
		int page   = address / PAGE_SIZE;
		int offset = address % PAGE_SIZE;

		while( size > 0 )
		{
			int count  = std::min( size, PAGE_SIZE - offset );

			memcpy( data, memory[ page ].data + offset, count );

			size -= count;
			data += count;

			page += 1;
			offset = 0;
		}
	}

	void Write( UINT16 address, int size, UINT8 *data )
	{
		int page   = address / PAGE_SIZE;
		int offset = address % PAGE_SIZE;

		while( size > 0 )
		{
			int count  = std::min( size, PAGE_SIZE - offset );

			if( memory[ page ].isROM == false )
			{
				memcpy( memory[ page ].data + offset, data, count );
			}

			size -= count;
			data += count;

			page += 1;
			offset = 0;
		}
	}

	inline UINT8 ReadByte( UINT16 address )
	{
		int page   = address / PAGE_SIZE;
		int offset = address % PAGE_SIZE;

		return memory[ page ].data[ offset ];
	}

	inline UINT16 ReadWord( UINT16 address )
	{
		int page   = address / PAGE_SIZE;
		int offset = address % PAGE_SIZE;

		return ( UINT16 ) (( memory[ page ].data[ offset ] << 8 ) | memory[ page ].data[ offset + 1 ] );
	}

	inline void WriteByte( UINT16 address, UINT8 data )
	{
		int page = address / PAGE_SIZE;

		if( memory[ page ].isROM == false )
		{
			int offset = address % PAGE_SIZE;

			memory[ page ].data[ offset ] = ( UINT8 ) data;
		}
	}

	inline void WriteWord( UINT16 address, UINT16 data )
	{
		int page = address / PAGE_SIZE;

		if( memory[ page ].isROM == false )
		{
			int offset = address % PAGE_SIZE;

			memory[ page ].data[ offset + 0 ] = ( UINT8 ) ( data >> 8 );
			memory[ page ].data[ offset + 1 ] = ( UINT8 ) data;
		}
	}

};

extern cMemoryManager<256>  cpuMemory;
extern cMemoryManager<8192> gplMemory;

#endif
