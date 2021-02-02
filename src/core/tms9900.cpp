//----------------------------------------------------------------------------
//
// File:        tms9900.cpp
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

#include <cstring>
#include "common.hpp"
#include "logger.hpp"
#include "tms9900.hpp"
#include "opcodes.hpp"
#include "memory.hpp"
#include "support.hpp"

DBG_REGISTER( __FILE__ );

void (*TimerHook)( );

static BREAKPOINT_FUNCTION DebugHandler;
static void               *DebugToken;

static UINT8 MemTrapIndex[ 0x10000 ];

sTrapInfo TrapList[ 16 ];

UINT8 CallTrapB( bool isRead, ADDRESS address, UINT8 value )
{
	FUNCTION_ENTRY( nullptr, "CallTrapB", false );

	if( MemFlags[ address ] & ( isRead ? MEMFLG_TRAP_READ : MEMFLG_TRAP_WRITE ))
	{
		int index = MemTrapIndex[ address ];
		sTrapInfo *pInfo = &TrapList[ index ];

		DBG_ASSERT( pInfo != nullptr );

		value = pInfo->function( pInfo->ptr, pInfo->data, isRead, address, value );
	}

	if( MemFlags[ address ] & ( isRead ? MEMFLG_READ : MEMFLG_WRITE ))
	{
		DBG_ASSERT( DebugHandler != nullptr );

		value = DebugHandler( DebugToken, address, false, value, isRead, false );
	}

	return value;
}

UINT16 CallTrapW( bool isRead, bool isFetch, ADDRESS address, UINT16 value )
{
	FUNCTION_ENTRY( nullptr, "CallTrapW", false );

	if( MemFlags[ address ] & ( isRead ? MEMFLG_TRAP_READ : MEMFLG_TRAP_WRITE ))
	{
		int index = MemTrapIndex[ address ];
		sTrapInfo *pInfo = &TrapList[ index ];

		DBG_ASSERT( pInfo != nullptr );

		UINT8 msb = pInfo->function( pInfo->ptr, pInfo->data, isRead, address, ( UINT8 ) ( value >> 8 ));
		UINT8 lsb = pInfo->function( pInfo->ptr, pInfo->data, isRead, address + 1, ( UINT8 ) value );

		value = ( msb << 8 ) | lsb;
	}

	if( MemFlags[ address ] & (( isRead ? MEMFLG_READ : MEMFLG_WRITE ) | ( isFetch ? MEMFLG_FETCH : 0 )))
	{
		DBG_ASSERT( DebugHandler != nullptr );

		value = DebugHandler( DebugToken, address, true, value, isRead, isFetch );
	}

	return value;
}

void InvalidOpcode( )
{
	FUNCTION_ENTRY( nullptr, "InvalidOpcode", true );

	DBG_ERROR( "PC = " << hex << ( UINT16 ) ProgramCounter << " OpCode: " << cpuMemory.ReadWord( ProgramCounter ));
}

cTMS9900::cTMS9900( ) :
	cBaseObject( "cTMS9900" ),
	cStateObject( )
{
	FUNCTION_ENTRY( this, "cTMS9900 ctor", true );

	InitOpCodeLookup( );

	memset( TrapList, 0, sizeof( TrapList ));

	memset( MemFlags, MEMFLG_8BIT, sizeof( MemFlags ));

	// Mark off the memory regions that are 16-bit (for access cycle counting)
	for( unsigned i = 0x0000; i < 0x2000; i++ )
	{
		MemFlags[ i ] &= ~MEMFLG_8BIT;
	}
	for( unsigned i = 0x8000; i < 0x8400; i++ )
	{
		MemFlags[ i ] &= ~MEMFLG_8BIT;
	}

	Reset( );
}

cTMS9900::~cTMS9900( )
{
	FUNCTION_ENTRY( this, "cTMS9900 dtor", true );
}

//----------------------------------------------------------------------------
// iBaseObject Methods
//----------------------------------------------------------------------------

const void *cTMS9900::GetInterface( const std::string &iName ) const
{
	FUNCTION_ENTRY( this, "cTMS9900::GetInterface", false );

	if( iName == "iTMS9900" )
	{
		return static_cast<const iTMS9900 *>( this );
	}

	return cStateObject::GetInterface( iName );
}

void cTMS9900::Reset( )
{
	SetPC( 0x0000 );
	SetWP( 0x0000 );
	SetST( 0x0000 );

	// Simulate a hardware powerup
	ContextSwitch( 0 );
}

void cTMS9900::SignalInterrupt( UINT8 level )
{
	InterruptFlag |= 1 << level;
}

void cTMS9900::ClearInterrupt( UINT8 level )
{
	InterruptFlag &= ~( 1 << level );
}

void cTMS9900::SetPC( ADDRESS address )
{
	ProgramCounter = address;
}

void cTMS9900::SetWP( ADDRESS address )
{
	WorkspacePtr = address;
}

void cTMS9900::SetST( UINT16 value )
{
	Status = value;
}

ADDRESS cTMS9900::GetPC( )
{
	return ProgramCounter;
}

ADDRESS cTMS9900::GetWP( )
{
	return WorkspacePtr;
}

UINT16 cTMS9900::GetST( )
{
	return Status;
}

void cTMS9900::Run( )
{
	::Run( );
}

void cTMS9900::Stop( )
{
	::Stop( );
}

bool cTMS9900::Step( )
{
	return ::Step( );
}

bool cTMS9900::IsRunning( )
{
	return ::IsRunning( );
}

UINT32 cTMS9900::GetClocks( )
{
	return ClockCycleCounter;
}

void cTMS9900::AddClocks( int clocks )
{
	ClockCycleCounter += clocks;
}

void cTMS9900::ResetClocks( )
{
	ClockCycleCounter = 0;
}

UINT32 cTMS9900::GetCounter( )
{
	return InstructionCounter;
}

void cTMS9900::ResetCounter( )
{
	InstructionCounter = 0;
}

//----------------------------------------------------------------------------
// iStateObject Methods
//----------------------------------------------------------------------------

std::string cTMS9900::GetIdentifier( )
{
	FUNCTION_ENTRY( this, "cTMS9900::GetIdentifier", true );

	return "TMS9900";
}

bool cTMS9900::ParseState( const sStateSection &state )
{
	FUNCTION_ENTRY( this, "cTMS9900::ParseState", true );

	state.load( "WP", WorkspacePtr, SaveFormat::HEXADECIMAL );
	state.load( "PC", ProgramCounter, SaveFormat::HEXADECIMAL );
	state.load( "ST", Status, SaveFormat::HEXADECIMAL );
	state.load( "InterruptFlag", InterruptFlag, SaveFormat::HEXADECIMAL );
	state.load( "InstructionCounter", InstructionCounter, SaveFormat::DECIMAL );
	state.load( "ClockCycleCounter", ClockCycleCounter, SaveFormat::DECIMAL );

	return true;
}

std::optional<sStateSection> cTMS9900::SaveState( )
{
	FUNCTION_ENTRY( this, "cTMS9900::SaveState", true );

	sStateSection save;

	save.name = "TMS9900";

	save.store( "WP", WorkspacePtr, SaveFormat::HEXADECIMAL );
	save.store( "PC", ProgramCounter, SaveFormat::HEXADECIMAL );
	save.store( "ST", Status, SaveFormat::HEXADECIMAL );
	save.store( "InterruptFlag", InterruptFlag, SaveFormat::HEXADECIMAL );
	save.store( "InstructionCounter", InstructionCounter, SaveFormat::DECIMAL );
	save.store( "ClockCycleCounter", ClockCycleCounter, SaveFormat::DECIMAL );

	return save;
}

UINT8 cTMS9900::RegisterTrapHandler( TRAP_FUNCTION function, void *ptr, int data )
{
	FUNCTION_ENTRY( this, "cTMS9900::RegisterTrapHandler", true );

	for( UINT8 i = 1; i < ( int ) SIZE( TrapList ); i++ )
	{
		if( TrapList[ i ].ptr == nullptr )
		{
			TrapList[ i ].ptr      = ptr;
			TrapList[ i ].data     = data;
			TrapList[ i ].function = function;
			return i;
		}
	}

	return ( UINT8 ) -1;
}

void cTMS9900::DeRegisterTrapHandler( UINT8 index )
{
	FUNCTION_ENTRY( this, "cTMS9900::DeRegisterTrapHandler", true );

	if(( index == 0 ) || ( index >= SIZE( TrapList )))
	{
		return;
	}

	ClearTrap( index, 0x0000, 0x10000 );

	TrapList[ index ].ptr      = nullptr;
	TrapList[ index ].data     = 0;
	TrapList[ index ].function = nullptr;
}

UINT8 cTMS9900::GetTrapIndex( TRAP_FUNCTION function, int data )
{
	FUNCTION_ENTRY( this, "cTMS9900::GetTrapIndex", true );

	for( UINT8 i = 1; i < SIZE( TrapList ); i++ )
	{
		if(( TrapList[ i ].function == function ) && ( TrapList[ i ].data == data ))
		{
			return i;
		}
	}

	return ( UINT8 ) -1;
}

bool cTMS9900::SetTrap( ADDRESS address, UINT8 type, UINT8 index )
{
	FUNCTION_ENTRY( this, "cTMS9900::SetTrap", false );

	if(( index == 0 ) || ( index >= SIZE( TrapList )))
	{
		return false;
	}
	if(( type == 0 ) || ( MemFlags[ address ] & MEMFLG_TRAP_ACCESS ))
	{
		return false;
	}

	MemFlags[ address ] |= type;
	MemTrapIndex[ address ] = index;

	return true;
}

void cTMS9900::ClearTrap( UINT8 index, ADDRESS offset, int length )
{
	FUNCTION_ENTRY( this, "cTMS9900::ClearTrap", true );

	if(( index == 0 ) || ( index >= SIZE( TrapList )))
	{
		return;
	}

	for( int i = 0; i < length; i++, offset++ )
	{
		if( MemTrapIndex[ offset ] == index )
		{
			MemFlags[ offset ] &= ( UINT8 ) ~MEMFLG_TRAP_ACCESS;
		}
	}
}

void cTMS9900::RegisterDebugHandler( BREAKPOINT_FUNCTION handler, void *token )
{
	FUNCTION_ENTRY( this, "cTMS9900::RegisterDebugHandler", true );

	DebugHandler = handler;
	DebugToken   = token;
}

void cTMS9900::DeRegisterDebugHandler( )
{
	FUNCTION_ENTRY( this, "cTMS9900::DeRegisterDebugHandler", true );

	DebugHandler = nullptr;
	DebugToken   = nullptr;

	for( int i = 0; i < 0x10000; i++ )
	{
		MemFlags[ i ] &= ( UINT8 ) ~MEMFLG_DEBUG;
	}
}

bool cTMS9900::SetBreakpoint( ADDRESS address, UINT8 flags )
{
	FUNCTION_ENTRY( this, "cTMS9900::SetBreakpoint", true );

	if(( flags & MEMFLG_DEBUG ) != flags )
	{
		return false;
	}

	MemFlags[ address ] |= flags;

	return true;
}

bool cTMS9900::ClearBreakpoint( ADDRESS address, UINT8 flags )
{
	FUNCTION_ENTRY( this, "cTMS9900::ClearBreakpoint", true );

	if(( flags & MEMFLG_DEBUG ) != flags )
	{
		return false;
	}

	if(( MemFlags[ address ] & flags ) != flags )
	{
		return false;
	}

	MemFlags[ address ] &= ( UINT8 ) ~flags;

	return true;
}
