//----------------------------------------------------------------------------
//
// File:        tms9918a.cpp
// Date:        23-Feb-1998
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

#include <algorithm>
#include <cstring>
#include "common.hpp"
#include "logger.hpp"
#include "compress.hpp"
#include "tms9918a.hpp"
#include "idevice.hpp"
#include "itms9901.hpp"
#include "support.hpp"

DBG_REGISTER( __FILE__ );

extern void Panic( char * );

cTMS9918A::cTMS9918A( int refreshRate ) :
	cBaseObject( "cTMS9918A" ),
	cStateObject( ),
	m_Memory( nullptr ),
	m_ImageTableIndex( 0 ),
	m_ColorTableIndex( 0 ),
	m_PatternTableIndex( 0 ),
	m_SpriteAttrTableIndex( 0 ),
	m_SpriteDescTableIndex( 0 ),
	m_ImageTableSize( 0 ),
	m_ColorTableSize( 0 ),
	m_PatternTableSize( 0 ),
	m_ColorTableMask( 0 ),
	m_PatternTableMask( 0 ),
	m_InterruptLevel( 0 ),
	m_PIC( nullptr ),
	m_Address( 0 ),
	m_Transfer( 0 ),
	m_Shift( 0 ),
	m_Status( 0 ),
	m_Register( ),
	m_Mode( 0 ),
	m_ReadAhead( 0 ),
	m_MemoryType( ),
	m_MaxSprite( ),
	m_SpritesDirty( false ),
	m_SpritesRefreshed( false ),
	m_CoincidenceFlag( false ),
	m_FifthSpriteFlag( false ),
	m_FifthSpriteIndex( 0 ),
	m_RefreshRate( refreshRate )
{
	FUNCTION_ENTRY( this, "cTMS9918A ctor", true );

	// Let Reset do the real initialization
	Reset( );
}

cTMS9918A::~cTMS9918A( )
{
	FUNCTION_ENTRY( this, "cTMS9918A dtor", true );
}

//----------------------------------------------------------------------------
// iBaseObject Methods
//----------------------------------------------------------------------------

const void *cTMS9918A::GetInterface( const std::string &iName ) const
{
	FUNCTION_ENTRY( this, "cTMS9918A::GetInterface", false );

	if( iName == "iTMS9918A" )
	{
		return static_cast< const iTMS9918A *>( this );
	}

	return cStateObject::GetInterface( iName );
}

void cTMS9918A::SetMemory( UINT8 *pMemory )
{
	m_Memory = pMemory;
}

void cTMS9918A::SetPIC( iTMS9901 *pic, int level )
{
	FUNCTION_ENTRY( this, "cTMS9918A::SetPIC", true );

	m_InterruptLevel = level;
	m_PIC            = pic;
}

void cTMS9918A::Reset( )
{
	FUNCTION_ENTRY( this, "cTMS9918A::Reset", true );

	m_Status = 0;

	// Set mode to ZERO here so we don't actually do any mode switch stuff
	m_Mode      = 0xFF;
	m_ReadAhead = 0x00;
	m_Address   = m_Transfer = m_Shift = 0;

	memset( m_Register, -1, sizeof( m_Register ));

	for( unsigned i = 0; i < SIZE( m_Register ); i++ )
	{
		WriteRegister( i, 0 );
	}
}

extern int HistoryIndex;
extern char HistoryBuffer[ 1024 ][ 80 ];

void cTMS9918A::SetAddress( UINT8 data )
{
	FUNCTION_ENTRY( this, "cTMS9918A::SetAddress", false );

	if( m_Shift == 0 )
	{
		m_Transfer &= 0xFF00;
		m_Transfer |= data;
		m_Shift = 8;
	}
	else
	{
		m_Transfer &= 0x00FF;
		m_Transfer |= data << 8;
		m_Shift = 0;
		if( m_Transfer & 0x8000 )
		{
			if( data & 0x08 )
			{
				DBG_ERROR( "Unexpected register value: " << hex << data );
			}
			WriteRegister( data & 0x07, ( UINT8 ) m_Transfer );
		}
		else
		{
			m_Address = m_Transfer & 0x3FFF;
			if(( m_Transfer & 0x4000 ) == 0 )
			{
				m_ReadAhead = m_Memory[ m_Address++ ];
			}
		}
	}
}

UINT16 cTMS9918A::GetAddress( )
{
	FUNCTION_ENTRY( this, "cTMS9918A::GetAddress", false );

	return ( UINT16 ) ( m_Address & 0x3FFF );
}

void cTMS9918A::WriteData( UINT8 data )
{
	FUNCTION_ENTRY( this, "cTMS9918A::WriteData", false );

	m_Shift = 0;

	UINT8 *MemPtr = &m_Memory[ m_Address & 0x3FFF ];

	if( *MemPtr != data )
	{
		int type = m_MemoryType[ m_Address & 0x3FFF ];
		if( type & ( MEM_SPRITE_ATTR_TABLE | MEM_SPRITE_DESC_TABLE ))
		{
			m_SpritesDirty = true;
		}
		*MemPtr = data;
	}

	m_Address += 1;
}

UINT8 cTMS9918A::ReadData( )
{
	FUNCTION_ENTRY( this, "cTMS9918A::ReadData", false );

	m_Shift = 0;

	UINT8 retVal = m_ReadAhead;

	m_ReadAhead = m_Memory[ m_Address++ & 0x3FFF ];

	return retVal;
}

void cTMS9918A::WriteRegister( size_t reg, UINT8 value )
{
	FUNCTION_ENTRY( this, "cTMS9918A::WriteRegister", true );

	static UINT8 mask[ 8 ] =
	{
		0xFF, 0xFF, 0x0F, 0xFF, 0x07, 0x7F, 0x07, 0xFF
	};

	DBG_ASSERT( reg < 8 );

	// Mask off unused bits - TI-99/4A ROM sets these bits to 1s
	value &= mask[ reg ];

	UINT8 changes = m_Register[ reg ] ^ value;

	m_Register[ reg ] = value;

	int newMode = m_Mode;

	switch( reg )
	{
		case 0 :
			m_ColorTableSize   = ( value & VDP_MODE_3_BIT ) ? 0x1800 : 0x0020;
			m_PatternTableSize = ( value & VDP_MODE_3_BIT ) ? 0x1800 : 0x0800;
			newMode &= ~VDP_M3;
			if( value & VDP_MODE_3_BIT )
			{
				newMode |= VDP_M3;
			}
			if( newMode != m_Mode )
			{
				value  = m_Register[ 3 ] & mask[ 3 ];
				m_ColorTableIndex = ( newMode & VDP_M3 ) ? ( value & 0x80 ) ? 0x2000 : 0 : value * sizeof( sColorTable );
				DBG_ASSERT( m_ColorTableIndex <= 0x4000 - sizeof( sColorTable ));
				value = m_Register[ 4 ] & mask[ 4 ];
				m_PatternTableIndex = ( newMode & VDP_M3 ) ? ( value & 0x04 ) ? 0x2000 : 0 : value * sizeof( sPatternDescriptor );
				DBG_ASSERT( m_PatternTableIndex <= 0x4000 - sizeof( sPatternDescriptor ));
			}
			SetMode( newMode );
			break;
		case 1 :
			m_ImageTableSize = ( value & VDP_MODE_1_BIT ) ? 0x03C0 : 0x0300;
			newMode &= ~( VDP_M2 | VDP_M1 );
			if( value & VDP_MODE_2_BIT )
			{
				newMode |= VDP_M2;
			}
			if( value & VDP_MODE_1_BIT )
			{
				newMode |= VDP_M1;
			}
			if( changes & VDP_SPRITE_MASK )
			{
				m_SpritesDirty = true;
			}
			SetMode( newMode );
			if(( value & VDP_INTERRUPT_MASK ) && ( m_Status & VDP_INTERRUPT_FLAG ) && ( m_PIC != nullptr ))
			{
				m_PIC->SignalInterrupt( m_InterruptLevel );
			}
			if( changes & VDP_16K_MASK )
			{
				FlipAddressing( );
			}
			break;
		case 2 :
			m_ImageTableIndex = value * 0x0400;
			break;
		case 3 :
			m_ColorTableIndex = ( m_Mode & VDP_M3 ) ? ( value & 0x80 ) ? 0x2000 : 0 : value * sizeof( sColorTable );
			DBG_ASSERT( m_ColorTableIndex <= 0x4000 - sizeof( sColorTable ));
			break;
		case 4 :
			m_PatternTableIndex = ( m_Mode & VDP_M3 ) ? ( value & 0x04 ) ? 0x2000 : 0 : value * sizeof( sPatternDescriptor );
			DBG_ASSERT( m_PatternTableIndex <= 0x4000 - sizeof( sPatternDescriptor ) );
			break;
		case 5 :
			m_SpriteAttrTableIndex = value * sizeof( sSpriteAttribute );
			DBG_ASSERT( m_SpriteAttrTableIndex <= 0x4000 - sizeof( sSpriteAttribute ));
			break;
		case 6 :
			m_SpriteDescTableIndex = value * sizeof( sSpriteDescriptor );
			DBG_ASSERT( m_SpriteDescTableIndex <= 0x4000 - sizeof( sSpriteDescriptor ) );
			break;
	}

	m_ColorTableMask = (( m_Register[ 3 ] & 0x7Fu ) << 3 ) | 0x0007u;
	m_PatternTableMask = (( m_Register[ 4 ] & 0x03u ) << 8 ) | (( m_Register[ 1 ] & 0x10 ) ? 0x00FF : ( m_ColorTableMask & 0x00FF ));

	memset( m_MemoryType, 0, SIZE( m_MemoryType ));
	FillTable( m_ImageTableIndex, m_ImageTableSize, MEM_IMAGE_TABLE );
	FillTable( m_ColorTableIndex, m_ColorTableSize, MEM_COLOR_TABLE );
	FillTable( m_PatternTableIndex, m_PatternTableSize, MEM_PATTERN_TABLE );
	FillTable( m_SpriteAttrTableIndex, sizeof( sSpriteAttribute ), MEM_SPRITE_ATTR_TABLE );
	FillTable( m_SpriteDescTableIndex, sizeof( sSpriteDescriptor ), MEM_SPRITE_DESC_TABLE );
}

UINT8 cTMS9918A::ReadRegister( size_t reg )
{
	FUNCTION_ENTRY( this, "cTMS9918A::ReadRegister", false );

	return m_Register[ reg ];
}

UINT8 cTMS9918A::ReadStatus( )
{
	FUNCTION_ENTRY( this, "cTMS9918A::ReadStatus", false );

	m_Shift = 0;

	// Don't check any sprite related info unless we've redrawn the screen at least once
	if( m_SpritesRefreshed == true )
	{
		m_SpritesRefreshed = false;

		// All the sprite related bits in m_Status are still 0 so we can just OR things in
		if( m_CoincidenceFlag )
		{
			m_Status |= VDP_COINCIDENCE_FLAG;
		}
		if( m_FifthSpriteFlag )
		{
			m_Status |= VDP_FIFTH_SPRITE_FLAG;
		}

		m_Status |= m_FifthSpriteIndex;
	}

	UINT8 retVal = m_Status;

	m_Status = 0;

	if( m_PIC != nullptr )
	{
		m_PIC->ClearInterrupt( m_InterruptLevel );
	}

	return retVal;
}

bool cTMS9918A::SetMode( int newMode )
{
	FUNCTION_ENTRY( this, "cTMS9918A::SetMode", true );

	if( newMode == m_Mode )
	{
		return false;
	}

	m_Mode = ( UINT8 ) ( newMode & ( VDP_M3 | VDP_M2 | VDP_M1 ));

	return true;
}

void cTMS9918A::FlipAddressing( )
{
	FUNCTION_ENTRY( this, "cTMS9918A::FlipAddressing", false );

	if( m_Memory != nullptr )
	{
		UINT8 newMemory[ 0x4000 ];

		if( m_Register[ 1 ] & VDP_16K_MASK )
		{
			for( int x = 0; x < 0x4000; x++ )
			{
				int y = ( x & 0x2000 ) | (( x & 0x0FC0 ) << 1 ) | (( x & 0x1000 ) >> 6 ) | ( x & 0x003F );
				newMemory[ x ] = m_Memory[ y ];
			}
		}
		else
		{
			for( int x = 0; x < 0x4000; x++ )
			{
				int y = ( x & 0x2000 ) | (( x & 0x0040 ) << 6 ) | (( x & 0x1F80 ) >> 1 ) | ( x & 0x003F );
				newMemory[ x ] = m_Memory[ y ];
			}
		}

		memcpy( m_Memory, newMemory, 0x4000 );
	}
}

void cTMS9918A::FillTable( size_t start, size_t length, UINT8 type )
{
	FUNCTION_ENTRY( this, "cTMS9918A::FillTable", false );

	size_t end = start + length;
	if( end > 0x4000 )
	{
		end = 0x4000;
	}
	for( size_t i = start; i < end; i++ )
	{
		m_MemoryType[ i ] |= type;
	}
}

void cTMS9918A::GetSpritePattern( int index, int loX, int hiX, int loY, int hiY, int data[ 32 ] )
{
	FUNCTION_ENTRY( this, "cTMS9918A::GetSpritePattern", false );

	sSpriteAttributeEntry *sprite = &reinterpret_cast<sSpriteAttribute *>( m_Memory + m_SpriteAttrTableIndex )->data[ index ];

	UINT8 *pattern = reinterpret_cast<sSpriteDescriptor *>( m_Memory + m_SpriteDescTableIndex )->data[ sprite->patternIndex ];

	int count = ( m_Register[ 1 ] & VDP_SPRITE_SIZE ) ? 2 : 1;

	if( m_Register[ 1 ] & VDP_SPRITE_MAGNIFY )
	{
		int col = loX / count;
		for( int i = 0, y = loY; y < hiY; y++ )
		{
			int row = y / count;
			int srcBits = (( pattern[ row ] << 8 ) | pattern[ row+16 ] ) << col;
			int dstBits = 0;
			if( srcBits != 0 )
			{
				for( int x = loX; x < hiX; x++ )
				{
					dstBits <<= 1;
					if( srcBits & 0x8000 )
					{
						dstBits |= 1;
					}
					if( x & 1 )
					{
						srcBits <<= 1;
					}
				}
			}
			data[ i++ ] = dstBits;
			if(( y & 1 ) == 0 )
			{
				data[ i++ ] = dstBits;
				y++;
			}
		}
	}
	else
	{
		int mask = ( 0xFFFF >> ( loX / count )) ^ ( 0xFFFF >> ( hiX / count ));
		for( int i = 0, y = loY; y < hiY; y++ )
		{
			int row = y / count;
			data[ i++ ] = (( pattern[ row ] << 8 ) | pattern[ row+16 ] ) & mask;
		}
	}
}

bool cTMS9918A::SpritesCoincident( int index1, int index2 )
{
	FUNCTION_ENTRY( this, "cTMS9918A::SpriteCoincident", false );

	sSpriteAttributeEntry *sprite1 = &reinterpret_cast<sSpriteAttribute *>( m_Memory + m_SpriteAttrTableIndex )->data[ index1 ];
	sSpriteAttributeEntry *sprite2 = &reinterpret_cast<sSpriteAttribute *>( m_Memory + m_SpriteAttrTableIndex )->data[ index2 ];

	int size  = ( m_Register[ 1 ] & VDP_SPRITE_MAGNIFY ) ? 16 : 8;
	int range = ( m_Register[ 1 ] & VDP_SPRITE_SIZE ) ? 2 * size : size;

	// First see if they overlap at all
	int posY1 = (( sprite1->posY < 0xF0 ) ? sprite1->posY : ( INT8 ) sprite1->posY ) + 1;
	int posY2 = (( sprite2->posY < 0xF0 ) ? sprite2->posY : ( INT8 ) sprite2->posY ) + 1;

	int deltaY = posY2 - posY1;
	if(( deltaY >= range ) || ( deltaY <= -range ))
	{
		return false;
	}

	int posX1 = ( int ) sprite1->posX;
	if( sprite1->earlyClock & 0x80 )
	{
		posX1 -= 32;
	}

	int posX2 = ( int ) sprite2->posX;
	if( sprite2->earlyClock & 0x80 )
	{
		posX2 -= 32;
	}

	int deltaX = posX2 - posX1;
	if(( deltaX >= range ) || ( deltaX <= -range ))
	{
		return false;
	}

	int loX = std::max( posX1, posX2 );
	int hiX = std::min( posX1, posX2 ) + range;

	if( loX < 0 )
	{
		loX = 0;
	}
	if( hiX > VDP_WIDTH )
	{
		hiX = VDP_WIDTH;
	}

	int loY = std::max( posY1, posY2 );
	int hiY = std::min( posY1, posY2 ) + range;

	if( loY < 0 )
	{
		loY = 0;
	}
	if( hiY > VDP_HEIGHT )
	{
		hiY = VDP_HEIGHT;
	}

	int pattern1[ 32 ], pattern2[ 32 ];
	GetSpritePattern( index1, loX - posX1, hiX - posX1, loY - posY1, hiY - posY1, pattern1 );
	GetSpritePattern( index2, loX - posX2, hiX - posX2, loY - posY2, hiY - posY2, pattern2 );

	int maxIndex = ( index1 > index2 ) ? index1 : index2;
	UINT8 *maxSprite = &m_MaxSprite[ loY ];

	for( int i = 0; i < hiY - loY; i++ )
	{
		// Make sure both sprites are being displayed on this row
		if( maxIndex > maxSprite[ i ] )
		{
			continue;
		}
		if( pattern1[ i ] & pattern2[ i ] )
		{
			return true;
		}
	}

	return false;
}

bool cTMS9918A::CheckCoincidence( const bool check[ 32 ] )
{
	FUNCTION_ENTRY( this, "cTMS9918A::CheckCoincidence", false );

	// NOTE: m_FifthSpriteIndex is still the highest valid sprite
	for( int i = m_FifthSpriteIndex; i >= 0; i-- )
	{
		// Only check sprites that were marked
		if( check[ i ] == false )
		{
			continue;
		}
		for( int j = i - 1; j >= 0; j-- )
		{
			if( check[ j ] == false )
			{
				continue;
			}
			if( SpritesCoincident( i, j ) == true )
			{
				return true;
			}
		}
	}

	return false;
}

void cTMS9918A::CheckSprites( )
{
	FUNCTION_ENTRY( this, "cTMS9918A::CheckSprites", false );

	bool check[ 32 ];
	memset( check, false, sizeof( check ));

	sSpriteAttributeEntry *sprite = &reinterpret_cast<sSpriteAttribute *>( m_Memory + m_SpriteAttrTableIndex )->data[ 0 ];

	size_t count[ 256 ][ 2 ];
	memset( count, 0, sizeof( count ));

	memset( m_MaxSprite, 0xFF, sizeof( m_MaxSprite ));

	int size  = ( m_Register[ 1 ] & VDP_SPRITE_MAGNIFY ) ? 16 : 8;
	int range = ( m_Register[ 1 ] & VDP_SPRITE_SIZE ) ? 2 * size : size;

	// Count the # of sprites on a line and find the last one to display
	for( size_t i = 0; i < 32; i++ )
	{
		m_FifthSpriteIndex = i;

		unsigned int y = sprite[ i ].posY;

		if( y == 0xD0 )
		{
			break;
		}

		// Offscreen sprites aren't checked
		if(( y >= 0xC0 ) && ( y < 0xE0 ))
		{
			continue;
		}

		for( unsigned int j = y + 1; j <= y + range; j++ )
		{
			int x = count[ j & 0xFF ][ 0 ]++;
			if( x < 4 )
			{
				m_MaxSprite[ j & 0xFF ] = i;
			}
			else if( x == 4 )
			{
				count[ j & 0xFF ][ 1 ] = i;
			}
		}

		// This sprite should be checked for coincidence
		check[ i ] = true;
	}

	m_CoincidenceFlag = CheckCoincidence( check );

	m_FifthSpriteFlag = false;

	// Look for 5 or more sprites on a row starting at the top of the screen
	for( size_t i = 0; i < VDP_HEIGHT; i++ )
	{
		if( count[ i ][ 0 ] >= 5 )
		{
			m_FifthSpriteFlag  = true;
			m_FifthSpriteIndex = count[ i ][ 1 ];
			break;
		}
	}
}

bool cTMS9918A::Retrace( )
{
	FUNCTION_ENTRY( this, "cTMS9918A::Retrace", false );

	m_SpritesRefreshed = true;

	// Only check if something has changed
	if( m_SpritesDirty == true )
	{
		m_SpritesDirty = false;
		CheckSprites( );
	}

	m_Status |= VDP_INTERRUPT_FLAG;

	if(( InterruptsEnabled( )) && ( m_PIC != nullptr ))
	{
		m_PIC->SignalInterrupt( m_InterruptLevel );
	}

	return false;
}

void cTMS9918A::Render( )
{
	FUNCTION_ENTRY( this, "cTMS9918A::Render", false );
}

//----------------------------------------------------------------------------
// iStateObject Methods
//----------------------------------------------------------------------------

std::string cTMS9918A::GetIdentifier( )
{
	FUNCTION_ENTRY( this, "cTMS9918A::GetIdentifier", true );

	return "TMS9918";
}

bool cTMS9918A::ParseState( const sStateSection &state )
{
	FUNCTION_ENTRY( this, "cTMS9918A::ParseState", true );

	state.load( "Address", m_Address, SaveFormat::HEXADECIMAL );
	state.load( "Transfer", m_Transfer, SaveFormat::HEXADECIMAL );
	state.load( "Shift", m_Shift, SaveFormat::HEXADECIMAL );
	state.load( "Status", m_Status, SaveFormat::HEXADECIMAL );

	UINT8 NewRegister[ 8 ];
	state.load( "Registers", NewRegister, SIZE( NewRegister ));

	state.load( "Memory", m_Memory, 0x4000 );

	m_Mode = 0xFF;      // Guarantee that SetMode will be called at least once
	for( unsigned i = 0; i < 8; i++ )
	{
		WriteRegister( i, NewRegister[ i ] );
	}

	return true;
}

std::optional<sStateSection> cTMS9918A::SaveState( )
{
	FUNCTION_ENTRY( this, "cTMS9918A::SaveState", true );

	sStateSection save;

	save.name = "TMS9918";

	save.store( "Address", m_Address, SaveFormat::HEXADECIMAL );
	save.store( "Transfer", m_Transfer, SaveFormat::HEXADECIMAL );
	save.store( "Shift", m_Shift, SaveFormat::HEXADECIMAL );
	save.store( "Status", m_Status, SaveFormat::HEXADECIMAL );
	save.store( "Registers", m_Register, SIZE( m_Register ));
	save.store( "Memory", m_Memory, 0x4000 );

	return save;
}
