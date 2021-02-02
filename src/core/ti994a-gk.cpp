//----------------------------------------------------------------------------
//
// File:        ti994a-gk.cpp
// Date:        28-Mar-2016
// Programmer:  Marc Rousseau
//
// Description:
//
// Copyright (c) 2016 Marc Rousseau, All Rights Reserved.
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
#include "cartridge.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "ti994a-gk.hpp"
#include "support.hpp"

DBG_REGISTER( __FILE__ );

extern int verbose;

const int INFO_MASK_GRAM0  = 0x00010000;
const int INFO_MASK_GRAM1  = 0x00020000;
const int INFO_MASK_GRAM2  = 0x00040000;
const int INFO_MASK_GRAM12 = 0x00060000;

cTI994AGK::cTI994AGK( iCartridge *ctg, iTMS9918A *vdp, iTMS9919 *sound, iTMS5220 *speech ) :
	cBaseObject( "cTI994AGK" ),
	cTI994A( ctg, vdp, sound, speech ),
	m_GK_Cartridge( nullptr ),
	m_GK_WriteProtect( WRITE_PROTECT::BANK1 ),
	m_GK_Enabled( false ),
	m_GK_OpSys( true ),
	m_GK_BASIC( true ),
	m_GK_LoaderOn( true )
{
	std::string romFile = LocateCartridge( "console", "Gram Kracker.ctg", { "a3bd5257c63e190800921b52dbe3ffa91ad91113" } );

	cRefPtr<cCartridge> pGK = new cCartridge( romFile );

	if( pGK->IsValid( ) == true )
	{
		if( verbose >= 2 )
		{
			fprintf( stdout, "Using Gram Kracker ROM \"%s\" - \"%s\"\n", pGK->GetFileName( ), pGK->GetTitle( ));
		}

		m_GK_Cartridge = pGK;

		// Write protect the GK's RAM
		SetWriteProtect( WRITE_PROTECT::ENABLED );
		SetEnabled( true );
		SetGRAM0( false );
		SetGRAM12( false );
		SetLoader( false );
	}

	if( verbose >= 1 )
	{
		fprintf( stdout, "Gram Kracker functions %s\n", ( m_GK_Cartridge != nullptr ) ? "enabled" : "disabled" );
	}
}

cTI994AGK::~cTI994AGK( )
{
	if( m_GK_Cartridge != nullptr )
	{
		RemoveCartridge( m_GK_Cartridge, -1 );
	}
}

//----------------------------------------------------------------------------
// iComputer methods
//----------------------------------------------------------------------------

bool cTI994AGK::InsertCartridge( iCartridge *pCartridge )
{
	FUNCTION_ENTRY( this, "cTI994AGK::InsertCartridge", true );

	if(( m_GK_Cartridge != nullptr ) && ( m_Cartridge == nullptr ) && ( m_GK_Enabled == true ))
	{
		RemoveCartridge( m_GK_Cartridge, INFO_MASK_CARTRIDGE );
	}

	return cTI994A::InsertCartridge( pCartridge );
}

void cTI994AGK::RemoveCartridge( )
{
	FUNCTION_ENTRY( this, "cTI994AGK::RemoveCartridge", true );

	if(( m_GK_Cartridge != nullptr ) && ( m_Cartridge != nullptr ) && ( m_GK_Enabled == true ))
	{
		RemoveCartridge( m_Cartridge, INFO_MASK_CARTRIDGE );

		AddCartridge( m_GK_Cartridge, INFO_MASK_CARTRIDGE );

		m_Cartridge = nullptr;

		Reset( );

		return;
	}

	cTI994A::RemoveCartridge( );
}

//----------------------------------------------------------------------------
// cTI994A methods
//----------------------------------------------------------------------------

std::optional<sStateSection> cTI994AGK::SaveState( )
{
	FUNCTION_ENTRY( this, "cTI994AGK::SaveState", true );

	auto save = cTI994A::SaveState( );

	if( save )
	{
		save->store( "GK.ROM", m_GK_Cartridge->GetDescriptor( ));
		save->addSubSection( m_GK_Cartridge );

		save->store( "GK.WriteProtect", static_cast<int>( m_GK_WriteProtect ), SaveFormat::DECIMAL );
		save->store( "GK.Enabled", m_GK_Enabled );
		save->store( "GK.OpSys", m_GK_OpSys );
		save->store( "GK.BASIC", m_GK_BASIC );
		save->store( "GK.LoaderOn", m_GK_LoaderOn );
	}

	return save;
}

bool cTI994AGK::ParseState( const sStateSection &state )
{
	FUNCTION_ENTRY( this, "cTI994AGK::ParseState", true );

	if( state.hasValue( "GK.ROM" ))
	{
		m_GK_Cartridge = cCartridge::LoadCartridge( state.getValue( "GK.ROM" ), "console" );
		state.loadSubSection( m_GK_Cartridge );

		state.load( "GK.WriteProtect", reinterpret_cast<int&>( m_GK_WriteProtect ), SaveFormat::DECIMAL );
		state.load( "GK.Enabled", m_GK_Enabled );
		state.load( "GK.OpSys", m_GK_OpSys );
		state.load( "GK.BASIC", m_GK_BASIC );
		state.load( "GK.LoaderOn", m_GK_LoaderOn );
	}

	if( cTI994A::ParseState( state ))
	{
		SetWriteProtect( WRITE_PROTECT::ENABLED );
		SetEnabled( m_GK_Enabled );
		SetGRAM0( !m_GK_OpSys );
		SetGRAM12( !m_GK_BASIC );
		SetLoader( m_GK_LoaderOn );

		return true;
	}

	return false;
}

//----------------------------------------------------------------------------
// Gram Kracker methods
//----------------------------------------------------------------------------

void cTI994AGK::GK_SetEnabled( bool state )
{
	FUNCTION_ENTRY( this, "cTI994AGK::GK_SetEnabled", true );

	if(( m_GK_Cartridge != nullptr ) && ( m_GK_Enabled != state ))
	{
		GK_SetEnabled( state );
	}
}

void cTI994AGK::GK_SetGRAM0( bool state )
{
	FUNCTION_ENTRY( this, "cTI994AGK::GK_SetGRAM0", true );

	if(( m_GK_Cartridge != nullptr ) && ( m_GK_OpSys == state ))
	{
		SetGRAM0( state );
	}
}

void cTI994AGK::GK_SetGRAM12( bool state )
{
	FUNCTION_ENTRY( this, "cTI994AGK::GK_SetGRAM12", true );

	if(( m_GK_Cartridge != nullptr ) && ( m_GK_BASIC == state ))
	{
		SetGRAM12( state );
	}
}

void cTI994AGK::GK_SetLoader( bool state )
{
	FUNCTION_ENTRY( this, "cTI994AGK::GK_ToggleLoader", true );

	if(( m_GK_Cartridge != nullptr ) && ( m_GK_LoaderOn != state ))
	{
		SetLoader( state );
	}
}

void cTI994AGK::GK_SetWriteProtect( WRITE_PROTECT state )
{
	FUNCTION_ENTRY( this, "cTI994AGK::GK_SetWriteProtect", true );

	if(( m_GK_Cartridge != nullptr ) && ( m_GK_WriteProtect != state ))
	{
		SetWriteProtect( state );
	}
}

//----------------------------------------------------------------------------
// Internal Gram Kracker methods
//----------------------------------------------------------------------------

void cTI994AGK::SetEnabled( bool state )
{
	FUNCTION_ENTRY( this, "cTI994AGK::SetEnabled", true );

	m_GK_Enabled = state;

	DBG_TRACE( "Gram Kracker: " << (( state == true ) ? "Enabled" : "Disabled" ));

	if( m_Cartridge == nullptr )
	{
		if( m_GK_Enabled == true )
		{
			AddCartridge( m_GK_Cartridge, INFO_MASK_CARTRIDGE );
		}
		else
		{
			RemoveCartridge( m_GK_Cartridge, INFO_MASK_CARTRIDGE );
		}
	}
}

void cTI994AGK::SetGRAM0( bool state )
{
	FUNCTION_ENTRY( this, "cTI994AGK::SetGRAM0", true );

	m_GK_OpSys = !state;

	DBG_TRACE( "Grak Kracker: Enabling " << (( state == true ) ? "GROM 0" : "Operating System" ));

	if( m_GK_OpSys == false )
	{
		AddCartridge( m_GK_Cartridge, INFO_MASK_GRAM0 );
	}
	else
	{
		RemoveCartridge( m_GK_Cartridge, INFO_MASK_GRAM0 );
	}
}

void cTI994AGK::SetGRAM12( bool state )
{
	FUNCTION_ENTRY( this, "cTI994AGK::SetGRAM12", true );

	m_GK_BASIC = !state;

	DBG_TRACE( "Gram Kracker: Enabling " << (( state == true ) ? "GROMS 1 & 2" : "TI BASIC" ));

	int mask = ( m_GK_LoaderOn == true ) ? INFO_MASK_GRAM2 : INFO_MASK_GRAM12;

	if( m_GK_BASIC == true )
	{
		RemoveCartridge( m_GK_Cartridge, mask );
	}
	else
	{
		AddCartridge( m_GK_Cartridge, mask );
	}
}

void cTI994AGK::SetLoader( bool state )
{
	FUNCTION_ENTRY( this, "cTI994AGK::ToggleLoader", true );

	m_GK_LoaderOn = state;

	DBG_TRACE( "Gram Kracker: Turning loader " << (( state == true ) ? "On" : "Off" ));

	// Switch CurBank to point to the correct bank
	m_GK_Cartridge->GetGromMemory( 1 )->CurBank = &m_GK_Cartridge->GetGromMemory( 1 )->Bank[ ( m_GK_LoaderOn == true ) ? 1 : 0 ];

	if( m_GK_BASIC == true )
	{
		if( m_GK_LoaderOn == true )
		{
			AddCartridge( m_GK_Cartridge, INFO_MASK_GRAM1 );
		}
		else
		{
			RemoveCartridge( m_GK_Cartridge, INFO_MASK_GRAM1 );
		}
		return;
	}

	UpdateMemory( INFO_MASK_GRAM1 );
}

void cTI994AGK::SetWriteProtect( WRITE_PROTECT state )
{
	FUNCTION_ENTRY( this, "cTI994AGK::SetWriteProtect", true );

	m_GK_WriteProtect = state;

	auto MarkRegion = []( sMemoryBank *bank, bool isReadOnly )
	{
		bank->Flags = isReadOnly ? ( bank->Flags | FLAG_READ_ONLY ) : ( bank->Flags & ~FLAG_READ_ONLY );
	};

	int bank = -1;

	switch( m_GK_WriteProtect )
	{
		case WRITE_PROTECT::BANK1 :
			bank = 0;
			DBG_TRACE( "Gram Kracker: BANK 1 Selected" );
			break;
		case WRITE_PROTECT::BANK2 :
			bank = 1;
			DBG_TRACE( "Gram Kracker: BANK 2 Selected" );
			break;
		case WRITE_PROTECT::ENABLED :
			DBG_TRACE( "Gram Kracker: Write-Protect Enabled" );
			break;
	}

	int mask = 0;

	bool isReadOnly = ( m_GK_WriteProtect == WRITE_PROTECT::ENABLED ) ? true : false;

	// Mark the ROM/GROM banks as RAM/ROM
	for( auto i : { 6, 7 } )
	{
		sMemoryRegion *region = m_GK_Cartridge->GetCpuMemory( i );
		region->CurBank = ( bank > 0 ) ? region->Bank + bank : region->CurBank;
		MarkRegion( region->Bank + 0, isReadOnly );
		MarkRegion( region->Bank + 1, isReadOnly );
		if( m_CpuMemoryInfo[ i ].back( ) == region )
		{
			mask |= ( 0x00001 << i );
		}
	}

	for( auto i : { 0, 3, 4, 5, 6, 7 } )
	{
		sMemoryRegion *region = m_GK_Cartridge->GetGromMemory( i );
		MarkRegion( region->Bank + 0, isReadOnly );
		if( m_GromMemoryInfo[ i ].back( ) == region )
		{
			mask |= ( 0x10000 << i );
		}
	}

	UpdateMemory( mask );
}
