//----------------------------------------------------------------------------
//
// File:        bitmap.cpp
// Date:        06-Apr-2000
// Programmer:  Marc Rousseau
//
// Description:
//
// Copyright (c) 2000-2004 Marc Rousseau, All Rights Reserved.
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
#include <cstdio>
#include <SDL.h>
#include "common.hpp"
#include "logger.hpp"
#include "tms9900.hpp"
#include "tms9918a.hpp"
#include "ti994a.hpp"
#include "bitmap.hpp"
#include "tms9918a-sdl.hpp"

DBG_REGISTER( __FILE__ );

cBitMap::cBitMap( int width, int height, bool useScale2x ) :
	m_Scale2x( useScale2x ),
	m_Width( width ),
	m_Height( height ),
	m_pData( new UINT32[ width * height ] )
{
	FUNCTION_ENTRY( this, "cBitMap ctor", true );

	memset( m_pData, 0, sizeof( UINT32 ) * width * height );
}

cBitMap::~cBitMap( )
{
	FUNCTION_ENTRY( this, "cBitMap dtor", true );

	delete [] m_pData;
}

template<class PIXEL> void CalculatePixels( PIXEL B, PIXEL D, PIXEL E, PIXEL F, PIXEL H, PIXEL &E0, PIXEL &E1, PIXEL &E2, PIXEL &E3 )
{
	// Here are the equations from the scale2x website (http://scale2x.sourceforge.net/algorithm.html):
	//
	//   E0 = D == B && B != F && D != H ? D : E;
	//   E1 = B == F && B != D && F != H ? F : E;
	//   E2 = D == H && D != B && H != F ? D : E;
	//   E3 = H == F && D != H && B != F ? F : E;
	//
	// Re-arranging terms and inverting the logic for E1 and E2 we get:
	//
	//   E0 = B == D && B != F && D != H           ? D : E;
	//   E1 = B == D || B != F ||           H == F ? E : F;
	//   E2 = B == D ||           D != H || H == F ? E : D;
	//   E3 =           B != F && D != H && H == F ? F : E;
	//
	// The following code eliminates redundant comparisions:

	if( B == D )
	{
		E1 = E;
		E2 = E;
		if( B != F )
		{
			if( D != H )
			{
				E0 = D;
				E3 = (( H == F ) ? F : E );
			}
			else
			{
				E0 = E;
				E3 = E;
			}
		}
		else
		{
			E0 = E;
			E3 = E;
		}
	}
	else
	{
		E0 = E;
		if( B != F )
		{
			E1 = E;
			if( D != H )
			{
				E2 = E;
				E3 = (( H == F ) ? F : E );
			}
			else
			{
				E2 = (( H == F ) ? E : D );
				E3 = E;
			}
		}
		else
		{
			E3 = E;
			if( H == F )
			{
				E1 = E;
				E2 = E;
			}
			else
			{
				E1 = F;
				E2 = (( D != H ) ? E : D );
			}
		}
	}

#if 0
	//
	// E0 = D == B && B != H && D != F ? D : E;
	// E1 = B == F && B != H && D != F ? F : E;
	// E2 = D == H && B != H && D != F ? D : E;
	// E3 = H == F && B != H && D != F ? F : E;
	//
	//
	// if (B != H && D != F)
	// {
	//     E0 = D == B ? D : E;
	//     E1 = B == F ? F : E;
	//     E2 = D == H ? D : E;
	//     E3 = H == F ? F : E;
	// } else {
	//     E0 = E;
	//     E1 = E;
	//     E2 = E;
	//     E3 = E;
	// }
	//

	if( ( B != H ) && ( D != F ) )
	{
		if( B == D )
		{
			E0 = D;
			E1 = E;
			E2 = E;
			E3 = ( H == F ) ? F : E;
		}
		else
		{
			E0 = E;
			if( B == F )
			{
				E1 = F;
				E2 = ( D == H ) ? D : E;
				E3 = E;
			}
			else
			{
				E1 = E;
				if( D == H )
				{
					E2 = D;
					E3 = E;
				}
				else
				{
					E2 = E;
					E3 = ( H == F ) ? F : E;
				}
			}
		}
	}
	else
	{
		E0 = E;
		E1 = E;
		E2 = E;
		E3 = E;
	}
#endif
}

void cBitMap::CalculateNewPixels( UINT32 B, UINT32 D, UINT32 E, UINT32 F, UINT32 H, UINT32 *pData1, UINT32 *pData2 )
{
	FUNCTION_ENTRY( this, "cBitMap::CalculateNewPixels", false );

	CalculatePixels<UINT32> ( B, D, E, F, H, pData1[ 0 ], pData1[ 1 ], pData2[ 0 ], pData2[ 1 ] );
}

void cBitMap::CalculateNewPixels( UINT32 B, UINT32 D, UINT32 E, UINT32 F, UINT32 H, UINT32 *pData1, UINT32 *pData2, UINT32 *pData3 )
{
	FUNCTION_ENTRY( this, "cBitMap::CalculateNewPixels", false );

	CalculatePixels<UINT32> ( B, D, E, F, H, pData1[ 0 ], pData1[ 2 ], pData3[ 0 ], pData3[ 2 ] );

	pData1[ 1 ] = ( UINT32 ) E;
	pData2[ 0 ] = ( UINT32 ) E;
	pData2[ 1 ] = ( UINT32 ) E;
	pData2[ 2 ] = ( UINT32 ) E;
	pData3[ 1 ] = ( UINT32 ) E;
}

void cBitMap::Scale2xImp( cBitMap *original, UINT32 *pDstData )
{
	FUNCTION_ENTRY( this, "cBitMap::Scale2xImp<>", false );

	UINT32 *pSrcData = original->GetData( );

	int width  = original->Width( );
	int height = original->Height( );

	UINT32 B, D, E, F, H;

	UINT32 *pLstData = pSrcData;
	UINT32 *pCurData = pSrcData;
	UINT32 *pNxtData = pSrcData + original->Width( );

	{
		UINT32 *pData1 = pDstData;
		UINT32 *pData2 = pDstData + Width( );

		E = pCurData[ 0 ];
		F = pCurData[ 0 ];

		for( int x = 0; x < width - 1; x++ )
		{
			B = pCurData[ x ];
			D = E;
			E = F;
			F = pCurData[ x + 1 ];
			H = pNxtData[ x ];

			CalculateNewPixels( B, D, E, F, H, pData1, pData2 );

			pData1 += 2;
			pData2 += 2;
		}

		CalculateNewPixels( B, E, F, F, pNxtData[ width - 1 ], pData1, pData2 );

		pLstData  = pCurData;
		pCurData  = pNxtData;
		pNxtData += original->Width( );
		pDstData += 2 * Width( );
	}

	for( int y = 1; y < height - 1; y++ )
	{
		UINT32 *pData1 = pDstData;
		UINT32 *pData2 = pDstData + Width( );

		E = pCurData[ 0 ];
		F = pCurData[ 0 ];

		for( int x = 0; x < width - 1; x++ )
		{
			B = pLstData[ x ];
			D = E;
			E = F;
			F = pCurData[ x + 1 ];
			H = pNxtData[ x ];

			CalculateNewPixels( B, D, E, F, H, pData1, pData2 );

			pData1 += 2;
			pData2 += 2;
		}

		CalculateNewPixels( B, E, F, F, pNxtData[ width - 1 ], pData1, pData2 );

		pLstData  = pCurData;
		pCurData  = pNxtData;
		pNxtData += original->Width( );
		pDstData += 2 * Width( );
	}

	{
		UINT32 *pData1 = pDstData ;
		UINT32 *pData2 = pDstData + Width( );

		E = pCurData[ 0 ];
		F = pCurData[ 0 ];

		for( int x = 0; x < width - 1; x++ )
		{
			B = pLstData[ x ];
			D = E;
			E = F;
			F = pCurData[ x + 1 ];
			H = pCurData[ x ];

			CalculateNewPixels( B, D, E, F, H, pData1, pData2 );

			pData1 += 2;
			pData2 += 2;
		}

		CalculateNewPixels( B, E, F, F, pNxtData[ width - 1 ], pData1, pData2 );
	}
}

void cBitMap::Scale3xImp( cBitMap *original, UINT32 *pDstData )
{
	FUNCTION_ENTRY( this, "cBitMap::Scale3xImp<>", false );

	UINT32 *pSrcData = original->GetData( );

	int width  = original->Width( );
	int height = original->Height( );

	UINT32 B, D, E, F, H;

	UINT32 *pLstData = pSrcData;
	UINT32 *pCurData = pSrcData;
	UINT32 *pNxtData = pSrcData + original->Width( );

	{
		UINT32 *pData1 = pDstData;
		UINT32 *pData2 = pDstData + Width( );
		UINT32 *pData3 = pDstData + Width( ) * 2;

		E = pCurData[ 0 ];
		F = pCurData[ 0 ];

		for( int x = 0; x < width - 1; x++ )
		{
			B = pCurData[ x ];
			D = E;
			E = F;
			F = pCurData[ x + 1 ];
			H = pNxtData[ x ];

			CalculateNewPixels( B, D, E, F, H, pData1, pData2, pData3 );

			pData1 += 3;
			pData2 += 3;
			pData3 += 3;
		}

		CalculateNewPixels( B, E, F, F, pNxtData[ width - 1 ], pData1, pData2, pData3 );

		pLstData  = pCurData;
		pCurData  = pNxtData;
		pNxtData += original->Width( );
		pDstData += 3 * Width( );
	}

	for( int y = 1; y < height - 1; y++ )
	{
		UINT32 *pData1 = pDstData;
		UINT32 *pData2 = pDstData + Width( );
		UINT32 *pData3 = pDstData + Width( ) * 2;

		E = pCurData[ 0 ];
		F = pCurData[ 0 ];

		for( int x = 0; x < width - 1; x++ )
		{
			B = pLstData[ x ];
			D = E;
			E = F;
			F = pCurData[ x + 1 ];
			H = pNxtData[ x ];

			CalculateNewPixels( B, D, E, F, H, pData1, pData2, pData3 );

			pData1 += 3;
			pData2 += 3;
			pData3 += 3;
		}

		CalculateNewPixels( B, E, F, F, pNxtData[ width - 1 ], pData1, pData2, pData3 );

		pLstData  = pCurData;
		pCurData  = pNxtData;
		pNxtData += original->Width( );
		pDstData += 3 * Width( );
	}

	{
		UINT32 *pData1 = pDstData;
		UINT32 *pData2 = pDstData + Width( );
		UINT32 *pData3 = pDstData + Width( ) * 2;

		E = pCurData[ 0 ];
		F = pCurData[ 0 ];

		for( int x = 0; x < width - 1; x++ )
		{
			B = pLstData[ x ];
			D = E;
			E = F;
			F = pCurData[ x + 1 ];
			H = pCurData[ x ];

			CalculateNewPixels( B, D, E, F, H, pData1, pData2, pData3 );

			pData1 += 3;
			pData2 += 3;
			pData3 += 3;
		}

		CalculateNewPixels( B, E, F, F, pNxtData[ width - 1 ], pData1, pData2, pData3 );
	}
}

void cBitMap::Scale2X( cBitMap *original )
{
	FUNCTION_ENTRY( this, "cBitMap::Scale2X", false );

	UINT32 *pDstData = GetData( );

	Scale2xImp( original, pDstData );
}

void cBitMap::Scale3X( cBitMap *original )
{
	FUNCTION_ENTRY( this, "cBitMap::Scale3X", false );

	UINT32 *pDstData = GetData( );

	Scale3xImp( original, pDstData );
}

void cBitMap::Copy( cBitMap *original )
{
	FUNCTION_ENTRY( this, "cBitMap::Copy", false );

	int scaleX = Width( ) / original->Width( );
	int scaleY = Height( ) / original->Height( );

	int scale = std::max( std::min( scaleX, scaleY ), 1 );

	// See if we can use the SDL blit
	if( scale == 1 )
	{
		memcpy( m_pData, original->m_pData, m_Width * m_Height * sizeof( UINT32 ));
	}
	else if(( m_Scale2x == true ) && ( scale == 2 ))
	{
		Scale2X( original );
	}
	else if(( m_Scale2x == true ) && ( scale == 3 ))
	{
		Scale3X( original );
	}
	else
	{
		// Not supported
	}
}

void cBitMap::Fill( UINT32 color )
{
	int size = m_Width * m_Height;

	for( int i = 0; i < size; i++ )
	{
		m_pData[ i ] = color;
	}

}
