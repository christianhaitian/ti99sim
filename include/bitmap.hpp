//----------------------------------------------------------------------------
//
// File:		bitmap.hpp
// Date:		23-Feb-1998
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

#ifndef BITMAP_HPP_
#define BITMAP_HPP_

#include "common.hpp"

struct sRECT
{
	int Left;
	int Right;
	int Top;
	int Bottom;
};

enum COLOR_INDEX_E
{
	COLOR_RED,
	COLOR_GREEN,
	COLOR_BLUE
};

struct sBitMapInfo
{
	int          BitsPerPixel;
	int          BytesPerPixel;
	int          ColorMask[ 3 ];
	int          ColorShift[ 3 ];
};

class cBitMap
{
private:

	bool         m_Scale2x;
	int          m_Width;
	int          m_Height;
	UINT32      *m_pData;

	// Routines for Scale2x
	void CalculateNewPixels( UINT32, UINT32, UINT32, UINT32, UINT32, UINT32 *, UINT32 * );

	// Routines for Scale2x
	void CalculateNewPixels( UINT32, UINT32, UINT32, UINT32, UINT32, UINT32 *, UINT32 *, UINT32 * );

	void Scale2xImp( cBitMap *, UINT32 * );
	void Scale3xImp( cBitMap *, UINT32 * );

	void Scale2X( cBitMap * );
	void Scale3X( cBitMap * );

public:

	cBitMap( int width, int height, bool );
	virtual ~cBitMap( );

	int Width( ) const			{ return m_Width;  }
	int Height( ) const			{ return m_Height; }
	int Pitch( ) const			{ return m_Width * 4; }

	UINT32 *GetData( ) const	{ return m_pData; }

	void Copy( cBitMap * );
	void Fill( UINT32 );

private:

	cBitMap( const cBitMap & ) = delete;         // no implementation
	void operator =( const cBitMap & ) = delete; // no implementation

};

#endif
