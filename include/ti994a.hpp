//----------------------------------------------------------------------------
//
// File:		ti994a.hpp
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

#ifndef TI994A_HPP_
#define TI994A_HPP_

#include "cBaseObject.hpp"
#include "icartridge.hpp"
#include "icomputer.hpp"
#include "tms9900.hpp"

const int CPU_SPEED_HZ = 3000000;

const int INFO_MASK_CARTRIDGE = 0x00F800C0;		// Normal cartridge (GROMS 3-7, ROMS 6,7)
const int INFO_MASK_DSR       = 0x00000030;		// Device cartridge (ROMS 4,5)

struct iTMS9900;
struct iTMS9901;
struct iTMS9918A;
struct iTMS9919;
struct iTMS5220;

struct iCartridge;

class cTI994A :
	public virtual cBaseObject,
	public virtual iComputer
{
protected:

	enum TRAP_TYPE_E
	{
		TRAP_BANK_SWITCH,
		TRAP_SCRATCH_PAD,
		TRAP_SOUND,
		TRAP_SPEECH,
		TRAP_VIDEO,
		TRAP_GROM
	};

	cRefPtr<iTMS9900>   m_CPU;
	cRefPtr<iTMS9901>   m_PIC;
	cRefPtr<iTMS9918A>  m_VDP;
	cRefPtr<iTMS9919>   m_SoundGenerator;
	cRefPtr<iTMS5220>   m_SpeechSynthesizer;

	UINT32              m_ClockSpeed;
	UINT32              m_RetraceInterval;
	UINT32              m_LastRetrace;

	cRefPtr<iCartridge> m_Console;
	cRefPtr<iCartridge> m_Cartridge;

	UINT16              m_ActiveCRU;
	iDevice            *m_ActiveDevice;
	cRefPtr<iDevice>    m_Device[ 32 ];

	ADDRESS             m_GromAddress;
	int                 m_GromReadShift;
	int                 m_GromWriteShift;

	UINT8               m_Scratchpad[ 256 ];
	sMemoryRegion       m_DefaultBank;

	std::vector<sMemoryRegion*> m_CpuMemoryInfo[ 16 ];	// Pointers 4K banks of CPU RAM
	std::vector<sMemoryRegion*> m_GromMemoryInfo[ 8 ];	// Pointers 8K banks of Graphics RAM

	UINT8              *m_VideoMemory;			// Pointer to 16K of Video RAM

public:

	cTI994A( iCartridge *, iTMS9918A * = nullptr, iTMS9919 * = nullptr, iTMS5220 * = nullptr );
	virtual ~cTI994A( ) override;

	// iBaseObject Methods
	virtual const void *GetInterface( const std::string &name ) const override;

	// iComputer Methods
	virtual iCartridge *GetConsole( ) const override;
	virtual iTMS9900 *GetCPU( ) const override;
	virtual iTMS9918A *GetVDP( ) const override;
	virtual iTMS9919 *GetSoundGenerator( ) const override;
	virtual iTMS5220 *GetSynthesizer( ) const override;
	virtual UINT8 *GetVideoMemory( ) const override;
	virtual void SetGromAddress( UINT16 addr ) override;
	virtual UINT16 GetGromAddress( ) const override;
	virtual int ReadCRU( UINT16 ) override;
	virtual void WriteCRU( UINT16, UINT16 ) override;
	virtual bool RegisterDevice( iDevice * ) override;
	virtual bool EnableDevice( iDevice * ) override;
	virtual bool DisableDevice( iDevice * ) override;
	virtual bool InsertCartridge( iCartridge * ) override;
	virtual void RemoveCartridge( ) override;
	virtual void Reset( ) override;
	virtual bool Sleep( int, UINT32 ) override;
	virtual bool WakeCPU( UINT32 ) override;
	virtual void Run( ) override;
	virtual void Stop( ) override;
	virtual bool Step( ) override;
	virtual bool IsRunning( ) override;
	virtual bool SaveImage( const char * ) override;
	virtual bool LoadImage( const char * ) override;

protected:

	virtual std::optional<sStateSection> SaveState( );
	virtual bool ParseState( const sStateSection & );

	iDevice *GetDevice( ADDRESS ) const;

	void ReplaceConsole( iCartridge *console );
	void ReplaceCartridge( iCartridge *cartridge );

	void AddCartridge( iCartridge *, int );
	void RemoveCartridge( iCartridge *, int );

	void UpdateMemory( int );
	void UpdateBreakpoint( int, bool );

	static void _TimerHookProc( );
	virtual void TimerHookProc( UINT32 );
	virtual bool VideoRetrace( );

	static UINT8 TrapFunction( void *, int, bool, ADDRESS, UINT8 );

	virtual UINT8 BankSwitch           ( ADDRESS, UINT8 );
	virtual UINT8 SoundBreakPoint      ( ADDRESS, UINT8 );
	virtual UINT8 SpeechWriteBreakPoint( ADDRESS, UINT8 );
	virtual UINT8 SpeechReadBreakPoint ( ADDRESS, UINT8 );
	virtual UINT8 VideoWriteBreakPoint ( ADDRESS, UINT8 );
	virtual UINT8 VideoReadBreakPoint  ( ADDRESS, UINT8 );
	virtual UINT8 GromWriteBreakPoint  ( ADDRESS, UINT8 );
	virtual UINT8 GromReadBreakPoint   ( ADDRESS, UINT8 );

private:

	cTI994A( const cTI994A & ) = delete;				// no implementation
	cTI994A & operator =( const cTI994A & ) = delete;	// no implementation

};

#endif
