//----------------------------------------------------------------------------
//
// File:        tms9919-sdl.cpp
// Date:        20-Jun-2000
// Programmer:  Marc Rousseau
//
// Description: This file contains SDL specific code for the TMS9919
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

#if defined( OS_WINDOWS )
	#include <windows.h>
#endif

#include <cstring>
#include "common.hpp"
#include "logger.hpp"
#include <SDL.h>
#include "tms9919-sdl.hpp"
#include "tms5220.hpp"

DBG_REGISTER( __FILE__ );

#if defined( OS_WINDOWS )
	// Windows NT needs a larger buffer
	const int DEFAULT_SAMPLES = ( GetVersion( ) & 0x80000000 ) ? 1024 : 2048;
#else
	const int DEFAULT_SAMPLES = 512;
#endif

// NOTE: These numbers were taken from the MESS source code
#define NOISE_RESET              0x00F35
#define NOISE_WHITE_GENERATOR    0x12000
#define NOISE_PERIODIC_GENERATOR 0x08000

#define CLOCK_FREQUENCY         3579545

cSdlTMS9919::cSdlTMS9919( int sampleFreq ) :
	cBaseObject( "cSdlTMS9919" ),
	m_VolumeTable( ),
	m_Initialized( false ),
	m_MasterVolume( 0 ),
	m_AudioSpec( ),
	m_Info( ),
	m_ShiftRegister( NOISE_RESET ),
	m_NoiseGenerator( 0 ),
	m_MixBuffer( nullptr )
{
	FUNCTION_ENTRY( this, "cSdlTMS9919 ctor", true );

	SetMasterVolume( 50 );

	// Set loudest volume to max possible level / 4 (so 4 audio channels won't clip)
	float volume = 32768.0f / 4.0f;
	for( unsigned int i = 0; i < SIZE( m_VolumeTable ) - 1; i++ )
	{
		m_VolumeTable[ i ] = ( int ) volume;
		volume = ( float ) ( volume / 1.258925412f );    // Reduce volume by 2dB
	}
	m_VolumeTable[ 15 ] = 0;

	SDL_AudioSpec wanted;
	memset( &wanted, 0, sizeof( SDL_AudioSpec ));

	if( sampleFreq > 44100 )
	{
		sampleFreq = 44100;
	}

	// Make sure the sample buffer is the right size based on the frequency
	int ratio   = 44100 / sampleFreq;
	int samples = DEFAULT_SAMPLES / ratio;

	// Set the audio format
	wanted.freq     = sampleFreq;
	wanted.format   = AUDIO_S16SYS;
	wanted.channels = 1;
	wanted.samples  = samples;
	wanted.callback = _AudioCallback;
	wanted.userdata = this;

	// Open the audio device, forcing the desired format
	if(( SDL_OpenAudio( &wanted, &m_AudioSpec ) < 0 ) || (( m_AudioSpec.format & 0x00FF ) != 16 ))
	{
		DBG_ERROR( "Couldn't open audio: " << SDL_GetError( ));
	}
	else
	{
		DBG_TRACE( "Using " << (( m_AudioSpec.format & 0x8000 ) ? "signed " : "unsigned " ) << ( m_AudioSpec.format & 0x00FF ) << "-bit " << m_AudioSpec.freq << "Hz Audio" );
		DBG_TRACE( "Buffer size: " << m_AudioSpec.samples );
		m_Initialized = true;
		m_MixBuffer   = new INT16 [ m_AudioSpec.samples ];
		memset( m_MixBuffer, m_AudioSpec.silence, sizeof( INT16 ) * m_AudioSpec.samples );
		SDL_PauseAudio( false );
	}

	uint8_t color = m_NoiseColor;
	int type     = m_NoiseType;
	m_NoiseColor = static_cast<uint8_t>( -1 );
	m_NoiseType  = -1;
	SetNoise( color, type );
}

cSdlTMS9919::~cSdlTMS9919( )
{
	FUNCTION_ENTRY( this, "cSdlTMS9919 dtor", true );

	if( m_Initialized == true )
	{
		SDL_PauseAudio( true );
		SDL_CloseAudio( );
	}

	delete [] m_MixBuffer;
}

void cSdlTMS9919::_AudioCallback( void *data, Uint8 *stream, int length )
{
	FUNCTION_ENTRY( data, "cSdlTMS9919::_AudioCallback", false );

	(( cSdlTMS9919 * ) data)->AudioCallback( stream, length );
}

void cSdlTMS9919::AudioCallback( Uint8 *stream, int length )
{
	FUNCTION_ENTRY( this, "cSdlTMS9919::AudioCallback", false );

	memset( stream, m_AudioSpec.silence, length );

	if( m_MasterVolume == 0 )
	{
		return;
	}

	memset( m_MixBuffer, m_AudioSpec.silence, length );

	bool mix = false;

	int samples = length / sizeof( INT16 );

	for( int i = 0; i < 4; i++ )
	{
		sVoiceInfo *info = &m_Info[ i ];
		if(( m_Attenuation[ i ] != 15 ) && ( info->period >= 1.0f ))
		{
			mix = true;
			int left = samples, j = 0;
			do
			{
				int count = ( info->toggle < left ) ? ( int ) info->toggle : left;
				left -= count;
				info->toggle -= count;
				while( count-- )
				{
					m_MixBuffer[ j++ ] += info->setting;
				}
				if( info->toggle < 1.0f )
				{
					info->toggle += info->period;
					if( i < 3 )
					{
						// Tone
						info->setting = -info->setting;
					}
					else
					{
						// Noise
						if( m_ShiftRegister & 1 )
						{
							m_ShiftRegister ^= m_NoiseGenerator;
							// Protect against 0
							if( m_ShiftRegister == 0 )
							{
								m_ShiftRegister = NOISE_RESET;
							}
							info->setting = -info->setting;
						}
						m_ShiftRegister >>= 1;
					}
				}
			}
			while( left > 0 );
		}
	}

	if( m_pSpeechSynthesizer != nullptr )
	{
		mix |= m_pSpeechSynthesizer->AudioCallback( m_MixBuffer, samples );
	}

	if( mix == true )
	{
		int volume = ( m_MasterVolume * SDL_MIX_MAXVOLUME ) / 100;
		SDL_MixAudio( stream, (Uint8*) m_MixBuffer, length, volume );
	}
}

int cSdlTMS9919::GetPlaybackFrequency( )
{
	FUNCTION_ENTRY( this, "cSdlTMS9919::GetPlaybackFrequency", true );

	return m_AudioSpec.freq;
}

void cSdlTMS9919::SetMasterVolume( int volume )
{
	FUNCTION_ENTRY( this, "cSdlTMS9919::SetMasterVolume", true );

	m_MasterVolume = volume;
}

void cSdlTMS9919::SetNoise( uint8_t color, uint8_t type )
{
	FUNCTION_ENTRY( this, "cSdlTMS9919::SetNoise", true );

	// The shift register is reset when the color is changed
	bool reset = ( color != m_NoiseColor ) ? true : false;

	cTMS9919::SetNoise( color, type );

	if( m_Initialized == true )
	{
		if( reset )
		{
			m_ShiftRegister = NOISE_RESET;
		}
		m_NoiseGenerator = ( color == NOISE_WHITE ) ? NOISE_WHITE_GENERATOR : NOISE_PERIODIC_GENERATOR;

		sVoiceInfo *info = &m_Info[ 3 ];

		if( m_Frequency[ 3 ] != 0 )
		{
			int volume = m_VolumeTable[ m_Attenuation[ 3 ]];
			info->period  = ( float ) m_AudioSpec.freq / ( CLOCK_FREQUENCY / ( float ) m_Frequency[ 3 ]) / 2.0f;
			info->setting = ( info->setting > 0 ) ? volume : -volume;
		}
		else
		{
			info->period = 0.0;
		}
	}
}

void cSdlTMS9919::SetFrequency( uint8_t tone, uint16_t freq )
{
	FUNCTION_ENTRY( this, "cSdlTMS9919::SetFrequency", true );

	cTMS9919::SetFrequency( tone, freq );

	if( m_Initialized == true )
	{
		sVoiceInfo *info = &m_Info[ tone ];

		if(( freq < m_AudioSpec.freq / 2 ) && ( freq != 0 ))
		{
			int volume = m_VolumeTable[ m_Attenuation[ tone ]];
			info->period  = ( float ) (( float ) m_AudioSpec.freq / ( CLOCK_FREQUENCY / ( float ) freq ) / 2.0f );
			info->setting = ( info->setting > 0 ) ? volume : -volume;
		}
		else
		{
			info->period  = m_AudioSpec.samples;
			info->setting = 0;
		}
	}
}

void cSdlTMS9919::SetAttenuation( uint8_t tone, uint8_t atten )
{
	FUNCTION_ENTRY( this, "cSdlTMS9919::SetAttenuation", true );

	cTMS9919::SetAttenuation( tone, atten );

	if( m_Initialized == true )
	{
		sVoiceInfo *info = &m_Info[ tone ];
		int volume = m_VolumeTable[ m_Attenuation[ tone ]];
		info->setting = ( info->setting > 0 ) ? volume : -volume;
	}
}
