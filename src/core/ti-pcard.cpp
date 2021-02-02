//----------------------------------------------------------------------------
//
// File:        ti-pcard.cpp
// Date:        26-Jun-2013
// Programmer:  Marc Rousseau
//
// Description:
//
// Copyright (c) 2013-2016 Marc Rousseau, All Rights Reserved.
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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "common.hpp"
#include "logger.hpp"
#include "cartridge.hpp"
#include "memory.hpp"
#include "tms9900.hpp"
#include "device.hpp"
#include "ti-pcard.hpp"

DBG_REGISTER( __FILE__ );

cUCSDDevice::cUCSDDevice( iCartridge *rom ) :
	cBaseObject( "cUCSDDevice" ),
	cStateObject( ),
	cDevice( rom ),
	m_BankSwapped( 0 ),
	m_GromAddress( 0 ),
	m_GromReadShift( 8 ),
	m_GromWriteShift( 8 )
{
	FUNCTION_ENTRY( this, "cUCSDDevice::cUCSDDevice", true );

	if( m_IsValid && ( m_CRU + 1 == 0 ))
	{
		DBG_ERROR( "Cartridge does not appear to be a valid UCSD p-System device" );
		m_IsValid = false;
	}

	sMemoryRegion empty{ 0, nullptr, { }};

	// Hide the GROM so we can access it on non-standard ports
	for( int i = 0; i < NUM_GROM_BANKS; i++ )
	{
		m_GromMemory[ i ] = *rom->GetGromMemory( i );
		*rom->GetGromMemory( i ) = empty;
	}

	// Hide the 2 banks from the console so it won't bank-switch on us
	rom->GetCpuMemory( 4 )->NumBanks = 1;
	rom->GetCpuMemory( 5 )->NumBanks = 1;
}

cUCSDDevice::~cUCSDDevice( )
{
	FUNCTION_ENTRY( this, "cUCSDDevice::~cUCSDDevice", true );
}

//----------------------------------------------------------------------------
// iBaseObject Methods
//----------------------------------------------------------------------------

const void *cUCSDDevice::GetInterface( const std::string &iName ) const
{
	FUNCTION_ENTRY( this, "cUCSDDevice::GetInterface", false );

	const void *pInterface = cStateObject::GetInterface( iName );

	if( pInterface == nullptr )
	{
		pInterface = cDevice::GetInterface( iName );
	}

	return pInterface;
}

//----------------------------------------------------------------------------
// iStateObject Methods
//----------------------------------------------------------------------------

std::string cUCSDDevice::GetIdentifier( )
{
	FUNCTION_ENTRY( this, "cUCSDDevice::GetIdentifier", true );

	return "UCSD";
}

bool cUCSDDevice::ParseState( const sStateSection &state )
{
	FUNCTION_ENTRY( this, "cUCSDDevice::ParseState", true );
/*
	// TODO - convert to use cStateObject::ParseState

	if( fread( &m_BankSwapped, sizeof( m_BankSwapped ), 1, file ) != 1 )
	{
		DBG_ERROR( "Error loading image from file" );
		return false;
	}

	return true;
*/
	return false;
}

std::optional<sStateSection> cUCSDDevice::SaveState( )
{
	FUNCTION_ENTRY( this, "cUCSDDevice::SaveState", true );
/*
	// TODO - convert to use cStateObject::SaveState

	if( fwrite( &m_BankSwapped, sizeof( m_BankSwapped ), 1, file ) != 1 )
	{
		DBG_ERROR( "Error saving image to file" );
		return false;
	}

	return true;
*/
	return { };
}

//----------------------------------------------------------------------------
// iDevice methods
//----------------------------------------------------------------------------

const char *cUCSDDevice::GetName( )
{
	return "UCSD p-System";
}

bool cUCSDDevice::Initialize( iComputer *computer )
{
	FUNCTION_ENTRY( this, "cUCSDDevice::Initialize", true );

	return cDevice::Initialize( computer );
}

void cUCSDDevice::WriteCRU( ADDRESS address, int val )
{
	FUNCTION_ENTRY( this, "cUCSDDevice::WriteCRU", true );

	// 1F00 - on/off
	// 1F80 - bank swap
	// 1F86 - LED on/off

	switch( address << 1 )
	{
		case 0x00 :
			EnableDevice( val );
			break;
		case 0x80 :
			m_BankSwapped = val;
			// Make the swap
			cpuMemory.SetMemory( 0x5000, ROM_BANK_SIZE, m_pROM->GetCpuMemory( 5 )->Bank[ m_BankSwapped ].Data, true );
			break;
		default :
			DBG_ERROR( "PC: " << hex << m_pCPU->GetPC( ) << " Unexpected Address - " << hex << address );
			break;
	}
}

int cUCSDDevice::ReadCRU( ADDRESS address )
{
	FUNCTION_ENTRY( this, "cUCSDDevice::ReadCRU", true );

	int retVal = 1;

	switch( address << 1 )
	{
		case 0x80 :
			retVal = m_BankSwapped;
			break;
		default :
			DBG_ERROR( "PC: " << hex << m_pCPU->GetPC( ) << "Unexpected Address - " << hex << address );
			break;
	}

	return retVal;
}

//----------------------------------------------------------------------------
// cDevice methods
//----------------------------------------------------------------------------

void cUCSDDevice::ActivateInternal( )
{
	FUNCTION_ENTRY( this, "cUCSDDevice::ActivateInternal", true );

	m_pCPU->SetTrap( 0x5BFC, ( UINT8 ) MEMFLG_TRAP_READ,  m_TrapIndex );
	m_pCPU->SetTrap( 0x5BFE, ( UINT8 ) MEMFLG_TRAP_READ,  m_TrapIndex );
	m_pCPU->SetTrap( 0x5FFC, ( UINT8 ) MEMFLG_TRAP_WRITE, m_TrapIndex );
	m_pCPU->SetTrap( 0x5FFE, ( UINT8 ) MEMFLG_TRAP_WRITE, m_TrapIndex );

	// Restore the proper bank of ROM
	cpuMemory.SetMemory( 0x5000, ROM_BANK_SIZE, m_pROM->GetCpuMemory( 5 )->Bank[ m_BankSwapped ].Data, true );
}

UINT8 cUCSDDevice::WriteMemory( ADDRESS address, UINT8 data )
{
	FUNCTION_ENTRY( this, "cUCSDDevice::WriteMemory", false );

	switch( address & 0x0002 )
	{
		case 0x0000 :					// GROM/GRAM Write Byte Port
			// GROM only - don't write any data - just update the address logic
			m_GromAddress = ( UINT16 ) (( m_GromAddress & 0xE000 ) | (( m_GromAddress + 1 ) & 0x1FFF ));
			m_GromWriteShift = 8;
			break;
		case 0x0002 :					// GROM/GRAM Write (set) Address Port
			m_GromAddress &= ( ADDRESS ) ( 0xFF00 >> m_GromWriteShift );
			m_GromAddress |= ( ADDRESS ) ( data << m_GromWriteShift );
			m_GromWriteShift = 8 - m_GromWriteShift;
			m_GromReadShift  = 8;
			break;
	}

	return data;
}

UINT8 cUCSDDevice::ReadMemory( ADDRESS address, UINT8 data )
{
	FUNCTION_ENTRY( this, "cUCSDDevice::ReadMemory", false );

	m_GromWriteShift = 8;

	switch( address & 0x0002 )
	{
		case 0x0000 :					// GROM/GRAM Read Byte Port
			data = m_GromMemory[ m_GromAddress >> 13 ].Bank[ 0 ].Data[ m_GromAddress & 0x1FFF ];
			m_GromAddress = ( UINT16 ) (( m_GromAddress & 0xE000 ) | (( m_GromAddress + 1 ) & 0x1FFF ));
			break;
		case 0x0002 :					// GROM/GRAM Read Address Port
			data = ( UINT8 ) ((( m_GromAddress + 1 ) >> m_GromReadShift ) & 0x00FF );
			m_GromReadShift  = 8 - m_GromReadShift;
			break;
	}

	return data;
}
