//----------------------------------------------------------------------------
//
// File:		tms9918a-sdl.hpp
// Date:		02-Apr-2000
// Programmer:	Marc Rousseau
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
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
//
// Revision History:
//
//----------------------------------------------------------------------------

#ifndef TMS9918A_SDL_HPP_
#define TMS9918A_SDL_HPP_

#include "tms9918a.hpp"

class cBitMap;

#define FILL_SIZE		256

#define COLOR_R(color)	( static_cast<UINT8>( color >>  0 ))
#define COLOR_G(color)	( static_cast<UINT8>( color >>  8 ))
#define COLOR_B(color)	( static_cast<UINT8>( color >> 16 ))
#define COLOR_A(color)	( static_cast<UINT8>( color >> 24 ))

struct sRGBQUAD
{
	UINT8         r;
	UINT8         g;
	UINT8         b;
	UINT8         a;
};

class cSdlTMS9918A :
	public cTMS9918A
{
	UINT32        m_ColorTable[ 17 ];

	bool          m_TextMode;
	bool          m_ChangesMade;
	bool          m_BlankChanged;
	bool          m_ColorsChanged;
	bool          m_SpritesChanged;

	bool          m_ScreenChanged[ 0x03C0 ];

	bool          m_PatternChanged[ 256 * 3 ];
	int           m_CharUse        [ 256 * 3 ];
	int           m_SpriteCharUse  [ 256 ];

	bool          m_Scale2x;

	SDL_Window   *m_sdlWindow;
	SDL_Renderer *m_sdlRenderer;
	SDL_Texture  *m_sdlTexture;

	cBitMap      *m_ScreenSource;
	cBitMap      *m_ScaledScreen;
	cBitMap      *m_BitmapScreen;
	cBitMap      *m_BitmapSpriteScreen;
	UINT8         m_CharacterPattern[ 3 * 256 ][ 8 * 8 ];

	SDL_mutex    *m_Mutex;

	bool          m_FullScreen;

	int           m_OnFrames;
	int           m_OffFrames;
	int           m_FrameCycle;

public:

	cSdlTMS9918A( sRGBQUAD[ 17 ], int = 60, bool = false, bool = false, int = 1 );

	// iStateObject methods
	virtual bool ParseState( const sStateSection &state ) override;

	// iTMS9918A methods
	virtual void Reset( ) override;
	virtual void WriteData( UINT8 ) override;
	virtual void WriteRegister( size_t, UINT8 ) override;
	virtual bool Retrace( ) override;
	virtual void Render( ) override;

	void SetColorTable( sRGBQUAD[ 17 ] );
	void SetFrameRate( int, int );

	void ResizeWindow( int x, int y );

	cBitMap *GetScreen( );

protected:

	void CreateMainWindow( int, int, int );
	void CreateMainWindowFullScreen( int );

	void DrawSprite( int );
	cBitMap *UpdateSprites( );

	void UpdateCharacterPatternGraphics( int, UINT8, UINT8, UINT8 * );
	void UpdateCharacterPatternBitMap( int, UINT8 * );

	void UpdateScreenGraphics( int x, int y, int ch );
	void UpdateScreenText( int x, int y, int ch );
	void UpdateScreenMultiColor( int x, int y, int ch );

	int ScreenSize( )		{ return m_TextMode ? 24 * 40 : 24 * 32; }
	int GetCellWidth( )		{ return m_TextMode ? 6 : 8; }
	int GetScreenWidth( )	{ return m_TextMode ? 40 : 32; }
	int GetScreenHeight( )	{ return 24; }

	void MarkScreenChanges( int );

	bool RefreshInvalid( );
	bool RefreshGraphics( );
	bool RefreshBitMap( );
	bool RefreshMultiColor( );

	void BlankScreen( );
	void UpdateScreen( );

	// cTMS9918A protected methods
	virtual bool SetMode( int ) override;
	virtual void FlipAddressing( ) override;

protected:

	virtual ~cSdlTMS9918A( ) override;

private:

	cSdlTMS9918A( const cSdlTMS9918A & ) = delete;		// no implementation
	void operator =( const cSdlTMS9918A & ) = delete;	// no implementation

};

#endif
