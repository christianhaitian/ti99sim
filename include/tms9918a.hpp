
//----------------------------------------------------------------------------
//
// File:		tms9918a.hpp
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

#ifndef TMS9918A_HPP_
#define TMS9918A_HPP_

#include "cBaseObject.hpp"
#include "stateobject.hpp"
#include "itms9918a.hpp"
#include "itms9900.hpp"

#define TI_TRANSPARENT			0x00
#define TI_BLACK				0x01
#define TI_MEDIUM_GREEN			0x02
#define TI_LIGHT_GREEN			0x03
#define TI_DARK_BLUE			0x04
#define TI_LIGHT_BLUE			0x05
#define TI_DARK_RED				0x06
#define TI_CYAN					0x07
#define TI_MEDIUM_RED			0x08
#define TI_LIGHT_RED			0x09
#define TI_DARK_YELLOW			0x0A
#define TI_LIGHT_YELLOW			0x0B
#define TI_DARK_GREEN			0x0C
#define TI_MAGENTA				0x0D
#define TI_GRAY					0x0E
#define TI_WHITE				0x0F

#define VDP_TIMER				0L
#define VPD_INTERRUPT_INTERVAL	17				// 60 Hz(16.666 msec)

#define VDP_WIDTH				256
#define VDP_HEIGHT				192

#define MEM_IMAGE_TABLE			0x01
#define MEM_PATTERN_TABLE		0x02
#define MEM_COLOR_TABLE			0x04
#define MEM_SPRITE_ATTR_TABLE	0x10
#define MEM_SPRITE_DESC_TABLE	0x20

// VDP Register 0
#define VDP_MODE_3_BIT			0x02
#define VDP_EXTERNAL_VIDEO_MASK 0x01

// VDP Register 1
#define VDP_16K_MASK			0x80
#define VDP_BLANK_MASK			0x40
#define VDP_INTERRUPT_MASK		0x20
#define VDP_MODE_1_BIT			0x10
#define VDP_MODE_2_BIT			0x08
#define VDP_SPRITE_SIZE			0x02
#define VDP_SPRITE_MAGNIFY		0x01
#define VDP_SPRITE_MASK			0x03

#define VDP_M3					0x01	// BitMapped
#define VDP_M2					0x02	// Multi-Color
#define VDP_M1					0x04	// Text

#define VDP_MODE_GRAPHICS_I			0x00
#define VDP_MODE_TEXT				0x04		// VDP_M1
#define VDP_MODE_MULTICOLOR			0x02		// VDP_M2
#define VDP_MODE_BITMAP				0x01		// VDP_M3
#define VDP_MODE_BITMAP_TEXT		0x03		// VDP_M2 | VDP_M3
#define VDP_MODE_BITMAP_MULTICOLOR	0x05		// VDP_M1 | VDP_M3
#define VDP_MODE_ILLEGAL			0x06		// VDP_M1 | VDP_M2

#define VDP_INTERRUPT_FLAG		0x80
#define VDP_FIFTH_SPRITE_FLAG	0x40
#define VDP_COINCIDENCE_FLAG	0x20
#define VDP_FIFTH_SPRITE_MASK	0x1F

struct sScreenImage
{
	union
	{
		UINT8                grapPos[ 24 ][ 32 ];
		UINT8                textPos[ 24 ][ 40 ];
		UINT8                data[ 0x0400 ];
	};
};

struct sColorTable
{
	UINT8                    data[ 64 ];
};

struct sBitmapColorTable
{
	UINT8                    data[ 256 ][ 8 ];
};

struct sPatternDescriptor
{
	UINT8                    data[ 256 ][ 8 ];
};

struct sSpriteAttributeEntry
{
	UINT8                    posY;
	UINT8                    posX;
	UINT8                    patternIndex;
	UINT8                    earlyClock;
};

struct sSpriteAttribute
{
	sSpriteAttributeEntry    data[ 32 ];
};

struct sSpriteDescriptor
{
	UINT8                    data[ 256 ][ 8 ];
};

class cTMS9918A :
	public virtual cBaseObject,
	public virtual cStateObject,
	public virtual iTMS9918A
{
protected:

	UINT8              *m_Memory;

	size_t              m_ImageTableIndex;
	size_t              m_ColorTableIndex;
	size_t              m_PatternTableIndex;
	size_t              m_SpriteAttrTableIndex;
	size_t              m_SpriteDescTableIndex;

	size_t              m_ImageTableSize;
	size_t              m_ColorTableSize;
	size_t              m_PatternTableSize;

	unsigned int        m_ColorTableMask;
	unsigned int        m_PatternTableMask;

	int                 m_InterruptLevel;
	iTMS9901           *m_PIC;

	ADDRESS             m_Address;
	UINT16              m_Transfer;
	UINT16              m_Shift;

	UINT8               m_Status;
	UINT8               m_Register[ 8 ];
	UINT8               m_Mode;

	UINT8               m_ReadAhead;

	UINT8               m_MemoryType[ 0x4000 ];

	UINT8               m_MaxSprite[ 256 ];
	bool                m_SpritesDirty;
	bool                m_SpritesRefreshed;
	bool                m_CoincidenceFlag;
	bool                m_FifthSpriteFlag;
	int                 m_FifthSpriteIndex;

	unsigned int        m_RefreshRate;

public:

	cTMS9918A( int = 60 );

	// iBaseObject Methods
	virtual const void *GetInterface( const std::string &name ) const override;

	// iStateObject Methods
	virtual std::string GetIdentifier( ) override;
	virtual std::optional<sStateSection> SaveState( ) override;
	virtual bool ParseState( const sStateSection &state ) override;

	// iTMS9918A Methods
	virtual void SetMemory( UINT8 * ) override;
	virtual void SetPIC( iTMS9901 *pic, int level ) override;
	virtual void Reset( ) override;
	virtual void SetAddress( UINT8 address ) override;
	virtual UINT16 GetAddress( ) override;
	virtual void WriteData( UINT8 data ) override;
	virtual void WriteRegister( size_t reg, UINT8 value ) override;
	virtual UINT8 ReadData( ) override;
	virtual UINT8 ReadRegister( size_t reg ) override;
	virtual UINT8 ReadStatus( ) override;
	virtual bool Retrace( ) override;
	virtual void Render( ) override;

	bool BlankEnabled( )						{ return ( m_Register[ 1 ] & VDP_BLANK_MASK ) ? false : true; }
	bool InterruptsEnabled( )					{ return ( m_Register[ 1 ] & VDP_INTERRUPT_MASK ) ? true : false; }

	unsigned int GetRefreshRate( )				{ return m_RefreshRate; }

	int GetMode( ) const						{ return m_Mode; }
	UINT8  *GetMemory( ) const					{ return m_Memory; }

	ADDRESS GetImageTable( ) const				{ return static_cast<ADDRESS>( m_ImageTableIndex ); }
	ADDRESS GetColorTable( ) const				{ return static_cast<ADDRESS>( m_ColorTableIndex ); }
	ADDRESS GetPatternTable( ) const			{ return static_cast<ADDRESS>( m_PatternTableIndex ); }
	ADDRESS GetSpriteAttrTable( ) const			{ return static_cast<ADDRESS>( m_SpriteAttrTableIndex ); }
	ADDRESS GetSpriteDescTable( ) const			{ return static_cast<ADDRESS>( m_SpriteDescTableIndex ); }

protected:

	virtual bool SetMode( int );
	virtual void FlipAddressing( );

	void FillTable( size_t, size_t, UINT8 );

	void GetSpritePattern( int index, int loX, int hiX, int loY, int hiY, int data[ 32 ] );
	bool SpritesCoincident( int, int );
	bool CheckCoincidence( const bool[ 32 ] );

	void CheckSprites( );

protected:

	virtual ~cTMS9918A( ) override;

private:

	cTMS9918A( const cTMS9918A & ) = delete;				// no implementation
	cTMS9918A &operator =( const cTMS9918A & ) = delete;	// no implementation

};

#endif
