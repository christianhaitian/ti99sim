//----------------------------------------------------------------------------
//
// File:        tms9918a-sdl.cpp
// Date:        02-Apr-2000
// Programmer:  Marc Rousseau
//
// Description: This file contains SDL specific code for the TMS9918A
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
#include <cstring>
#include <SDL.h>
#include "common.hpp"
#include "logger.hpp"
#include "bitmap.hpp"
#include "tms9918a-sdl.hpp"

DBG_REGISTER( __FILE__ );

cSdlTMS9918A::cSdlTMS9918A( sRGBQUAD colorTable[ 17 ], int refreshRate, bool useScale2x, bool fullScreen, int scale ) :
	cBaseObject( "cSdlTMS9918A" ),
	cTMS9918A( refreshRate ),
	m_TextMode( false ),
	m_ChangesMade( false ),
	m_BlankChanged( false ),
	m_ColorsChanged( false ),
	m_SpritesChanged( false ),
	m_ScreenChanged( ),
	m_PatternChanged( ),
	m_CharUse( ),
	m_SpriteCharUse( ),
	m_Scale2x( useScale2x ),
	m_sdlWindow( nullptr ),
	m_sdlRenderer( nullptr ),
	m_sdlTexture( nullptr ),
	m_ScreenSource( nullptr ),
	m_ScaledScreen( nullptr ),
	m_BitmapScreen( nullptr ),
	m_BitmapSpriteScreen( nullptr ),
	m_CharacterPattern( ),
	m_Mutex( nullptr ),
	m_FullScreen( false ),
	m_OnFrames( 1 ),
	m_OffFrames( 0 ),
	m_FrameCycle( 1 )
{
	FUNCTION_ENTRY( this, "cSdlTMS9918A ctor", true );

	m_Mutex = SDL_CreateMutex( );

	memset( m_ColorTable, 0, sizeof( m_ColorTable ));

	SetColorTable( colorTable );

	if( scale < 0 )
	{
		SDL_DisplayMode current;
		if( SDL_GetCurrentDisplayMode( 0, &current ) == 0 )
		{
			int hScale = ( 0.9 * current.w ) / VDP_WIDTH;
			int vScale = ( 0.9 * current.h ) / VDP_HEIGHT;

			scale = std::max( std::min( hScale, vScale ), 1 );
		}
	}

	m_BitmapScreen       = new cBitMap( VDP_WIDTH, VDP_HEIGHT, false );
	m_BitmapSpriteScreen = new cBitMap( VDP_WIDTH, VDP_HEIGHT, false );

	int sdlScale = useScale2x ? std::max( std::min( scale / 2, 3 ), 2 ) : 1;

	if( sdlScale > 1 )
	{
		m_ScaledScreen = new cBitMap( VDP_WIDTH * sdlScale, VDP_HEIGHT * sdlScale, true );
	}

	// See if we're starting in fullscreen mode
	if( fullScreen == true )
	{
		CreateMainWindowFullScreen( sdlScale );
	}

	if( m_sdlWindow == nullptr )
	{
		CreateMainWindow( VDP_WIDTH * scale, VDP_HEIGHT * scale, sdlScale );
	}

	// Make sure we clear out the initial texture contents
	cBitMap *screen = m_ScaledScreen ? m_ScaledScreen : m_BitmapScreen;
	SDL_UpdateTexture( m_sdlTexture, nullptr, screen->GetData( ), screen->Pitch( ));
}

cSdlTMS9918A::~cSdlTMS9918A( )
{
	FUNCTION_ENTRY( this, "cSdlTMS9918A dtor", true );

	SDL_DestroyMutex( m_Mutex );

	delete m_BitmapSpriteScreen;
	delete m_BitmapScreen;
	delete m_ScaledScreen;
}

//----------------------------------------------------------------------------
// iStateObject methods
//----------------------------------------------------------------------------

bool cSdlTMS9918A::ParseState( const sStateSection &state )
{
	FUNCTION_ENTRY( this, "cSdlTMS9918A::ParseState", true );

	if( cTMS9918A::ParseState( state ) == true )
	{
		m_ChangesMade    = true;
		m_SpritesChanged = true;

		memset( m_ScreenChanged, true, sizeof( m_ScreenChanged ));
		memset( m_PatternChanged, true, sizeof( m_PatternChanged ));

		return true;
	}

	return false;
}

//----------------------------------------------------------------------------
// iTMS9918A methods
//----------------------------------------------------------------------------

void cSdlTMS9918A::Reset( )
{
	FUNCTION_ENTRY( this, "cSdlTMS9918A::Reset", true );

	cTMS9918A::Reset( );

	m_TextMode       = false;
	m_ChangesMade    = true;
	m_BlankChanged   = true;
	m_ColorsChanged  = true;
	m_SpritesChanged = false;

	memset( m_ScreenChanged, false, sizeof( m_ScreenChanged ));
	memset( m_PatternChanged, false, sizeof( m_PatternChanged ));
	memset( m_CharUse, 0, sizeof( m_CharUse ));
	memset( m_SpriteCharUse, 0, sizeof( m_SpriteCharUse ));

	if( m_Mode & VDP_M3 )
	{
		m_CharUse[ 0x0000 ] = m_ImageTableSize / 3;
		m_CharUse[ 0x0100 ] = m_ImageTableSize / 3;
		m_CharUse[ 0x0200 ] = m_ImageTableSize / 3;
	}
	else
	{
		m_CharUse[ 0x0000 ] = m_ImageTableSize;
	}

	m_SpriteCharUse[ 0 ] = 32;
}

void cSdlTMS9918A::WriteData( UINT8 data )
{
	FUNCTION_ENTRY( this, "cSdlTMS9918A::WriteData", false );

	unsigned address = m_Address & 0x3FFF;

	UINT8 *MemPtr = &m_Memory[ address ];

	if( data != *MemPtr )
	{
		int type = m_MemoryType[ address ];

		if( type )
		{
			if( type & MEM_IMAGE_TABLE )
			{
				unsigned offset = address - m_ImageTableIndex;
				DBG_ASSERT( offset < m_ImageTableSize );
				unsigned index  = ( m_Mode & VDP_M3 ) ? offset & 0xFF00 : 0;
				DBG_ASSERT( index <= 256 * 2 );

				m_ChangesMade             = true;
				m_ScreenChanged[ offset ] = true;

				m_CharUse[ index + *MemPtr ]--;
				m_CharUse[ index + data ]++;
			}

			if( type & MEM_PATTERN_TABLE )
			{
				unsigned offset = address - m_PatternTableIndex;
				DBG_ASSERT( offset < m_PatternTableSize );

				m_ChangesMade                  = true;
				m_PatternChanged[ offset / 8 ] = true;
			}

			if( !m_TextMode )
			{
				if( type & MEM_COLOR_TABLE )
				{
					unsigned offset = address - m_ColorTableIndex;

					DBG_ASSERT( offset < m_ColorTableSize );

					m_ChangesMade = true;

					if( m_Mode & VDP_M3 )
					{
						m_PatternChanged[ offset / 8 ] = true;
					}
					else
					{
						memset( &m_PatternChanged[ offset * 8 ], true, 8 );
					}
				}

				if( type & MEM_SPRITE_ATTR_TABLE )
				{
					unsigned index = ( address - m_SpriteAttrTableIndex ) / sizeof( sSpriteAttributeEntry );
					DBG_ASSERT( index < SIZE( sSpriteAttribute::data ));

					sSpriteAttributeEntry *sprite = &(( sSpriteAttribute * ) ( m_Memory + m_SpriteAttrTableIndex ))->data[ index ];
					if( MemPtr == &sprite->patternIndex )
					{
						int count = ( m_Register[ 1 ] & VDP_SPRITE_SIZE ) ? 4 : 1;
						for( int i = 0; i < count; i++ )
						{
							m_SpriteCharUse[( i + *MemPtr ) % 256 ]--;
							m_SpriteCharUse[( i + data ) % 256 ]++;
						}
					}
					m_SpritesChanged = true;
				}

				if( type & MEM_SPRITE_DESC_TABLE )
				{
					unsigned index = ( address - m_SpriteDescTableIndex ) / 8;
					DBG_ASSERT( index < SIZE( m_SpriteCharUse ));

					if( m_SpriteCharUse[ index ] > 0 )
					{
						m_SpritesChanged = true;
					}
				}
			}
		}
	}

	cTMS9918A::WriteData( data );
}

void cSdlTMS9918A::WriteRegister( size_t reg, UINT8 value )
{
	FUNCTION_ENTRY( this, "cSdlTMS9918A::WriteRegister", true );

	UINT8 oldReg = m_Register[ reg ];
	cTMS9918A::WriteRegister( reg, value );
	UINT8 newReg = m_Register[ reg ];

	if( oldReg == newReg )
	{
		return;
	}

	bool patternChanged = false;

	switch( reg )
	{
		case 0 :
			patternChanged = true;
			break;

		case 1 :
			if(( oldReg ^ newReg ) & VDP_BLANK_MASK )
			{
				m_BlankChanged = true;
				if(( oldReg ^ newReg ) == VDP_BLANK_MASK )
				{
					break;
				}
			}
			if(( oldReg ^ newReg ) & VDP_SPRITE_SIZE )
			{
				memset( m_SpriteCharUse, 0, sizeof( m_SpriteCharUse ));
				int count = ( newReg & VDP_SPRITE_SIZE ) ? 4 : 1;
				for( int s = 0; s < 32; s++ )
				{
					sSpriteAttributeEntry *sprite = &(( sSpriteAttribute * ) ( m_Memory + m_SpriteAttrTableIndex))->data[ s ];
					for( int i = 0; i < count; i++ )
					{
						m_SpriteCharUse[( i + sprite->patternIndex ) % 256 ]++;
					}
				}
				m_SpritesChanged = true;
			}
			// Fall through

		case 2 :                                // Image Table
		{
			m_ChangesMade = true;
			memset( m_CharUse, 0, sizeof( m_CharUse ));
			UINT8 *chr = ( UINT8 * ) ( m_Memory + m_ImageTableIndex );
			for( unsigned int i = 0; i < m_ImageTableSize; i++ )
			{
				int index = ( m_Mode & VDP_M3 ) ? ( i & 0xFF00 ) : 0;
				m_CharUse[ index + *chr++ ]++;
			}
			memset( m_ScreenChanged, true, m_ImageTableSize );
			break;
		}

		case 7 :                                // Foreground / Background colors
		{
			m_ColorsChanged = true;

			int bg = ( newReg & 0x0F ) ? ( newReg & 0x0F ) : TI_BLACK;
			int fg = ( newReg & 0xF0 ) ? ( newReg >> 4   ) : bg;

			m_ColorTable[  0 ] = m_ColorTable[ bg ];
			m_ColorTable[ 16 ] = m_ColorTable[ fg ];

			// Fall through
		}
		case 3 :                                // Color Table
		case 4 :                                // Pattern Table
			patternChanged = true;
			break;
	}

	if( patternChanged )
	{
		m_ChangesMade = true;
		memset( m_PatternChanged, true, sizeof( m_PatternChanged ));
	}
}

bool cSdlTMS9918A::Retrace( )
{
	FUNCTION_ENTRY( this, "cSdlTMS9918A::Retrace", false );

	cTMS9918A::Retrace( );

	// See if we should skip a frame
	if( m_FrameCycle <= 0 )
	{
		m_FrameCycle += m_OnFrames;
		return false;
	}

	m_FrameCycle -= m_OffFrames;

	if( BlankEnabled( ) == true )
	{
		if( m_BlankChanged )
		{
			m_BlankChanged = false;
			m_ScreenSource = nullptr;
			return true;
		}

		return false;
	}

	if( !m_ChangesMade && !m_SpritesChanged && !m_BlankChanged )
	{
		return false;
	}

	SDL_LockMutex( m_Mutex );

	bool bNeedsUpdate = false;

	if(( m_Mode & VDP_MODE_ILLEGAL ) == VDP_MODE_ILLEGAL )
	{
		bNeedsUpdate = RefreshInvalid( );
	}
	else if( m_Mode & VDP_M3 )
	{
		bNeedsUpdate = RefreshBitMap( );
	}
	else if( m_Mode & VDP_M2 )
	{
		bNeedsUpdate = RefreshMultiColor( );
	}
	else
	{
		bNeedsUpdate = RefreshGraphics( );
	}

	SDL_UnlockMutex( m_Mutex );

	m_ChangesMade   = false;
	m_ColorsChanged = false;

	if( bNeedsUpdate || m_SpritesChanged || m_BlankChanged )
	{
		UpdateScreen( );

		m_BlankChanged = false;

		return true;
	}

	return false;
}

void cSdlTMS9918A::Render( )
{
	FUNCTION_ENTRY( this, "cSdlTMS9918A::Render", false );

	SDL_LockMutex( m_Mutex );

	UINT32 background = m_ColorTable[ 0 ];

	SDL_SetRenderDrawColor( m_sdlRenderer, COLOR_R( background ), COLOR_G( background ), COLOR_B( background ), 255 );

	SDL_RenderClear( m_sdlRenderer );

	if( m_ScreenSource != nullptr )
	{
		SDL_UpdateTexture( m_sdlTexture, nullptr, m_ScreenSource->GetData( ), m_ScreenSource->Pitch( ));

		SDL_RenderCopy( m_sdlRenderer, m_sdlTexture, nullptr, nullptr );
	}

	SDL_RenderPresent( m_sdlRenderer );

	SDL_UnlockMutex( m_Mutex );
}

//----------------------------------------------------------------------------

void cSdlTMS9918A::CreateMainWindow( int width, int height, int scale )
{
	FUNCTION_ENTRY( this, "cSdlTMS9918A::CreateMainWindow", true );

	DBG_TRACE( "Geometry: " << width << "x" << height );

	if( m_sdlWindow == nullptr )
	{
		SDL_SetHint( SDL_HINT_RENDER_SCALE_QUALITY, "closest" );

		width = std::max( width, VDP_WIDTH * scale );
		height = std::max( height, VDP_HEIGHT * scale );

		m_sdlWindow = SDL_CreateWindow( "TI-99/sim", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
								  width, height, SDL_WINDOW_RESIZABLE );

		m_sdlRenderer = SDL_CreateRenderer( m_sdlWindow, -1, SDL_RENDERER_PRESENTVSYNC );

		SDL_RenderSetLogicalSize( m_sdlRenderer, VDP_WIDTH * scale, VDP_HEIGHT * scale );
		SDL_RenderSetIntegerScale( m_sdlRenderer, SDL_TRUE );

		m_sdlTexture = SDL_CreateTexture( m_sdlRenderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, VDP_WIDTH * scale, VDP_HEIGHT * scale );

		m_FullScreen = false;
	}
}

void cSdlTMS9918A::CreateMainWindowFullScreen( int scale )
{
	FUNCTION_ENTRY( this, "cSdlTMS9918A::CreateMainWindowFullScreen", true );

	if( m_sdlWindow == nullptr )
	{
		SDL_ShowCursor( SDL_DISABLE );

		SDL_SetHint( SDL_HINT_RENDER_SCALE_QUALITY, "nearest" );

		SDL_CreateWindowAndRenderer( 0, 0, SDL_WINDOW_FULLSCREEN_DESKTOP, &m_sdlWindow, &m_sdlRenderer );

		SDL_RenderSetLogicalSize( m_sdlRenderer, VDP_WIDTH * scale, VDP_HEIGHT * scale );

		m_sdlTexture = SDL_CreateTexture( m_sdlRenderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, VDP_WIDTH * scale, VDP_HEIGHT * scale );

		m_FullScreen = true;
	}
}

void cSdlTMS9918A::ResizeWindow( int width, int height )
{
	FUNCTION_ENTRY( this, "cSdlTMS9918A::ResizeWindow", true );

	if( m_FullScreen == false )
	{
		SDL_LockMutex( m_Mutex );

		Render( );

		SDL_UnlockMutex( m_Mutex );
	}
}

void cSdlTMS9918A::SetColorTable( sRGBQUAD colorTable[ 17 ] )
{
	FUNCTION_ENTRY( this, "cSdlTMS9918A::SelectColorTable", true );

	if( memcmp( m_ColorTable, colorTable, sizeof( m_ColorTable )) != 0 )
	{
		memcpy( m_ColorTable, colorTable, sizeof( m_ColorTable ));

		SDL_LockMutex( m_Mutex );

		m_ChangesMade    = true;
		m_SpritesChanged = true;
		memset( m_ScreenChanged, true, sizeof( m_ScreenChanged ));
		memset( m_PatternChanged, true, sizeof( m_PatternChanged ));

		SDL_UnlockMutex( m_Mutex );
	}
}

void cSdlTMS9918A::SetFrameRate( int onFrames, int offFrames )
{
	FUNCTION_ENTRY( this, "cSdlTMS9918A::SetFrequency", true );

	DBG_ASSERT( onFrames > 0 );
	DBG_ASSERT( offFrames >= 0 );

	// Start off on an "on" frame
	m_FrameCycle = onFrames;
	m_OnFrames   = onFrames;
	m_OffFrames  = offFrames;
}

void cSdlTMS9918A::UpdateCharacterPatternGraphics( int ch, UINT8 fore, UINT8 back, UINT8 *pattern )
{
	FUNCTION_ENTRY( this, "cSdlTMS9918A::UpdateCharacterPatternGraphics", false );

	DBG_ASSERT( ch < 256 );

	UINT8 *pData = m_CharacterPattern[ ch ];

	for( int y = 0; y < 8; y++ )
	{
		UINT8 row = *pattern++;
		*pData++ = ( row & 0x80 ) ? fore : back;
		*pData++ = ( row & 0x40 ) ? fore : back;
		*pData++ = ( row & 0x20 ) ? fore : back;
		*pData++ = ( row & 0x10 ) ? fore : back;
		*pData++ = ( row & 0x08 ) ? fore : back;
		*pData++ = ( row & 0x04 ) ? fore : back;
		*pData++ = ( row & 0x02 ) ? fore : back;
		*pData++ = ( row & 0x01 ) ? fore : back;
	}
}

void cSdlTMS9918A::UpdateCharacterPatternBitMap( int ch, UINT8 *pattern )
{
	FUNCTION_ENTRY( this, "cSdlTMS9918A::UpdateCharacterPatternBitMap", false );

	DBG_ASSERT( ch < 3 * 256 );

	UINT8 *pColor = (( sBitmapColorTable * ) ( m_Memory + m_ColorTableIndex ))->data[ ch & m_ColorTableMask ];
	UINT8 *pData = m_CharacterPattern[ ch ];

	for( int y = 0; y < 8; y++ )
	{
		UINT8 row  = *pattern++;
		UINT8 fore = ( UINT8 ) ( pColor[ y ] >> 4 );
		UINT8 back = ( UINT8 ) ( pColor[ y ] & 0x0F );
		*pData++ = ( row & 0x80 ) ? fore : back;
		*pData++ = ( row & 0x40 ) ? fore : back;
		*pData++ = ( row & 0x20 ) ? fore : back;
		*pData++ = ( row & 0x10 ) ? fore : back;
		*pData++ = ( row & 0x08 ) ? fore : back;
		*pData++ = ( row & 0x04 ) ? fore : back;
		*pData++ = ( row & 0x02 ) ? fore : back;
		*pData++ = ( row & 0x01 ) ? fore : back;
    }
}

void cSdlTMS9918A::UpdateScreenGraphics( int x, int y, int ch )
{
	FUNCTION_ENTRY( this, "cSdlTMS9918A::UpdateScreenGraphics", false );

	DBG_ASSERT( x < 32 );
	DBG_ASSERT( y < 24 );
	DBG_ASSERT( ch < 3 * 256 );

	UINT8 *pSrcData = m_CharacterPattern[ ch ];
	UINT32 *pDstData = m_BitmapScreen->GetData( );

	pDstData += (( y * 8 ) * VDP_WIDTH + ( x * 8 ));

	for( y = 0; y < 8; y++ )
	{
		UINT32 *pDst = pDstData;
		for( x = 0; x < 8; x++ )
		{
			*pDst++ = m_ColorTable[ *pSrcData++ ];
		}
		pDstData += VDP_WIDTH;
	}
}

void cSdlTMS9918A::UpdateScreenText( int x, int y, int ch )
{
	FUNCTION_ENTRY( this, "cSdlTMS9918A::UpdateScreenText", false );

	DBG_ASSERT( x < 40 );
	DBG_ASSERT( y < 24 );
	DBG_ASSERT( ch < 256 );

	UINT8 *pSrcData = m_CharacterPattern[ ch ];
	UINT32 *pDstData = m_BitmapScreen->GetData( );

	pDstData += y * 8 * VDP_WIDTH + x * 6 + 8;

	for( y = 0; y < 8; y++ )
	{
		UINT32 *pDst = pDstData;
		for( x = 0; x < 6; x++ )
		{
			*pDst++ = m_ColorTable[ *pSrcData++ ];
		}
		pSrcData += 2;
		pDstData += VDP_WIDTH;
	}
}

void cSdlTMS9918A::UpdateScreenMultiColor( int x, int y, int ch )
{
	FUNCTION_ENTRY( this, "cSdlTMS9918A::UpdateScreenMultiColor", false );

	DBG_ASSERT( x < 32 );
	DBG_ASSERT( y < 24 );
	DBG_ASSERT( ch < 256 );

	UINT8 back = ( UINT8 ) ( m_Register[ 7 ] & 0x0F );

	UINT8 *pSrcData = &(( sPatternDescriptor * ) ( m_Memory + m_PatternTableIndex ))->data[ ch ][( y & 0x03 ) * 2 ];
	UINT32 *pDstData = m_BitmapScreen->GetData( );

	pDstData += y * 8 * VDP_WIDTH + x * 8;

	for( int i = 0; i < 2; i++ )
	{
		int leftColor  = ( *pSrcData & 0xF0 ) ? *pSrcData >> 4 : back;
		int rightColor = ( *pSrcData & 0x0F ) ? *pSrcData & 0x0F : back;

		for( y = 0; y < 4; y++ )
		{
			UINT32 *pDst = pDstData;
			for( x = 0; x < 4; x++ )
			{
				*pDst++ = m_ColorTable[ leftColor ];
			}
			for( x = 0; x < 4; x++ )
			{
				*pDst++ = m_ColorTable[ rightColor ];
			}
			pDstData += VDP_WIDTH;
		}

		pSrcData++;
	}
}

void cSdlTMS9918A::MarkScreenChanges( int ch )
{
	FUNCTION_ENTRY( this, "cSdlTMS9918A::MarkScreenChanges", false );

	DBG_ASSERT( ch < 3 * 256 );

	UINT8 *screen = ( UINT8 * ) ( m_Memory + m_ImageTableIndex );
	bool *changed = m_ScreenChanged;
	int count     = m_CharUse[ ch ];
	int max       = m_ImageTableSize;

	if( m_Mode & VDP_M3 )
	{
		int index = ch & 0xFF00;
		screen  += index;
		changed += index;
		ch  &= 0xFF;
		max /= 3;
	}

	for( int i = max; i; i-- )
	{
		if( *screen++ == ch )
		{
			*changed = true;
			if( --count == 0 )
			{
				break;
			}
		}
		changed++;
	}
}

void cSdlTMS9918A::DrawSprite( int index )
{
	FUNCTION_ENTRY( this, "cSdlTMS9918A::DrawSprite", false );

	sSpriteAttributeEntry *sprite = &(( sSpriteAttribute * ) ( m_Memory + m_SpriteAttrTableIndex ))->data[ index ];

	UINT8 colorIndex = ( UINT8 ) ( sprite->earlyClock & 0x0F );
	if( colorIndex == 0 )
	{
		return;
	}

	int count = ( m_Register[ 1 ] & VDP_SPRITE_SIZE ) ? 4 : 1;
	int size  = ( m_Register[ 1 ] & VDP_SPRITE_MAGNIFY ) ? 16 : 8;

	UINT32 *pScreen = m_BitmapSpriteScreen->GetData( );

	for( int i = 0; i < count; i++ )
	{
		int posX = ( int ) sprite->posX + ( i / 2 ) * size;
		if( sprite->earlyClock & 0x80 )
		{
			posX -= 32;
		}

		if( posX >= VDP_WIDTH )
		{
			continue;
		}

		UINT8 row       = ( UINT8 ) ( sprite->posY + 1 + ( i % 2 ) * size );
		UINT32 *pDstData = pScreen + posX;
		UINT8 *pattern  = (( sSpriteDescriptor * ) ( m_Memory + m_SpriteDescTableIndex ))->data[( sprite->patternIndex + i ) % 256 ];

		for( int y = 0; y < size; y++, row++ )
		{
			// Make sure the current row and sprite are visible
			if(( row < VDP_HEIGHT ) && ( index <= m_MaxSprite[ row ] ))
			{
				UINT8 bits   = *pattern;
				UINT32 *pData = pDstData + row * VDP_WIDTH;

				if( bits )
				{
					for( int x = 0, col = posX; x < size; x++, col++ )
					{
						if( col >= VDP_WIDTH )
						{
							break;
						}
						if(( bits & 0x80 ) && ( col >= 0 ))
						{
							*pData = m_ColorTable[ colorIndex ];
						}
						pData++;
						if(( size == 8 ) || ( x & 1 ))
						{
							bits <<= 1;
						}
					}
				}
			}

			if(( size == 8 ) || ( y & 1 ))
			{
				pattern++;
			}
		}
	}
}

bool cSdlTMS9918A::RefreshInvalid( )
{
	FUNCTION_ENTRY( this, "cSdlTMS9918A::RefreshInvalid", false );

	bool needsUpdate = m_ColorsChanged;

	if( needsUpdate )
	{
		m_ColorsChanged = false;

		UINT8 fore = ( UINT8 ) ( m_Register[ 7 ] >> 4 );
		UINT8 back = ( UINT8 ) ( m_Register[ 7 ] & 0x0F );

		m_BitmapScreen->Fill( m_ColorTable[ back ]);

		for( int y = 0; y < 192; y++ )
		{
			for( int x = 0; x < 40 * 6; x++ )
			{
				UINT32 *pDst = m_BitmapScreen->GetData( ) + y * m_BitmapScreen->Width( ) + ( 8 + x );

				UINT8 srcColor = ( x % 6 < 4 ) ? fore : back;

				*pDst = m_ColorTable[ srcColor ];
			}
		}
	}

	return needsUpdate;
}

bool cSdlTMS9918A::RefreshGraphics( )
{
	FUNCTION_ENTRY( this, "cSdlTMS9918A::RefreshGraphics", false );

	// Check for changes in the character patterns
	UINT8 fore = ( UINT8 ) ( m_Register[ 7 ] >> 4 );
	UINT8 back = ( UINT8 ) ( m_Register[ 7 ] & 0x0F );

	for( int ch = 0; ch < 256; ch++ )
	{
		if( !m_TextMode && (( ch & 0x07 ) == 0 ))
		{
			fore = ( UINT8 ) ((( sColorTable * ) ( m_Memory + m_ColorTableIndex ))->data[ ch / 8 ] >> 4 );
			back = ( UINT8 ) ((( sColorTable * ) ( m_Memory + m_ColorTableIndex ))->data[ ch / 8 ] & 0x0F );
		}
		if( m_PatternChanged[ ch ] && m_CharUse[ ch ] )
		{
			UpdateCharacterPatternGraphics( ch, fore, back, ( UINT8 * ) &(( sPatternDescriptor * ) ( m_Memory + m_PatternTableIndex ))->data[ ch ] );
			m_PatternChanged[ ch ] = false;
			MarkScreenChanges( ch );
		}
	}

	bool needsUpdate = false;
	bool *changed = m_ScreenChanged;
	UINT8 *chr = ( UINT8 * ) ( m_Memory + m_ImageTableIndex );

	int height = GetScreenHeight( );
	int width  = GetScreenWidth( );
	for( int y = 0; y < height; y++ )
	{
		for( int x = 0; x < width; x++ )
		{
			if( *changed++ )
			{
				if( m_TextMode )
				{
					UpdateScreenText( x, y, *chr );
				}
				else
				{
					UpdateScreenGraphics( x, y, *chr );
				}
				needsUpdate = true;
			}
			chr++;
		}
	}

	if( needsUpdate )
	{
		memset( m_ScreenChanged, false, sizeof( m_ScreenChanged ));
	}

	if( m_TextMode && m_ColorsChanged )
	{
		UINT32 *pDstData = m_BitmapScreen->GetData( );

		UINT32 *pLeft    = pDstData;
		UINT32 *pRight   = pDstData + ( 40 * 6 + 8 );

		UINT32 *pDst = pDstData;
		for( int x = 0; x < 8; x++ )
		{
			*pDst++ = m_ColorTable[ back ];
		}

		for( int y = 0; y < height * 8; y++ )
		{
			memcpy( pLeft, pDstData, 8 * 4 );
			memcpy( pRight, pDstData, 8 * 4 );
			pLeft  += VDP_WIDTH;
			pRight += VDP_WIDTH;
		}

		m_ColorsChanged = false;
	}

	return needsUpdate;
}

bool cSdlTMS9918A::RefreshBitMap( )
{
	FUNCTION_ENTRY( this, "cSdlTMS9918A::RefreshBitMap", false );

	// Check for changes in the character patterns
	for( int ch = 0; ch < 3 * 256; ch++ )
	{
		if( m_PatternChanged[ ch ] && m_CharUse[ ch ] )
		{
			UpdateCharacterPatternBitMap( ch, ( UINT8 * ) (( sPatternDescriptor * ) ( m_Memory + m_PatternTableIndex ))->data[ ch & m_PatternTableMask ] );
			m_PatternChanged[ ch ] = false;
			MarkScreenChanges( ch );
		}
	}

	bool needsUpdate = false;
	UINT8 *chr = ( UINT8 * ) ( m_Memory + m_ImageTableIndex );

	for( unsigned int i = 0; i < m_ImageTableSize; i++ )
	{
		if( m_ScreenChanged[ i ] )
		{
			if( m_TextMode )
			{
				UpdateScreenText( i % 40, i / 40, chr[ i ] );
			}
			else
			{
				UpdateScreenGraphics( i % 32, i / 32, ( i & 0xFF00 ) + chr[ i ] );
			}
			needsUpdate = true;
		}
	}

	if( needsUpdate )
	{
		memset( m_ScreenChanged, false, sizeof( m_ScreenChanged ));
	}

	return needsUpdate;
}

bool cSdlTMS9918A::RefreshMultiColor( )
{
	FUNCTION_ENTRY( this, "cSdlTMS9918A::RefreshMultiColor", false );

	bool needsUpdate = false;

	UINT8 *chr = ( UINT8 * ) ( m_Memory + m_ImageTableIndex );

	for( unsigned int i = 0; i < m_ImageTableSize; i++ )
	{
		UINT8 index = chr[ i ];
		if( m_ScreenChanged[ i ] || m_PatternChanged[ index ] )
		{
			UpdateScreenMultiColor( i % 32, i / 32, index );
			needsUpdate = true;
		}
	}

	if( needsUpdate )
	{
		memset( m_ScreenChanged, false, sizeof( m_ScreenChanged ));
		memset( m_PatternChanged, false, sizeof( m_PatternChanged ));
	}

	return needsUpdate;
}

cBitMap *cSdlTMS9918A::UpdateSprites( )
{
	FUNCTION_ENTRY( this, "cSdlTMS9918A::UpdateSprites", false );

	sSpriteAttributeEntry *sprite = &(( sSpriteAttribute * ) ( m_Memory + m_SpriteAttrTableIndex))->data[ 0 ];

	int i;
	for( i = 0; i < 32; i++ )
	{
		if( sprite[ i ].posY == 0xD0 )
		{
			break;
		}
	}

	// Don't waste our time if there are no active sprites
	if( i == 0 )
	{
		return m_BitmapScreen;
	}

	m_BitmapSpriteScreen->Copy( m_BitmapScreen );

	// Draw sprites in reverse order (ie: lowest numbered sprite is on top)
	while( --i >= 0 )
	{
		DrawSprite( i );
	}

	m_SpritesChanged = false;

	return m_BitmapSpriteScreen;
}

void cSdlTMS9918A::UpdateScreen( )
{
	FUNCTION_ENTRY( this, "cSdlTMS9918A::UpdateScreen", false );

	SDL_LockMutex( m_Mutex );

	m_ScreenSource = m_TextMode ? m_BitmapScreen : UpdateSprites( );

	if( m_ScaledScreen )
	{
		m_ScaledScreen->Copy( m_ScreenSource );
		m_ScreenSource = m_ScaledScreen;
	}

	SDL_UnlockMutex( m_Mutex );
}

void cSdlTMS9918A::FlipAddressing( )
{
	cTMS9918A::FlipAddressing( );
	// The world just changed!
}

bool cSdlTMS9918A::SetMode( int mode )
{
	FUNCTION_ENTRY( this, "cSdlTMS9918A::SetMode", true );

	if( cTMS9918A::SetMode( mode ) == false )
	{
		return false;
	}

	m_TextMode = ( m_Mode & VDP_M1 ) ? true : false;

	// Force borders to repaint
	m_ColorsChanged = true;

	// We changed video modes, force a refresh of the screen
	m_ChangesMade = true;

	memset( m_PatternChanged, true, sizeof( m_PatternChanged ));
	memset( m_CharUse, 0, sizeof( m_CharUse ));

	UINT8 *chr = ( UINT8 * ) ( m_Memory + m_ImageTableIndex );

	for( unsigned int i = 0; i < m_ImageTableSize; i++ )
	{
		int index = ( m_Mode & VDP_M3 ) ? ( i & 0xFF00 ) : 0;
		m_CharUse[ index + *chr++ ]++;
	}

	return true;
}
