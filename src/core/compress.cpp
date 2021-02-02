//----------------------------------------------------------------------------
//
// File:        compress.cpp
// Date:        27-Mar-1998
// Programmer:  Marc Rousseau
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
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307, USA.
//
// Revision History:
//
//----------------------------------------------------------------------------

#include <cstdio>
#include "common.hpp"
#include "compress.hpp"
#include "logger.hpp"

DBG_REGISTER( __FILE__ );

#define MIN_RUN         4

static size_t GetRunLength( int bytesLeft, const UINT8 *ptr, UINT8 lastChar )
{
	FUNCTION_ENTRY( nullptr, "GetRunLength", true );

	size_t runLength = 0;
	while( bytesLeft && ( *ptr++ == lastChar ))
	{
		if( ++runLength >= 0x7FFF )
		{
			break;
		}
		bytesLeft--;
	}
	return runLength;
}

static bool WriteUINT16( FILE *file, UINT16 value )
{
	int loByte = fputc(( value >> 0 ) & 0xFF, file );
	int hiByte = fputc(( value >> 8 ) & 0xFF, file );

	if(( loByte < 0 ) || ( hiByte < 0 ))
	{
		return false;
	}

	return true;
}

static bool ReadUINT16( FILE *file, UINT16 *value )
{
	int loByte = fgetc( file );
	int hiByte = fgetc( file );

	if(( loByte < 0 ) || ( hiByte < 0 ))
	{
		return false;
	}

	*value = static_cast<UINT16>(( hiByte << 8 ) | loByte );

	return true;
}

bool SaveBuffer( size_t length, const UINT8 *ptr, FILE *file )
{
	FUNCTION_ENTRY( nullptr, "SaveBuffer", true );

	while( length )
	{
		UINT16 tag;
		size_t count;

		size_t runLength = GetRunLength( length, ptr, *ptr );
		if( runLength >= MIN_RUN )
		{
			tag = static_cast<UINT16>( runLength | 0x8000 );
			count = 1;
		}
		else
		{
			size_t bytesLeft = length - runLength;
			UINT8 lastChar = *ptr;
			const UINT8 *nextPtr = ptr + runLength;
			while( bytesLeft )
			{
				while( bytesLeft && ( *nextPtr != lastChar ))
				{
					bytesLeft--;
					lastChar = *nextPtr++;
					if( ++runLength >= 0x7FFF )
					{
						break;
					}
				}
				if( runLength >= 0x7FFF )
				{
					break;
				}
				if( bytesLeft )
				{
					size_t tempRun = GetRunLength( bytesLeft, nextPtr, *nextPtr );
					if( tempRun >= MIN_RUN )
					{
						break;
					}
					runLength += tempRun;
					if( runLength >= 0x7FFF )
					{
						runLength = 0x7FFF;
						break;
					}
					nextPtr   += tempRun;
					bytesLeft -= tempRun;
				}
			}
			tag = count = static_cast<UINT16>( runLength );
		}

		WriteUINT16( file, tag );

		if( fwrite( ptr, 1, count, file ) != count )
		{
			return false;
		}

		ptr    += runLength;
		length -= runLength;
	}

	return true;
}

bool LoadBuffer( size_t length, UINT8 *ptr, FILE *file )
{
	FUNCTION_ENTRY( nullptr, "LoadBuffer", true );

	while( length > 0 )
	{
		UINT16 tag = 0;

		if( !ReadUINT16( file, &tag ))
		{
			DBG_ERROR( "Invalid compressed buffer" );
			return false;
		}

		DBG_ASSERT(( tag & 0x7FFF ) <= length );

		if( tag & 0x8000 )
		{
			UINT8 runChar;
			if( fread( &runChar, 1, 1, file ) != 1 )
			{
				return false;
			}
			size_t count = tag & 0x7FFF;
			length -= count;
			while( count-- )
			{
				*ptr++ = runChar;
			}
		}
		else
		{
			if( tag == 0 )
			{
				DBG_ERROR( "Invalid compressed buffer" );
				return false;
			}
			if( fread( ptr, 1, tag, file ) != tag )
			{
				return false;
			}
			ptr += tag;
			length -= tag;
		}
	}

	return ( length == 0 );
}
