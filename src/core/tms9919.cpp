//----------------------------------------------------------------------------
//
// File:        tms9919.cpp
// Date:        20-Mar-1998
// Programmer:  Marc Rousseau
//
// Description: Default class for the TMS9919 Sound Generator Chip
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

#include "common.hpp"
#include "logger.hpp"
#include "tms9919.hpp"
#include "itms5220.hpp"
#include "support.hpp"

DBG_REGISTER( __FILE__ );

cTMS9919::cTMS9919( ) :
	cBaseObject( "cTMS9919" ),
	m_pSpeechSynthesizer( nullptr ),
	m_LastData( 0 ),
	m_Frequency( ),
	m_Attenuation( ),
	m_NoiseColor( NOISE_WHITE ),
	m_NoiseType( 0 )
{
	FUNCTION_ENTRY( this, "cTMS9919 ctor", true );

	m_Attenuation[ 0 ] = 0x0F;
	m_Attenuation[ 1 ] = 0x0F;
	m_Attenuation[ 2 ] = 0x0F;
	m_Attenuation[ 3 ] = 0x0F;
}

cTMS9919::~cTMS9919( )
{
	FUNCTION_ENTRY( this, "cTMS9919 dtor", true );
}

//----------------------------------------------------------------------------
// iBaseObject Methods
//----------------------------------------------------------------------------

const void *cTMS9919::GetInterface( const std::string &iName ) const
{
	FUNCTION_ENTRY( this, "cTMS9919::GetInterface", false );

	if( iName == "iTMS9919" )
	{
		return static_cast<const iTMS9919 *>( this );
	}

	if( iName == "iStateObject" )
	{
		return static_cast< const iStateObject * >( this );
	}

	return cBaseObject::GetInterface( iName );
}

void cTMS9919::SetNoise( uint8_t color, uint8_t type )
{
	FUNCTION_ENTRY( this, "cTMS9919::SetNoise", true );

	m_NoiseColor = color;
	m_NoiseType  = type;

	switch( type )
	{
		case 0 :
			m_Frequency[ 3 ] = 512;
			break;
		case 1 :
			m_Frequency[ 3 ] = 1024;
			break;
		case 2 :
			m_Frequency[ 3 ] = 2048;
			break;
		case 3 :
			m_Frequency[ 3 ] = m_Frequency[ 2 ];
			break;
		default :
			DBG_ERROR( "Invalid noise type selected: " << type );
			break;
	}
}

void cTMS9919::SetFrequency( uint8_t tone, uint16_t freq )
{
	FUNCTION_ENTRY( this, "cTMS9919::SetFrequency", true );

	m_Frequency[ tone ] = freq;

	if(( tone == 2 ) && ( m_NoiseType == 3 ))
	{
		SetNoise( m_NoiseColor, m_NoiseType );
	}
}

void cTMS9919::SetAttenuation( uint8_t tone, uint8_t atten )
{
	FUNCTION_ENTRY( this, "cTMS9919::SetAttenuation", true );

	m_Attenuation[ tone ] = atten;
}

void cTMS9919::SetSpeechSynthesizer( iTMS5220 *speech )
{
	FUNCTION_ENTRY( this, "cTMS9919::SetSpeechSynthesizer", true );

	m_pSpeechSynthesizer = speech;
}

void cTMS9919::WriteData( UINT8 data )
{
	FUNCTION_ENTRY( this, "cTMS9919::WriteData", true );

	if( data & 0x80 )
	{
		m_LastData = data;
	}

	int reg = ( m_LastData >> 4 ) & 0x07;
	int tone = reg >> 1;

	if( reg & 1 )
	{
		// Handle Attenuation
		SetAttenuation( tone, data & 0x0F );
	}
	else
	{
		// Handle Frequency
		if( tone == 3 )
		{
			// Handle Noise
			SetNoise(( data & 0x04 ) ? NOISE_WHITE : NOISE_PERIODIC, ( data & 0x03 ));
		}
		else if(( data & 0x80 ) == 0 )
		{
			// Handle Tone
			int divisor = (( data & 0x3F ) << 4 ) | ( m_LastData & 0x0F );
			if( divisor != 0 )
			{
				SetFrequency( tone, divisor * 32 );
			}
		}
	}
}

int cTMS9919::GetPlaybackFrequency( )
{
	FUNCTION_ENTRY( this, "cTMS9919::GetPlaybackFrequency", true );

	return -1;
}

//----------------------------------------------------------------------------
// iStateObject Methods
//----------------------------------------------------------------------------

std::string cTMS9919::GetIdentifier( )
{
	FUNCTION_ENTRY( this, "cTMS9919::GetIdentifier", true );

	return "TMS9919";
}

bool cTMS9919::ParseState( const sStateSection &state )
{
	FUNCTION_ENTRY( this, "cTMS9919::ParseState", true );

	state.load( "LastData", m_LastData, SaveFormat::HEXADECIMAL );
	state.load( "Frequency", m_Frequency, SIZE( m_Frequency ));
	state.load( "Attenuation", m_Attenuation, SIZE( m_Attenuation ));
	state.load( "NoiseColor", m_NoiseColor, SaveFormat::HEXADECIMAL );
	state.load( "NoiseType", m_NoiseType, SaveFormat::HEXADECIMAL );

	return false;
}

std::optional<sStateSection> cTMS9919::SaveState( )
{
	FUNCTION_ENTRY( this, "cTMS9919::SaveState", true );

	sStateSection save;

	save.name = "TMS9919";

	save.store( "LastData", m_LastData, SaveFormat::HEXADECIMAL );
	save.store( "Frequency", m_Frequency, SIZE( m_Frequency ));
	save.store( "Attenuation", m_Attenuation, SIZE( m_Attenuation ));
	save.store( "NoiseColor", m_NoiseColor, SaveFormat::HEXADECIMAL );
	save.store( "NoiseType", m_NoiseType, SaveFormat::HEXADECIMAL );

	return save;
}
