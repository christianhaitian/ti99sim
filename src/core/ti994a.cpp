//----------------------------------------------------------------------------
//
// File:        ti994a.cpp
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

#include <exception>
#include <stdexcept>
#include <cstdio>
#include <cstring>
#include "common.hpp"
#include "device-support.hpp"
#include "logger.hpp"
#include "compress.hpp"
#include "ti994a.hpp"
#include "cartridge.hpp"
#include "memory.hpp"
#include "opcodes.hpp"
#include "tms9900.hpp"
#include "tms9901.hpp"
#include "tms9918a.hpp"
#include "tms9919.hpp"
#include "tms5220.hpp"
#include "support.hpp"

DBG_REGISTER( __FILE__ );

extern cTI994A *CRU_Object;
extern void   (*TimerHook)( );

static cTI994A *pTimerObj;

cTI994A::cTI994A( iCartridge *console, iTMS9918A *vdp, iTMS9919 *sound, iTMS5220 *speech ) :
	cBaseObject( "cTI994A" ),
	m_CPU( new cTMS9900 ),
	m_PIC( new cTMS9901 ),
	m_VDP( vdp ? vdp : new cTMS9918A ),
	m_SoundGenerator( sound ? sound : new cTMS9919 ),
	m_SpeechSynthesizer( speech ),
	m_ClockSpeed( CPU_SPEED_HZ ),
	m_RetraceInterval( 0 ),
	m_LastRetrace( 0 ),
	m_Console( console ),
	m_Cartridge( nullptr ),
	m_ActiveCRU( 0 ),
	m_ActiveDevice( nullptr ),
	m_Device( ),
	m_GromAddress( 0 ),
	m_GromReadShift( 8 ),
	m_GromWriteShift( 8 ),
	m_Scratchpad( ),
	m_DefaultBank( ),
	m_CpuMemoryInfo( ),
	m_GromMemoryInfo( ),
	m_VideoMemory( new UINT8[ 0x4000 ] )
{
	FUNCTION_ENTRY( this, "cTI994A ctor", true );

	// Store a copy of our 'this' pointer for the TimerHook function
	pTimerObj = this;

	// Define CRU_Object for _OpCodes.asm
	CRU_Object = this;

	memset( m_VideoMemory, 0, 0x4000 );

	m_VDP->SetMemory( m_VideoMemory );
	m_VDP->SetPIC( m_PIC, 2 );

	m_SoundGenerator->SetSpeechSynthesizer( m_SpeechSynthesizer );

	if( m_SpeechSynthesizer != nullptr )
	{
		m_SpeechSynthesizer->SetSoundChip( m_SoundGenerator );
		m_SpeechSynthesizer->SetComputer( this );
	}

	// Add the TMS9901 programmable timer
	RegisterDevice( static_cast<iDevice *>( m_PIC->GetInterface( "iDevice" )));

	TimerHook = _TimerHookProc;

	// Register the bank swap trap function here - used in UpdateBreakpoint
	m_CPU->RegisterTrapHandler( TrapFunction, this, TRAP_BANK_SWITCH );

	// Default to a system with ROM in every bank
	m_DefaultBank.CurBank = m_DefaultBank.Bank;

	for( auto &bank : m_CpuMemoryInfo )
	{
		bank.push_back( &m_DefaultBank );
	}
	for( auto &bank : m_GromMemoryInfo )
	{
		bank.push_back( &m_DefaultBank );
	}

	if( m_Console == nullptr )
	{
		std::string romFile = LocateCartridge( "console", "TI-994A.ctg",
			{
				"0264512c7d9e7fa091a48e5c8734782ea031a52d",
				"16e275faae427465ba4dd4c2bf8569f6546d32dd"		// console version 2.2
			} );

		if( !romFile.empty( ))
		{
			m_Console = new cCartridge( romFile );
		}
	}

	// Add the console ROM if one was provided
	if( m_Console != nullptr )
	{
		AddCartridge( m_Console, 0x00FFFFFF );

		// Do a quick sanity check on the system ROM before starting
		UINT16 wp = cpuMemory.ReadWord( 0x0000 );
		UINT16 pc = cpuMemory.ReadWord( 0x0002 );

		// Make sure that the level 0 interrupt vector is valid:
		//  1) The workspace pointer is in scratchpad RAM
		//  2) The startup code is in system ROM
		if((( wp & 0xFF00 ) != 0x8300 ) || (( pc & 0xE000 ) != 0x0000 ))
		{
			fprintf( stderr, "WARNING: System ROM appears to be invalid!\n" );
		}

		if( UINT8 MHz = cpuMemory.ReadByte( 0x000C ))
		{
			m_ClockSpeed = 1000000 * MHz / 16;
		}

		Reset( );
	}

	m_RetraceInterval = m_ClockSpeed / dynamic_cast<cTMS9918A *>( m_VDP.get( ))->GetRefreshRate( );

	// Mark the scratchpad RAM area so that we alias it correctly
	cpuMemory.SetMemory( 0x8000, 0x0100, m_Scratchpad, false );
	cpuMemory.SetMemory( 0x8100, 0x0100, m_Scratchpad, false );
	cpuMemory.SetMemory( 0x8200, 0x0100, m_Scratchpad, false );
	cpuMemory.SetMemory( 0x8300, 0x0100, m_Scratchpad, false );

	UINT8 index;

	index = m_CPU->RegisterTrapHandler( TrapFunction, this, TRAP_SOUND );
	for( UINT16 address = 0x8400; address < 0x8800; address += ( UINT16 ) 1 )
	{
		m_CPU->SetTrap( address, MEMFLG_TRAP_WRITE, index );									// Sound chip Port
	}

	index = m_CPU->RegisterTrapHandler( TrapFunction, this, TRAP_VIDEO );
	for( UINT16 address = 0x8800; address < 0x8C00; address += ( UINT16 ) 4 )
	{
		m_CPU->SetTrap(( UINT16 ) ( address | 0x0000 ), MEMFLG_TRAP_READ, index );				// VDP Read Byte Port
		m_CPU->SetTrap(( UINT16 ) ( address | 0x0002 ), MEMFLG_TRAP_READ, index );				// VDP Read Status Port
		m_CPU->SetTrap(( UINT16 ) ( address | 0x0400 ), MEMFLG_TRAP_WRITE, index );				// VDP Write Byte Port
		m_CPU->SetTrap(( UINT16 ) ( address | 0x0402 ), MEMFLG_TRAP_WRITE, index );				// VDP Write (set) Address Port
	}

	// Make this bank look like ROM (writes aren't stored)
	cpuMemory.SetMemory( 0x9000, ROM_BANK_SIZE, nullptr, true );

	index = m_CPU->RegisterTrapHandler( TrapFunction, this, TRAP_SPEECH );
	for( UINT16 address = 0x9000; address < 0x9400; address += ( UINT16 ) 2 )
	{
		m_CPU->SetTrap(( UINT16 ) ( address | 0x0000 ), ( UINT8 ) MEMFLG_TRAP_READ, index );	// Speech Read Port
		m_CPU->SetTrap(( UINT16 ) ( address | 0x0400 ), ( UINT8 ) MEMFLG_TRAP_WRITE, index );	// Speech Write Port
	}

	index = m_CPU->RegisterTrapHandler( TrapFunction, this, TRAP_GROM );
	for( UINT16 address = 0x9800; address < 0x9C00; address += ( UINT16 ) 2 )
	{
		m_CPU->SetTrap(( UINT16 ) ( address | 0x0000 ), ( UINT8 ) MEMFLG_TRAP_READ, index );	// GROM Read Port
		m_CPU->SetTrap(( UINT16 ) ( address | 0x0400 ), ( UINT8 ) MEMFLG_TRAP_WRITE, index );	// GROM Write Port
	}
}

cTI994A::~cTI994A( )
{
	FUNCTION_ENTRY( this, "cTI994A dtor", true );

	delete [] m_VideoMemory;
}

//----------------------------------------------------------------------------
// iBaseObject Methods
//----------------------------------------------------------------------------

const void *cTI994A::GetInterface( const std::string &iName ) const
{
	if( iName == "iComputer" )
	{
		return static_cast<const iComputer *>( this );
	}

	return cBaseObject::GetInterface( iName );
}

//----------------------------------------------------------------------------
// iComputer Methods
//----------------------------------------------------------------------------

iCartridge *cTI994A::GetConsole( ) const
{
	FUNCTION_ENTRY( this, "cTI994A::GetConsole", false );

	return m_Console;
}

iTMS9900 *cTI994A::GetCPU( ) const
{
	FUNCTION_ENTRY( this, "cTI994A::GetCPU", false );

	return m_CPU;
}

iTMS9918A *cTI994A::GetVDP( ) const
{
	FUNCTION_ENTRY( this, "cTI994A::GetVDP", false );

	return m_VDP;
}

iTMS9919 *cTI994A::GetSoundGenerator( ) const
{
	FUNCTION_ENTRY( this, "cTI994A::GetSoundGenerator", false );

	return m_SoundGenerator;
}

iTMS5220 *cTI994A::GetSynthesizer( ) const
{
	FUNCTION_ENTRY( this, "cTI994A::GetSynthesizer", false );

	return m_SpeechSynthesizer;
}

UINT8 *cTI994A::GetVideoMemory( ) const
{
	FUNCTION_ENTRY( this, "cTI994A::GetVideoMemory", false );

	return m_VideoMemory;
}

void cTI994A::SetGromAddress( UINT16 addr )
{
	FUNCTION_ENTRY( this, "cTI994A::SetGromAddress", false );

	m_GromAddress = addr;
}

UINT16 cTI994A::GetGromAddress( ) const
{
	FUNCTION_ENTRY( this, "cTI994A::GetGromAddress", false );

	return m_GromAddress;
}

iDevice *cTI994A::GetDevice( ADDRESS address ) const
{
	FUNCTION_ENTRY( this, "cTI994A::GetDevice", false );

	// CRU Allocations:
	//  0000-0FFE       System Console
	//  1000-10FE       Unassigned
	//  1100-11FE       Disk Controller Card
	//  1200-12FE       Reserved
	//  1300-13FE       RS-232 (primary)
	//  1400-14FE       Unassigned
	//  1500-15FE       RS-232 (secondary)
	//  1600-16FE       Unassigned
	//  1700-17FE       HEX-BUS™ Interface
	//  1800-18FE       Thermal Printer
	//  1900-10FE       Reserved
	//  1A00-1AFE       Unassigned
	//  1B00-1BFE       Unassigned
	//  1C00-1CFE       Video Controller Card
	//  1D00-1DFE       IEE-488 Bus Controller Card
	//  1E00-1EFE       Unassigned
	//  1F00-1FFE       P-Code Card

	return m_Device[( address < 0x1000 ) ? 0 : ( address >> 8 ) & 0x1F ];
}

void cTI994A::_TimerHookProc( )
{
	FUNCTION_ENTRY( nullptr, "cTI994A::_TimerHookProc", false );

	UINT32 clockCycles = pTimerObj->m_CPU->GetClocks( );

	pTimerObj->TimerHookProc( clockCycles );
}

void cTI994A::TimerHookProc( UINT32 clockCycles )
{
	FUNCTION_ENTRY( this, "cTI994A::TimerHookProc", false );

	// Simulate a 50/60Hz VDP interrupt
	if( clockCycles - m_LastRetrace > m_RetraceInterval )
	{
		m_LastRetrace += m_RetraceInterval;
		VideoRetrace( );
	}
}

bool cTI994A::VideoRetrace( )
{
	FUNCTION_ENTRY( this, "cTI994A::VideoRetrace", false );

	return m_VDP->Retrace( );
}

UINT8 cTI994A::TrapFunction( void *ptr, int type, bool read, ADDRESS address, UINT8 value )
{
	FUNCTION_ENTRY( ptr, "cTI994A::TrapFunction", false );

	cTI994A *pThis = static_cast<cTI994A *>( ptr );
	UINT8 retVal = value;

	if(( address & 1 ) == 0 )
	{
		if( read == true )
		{
			switch( type )
			{
				case TRAP_SOUND :
					retVal = pThis->SoundBreakPoint( address, value );
					break;
				case TRAP_SPEECH :
					pThis->m_CPU->AddClocks( 48 );
					retVal = pThis->SpeechReadBreakPoint( address, value );
					break;
				case TRAP_VIDEO :
					retVal = pThis->VideoReadBreakPoint( address, value );
					break;
				case TRAP_GROM :
					retVal = pThis->GromReadBreakPoint( address, value );
					break;
				default :
					fprintf( stderr, "Invalid index %d for read access in TrapFunction\n", type );
					break;
			}
		}
		else
		{
			switch( type )
			{
				case TRAP_BANK_SWITCH :
					retVal = pThis->BankSwitch( address, value );
					break;
				case TRAP_SOUND :
					pThis->m_CPU->AddClocks( 28 );
					retVal = pThis->SoundBreakPoint( address, value );
					break;
				case TRAP_SPEECH :
					pThis->m_CPU->AddClocks( 64 );
					retVal = pThis->SpeechWriteBreakPoint( address, value );
					break;
				case TRAP_VIDEO :
					retVal = pThis->VideoWriteBreakPoint( address, value );
					break;
				case TRAP_GROM :
					retVal = pThis->GromWriteBreakPoint( address, value );
					break;
				default :
					fprintf( stderr, "Invalid index %d for write access in TrapFunction\n", type );
					break;
			}
		}
	}

	return retVal;
}

UINT8 cTI994A::BankSwitch( ADDRESS address, UINT8 )
{
	FUNCTION_ENTRY( this, "cTI994A::BankSwitch", false );

	ADDRESS baseAddress = ( ADDRESS ) ( address & 0xE000 );

	sMemoryRegion *region = m_CpuMemoryInfo[ baseAddress / ROM_BANK_SIZE ].back( );
	int newBank = ( address >> 1 ) % region->NumBanks;

	DBG_TRACE( "Switching to bank " << newBank << " (write to address 0x" << hex << address << ")" );

	region[ 0 ].CurBank = &region[ 0 ].Bank[ newBank ];
	region[ 1 ].CurBank = &region[ 1 ].Bank[ newBank ];

	cpuMemory.SetMemory( baseAddress + 0 * ROM_BANK_SIZE, ROM_BANK_SIZE, region[ 0 ].CurBank->Data, true );
	cpuMemory.SetMemory( baseAddress + 1 * ROM_BANK_SIZE, ROM_BANK_SIZE, region[ 1 ].CurBank->Data, true );

	return cpuMemory.ReadByte( address );
}

UINT8 cTI994A::SoundBreakPoint( ADDRESS, UINT8 data )
{
	FUNCTION_ENTRY( this, "cTI994A::SoundBreakPoint", false );

	m_SoundGenerator->WriteData( data );

	return data;
}

UINT8 cTI994A::SpeechWriteBreakPoint( ADDRESS, UINT8 data )
{
	FUNCTION_ENTRY( this, "cTI994A::SpeechWriteBreakPoint", false );

	if( m_SpeechSynthesizer != nullptr )
	{
		m_SpeechSynthesizer->WriteData( data );
	}

	return data;
}

UINT8 cTI994A::SpeechReadBreakPoint( ADDRESS, UINT8 data )
{
	FUNCTION_ENTRY( this, "cTI994A::SpeechReadBreakPoint", false );

	if( m_SpeechSynthesizer != nullptr )
	{
		data = m_SpeechSynthesizer->ReadData( data );
	}

	return data;
}

UINT8 cTI994A::VideoReadBreakPoint( ADDRESS address, UINT8 data )
{
	FUNCTION_ENTRY( this, "cTI994A::VideoReadBreakPoint", false );

	switch( address & 0x0002 )
	{
		case 0x0000 :
			data = m_VDP->ReadData( );
			break;
		case 0x0002 :
			data = m_VDP->ReadStatus( );
			break;
	}

	return data;
}

UINT8 cTI994A::VideoWriteBreakPoint( ADDRESS address, UINT8 data )
{
	FUNCTION_ENTRY( this, "cTI994A::VideoWriteBreakPoint", false );

	switch( address & 0x0002 )
	{
		case 0x0000 :
			m_VDP->WriteData( data );
			break;
		case 0x0002 :
			m_VDP->SetAddress( data );
			break;
	}

	return data;
}

UINT8 cTI994A::GromReadBreakPoint( ADDRESS address, UINT8 data )
{
	FUNCTION_ENTRY( this, "cTI994A::GromReadBreakPoint", false );

	m_GromWriteShift = 8;

	switch( address & 0x0002 )
	{
		case 0x0000 :					// GROM/GRAM Read Byte Port
			m_CPU->AddClocks( 19 );
			data = gplMemory.ReadByte( m_GromAddress );
			m_GromAddress = ( UINT16 ) (( m_GromAddress & 0xE000 ) | (( m_GromAddress + 1 ) & 0x1FFF ));
			break;
		case 0x0002 :					// GROM/GRAM Read Address Port
			m_CPU->AddClocks( 13 );
			data = ( UINT8 ) ((( m_GromAddress + 1 ) >> m_GromReadShift ) & 0x00FF );
			m_GromReadShift  = 8 - m_GromReadShift;
			break;
	}

	return data;
}

UINT8 cTI994A::GromWriteBreakPoint( ADDRESS address, UINT8 data )
{
	FUNCTION_ENTRY( this, "cTI994A::GromWriteBreakPoint", false );

	switch( address & 0x0002 )
	{
		case 0x0000 :					// GROM/GRAM Write Byte Port
			m_CPU->AddClocks( 22 );
			gplMemory.WriteByte( m_GromAddress, data );
			m_GromAddress = ( UINT16 ) (( m_GromAddress & 0xE000 ) | (( m_GromAddress + 1 ) & 0x1FFF ));
			m_GromWriteShift = 8;
			break;
		case 0x0002 :					// GROM/GRAM Write (set) Address Port
			m_CPU->AddClocks( m_GromWriteShift ? 15 : 21 );
			m_GromAddress &= ( ADDRESS ) ( 0xFF00 >> m_GromWriteShift );
			m_GromAddress |= ( ADDRESS ) ( data << m_GromWriteShift );
			m_GromWriteShift = 8 - m_GromWriteShift;
			m_GromReadShift  = 8;
			break;
	}

	return data;
}

void WriteCRU( cTI994A *ti, ADDRESS address, int count, UINT16 value )
{
	FUNCTION_ENTRY( nullptr, "cTI994A::WriteCRU", false );

	while( count-- )
	{
		ti->WriteCRU(( ADDRESS ) ( address++ & 0x1FFF ), ( UINT16 ) ( value & 1 ));
		value >>= 1;
	}
}

int ReadCRU( cTI994A *ti, ADDRESS address, int count )
{
	FUNCTION_ENTRY( nullptr, "cTI994A::ReadCRU", false );

	int value = 0;
	address += ( UINT16 ) count;
	while( count-- )
	{
		value <<= 1;
		value |= ti->ReadCRU(( ADDRESS ) ( --address & 0x1FFF ));
	}
	return value;
}

int cTI994A::ReadCRU( ADDRESS address )
{
	FUNCTION_ENTRY( this, "cTI994A::ReadCRU", false );

	address <<= 1;
	iDevice *dev = GetDevice( address );

	int val = ( dev != nullptr ) ? dev->ReadCRU(( ADDRESS ) (( address - dev->GetCRU( )) >> 1 )) : 1;

	if( address > 0x07FF )
	{
		DBG_TRACE( "CRU read from address 0x" << hex << address << " - " << ( UINT8 ) val );
	}

	return val;
}

void cTI994A::WriteCRU( ADDRESS address, UINT16 val )
{
	FUNCTION_ENTRY( this, "cTI994A::WriteCRU", false );

	address <<= 1;
	iDevice *dev = GetDevice( address );

	if( address > 0x07FF )
	{
		DBG_TRACE( "CRU write to address 0x" << hex << address << " - " << ( UINT8 ) val );
	}

	if( dev != nullptr )
	{
		dev->WriteCRU(( UINT16 ) (( address - dev->GetCRU( )) >> 1 ), val );
	}
}

void cTI994A::Run( )
{
	FUNCTION_ENTRY( this, "cTI994A::Run", true );

	m_CPU->Run( );
}

bool cTI994A::Step( )
{
	FUNCTION_ENTRY( this, "cTI994A::Step", true );

	return m_CPU->Step( );
}

void cTI994A::Stop( )
{
	FUNCTION_ENTRY( this, "cTI994A::Stop", true );

	m_CPU->Stop( );
}

bool cTI994A::IsRunning( )
{
	FUNCTION_ENTRY( this, "cTI994A::IsRunning", true );

	return m_CPU->IsRunning( );
}

bool cTI994A::SaveImage( const char *filename )
{
	FUNCTION_ENTRY( this, "cTI994A::SaveImage", true );

	if( auto save = SaveState( ))
	{
		CreateHomePath( );

		auto path = std::filesystem::path{ GetHomePath( )} / std::filesystem::path( filename );

		save->SaveImage( path );

		return true;
	}

	return false;
}

bool cTI994A::LoadImage( const char *filename )
{
	FUNCTION_ENTRY( this, "cTI994A::LoadImage", true );

	auto restore = SaveState( );

	try
	{
		auto path = std::filesystem::path{ GetHomePath( )} / std::filesystem::path( filename );

		if( auto save =  sStateSection::LoadImage( path ))
		{
			if( ParseState( *save ))
			{
				return true;
			}
		}

		fprintf( stderr, "Failed to load saved image!\n" );
	}
	catch( const std::string &error )
	{
		fprintf( stderr, "EXCEPTION: %s!\n", error.c_str( ));
	}

	return ParseState( *restore );
}

std::optional<sStateSection> cTI994A::SaveState( )
{
	FUNCTION_ENTRY( this, "cTI994A::SaveState", true );

	sStateSection save;

	save.name = "TI-994/A Memory Image File";

	iBaseObject *chips[] = { m_CPU, m_VDP, m_PIC, m_SoundGenerator, m_SpeechSynthesizer };

	for( auto &chip : chips )
	{
		save.addSubSection( chip );
	}

	save.store( "LastRetrace", m_CPU->GetClocks( ) - m_LastRetrace, SaveFormat::DECIMAL );

	if( m_Console )
	{
		save.store( "Console", m_Console->GetDescriptor( ));
		save.addSubSection( m_Console );
	}

	if( m_Cartridge )
	{
		save.store( "Cartridge", m_Cartridge->GetDescriptor( ));
		save.addSubSection( m_Cartridge );
	}

	save.store( "ActiveCRU", m_ActiveCRU, SaveFormat::HEXADECIMAL );

	auto pic = dynamic_cast<iDevice *>( m_PIC.get( ));
	sStateSection devices;
	for( auto &device : m_Device )
	{
		if( device.get( ) != pic )
		{
			devices.addSubSection( device );
		}
	}

	if( !devices.subsections.empty( ))
	{
		devices.name = "Devices";
		save.subsections.push_back( devices );
	}

	save.store( "GromAddress", m_GromAddress, SaveFormat::HEXADECIMAL );
	save.store( "GromReadShift", m_GromReadShift, SaveFormat::DECIMAL );
	save.store( "GromWriteShift", m_GromWriteShift, SaveFormat::DECIMAL );
	save.store( "PAD", m_Scratchpad, sizeof( m_Scratchpad ));

	return save;
}

bool cTI994A::ParseState( const sStateSection &save )
{
	FUNCTION_ENTRY( this, "cTI994A::ParseState", true );

	if( save.name != "TI-994/A Memory Image File" )
	{
		return false;
	}

	iBaseObject *chips[] = { m_CPU, m_VDP, m_PIC, m_SoundGenerator, m_SpeechSynthesizer };

	for( auto &chip : chips )
	{
		save.loadSubSection( chip );
	}

	UINT32 lastRetrace;
	save.load( "LastRetrace", lastRetrace, SaveFormat::DECIMAL );
	m_LastRetrace = m_CPU->GetClocks( ) - lastRetrace;

	if( save.hasValue( "Console" ))
	{
		auto consoleRef = save.getValue( "Console" );
		if( auto console = cCartridge::LoadCartridge( consoleRef, "console" ))
		{
			save.loadSubSection( console );
			ReplaceConsole( console );

			if( UINT8 MHz = cpuMemory.ReadByte( 0x000C ))
			{
				m_ClockSpeed = 1000000 * MHz / 16;
			}
		}
	}

	if( save.hasValue( "Cartridge" ))
	{
		auto cartridgeRef = save.getValue( "Cartridge" );
		if( auto cartridge = cCartridge::LoadCartridge( cartridgeRef, "cartridges" ))
		{
			save.loadSubSection( cartridge );
			ReplaceCartridge( cartridge );
		}
	}

	if( m_ActiveCRU )
	{
		DisableDevice( GetDevice( m_ActiveCRU ));
	}

	save.load( "ActiveCRU", m_ActiveCRU, SaveFormat::HEXADECIMAL );

	if( save.hasSubsection( "Devices" ))
	{
		auto pic = dynamic_cast<iDevice *>( m_PIC.get( ));
		for( auto &device : m_Device )
		{
			if( device.get( ) != pic )
			{
				device = nullptr;
			}
		}

		auto &devices = save.getSubsection( "Devices" );
		for( auto &section : devices.subsections )
		{
			if( section.hasValue( "ROM" ))
			{
				auto device = LoadDevice( section.getValue( "ROM" ), "console" );
				RegisterDevice( device );
				devices.loadSubSection( device );
			}
		}
	}

	if( m_ActiveCRU )
	{
		EnableDevice( GetDevice( m_ActiveCRU ));
	}

	save.load( "GromAddress", m_GromAddress, SaveFormat::HEXADECIMAL );
	save.load( "GromReadShift", m_GromReadShift, SaveFormat::DECIMAL );
	save.load( "GromWriteShift", m_GromWriteShift, SaveFormat::DECIMAL );
	save.load( "PAD", m_Scratchpad, sizeof( m_Scratchpad ));

	return true;
}

void cTI994A::Reset( )
{
	FUNCTION_ENTRY( this, "cTI994A::Reset", true );

	if( m_CPU != nullptr )
	{
		m_CPU->Reset( );
	}
	if( m_VDP != nullptr )
	{
		m_VDP->Reset( );
	}
	if( m_SpeechSynthesizer != nullptr )
	{
		m_SpeechSynthesizer->Reset( );
	}
}

bool cTI994A::Sleep( int cycles, UINT32 timeout )
{
	FUNCTION_ENTRY( this, "cTI994A::Sleep", false );

	m_CPU->AddClocks( cycles );

	return false;
}

bool cTI994A::WakeCPU( UINT32 )
{
	FUNCTION_ENTRY( this, "cTI994A::WakeCPU", false );

	return false;
}

bool cTI994A::RegisterDevice( iDevice *device )
{
	FUNCTION_ENTRY( this, "cTI994A::RegisterDevice", true );

	int index = ( device->GetCRU( ) >> 8 ) & 0x1F;

	if( m_Device[ index ] != nullptr )
	{
		DBG_WARNING( "A CRU device already exists at address " << index );
		return false;
	}

	m_Device[ index ] = device;

	return device->Initialize( this );
}

bool cTI994A::EnableDevice( iDevice *device )
{
	FUNCTION_ENTRY( this, "cTI994A::EnableDevice", true );

	if( m_ActiveDevice != device )
	{
		if( m_ActiveDevice != nullptr )
		{
			DBG_WARNING( "Another device is already active - disabling" );
			DisableDevice( m_ActiveDevice );
		}

		m_ActiveDevice = device;
		AddCartridge( device->GetROM( ), INFO_MASK_DSR );
	}

	return true;
}

bool cTI994A::DisableDevice( iDevice *device )
{
	FUNCTION_ENTRY( this, "cTI994A::DisableDevice", true );

	if( m_ActiveDevice == device )
	{
		m_ActiveDevice = nullptr;
		RemoveCartridge( device->GetROM( ), INFO_MASK_DSR );
	}

	return true;
}

bool cTI994A::InsertCartridge( iCartridge *cartridge )
{
	FUNCTION_ENTRY( this, "cTI994A::InsertCartridge", true );

	DBG_ASSERT( cartridge != nullptr );

	if( m_Cartridge != nullptr )
	{
		DBG_WARNING( "A cartridge is already inserted - removing" );

		return false;
	}

	m_Cartridge = cartridge;

	for( size_t i = 0; i < SIZE( m_CpuMemoryInfo ); i++ )
	{
		sMemoryRegion *region = cartridge->GetCpuMemory( i );
		region->CurBank = region->Bank;
	}

	for( size_t i = 0; i < SIZE( m_GromMemoryInfo ); i++ )
	{
		sMemoryRegion *region = cartridge->GetGromMemory( i );
		region->CurBank = region->Bank;
	}

	AddCartridge( cartridge, INFO_MASK_CARTRIDGE );

	Reset( );

	return true;
}

void cTI994A::RemoveCartridge( )
{
	FUNCTION_ENTRY( this, "cTI994A::RemoveCartridge", true );

	if( m_Cartridge != nullptr )
	{
		RemoveCartridge( m_Cartridge, INFO_MASK_CARTRIDGE );

		m_Cartridge = nullptr;

		Reset( );
	}
}

void cTI994A::ReplaceConsole( iCartridge *console )
{
	FUNCTION_ENTRY( this, "cTI994A::ReplaceConsole", true );

	if( m_Console != nullptr )
	{
		RemoveCartridge( m_Console, 0x00FFFFFF );
	}

	m_Console = console;

	AddCartridge( m_Console, 0x00FFFFFF );
}

void cTI994A::ReplaceCartridge( iCartridge *cartridge )
{
	FUNCTION_ENTRY( this, "cTI994A::ReplaceCartridge", true );

	if( m_Cartridge != nullptr )
	{
		RemoveCartridge( m_Cartridge, INFO_MASK_CARTRIDGE );
	}

	m_Cartridge = cartridge;

	AddCartridge( m_Cartridge, INFO_MASK_CARTRIDGE );
}

void cTI994A::AddCartridge( iCartridge *cartridge, int mask )
{
	FUNCTION_ENTRY( this, "cTI994A::AddCartridge", true );

	DBG_ASSERT( cartridge != nullptr );

	int changed = 0;

	for( unsigned i = 0; i < SIZE( m_CpuMemoryInfo ); i++ )
	{
		if( mask & ( 0x00001 << i ))
		{
			sMemoryRegion *region = cartridge->GetCpuMemory( i );
			if( region->NumBanks > 0 )
			{
				m_CpuMemoryInfo[ i ].push_back( region );
				changed |= ( 0x00001 << i );
			}
		}
	}

	for( unsigned i = 0; i < SIZE( m_GromMemoryInfo ); i++ )
	{
		if( mask & ( 0x10000 << i ))
		{
			sMemoryRegion *region = cartridge->GetGromMemory( i );
			if( region->NumBanks > 0 )
			{
				m_GromMemoryInfo[ i ].push_back( region );
				changed |= ( 0x10000 << i );
			}
		}
	}

	UpdateMemory( changed );
}

void cTI994A::RemoveCartridge( iCartridge *cartridge, int mask )
{
	FUNCTION_ENTRY( this, "cTI994A::RemoveCartridge", true );

	int changed = 0;

	for( unsigned i = 0; i < SIZE( m_CpuMemoryInfo ); i++ )
	{
		if( mask & ( 0x00001 << i ))
		{
			sMemoryRegion *region = cartridge->GetCpuMemory( i );
			if( m_CpuMemoryInfo[ i ].back( ) == region )
			{
				m_CpuMemoryInfo[ i ].pop_back( );
				changed |= ( 0x00001 << i );
			}
		}
	}

	for( unsigned i = 0; i < SIZE( m_GromMemoryInfo ); i++ )
	{
		if( mask & ( 0x10000 << i ))
		{
			sMemoryRegion *region = cartridge->GetGromMemory( i );
			if( m_GromMemoryInfo[ i ].back( ) == region )
			{
				m_GromMemoryInfo[ i ].pop_back( );
				changed |= ( 0x10000 << i );
			}
		}
	}

	UpdateMemory( changed );
}

void cTI994A::UpdateMemory( int mask )
{
	FUNCTION_ENTRY( this, "cTI994A::UpdateMemory", true );

	for( unsigned i = 0; i < SIZE( m_CpuMemoryInfo ); i++ )
	{
		if( mask & ( 0x00001 << i ))
		{
			sMemoryRegion *region = m_CpuMemoryInfo[ i ].back( );
			cpuMemory.SetMemory( i * ROM_BANK_SIZE, ROM_BANK_SIZE, region->CurBank->Data, region->CurBank->Flags & FLAG_READ_ONLY );
			UpdateBreakpoint( i * ROM_BANK_SIZE, region->NumBanks > 1 );
		}
	}

	for( unsigned i = 0; i < SIZE( m_GromMemoryInfo ); i++ )
	{
		if( mask & ( 0x10000 << i ))
		{
			sMemoryRegion *region = m_GromMemoryInfo[ i ].back( );
			gplMemory.SetMemory( i * GROM_BANK_SIZE, GROM_BANK_SIZE, region->CurBank->Data, region->CurBank->Flags & FLAG_READ_ONLY );
		}
	}
}

void cTI994A::UpdateBreakpoint( int address, bool set )
{
	FUNCTION_ENTRY( this, "cTI994A::UpdateBreakpoint", true );

	UINT8 index = m_CPU->GetTrapIndex( TrapFunction, TRAP_BANK_SWITCH );

	if( set )
	{
		for( int j = 0; j < ROM_BANK_SIZE; j++ )
		{
			m_CPU->SetTrap( address++, MEMFLG_TRAP_WRITE, index );
		}
	}
	else
	{
		m_CPU->ClearTrap( index, address, ROM_BANK_SIZE );
	}
}
