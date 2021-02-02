//----------------------------------------------------------------------------
//
// File:        device.cpp
// Date:        27-Mar-1998
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

#include <cstdio>
#include "common.hpp"
#include "logger.hpp"
#include "cartridge.hpp"
#include "tms9900.hpp"
#include "device.hpp"
#include "icomputer.hpp"

DBG_REGISTER( __FILE__ );

cDevice::cDevice( iCartridge *rom ) :
	cBaseObject( "cDevice" ),
	m_pROM( rom ),
	m_pComputer( nullptr ),
	m_pCPU( nullptr ),
	m_CRU( rom ? rom->GetCRU( ) : 0 ),
	m_IsValid( true ),
	m_IsActive( false ),
	m_IsRecognized( true ),
	m_TrapIndex( (UINT8) -1 ),
	m_mapMemRead( ),
	m_mapMemWrite( ),
	m_mapIORead( ),
	m_mapIOWrite( )
{
	FUNCTION_ENTRY( this, "cDevice ctor", true );

	if( m_pROM && ( !m_pROM->IsValid( ) ||
		( m_pROM->GetCpuMemory( 4 )->Bank[ 0 ].Data == nullptr ) ||
		( m_pROM->GetCpuMemory( 5 )->Bank[ 0 ].Data == nullptr )))
	{
		DBG_ERROR( "Cartridge does not appear to be a valid device" );
		m_IsValid = false;
	}
}

cDevice::~cDevice( )
{
	FUNCTION_ENTRY( this, "cDevice dtor", true );

	if( m_IsRecognized == false )
	{
		DBG_WARNING( "Unrecognized access to device at CRU >" << hex << m_CRU );

		DumpDataPool( m_mapMemRead,  "Memory Reads" );
		DumpDataPool( m_mapMemWrite, "Memory Wrtes" );
		DumpDataPool( m_mapIORead,   "CRU Reads" );
		DumpDataPool( m_mapIOWrite,  "CRU Writes" );
	}
}

//----------------------------------------------------------------------------
// iBaseObject Methods
//----------------------------------------------------------------------------

const void *cDevice::GetInterface( const std::string &iName ) const
{
	FUNCTION_ENTRY( this, "cDevice::GetInterface", false );

	if( iName == "iDevice" )
	{
		return static_cast<const iDevice *>( this );
	}

	return cBaseObject::GetInterface( iName );
}

//----------------------------------------------------------------------------
// iDevice Methods
//----------------------------------------------------------------------------

bool cDevice::Initialize( iComputer *computer )
{
	FUNCTION_ENTRY( this, "cDevice::Initialize", true );

	m_pComputer = computer;
	m_pCPU = computer ? computer->GetCPU( ) : nullptr;
	m_IsActive = false;

	return true;
}

void cDevice::WriteCRU( ADDRESS address, int value )
{
	FUNCTION_ENTRY( this, "cDevice::WriteCRU", false );

	m_IsRecognized = false;

	DBG_EVENT ( "PC: " << hex << m_pCPU->GetPC( ) << "   I/O Wr: " << hex << ( UINT8 ) address << " => " << dec << value );

	m_mapIOWrite[ address ].insert( m_pCPU->GetPC( ));
}

int cDevice::ReadCRU( ADDRESS address )
{
	FUNCTION_ENTRY( this, "cDevice::ReadCRU", false );

	m_IsRecognized = false;

	int retVal = 0;

	DBG_EVENT ( "PC: " << hex << m_pCPU->GetPC( ) << "   I/O Rd: " << hex << ( UINT8 ) address << " => " << dec << retVal );

	m_mapIORead[ address ].insert( m_pCPU->GetPC( ));

	return retVal;
}

void cDevice::DumpDataPool( const DataPool &pool, const std::string &header )
{
	FUNCTION_ENTRY( nullptr, "cDevice::DumpDataPool", true );

	if( ! pool.empty( ))
	{
		DBG_WARNING( "  " << header.c_str( ) << ":" );

		for( auto &item : pool )
		{
			char buffer[ 4096 ], *ptr = buffer;
			ptr += sprintf( ptr, "    >%04X:", item.first );
			for( auto &address : item.second )
			{
				ptr += sprintf( ptr, " %04X", address );
			}
			DBG_WARNING( buffer );
		}
	}
}

UINT8 cDevice::TrapFunction( void *ptr, int, bool read, ADDRESS address, UINT8 value )
{
	FUNCTION_ENTRY( ptr, "cDevice::TrapFunction", false );

	cDevice *device = static_cast<cDevice *>( ptr );

	if( read == true )
	{
		value = device->ReadMemory( address, value );
	}
	else
	{
		value = device->WriteMemory( address, value );
	}

	return value;
}

bool cDevice::RegisterTrapHandler( )
{
	FUNCTION_ENTRY( this, "cDevice::RegisterTrapHandler", true );

	// Make sure we have a valid DSR before going any further
	if( m_IsValid == false )
	{
		return false;
	}

	if( m_TrapIndex == ( UINT8 ) -1 )
	{
		m_TrapIndex = m_pCPU->RegisterTrapHandler( TrapFunction, this, 0 );

		return true;
	}

	return false;
}

void cDevice::DeRegisterTrapHandler( )
{
	FUNCTION_ENTRY( this, "cDevice::DeRegisterTrapHandler", true );

	if( m_TrapIndex != ( UINT8 ) -1 )
	{
		m_pCPU->DeRegisterTrapHandler( m_TrapIndex );

		m_TrapIndex = ( UINT8 ) -1;
	}
}

void cDevice::EnableDevice( bool enable )
{
	FUNCTION_ENTRY( this, "cDevice::EnableDevice", true );

	if( enable )
	{
		m_pComputer->EnableDevice( this );
		Activate( );
	}
	else
	{
		DeActivate( );
		m_pComputer->DisableDevice( this );
	}
}

void cDevice::Activate( )
{
	FUNCTION_ENTRY( this, "cDevice::Activate", true );

	if( RegisterTrapHandler( ))
	{
		m_IsActive = true;

		ActivateInternal( );
	}
}

void cDevice::DeActivate( )
{
	FUNCTION_ENTRY( this, "cDevice::DeActivate", true );

	if( m_IsActive )
	{
		DeRegisterTrapHandler( );

		DeActivateInternal( );

		m_IsActive = false;
	}
}

void cDevice::ActivateInternal( )
{
	FUNCTION_ENTRY( this, "cDevice::ActivateInternal", true );

	for( int i = 0x4000; i < 0x6000; i += 1 )
	{
		m_pCPU->SetTrap( i, MEMFLG_TRAP_WRITE, m_TrapIndex );
	}
}

void cDevice::DeActivateInternal( )
{
	FUNCTION_ENTRY( this, "cDevice::DeActivateInternal", true );
}

UINT8 cDevice::WriteMemory( ADDRESS address, UINT8 value )
{
	FUNCTION_ENTRY( this, "cDevice::WriteMemory", true );

	m_IsRecognized = false;

	DBG_EVENT ( "PC: " << hex << m_pCPU->GetPC( ) << "   Mem Wr: " << hex << address << " => " << value );

	m_mapMemRead[ address ].insert( m_pCPU->GetPC( ));

	return value;
}

UINT8 cDevice::ReadMemory( ADDRESS address, UINT8 )
{
	FUNCTION_ENTRY( this, "cDevice::ReadMemory", true );

	m_IsRecognized = false;

	UINT8 retVal = 0;

	DBG_EVENT ( "PC: " << hex << m_pCPU->GetPC( ) << "   Mem Rd: " << hex << address << " => " << retVal );

	m_mapMemRead[ address ].insert( m_pCPU->GetPC( ));

	return retVal;
}
