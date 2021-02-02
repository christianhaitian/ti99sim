
//----------------------------------------------------------------------------
//
// File:		tms5220.hpp
// Date:		27-Nov-2000
// Programmer:	Marc Rousseau
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
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
//
// Revision History:
//
//----------------------------------------------------------------------------

#ifndef TMS5220_HPP_
#define TMS5220_HPP_

#include "cBaseObject.hpp"
#include "itms5220.hpp"
#include "istateobject.hpp"

#define TMS5220_TS	0x80	// Talk Status
#define TMS5220_BL	0x40	// Buffer Low
#define TMS5220_BE	0x20	// Buffer Empty

#define SPEECH_COMMAND_MASK 0x70

#define REFLECTION_K1		0
#define REFLECTION_K2		1
#define REFLECTION_K3		2
#define REFLECTION_K4		3
#define REFLECTION_K5		4
#define REFLECTION_K6		5
#define REFLECTION_K7		6
#define REFLECTION_K8		7
#define REFLECTION_K9		8
#define REFLECTION_K10		9

#define FIFO_BYTES			16
#define FIFO_BITS			(FIFO_BYTES * 8)

const int TMS5220_FRAME_RATE             = 40;		// 40 Hz
const int TMS5220_FRAME_PERIOD           = 25000;	// 25 ms
const int TMS5220_INTERPOLATION_RATE     = 320;		// 320 Hz
const int TMS5220_INTERPOLATION_INTERVAL = 3125;	// 3.125 ms
const int TMS5220_SAMPLE_RATE            = 8000;	// 8 KHz
const int TMS5220_SAMPLE_PERIOD          = 125;		// 125 us
const int TMS5220_ROM_CLOCK_RATE         = 160000;	// 160 KHz
const int TMS5220_ROM_CLOCK_PERIOD       = 6;		// 6.25 us
const int TMS5220_RC_OSC_RATE            = 640000;	// 640 KHz
const int TMS5220_RC_OSC_PERIOD          = 1;		// 1562.5 ns

const int RC_ORDER                       = 10;		// # of Reflection Coefficients
const int SAMPLE_RATE                    = 8000;	// 8 KHz
const int INTERPOLATION_SAMPLES          = 25;		// 3.125 ms

struct iTI994A;
struct iTMS9919;

class cTMS5220 :
	public virtual cBaseObject,
	public virtual iTMS5220,
	public virtual iStateObject
{
protected:

	struct sSpeechParams
	{
		int        Energy;
		int        Pitch;
		bool       Repeat;
		bool       Stop;
		double     Reflection[ RC_ORDER ];
	};

	struct sReadState
	{
		bool           ReadByte;
		bool           SpeakExternal;
		bool           ReadingEnergy;
		struct
		{
			int            LoadPointer;
			uint32_t       Address;
			int            BitOffset;
		}              vsm;
		struct
		{
			int            GetIndex;
			int            PutIndex;
			int            BitsLeft;
		}              fifo;
	};

	sReadState     m_State;

	UINT8          m_SpeechRom[ 0x8000 ];

	// 16-byte/128-bit parallel-serial FIFO
	UINT8          m_FIFO[ FIFO_BYTES ];

	// Hardware registers
	bool           m_TalkStatus;

	// Current/Target parameters & interpolation information
	sSpeechParams  m_StartParams;
	sSpeechParams  m_TargetParams;
	int            m_InterpolationStage;

	iComputer     *m_Computer;

	// 8 KHz speech buffer
	int            m_PitchIndex;
	double         m_FilterHistory[ 2 ][ RC_ORDER + 1 ];
	double         m_RawDataBuffer[ INTERPOLATION_SAMPLES ];

	// Resampled synthesized speech buffer
	int            m_PlaybackInterval;
	int            m_PlaybackBufferSize;
	double        *m_PlaybackBuffer;
	double         m_PlaybackRatio;
	double         m_PlaybackOffset;

	int            m_PlaybackSamplesLeft;
	double        *m_PlaybackDataPtr;

	void LoadAddress( UINT8 data );

	bool WaitForBitsFIFO( int );

	void StoreDataFIFO( UINT8 data );

	UINT8 ReadBitsFIFO( int );
	UINT8 ReadBitsROM( int );
	UINT8 ReadBits( int );

	bool CreateNextBuffer( );
	bool ConvertBuffer( );
	bool GetNextBuffer( );

	static char *FormatParameters( const sSpeechParams &, bool );
	static void InterpolateParameters( int, const sSpeechParams &, const sSpeechParams &, sSpeechParams * );

	bool ReadFrame( sSpeechParams *, bool );

public:

	cTMS5220( );

	// iBaseObject Methods
	virtual const void *GetInterface( const std::string &name ) const override;

	// iTMS5220 methods
	virtual void SetSoundChip( iTMS9919 * ) override;
	virtual void SetComputer( iComputer * ) override;
	virtual bool AudioCallback( INT16 *, int ) override;
	virtual void Reset( ) override;
	virtual UINT8 WriteData( UINT8 ) override;
	virtual UINT8 ReadData( UINT8 ) override;

	// iStateObject Methods
	virtual std::string GetIdentifier( ) override;
	virtual std::optional<sStateSection> SaveState( ) override;
	virtual bool ParseState( const sStateSection &state ) override;

protected:

	UINT8 GetStatus( ) const;

	virtual ~cTMS5220( ) override;

private:

	cTMS5220( const cTMS5220 & ) = delete;			// no implementation
	void operator =( const cTMS5220 & ) = delete;	// no implementation

};

#endif
