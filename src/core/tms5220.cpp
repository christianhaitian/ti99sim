//----------------------------------------------------------------------------
//
// File:        tms5220.cpp
// Date:        27-Nov-2000
// Programmer:  Marc Rousseau
//
// Description: Default class for the TMS5220 Speech Synthesizer Chip
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
#include <array>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include "common.hpp"
#include "logger.hpp"
#include "support.hpp"
#include "tms5220.hpp"
#include "itms9919.hpp"
#include "icomputer.hpp"

DBG_REGISTER( __FILE__ );

const int SINC_WINDOW_SIZE	= 5;
const int LOOKUP_SCALE		= 1024;

extern int verbose;

template<class... Values, class T = typename std::decay<typename std::common_type<Values...>::type>::type>
std::array<double, sizeof...(Values)> COEFF_CONVERT( Values&&... values )
{
	auto cvt = []( T v ) { return v / 512.0; };

	return {{ cvt( values )... }};
}

const int COEFF_ENERGY[ 0x10 ] =
{
	0, 1, 2, 3, 4, 6, 8, 11, 16, 23, 33, 47, 63, 85, 114, 0
};

const int COEFF_PITCH[ 0x40 ] =
{
	  0,  14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,
	 29,  30,  31,  32,  34,  36,  38,  40,  41,  43,  45,  48,  49,  51,  54,  55,
	 57,  60,  62,  64,  68,  72,  74,  76,  81,  85,  87,  90,  96,  99, 103, 107,
	112, 117, 122, 127, 133, 139, 145, 151, 157, 164, 171, 178, 186, 194, 202, 211
};

const std::array<double,0x20> COEFF_K1 = COEFF_CONVERT(
	-501, -498, -495, -490, -485, -478, -469, -459,
	-446, -431, -412, -389, -362, -331, -295, -253,
	-207, -156, -102,  -45,   13,   70,  126,  179,
	 228,  272,  311,  345,  374,  399,  420,  437
);

const std::array<double,0x20> COEFF_K2 = COEFF_CONVERT(
	-376, -357, -335, -312, -286, -258, -227, -195,
	-161, -124,  -87,  -49,  -10,   29,   68,  106,
	 143,  178,  212,  243,  272,  299,  324,  346,
	 366,  384,  400,  414,  427,  438,  448,  506
);

const std::array<double,0x10> COEFF_K3 = COEFF_CONVERT(
	-407, -381, -349, -311, -268, -218, -162, -102,
	 -39,   25,   89,  149,  206,  257,  302,  341
);

const std::array<double,0x10> COEFF_K4 = COEFF_CONVERT(
	-290, -252, -209, -163, -114,  -62,   -9,   44,
	  97,  147,  194,  238,  278,  313,  344,  371
);

const std::array<double,0x10> COEFF_K5 = COEFF_CONVERT(
	-318, -283, -245, -202, -156, -107,  -56,   -3,
	  49,  101,  150,  196,  239,  278,  313,  344
);

const std::array<double,0x10> COEFF_K6 = COEFF_CONVERT(
	-193, -152, -109,  -65,  -20,   26,   71,  115,
	 158,  198,  235,  270,  301,  330,  355,  377
);

const std::array<double,0x10> COEFF_K7 = COEFF_CONVERT(
	-254, -218, -180, -140,  -97,  -53,   -8,   36,
	  81,  124,  165,  204,  240,  274,  304,  332
);

const std::array<double,0x08> COEFF_K8 = COEFF_CONVERT(
	-205, -112,  -10,   92,  187,  269,  336,  387
);

const std::array<double,0x08> COEFF_K9 = COEFF_CONVERT(
	-249, -183, -110,  -32,   48,  126,  198,  261
);

const std::array<double,0x08> COEFF_K10 = COEFF_CONVERT(
	-190, -133,  -73,  -10,   53,  115,  173,  227
);

static const int chirpTable[ 51 ] =
{
	0x00, 0x03, 0x0F, 0x28, 0x4C, 0x6C, 0x71, 0x50, 0x25, 0x26,
	0x4C, 0x44, 0x1A, 0x32, 0x3B, 0x13, 0x37, 0x1A, 0x25, 0x1F,
	0x1D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00
};

static double sinc( double x )
{
	if( x == 0.0 ) return 1;

	x *= M_PI;

	return sin( x ) / x;
}

class filter
{
	static double lookup[ SINC_WINDOW_SIZE * LOOKUP_SCALE ];

public:

	filter( )
	{
		lookup[ 0 ] = 1.0;

		for( int i = 1; i < SINC_WINDOW_SIZE * LOOKUP_SCALE; i ++ )
		{
			double x = ( double ) i / ( double ) LOOKUP_SCALE;

			lookup[ i ] = sinc( x ) * sinc( x / SINC_WINDOW_SIZE );
		}
	}

	static double lanczos( double x )
	{
		if(( x <= -SINC_WINDOW_SIZE ) || ( x >= SINC_WINDOW_SIZE )) return 0.0;

		return lookup[ ( int ) floor( fabs( x ) * LOOKUP_SCALE ) ];
	}

};

double filter::lookup[ SINC_WINDOW_SIZE * LOOKUP_SCALE ];

static filter sinc_lookup;

cTMS5220::cTMS5220( ) :
	cBaseObject( "cTMS5220" ),
	m_State( ),
	m_SpeechRom( ),
	m_FIFO( ),
	m_TalkStatus( false ),
	m_StartParams( ),
	m_TargetParams( ),
	m_InterpolationStage( 0 ),
	m_Computer( nullptr ),
	m_PitchIndex( 0 ),
	m_FilterHistory( ),
	m_RawDataBuffer( ),
	m_PlaybackInterval( 0 ),
	m_PlaybackBuffer( nullptr ),
	m_PlaybackSamplesLeft( 0 ),
	m_PlaybackDataPtr( nullptr )
{
	FUNCTION_ENTRY( this, "cTMS5220 ctor", true );

	std::string filename = LocateFile( "console", "spchrom.bin" );
	if( filename.empty( ) == false )
	{
		FILE *file = fopen( filename.c_str( ), "rb" );
		if( file != nullptr )
		{
			if( fread( m_SpeechRom, 1, 0x8000, file ) != 0x8000 )
			{
				DBG_WARNING( "Unable to read speech ROM file" );
				m_SpeechRom[ 0 ] = 0;
			}
			fclose( file );
		}
	}

	if( m_SpeechRom[ 0 ] != 0xAA )
	{
		if( verbose >= 1 )
		{
			fprintf( stdout, "A valid speech ROM was not found\n" );
		}
		DBG_WARNING( "A valid speech ROM was not found" );
		// Create an empty ROM image
		m_SpeechRom[ 0 ] = 0xAA;
	}
	else
	{
		if( verbose >= 2 )
		{
			fprintf( stdout, "Using speech ROM \"%s\"\n", filename.c_str( ));
		}
	}

	Reset( );
}

cTMS5220::~cTMS5220( )
{
	FUNCTION_ENTRY( this, "cTMS5220 dtor", true );

	delete [] m_PlaybackBuffer;
	m_PlaybackBuffer = nullptr;
}

//----------------------------------------------------------------------------
// iBaseObject Methods
//----------------------------------------------------------------------------

const void *cTMS5220::GetInterface( const std::string &iName ) const
{
	FUNCTION_ENTRY( this, "cTMS5220::GetInterface", false );

	if( iName == "iTMS5220" )
	{
		return static_cast<const iTMS5220 *>( this );
	}

	if( iName == "iStateObject" )
	{
		return static_cast< const iStateObject * >( this );
	}

	return cBaseObject::GetInterface( iName );
}

void cTMS5220::LoadAddress( UINT8 data )
{
	FUNCTION_ENTRY( this, "cTMS5220::LoadAddress", true );

	switch( m_State.vsm.LoadPointer++ )
	{
		case 0 :
			m_State.vsm.Address = ( UINT16 ) (( m_State.vsm.Address & 0xFFFF0 ) | (( data & 0x0F ) << 0 ));
			break;
		case 1 :
			m_State.vsm.Address = ( UINT16 ) (( m_State.vsm.Address & 0xFFF0F ) | (( data & 0x0F ) << 4 ));
			break;
		case 2 :
			m_State.vsm.Address = ( UINT16 ) (( m_State.vsm.Address & 0xFF0FF ) | (( data & 0x0F ) << 8 ));
			break;
		case 3 :
			m_State.vsm.Address = ( UINT16 ) (( m_State.vsm.Address & 0xF0FFF ) | (( data & 0x0F ) << 12 ));
			break;
		case 4 :
			m_State.vsm.Address = ( UINT16 ) (( m_State.vsm.Address & 0x0FFFF ) | (( data & 0x0F ) << 16 ));
			DBG_TRACE( "Chip: " << hex << ( UINT8 ) ( m_State.vsm.Address >> 14 ) << "  Address: " << hex << ( m_State.vsm.Address & 0x3FFF ));
			// Simulate the dummy read that resets the LoadPointer
			m_State.vsm.LoadPointer = 0;
			m_State.vsm.BitOffset = 0;
			break;
		default :
			// Just to shut up the compiler - can't get here
			break;
	}
}

bool cTMS5220::WaitForBitsFIFO( int bits )
{
	FUNCTION_ENTRY( this, "cTMS5220::WaitForBitsFIFO", true );

	if( m_Computer != nullptr )
	{
		if( m_State.fifo.BitsLeft < bits )
		{
			for( int retry = 0; retry < 10; retry++ )
			{
				DBG_WARNING( "waiting for bits..." );
				m_Computer->WakeCPU( 1 );
				if( m_State.fifo.BitsLeft >= bits )
				{
					break;
				}
			}
		}
	}

	return ( m_State.fifo.BitsLeft >= bits ) ? true : false;
}

void cTMS5220::StoreDataFIFO( UINT8 data )
{
	FUNCTION_ENTRY( this, "cTMS5220::StoreDataFIFO", true );

	// TBD - Acquire MUTEX
	m_FIFO[ m_State.fifo.PutIndex ] = data;

	m_State.fifo.BitsLeft += 8;

	int nextIndex = ( m_State.fifo.PutIndex + 1 ) % FIFO_BYTES;

	if(( m_PlaybackBuffer != nullptr ) && ( nextIndex == m_State.fifo.GetIndex ))
	{
		if( m_Computer != nullptr )
		{
			DBG_WARNING( "FIFO full - stalling CPU... (" << m_State.fifo.PutIndex << "/" << m_State.fifo.GetIndex << ")" );

			for( int retry = 0; retry < 10; retry++ )
			{
				// TBD - calculate a good approximation for the # of CPU cycles to stall
				m_Computer->Sleep( 100, TMS5220_FRAME_PERIOD / 1000 );

				// Check TalkStatus in case we stalled
				if(( m_TalkStatus == false ) || ( nextIndex != m_State.fifo.GetIndex ))
				{
					break;
				}
			}

			if( nextIndex == m_State.fifo.GetIndex )
			{
				DBG_ERROR( "Audio callback failed to read bits from FIFO" );

				// Remove 1 speech frame to make room (minimizes audio corruption compared to dropping a byte from the FIFO)
				sSpeechParams temp;
				ReadFrame( &temp, true );
			}

			DBG_TRACE( "CPU stall complete... (" << m_State.fifo.PutIndex << "/" << m_State.fifo.GetIndex << ")" );
		}
	}

	m_State.fifo.PutIndex = nextIndex;

	if(( m_TalkStatus == false ) && (( GetStatus( ) & TMS5220_BL ) == 0 ))
	{
		m_TalkStatus = true;
	}

	// TBD - Release MUTEX

	// If we aren't getting audio callbacks, try to empty the buffer
	if(( m_PlaybackBuffer == nullptr ) && ( m_TalkStatus == true ))
	{
		sSpeechParams temp;
		ReadFrame( &temp, true );
	}
}

UINT8 cTMS5220::ReadBitsFIFO( int count )
{
	FUNCTION_ENTRY( this, "cTMS5220::ReadBitsFIFO", true );

	DBG_TRACE( "Reading " << count << "/" << m_State.fifo.BitsLeft << " bits" );

	// TBD - Acquire MUTEX

	if( WaitForBitsFIFO( count ) == false )
	{
		// TBD - Release MUTEX
		throw std::underflow_error( "FIFO empty" );
	}

	UINT8 data = 0;

	while( count-- )
	{
		int bit = ( m_FIFO[ m_State.fifo.GetIndex ] >> ( 7 - (( m_State.fifo.BitsLeft - 1 ) % 8 ))) & 1;

		data = ( UINT8 ) (( data << 1 ) | bit );

		if(( --m_State.fifo.BitsLeft % 8 ) == 0 )
		{
			m_State.fifo.GetIndex = ( m_State.fifo.GetIndex + 1 ) % FIFO_BYTES;
		}
	}

	// See if we drained the bit pool but didn't finish with a STOP code
	if(( m_State.fifo.BitsLeft == 0 ) && (( m_State.ReadingEnergy == false ) || ( data != 0x0F )))
	{
		// Wait for at least 1 bit before clearing talk status & speak external flags
		if( WaitForBitsFIFO( 1 ) == false )
		{
			DBG_WARNING( "FIFO drained before STOP code" );
		}
	}

	if( m_State.fifo.BitsLeft == 0 )
	{
		m_State.SpeakExternal = false;
		m_TalkStatus = false;
	}

	// TBD - Release MUTEX

	return data;
}

UINT8 cTMS5220::ReadBitsROM( int count )
{
	FUNCTION_ENTRY( this, "cTMS5220::ReadBitsROM", true );

	UINT8 data = 0;

	while(( count-- ) && ( m_State.vsm.Address < 0x8000 ))
	{
		int bit = ( m_SpeechRom[ m_State.vsm.Address ] >> ( 7 - m_State.vsm.BitOffset )) & 1;

		data = ( UINT8 ) (( data << 1 ) | bit );

		if( ++m_State.vsm.BitOffset == 8 )
		{
			m_State.vsm.Address += 1;
			m_State.vsm.BitOffset = 0;
		}
	}

	return data;
}

UINT8 cTMS5220::ReadBits( int count )
{
	FUNCTION_ENTRY( this, "cTMS5220::ReadBits", true );

	return ( m_State.SpeakExternal == true ) ? ReadBitsFIFO( count ) : ReadBitsROM( count );
}

bool cTMS5220::CreateNextBuffer( )
{
	FUNCTION_ENTRY( this, "cTMS5220::CreateNextBuffer", true );

	if( m_InterpolationStage == 0 )
	{
		// Update the start & target parameters
		memcpy( &m_StartParams, &m_TargetParams, sizeof( sSpeechParams ));

		// Try to get the next set of parameters
		sSpeechParams newFrame = m_StartParams;
		if( ReadFrame( &newFrame, false ) == false )
		{
			DBG_ERROR( "** UNDER-RUN **" );
			return false;
		}

		m_TargetParams = newFrame;
	}

	sSpeechParams param;
	InterpolateParameters( m_InterpolationStage, m_StartParams, m_TargetParams, &param );
	m_InterpolationStage = ( m_InterpolationStage + 1 ) % 8;

	memcpy( &m_StartParams, &param, sizeof( sSpeechParams ));

	if( m_PitchIndex >= param.Pitch )
	{
		m_PitchIndex = 0;
	}

	for( int i = 0; i < INTERPOLATION_SAMPLES; i++ )
	{
		double sample = 0.0;

		if( param.Pitch == 0 )
		{
			sample = ( rand( ) & 1 ) ? 64 : -64;
		}
		else
		{
			if( m_PitchIndex < ( int ) SIZE( chirpTable ))
			{
				sample = chirpTable[ m_PitchIndex ];
			}
			m_PitchIndex = ( m_PitchIndex + 1 ) % param.Pitch;
		}

		// Synthesize...
		m_FilterHistory[ 0 ][ RC_ORDER ] = sample * param.Energy;

		// Forward path
		for( int j = RC_ORDER - 1; j >= 0; j-- )
		{
			m_FilterHistory[ 0 ][ j ] = m_FilterHistory[ 0 ][ j + 1 ] - param.Reflection[ j ] * m_FilterHistory[ 1 ][ j ];
		}

		// Backward path
		for( int j = RC_ORDER - 1; j >= 1; j-- )
		{
			m_FilterHistory[ 1 ][ j ] = m_FilterHistory[ 1 ][ j - 1 ] + param.Reflection[ j - 1 ] * m_FilterHistory[ 0 ][ j - 1 ];
		}

		double cliptemp = m_FilterHistory[ 1 ][ 0 ] = m_FilterHistory[ 0 ][ 0 ];

		m_RawDataBuffer[ i ] = std::min( 32767.0, std::max( -32768.0, cliptemp ));
	}

	if(( m_InterpolationStage == 0 ) && ( m_TargetParams.Stop == true ))
	{
		m_TalkStatus = false;
	}

	return true;
}

bool cTMS5220::ConvertBuffer( )
{
	FUNCTION_ENTRY( this, "cTMS5220::ConvertBuffer", true );

	int overlap = m_PlaybackBufferSize - m_PlaybackInterval;

	// Copy the extra data from the last conversion to the beginning of the buffer and zero out the rest
	memcpy( m_PlaybackBuffer, m_PlaybackBuffer + m_PlaybackInterval, overlap * sizeof( double ));
	memset( m_PlaybackBuffer + overlap, 0, m_PlaybackInterval * sizeof( double ));

	// Run the re-sampling filter

	for( int i = 0; i < m_PlaybackBufferSize; i++ )
	{
		double x = m_PlaybackOffset + ( double ) i / m_PlaybackRatio;
		for( int j = -SINC_WINDOW_SIZE; j <= SINC_WINDOW_SIZE; j++ )
		{
			int y = ( int ) floor( x ) + j - SINC_WINDOW_SIZE;
			if(( y >= 0 ) && ( y < INTERPOLATION_SAMPLES ))
			{
				double index = x - y - SINC_WINDOW_SIZE;
				m_PlaybackBuffer[ i ] += m_RawDataBuffer[ y ] * filter::lanczos( index );
			}
		}
	}

	m_PlaybackOffset = fmod( m_PlaybackOffset + m_PlaybackBufferSize / m_PlaybackRatio, 1.0 );

	m_PlaybackDataPtr     = m_PlaybackBuffer;
	m_PlaybackSamplesLeft = m_PlaybackInterval;

	return true;
}

bool cTMS5220::GetNextBuffer( )
{
	FUNCTION_ENTRY( this, "cTMS5220::GetNextBuffer", true );

	if( CreateNextBuffer( ) == false )
	{
		return false;
	}

	if( ConvertBuffer( ) == false )
	{
		return false;
	}

	return true;
}

char *cTMS5220::FormatParameters( const sSpeechParams &param, bool showAll )
{
	FUNCTION_ENTRY( nullptr, "cTMS5220::FormatParameters", true );

	static char buffer[ 256 ];

	char *ptr = buffer;

	ptr += sprintf( ptr, "En: %4d  Rpt: %d  Pi: %3d", param.Energy, param.Repeat, param.Pitch );

	if(( showAll == true ) || ( param.Repeat == false ))
	{
		int max = (( showAll == true ) || ( param.Pitch != 0 )) ? 10 : 4;
		ptr += sprintf( ptr, "  K:" );
		for( int i = 0; i < max; i++ )
		{
			ptr += sprintf( ptr, " %8.5f", param.Reflection[ i ] );
		}
	}

	return buffer;
}

void cTMS5220::InterpolateParameters( int stage, const sSpeechParams &start, const sSpeechParams &end, sSpeechParams *param )
{
	FUNCTION_ENTRY( nullptr, "cTMS5220::InterpolateParameters", true );

	memcpy( param, &start, sizeof( sSpeechParams ));

	bool inhibit = false;

	inhibit |= ( start.Pitch != 0 ) && ( end.Pitch == 0 );
	inhibit |= ( start.Pitch == 0 ) && ( end.Pitch != 0 );
	inhibit |= ( start.Energy == 0 ) && ( end.Energy != 0 );
	inhibit |= ( start.Pitch == 0 ) && ( end.Energy == 0 );

	if( !inhibit )
	{
		constexpr int X[ 8 ] =
		{
			8, 8, 8, 4, 4, 2, 2, 1
		};

		param->Energy += ( end.Energy - param->Energy ) / X[ stage ];
		param->Pitch  += ( end.Pitch - param->Pitch ) / X[ stage ];

		for( int i = 0; i < RC_ORDER; i++ )
		{
			param->Reflection[ i ] += ( end.Reflection[ i ] - param->Reflection[ i ] ) / X[ stage ];
		}
	}

	DBG_TRACE( FormatParameters( *param, false ));
}

bool cTMS5220::ReadFrame( sSpeechParams *frame, bool restore )
{
	FUNCTION_ENTRY( this, "cTMS5220::ReadFrame", true );

	sReadState state( m_State );

	try
	{
		m_State.ReadingEnergy = true;
		int index = ReadBits( 4 );
		m_State.ReadingEnergy = false;

		if( index == 15 )
		{
			DBG_TRACE( "--Stop--" );

			frame->Stop = true;

			return true;
		}
		else
		{
			frame->Stop = false;

			frame->Energy = COEFF_ENERGY[ index ];

			if( index != 0 )
			{
				frame->Repeat = ReadBits( 1 ) ? true : false;
				frame->Pitch = COEFF_PITCH[ ReadBits( 6 ) ];

				if( frame->Repeat == false )
				{
					frame->Reflection[ REFLECTION_K1 ] = COEFF_K1[ ReadBits( 5 ) ];
					frame->Reflection[ REFLECTION_K2 ] = COEFF_K2[ ReadBits( 5 ) ];
					frame->Reflection[ REFLECTION_K3 ] = COEFF_K3[ ReadBits( 4 ) ];
					frame->Reflection[ REFLECTION_K4 ] = COEFF_K4[ ReadBits( 4 ) ];

					frame->Reflection[ REFLECTION_K5 ] = ( frame->Pitch != 0 ) ? COEFF_K5[ ReadBits( 4 ) ] : 0.0;
					frame->Reflection[ REFLECTION_K6 ] = ( frame->Pitch != 0 ) ? COEFF_K6[ ReadBits( 4 ) ] : 0.0;
					frame->Reflection[ REFLECTION_K7 ] = ( frame->Pitch != 0 ) ? COEFF_K7[ ReadBits( 4 ) ] : 0.0;
					frame->Reflection[ REFLECTION_K8 ] = ( frame->Pitch != 0 ) ? COEFF_K8[ ReadBits( 3 ) ] : 0.0;
					frame->Reflection[ REFLECTION_K9 ] = ( frame->Pitch != 0 ) ? COEFF_K9[ ReadBits( 3 ) ] : 0.0;
					frame->Reflection[ REFLECTION_K10 ] = ( frame->Pitch != 0 ) ? COEFF_K10[ ReadBits( 3 ) ] : 0.0;
				}

				DBG_TRACE( FormatParameters( *frame, false ));
			}
			else
			{
				DBG_TRACE( "--Silence--" );
			}
		}
	}
	catch( ... )
	{
		if( restore == true )
		{
			if( m_State.SpeakExternal == true )
			{
				if( m_State.fifo.PutIndex != state.fifo.PutIndex )
				{
					DBG_WARNING( "FIFO data written while parsing frame" );
				}
			}

			m_State = state;
		}
		else
		{
			Reset( );
		}
		return false;
	}

	return true;
}

void cTMS5220::SetSoundChip( iTMS9919 *pSound )
{
	delete [] m_PlaybackBuffer;
	m_PlaybackBuffer = nullptr;

	if( pSound != nullptr )
	{
		m_PlaybackRatio      = ( double ) pSound->GetPlaybackFrequency( ) / ( double ) SAMPLE_RATE;
		m_PlaybackInterval   = ( int ) ceil( INTERPOLATION_SAMPLES * m_PlaybackRatio );
		m_PlaybackBufferSize = ( int ) ceil(( INTERPOLATION_SAMPLES + 2 * SINC_WINDOW_SIZE ) * m_PlaybackRatio );
		m_PlaybackBuffer     = new double[ m_PlaybackBufferSize ];
		m_PlaybackOffset     = 0.0;

		memset( m_PlaybackBuffer, 0, m_PlaybackBufferSize * sizeof( double ));
	}
}

void cTMS5220::SetComputer( iComputer *computer )
{
	FUNCTION_ENTRY( this, "cTMS5220::SetComputer", true );

	m_Computer = computer;
}

bool cTMS5220::AudioCallback( INT16 *buffer, int count )
{
	FUNCTION_ENTRY( this, "cTMS5220::AudioCallback", false );

	if( m_TalkStatus == false )
	{
		return false;
	}

	bool modified = false;

	while(( count > 0 ) && ( m_TalkStatus == true ))
	{
		if(( m_PlaybackSamplesLeft == 0 ) && ( GetNextBuffer( ) == false ))
		{
			break;
		}

		modified = true;

		int size = std::min( count, m_PlaybackSamplesLeft );
		for( int i = 0; i < size; i++ )
		{
			int sample = ( int ) ( *buffer + *m_PlaybackDataPtr++ );
			*buffer++ = std::min( 32767, std::max( -32768, sample ));
		}

		count -= size;
		m_PlaybackSamplesLeft -= size;
	}

	if( m_TalkStatus == false )
	{
		Reset( );
	}

	return modified;
}

void cTMS5220::Reset( )
{
	FUNCTION_ENTRY( this, "cTMS5220::Reset", true );

	DBG_TRACE( "Resetting the TMS5220" );

	memset( m_FIFO, 0, sizeof( m_FIFO ));

	m_State = { false, false, false, { 0, 0, 0 }, { 0, 0, 0 }};

	m_TalkStatus = false;

	memset( &m_StartParams, 0, sizeof( m_StartParams ));
	memset( &m_TargetParams, 0, sizeof( m_TargetParams ));

	memset( m_FilterHistory, 0, sizeof( m_FilterHistory ));
	memset( m_RawDataBuffer, 0, sizeof( m_RawDataBuffer ));

	m_InterpolationStage  = 0;
	m_PlaybackSamplesLeft = 0;
}

UINT8 cTMS5220::WriteData( UINT8 data )
{
	FUNCTION_ENTRY( this, "cTMS5220::WriteData", false );

	if( m_State.SpeakExternal == true )
	{
		DBG_TRACE( "External data: " << hex << data );

		StoreDataFIFO( data );
	}
	else
	{
		switch( data & SPEECH_COMMAND_MASK )
		{
			case 0x00 : // Load-Frame-Rate
				DBG_WARNING( "CMD: Load-Frame-Rate ** Unsupported **" );
				break;
			case 0x10 : // Read-Byte
				DBG_TRACE( "CMD: Read-Byte" );
				m_State.ReadByte = true;
				break;
			case 0x20 :
				DBG_WARNING( "Unsupported command received" );
				break;
			case 0x30 : // Read-and-Branch
				m_State.vsm.Address &= 0xFC000;
				m_State.vsm.Address |= 0x03FFF & (( ReadBitsROM( 8 ) << 8 ) | ReadBitsROM( 8 ));
				DBG_TRACE( "CMD: Read-and-Branch - Address = " << hex << m_State.vsm.Address );
				break;
			case 0x40 : // Load-Address (expect 4 more)
				DBG_TRACE( "CMD: Load-Address" );
				LoadAddress( data );
				break;
			case 0x50 : // Speak
				DBG_TRACE( "CMD: Speak" );
				m_TalkStatus = true;
				break;
			case 0x60 : // Speak-External (accepts an unlimited number of data bytes)
				DBG_TRACE( "CMD: Speak-External" );
				m_State.SpeakExternal = true;
				m_State.fifo.GetIndex = 0;
				m_State.fifo.PutIndex = 0;
				m_State.fifo.BitsLeft = 0;
				break;
			case 0x70 : // Reset
				Reset( );
				break;
			default :
				// Just to shut up the compiler - can't get here
				break;
		}
	}

	return data;
}

UINT8 cTMS5220::ReadData( UINT8 data )
{
	FUNCTION_ENTRY( this, "cTMS5220::ReadData", false );

	if( m_State.ReadByte == true )
	{
		m_State.ReadByte = false;
		data = ReadBitsROM( 8 );
		DBG_TRACE( "READ: " << hex << data << " (" << ( char ) ( isprint( data ) ? data : '.' ) << ")" );
	}
	else
	{
		data = GetStatus( );
		DBG_TRACE( "Status: " << hex << data );
	}

	return data;
}

//----------------------------------------------------------------------------
// iStateObject Methods
//----------------------------------------------------------------------------

std::string cTMS5220::GetIdentifier( )
{
	FUNCTION_ENTRY( this, "cTMS5220::GetIdentifier", true );

	return "TMS5220";
}

bool cTMS5220::ParseState( const sStateSection &state )
{
	FUNCTION_ENTRY( this, "cTMS5220::ParseState", false );

	state.load( "ReadByte", m_State.ReadByte );
	state.load( "SpeakExternal", m_State.SpeakExternal );

	if( m_State.SpeakExternal )
	{
		state.load( "FIFO", m_FIFO, SIZE( m_FIFO ));
		state.load( "GetIndex", m_State.fifo.GetIndex, SaveFormat::DECIMAL );
		state.load( "PutIndex", m_State.fifo.PutIndex, SaveFormat::DECIMAL );
		state.load( "BitsLeft", m_State.fifo.BitsLeft, SaveFormat::DECIMAL );
	}
	else
	{
		state.load( "LoadPointer", m_State.vsm.LoadPointer, SaveFormat::DECIMAL );
		state.load( "Address", m_State.vsm.Address, SaveFormat::HEXADECIMAL );
		state.load( "BitOffset", m_State.vsm.BitOffset, SaveFormat::DECIMAL );
	}

	state.load( "TalkStatus", m_TalkStatus );

	// TODO - Restore internal filter state

	return true;
}

std::optional<sStateSection> cTMS5220::SaveState( )
{
	FUNCTION_ENTRY( this, "cTMS5220::SaveState", false );

	sStateSection save;

	save.name = "TMS5220";

	save.store( "ReadByte", m_State.ReadByte );
	save.store( "SpeakExternal", m_State.SpeakExternal );

	if( m_State.SpeakExternal )
	{
		save.store( "FIFO", m_FIFO, SIZE( m_FIFO ));
		save.store( "GetIndex", m_State.fifo.GetIndex, SaveFormat::DECIMAL );
		save.store( "PutIndex", m_State.fifo.PutIndex, SaveFormat::DECIMAL );
		save.store( "BitsLeft", m_State.fifo.BitsLeft, SaveFormat::DECIMAL );
	}
	else
	{
		save.store( "LoadPointer", m_State.vsm.LoadPointer, SaveFormat::DECIMAL );
		save.store( "Address", m_State.vsm.Address, SaveFormat::HEXADECIMAL );
		save.store( "BitOffset", m_State.vsm.BitOffset, SaveFormat::DECIMAL );
	}

	save.store( "TalkStatus", m_TalkStatus );

/*
	// TODO - Save internal filter state

	sSpeechParams  m_StartParams;
	sSpeechParams  m_TargetParams;
	int            m_InterpolationStage;

	// 8 KHz speech buffer
	int            m_PitchIndex;
	double         m_FilterHistory[ 2 ][ RC_ORDER + 1 ];
	double         m_RawDataBuffer[ INTERPOLATION_SAMPLES ];

	// Resampled synthesized speech buffer
	*** int            m_PlaybackInterval;
	*** int            m_PlaybackBufferSize;
	*** double        *m_PlaybackBuffer;
	*** double         m_PlaybackRatio;
	double         m_PlaybackOffset;

	int            m_PlaybackSamplesLeft;
	double        *m_PlaybackDataPtr;
*/

	return save;
}

UINT8 cTMS5220::GetStatus( ) const
{
	FUNCTION_ENTRY( this, "cTMS5220::GetStatus", false );

	int bufferLow   = ( m_State.fifo.BitsLeft < FIFO_BITS / 2 ) ? TMS5220_BL : 0;
	int bufferEmpty = ( m_State.fifo.BitsLeft == 0 ) ? TMS5220_BE : 0;
	int talkStatus  = m_TalkStatus ? TMS5220_TS : 0;

	return talkStatus | bufferLow | bufferEmpty;
}
