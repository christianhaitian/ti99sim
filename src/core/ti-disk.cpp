//----------------------------------------------------------------------------
//
// File:        ti-disk.cpp
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
#include <cstdlib>
#include <cstring>
#include "common.hpp"
#include "logger.hpp"
#include "cartridge.hpp"
#include "tms9900.hpp"
#include "device.hpp"
#include "ti-disk.hpp"
#include "support.hpp"

DBG_REGISTER( __FILE__ );

extern int verbose;

std::string cDiskDevice::DiskImage[ 3 ] =
{
	"dsk1.dsk",
	"dsk2.dsk",
	"dsk3.dsk"
};

cDiskDevice::cDiskDevice( iCartridge *rom ) :
	cBaseObject( "cDiskDevice" ),
	cStateObject( ),
	cDevice( rom ),
	m_StepDirection( 0 ),
	m_ClocksPerRev( 600000 ),
	m_ClockStart( 0 ),
	m_HardwareBits( 0 ),
	m_DriveSelect( 0 ),
	m_HeadSelect( 0 ),
	m_TrackSelect( 0 ),
	m_IsFD1771( true ),
	m_TransferEnabled( false ),
	m_CurDisk( nullptr ),
	m_CurTrack( nullptr ),
	m_CurSector( nullptr ),
	m_DataMark( 0 ),
	m_StatusRegister( 0 ),
	m_TrackRegister( 0 ),
	m_SectorRegister( 0 ),
	m_LastData( 0x00 ),
	m_BytesExpected( 0 ),
	m_BytesLeft( 0 ),
	m_ReadDataPtr( nullptr ),
	m_CmdInProgress( CMD_NONE )
{
	FUNCTION_ENTRY( this, "cDiskDevice::cDiskDevice", true );

	for( auto &diskMedia : m_DiskMedia )
	{
		diskMedia = new cDiskMedia( nullptr );
	}

	if( m_IsValid && ( m_CRU + 1 == 0 ))
	{
		DBG_ERROR( "Cartridge does not appear to be a valid disk device" );
		m_IsValid = false;
	}

	m_DataBuffer.reserve( MAX_TRACK_SIZE );
}

cDiskDevice::~cDiskDevice( )
{
	FUNCTION_ENTRY( this, "cDiskDevice::~cDiskDevice", true );

	for( size_t i = 0; i < SIZE( m_DiskMedia ); i++ )
	{
		FlushDisk( m_DiskMedia[ i ]);
	}
}

//----------------------------------------------------------------------------
// iBaseObject Methods
//----------------------------------------------------------------------------

const void *cDiskDevice::GetInterface( const std::string &iName ) const
{
	FUNCTION_ENTRY( this, "cDiskDevice::GetInterface", false );

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

std::string cDiskDevice::GetIdentifier( )
{
	FUNCTION_ENTRY( this, "cDiskDevice::GetIdentifier", true );

	return "PHP1240";
}

bool cDiskDevice::ParseState( const sStateSection &state )
{
	FUNCTION_ENTRY( this, "cDiskDevice::ParseState", true );

	m_pROM = cCartridge::LoadCartridge( state.getValue( "ROM" ), "console" );
	state.loadSubSection( m_pROM );

	state.load( "CRU", m_CRU, SaveFormat::HEXADECIMAL );
	state.load( "IsActive", m_IsActive );

	state.load( "StepDirection", m_StepDirection, SaveFormat::DECIMAL );
	state.load( "ClocksPerRev", m_ClocksPerRev, SaveFormat::DECIMAL );
	state.load( "ClockStart", m_ClockStart, SaveFormat::DECIMAL );

	state.load( "HardwareBits", m_HardwareBits, SaveFormat::DECIMAL );
	state.load( "DriveSelect", m_DriveSelect, SaveFormat::DECIMAL );
	state.load( "HeadSelect", m_HeadSelect, SaveFormat::DECIMAL );
	state.load( "TrackSelect", m_TrackSelect, SaveFormat::DECIMAL );

	state.load( "IsFD1771", m_IsFD1771 );
	state.load( "TransferEnabled", m_TransferEnabled );

	state.load( "DataMark", m_DataMark, SaveFormat::HEXADECIMAL );

	state.load( "StatusRegister", m_StatusRegister, SaveFormat::HEXADECIMAL );
	state.load( "TrackRegister", m_TrackRegister, SaveFormat::HEXADECIMAL );
	state.load( "SectorRegister", m_SectorRegister, SaveFormat::HEXADECIMAL );

	for( size_t i = 0; i < SIZE( m_DiskMedia ); i++ )
	{
		auto disk = std::string( "DSK" ) + "123"[ i ];
		if( state.hasValue( disk ))
		{
			std::string filename;
			state.load( disk, filename );
			m_DiskMedia[ i ]->LoadFile( filename.c_str( ), FORMAT_UNKNOWN );
		}
		else
		{
			m_DiskMedia[ i ]->ClearDisk( );
		}
	}

	state.load( "LastData", m_LastData, SaveFormat::HEXADECIMAL );
	state.load( "BytesExpected", m_BytesExpected, SaveFormat::DECIMAL );
	state.load( "BytesLeft", m_BytesLeft, SaveFormat::DECIMAL );

	if( state.hasValue( "Buffer" ))
	{
		state.load( "Buffer", m_DataBuffer.data( ), m_DataBuffer.size( ));
	}
	else
	{
		std::fill( std::begin( m_DataBuffer ), std::end( m_DataBuffer ), 0 );
	}

	state.load( "CmdInProgress", reinterpret_cast<int&>( m_CmdInProgress ), SaveFormat::DECIMAL );

	// Now update derived member variables

	switch( m_DriveSelect )
	{
		case 0x01 :
			m_CurDisk = m_DiskMedia[ 0 ];
			break;
		case 0x02 :
			m_CurDisk = m_DiskMedia[ 1 ];
			break;
		case 0x04 :
			m_CurDisk = m_DiskMedia[ 2 ];
			break;
		default :
			m_CurDisk = nullptr;
			break;
	}

	Restore( 0 );

	FindSector( );

	m_ReadDataPtr = state.hasValue( "ReadDataPtr" ) ? &m_DataBuffer[ m_BytesExpected - m_BytesLeft ] : nullptr;

	return true;
}

std::optional<sStateSection> cDiskDevice::SaveState( )
{
	FUNCTION_ENTRY( this, "cDiskDevice::SaveState", true );

	sStateSection save;

	save.name = "PHP1240";

	save.store( "ROM", m_pROM->GetDescriptor( ));
	save.addSubSection( m_pROM );

	save.store( "CRU", m_CRU, SaveFormat::HEXADECIMAL );
	save.store( "IsActive", m_IsActive );

	save.store( "StepDirection", m_StepDirection, SaveFormat::DECIMAL );
	save.store( "ClocksPerRev", m_ClocksPerRev, SaveFormat::DECIMAL );
	save.store( "ClockStart", m_ClockStart, SaveFormat::DECIMAL );

	save.store( "HardwareBits", m_HardwareBits, SaveFormat::DECIMAL );
	save.store( "DriveSelect", m_DriveSelect, SaveFormat::DECIMAL );
	save.store( "HeadSelect", m_HeadSelect, SaveFormat::DECIMAL );
	save.store( "TrackSelect", m_TrackSelect, SaveFormat::DECIMAL );

	save.store( "IsFD1771", m_IsFD1771 );
	save.store( "TransferEnabled", m_TransferEnabled );

	save.store( "DataMark", m_DataMark, SaveFormat::HEXADECIMAL );

	save.store( "StatusRegister", m_StatusRegister, SaveFormat::HEXADECIMAL );
	save.store( "TrackRegister", m_TrackRegister, SaveFormat::HEXADECIMAL );
	save.store( "SectorRegister", m_SectorRegister, SaveFormat::HEXADECIMAL );

	for( size_t i = 0; i < SIZE( m_DiskMedia ); i++ )
	{
		if( auto name = m_DiskMedia[ i ]->GetName( ))
		{
			save.store( std::string( "DSK" ) + "123"[ i ], std::string( name ));
			FlushDisk( m_DiskMedia[ i ]);
		}
	}

	save.store( "LastData", m_LastData, SaveFormat::HEXADECIMAL );
	save.store( "BytesExpected", m_BytesExpected, SaveFormat::DECIMAL );
	save.store( "BytesLeft", m_BytesLeft, SaveFormat::DECIMAL );

	if( m_ReadDataPtr != nullptr )
	{
		save.store( "ReadDataPtr", true );
	}

	if( !m_DataBuffer.empty( ))
	{
		save.store( "Buffer", m_DataBuffer.data( ), m_DataBuffer.size( ));
	}

	save.store( "CmdInProgress", static_cast<int>( m_CmdInProgress ), SaveFormat::DECIMAL );

	return save;
}

//----------------------------------------------------------------------------
// iDevice methods
//----------------------------------------------------------------------------

const char *cDiskDevice::GetName( )
{
	return "TI-Disk Controller";
}

bool cDiskDevice::Initialize( iComputer *computer )
{
	FUNCTION_ENTRY( this, "cDiskDevice::Initialize", true );

	// Look for DSKn images
	for( unsigned i = 0; i < SIZE( DiskImage ); i++ )
	{
		auto &imageName = DiskImage[ i ];

		auto validName = LocateFile( "disks", imageName );
		if( validName.empty( ))
		{
			validName = imageName;
			if( !validName.is_absolute( ))
			{
				validName = GetHomePath( "disks" ) / imageName;
			}
		}
		LoadDisk( i, validName.c_str( ));
	}

	return cDevice::Initialize( computer );
}

void cDiskDevice::WriteCRU( ADDRESS address, int val )
{
	FUNCTION_ENTRY( this, "cDiskDevice::WriteCRU", true );

	int mask = 1 << address;
	if( val != 0 )
	{
		m_HardwareBits |= mask;
	}
	else
	{
		m_HardwareBits &= ~mask;
	}

	switch( address )
	{
		case 0 :
			EnableDevice( val );
			break;
		case 1 :
			// Don't care - triggers a 4.23 second pulse to the selected drive's motor
			break;
		case 2 :
			m_TransferEnabled = ( val != 0 ) ? true : false;
			DBG_EVENT( "Transfer " << (( val != 0 ) ? "enabled" : "disabled" ));
			return;
		case 3 :
			// Don't care - head load
			break;
		case 4 :                                        // Bits 4,5,6
		case 5 :
			return;
		case 6 :
			m_DriveSelect = ( m_HardwareBits >> 4 ) & 0x07;
			int drive;
			switch( m_DriveSelect )
			{
				case 0x01 :
					m_CurDisk = m_DiskMedia[ drive=0 ];
					break;
				case 0x02 :
					m_CurDisk = m_DiskMedia[ drive=1 ];
					break;
				case 0x04 :
					m_CurDisk = m_DiskMedia[ drive=2 ];
					break;
				default :
					m_CurDisk = nullptr;
					drive = -1;
					break;
			}
			m_CurTrack = m_CurDisk ? m_CurDisk->GetTrack( m_TrackSelect, m_HeadSelect ) : nullptr;
			m_CurSector = nullptr;
			DBG_EVENT( "PC: " << hex << m_pCPU->GetPC( ) << " Drive " << dec << drive << " selected" );
			return;
		case 7 :
			m_HeadSelect = ( UINT8 ) val;
			m_CurTrack = m_CurDisk ? m_CurDisk->GetTrack( m_TrackSelect, m_HeadSelect ) : nullptr;
			m_CurSector = nullptr;
			DBG_EVENT( "PC: " << hex << m_pCPU->GetPC( ) << " Head " << dec << val << " selected" );
			return;
		case 11 :
// 			return;
		default :
			DBG_ERROR( "PC: " << hex << m_pCPU->GetPC( ) << " Unexpected Address - " << hex << address );
			break;
	}

////	DBG_EVENT ( "PC: " << hex << m_pCPU->GetPC () << "   I/O Wr: " << hex << ( UINT8 ) address << " => " << dec << val );
}

int cDiskDevice::ReadCRU( ADDRESS address )
{
	FUNCTION_ENTRY( this, "cDiskDevice::ReadCRU", true );

	int retVal = 1;
	switch( address )
	{
		case 7 :
			// Matches output pin 7
			retVal = m_HeadSelect;
			break;
		case 6 :
			// Tied to +5V
			break;
		case 5 :
			// Ground
			retVal = 0;
			break;
		case 3 :
			retVal = ( m_HardwareBits & 0x40 ) ? 1 : 0;
			break;
		case 2 :
			retVal = ( m_HardwareBits & 0x20 ) ? 1 : 0;
			break;
		case 1 :
			retVal = ( m_HardwareBits & 0x10 ) ? 1 : 0;
			break;
		default :
			DBG_ERROR( "Unexpected Address - " << address );
			break;
	}

////	DBG_EVENT ( "PC: " << hex << m_pCPU->GetPC () << "   I/O Rd: " << hex << ( UINT8 ) address << " => " << dec << retVal );

	return retVal;
}

void cDiskDevice::LoadDisk( int index, const char *filename )
{
	FUNCTION_ENTRY( this, "cDiskDevice::LoadDisk", true );

	DBG_EVENT( "Loading file: " << filename );

	if( m_DiskMedia[ index ]->LoadFile( filename, FORMAT_UNKNOWN ) == true )
	{
		DBG_EVENT( "Disk image loaded successfully" );

		if( verbose >= 2 )
		{
			fprintf( stdout, "Loaded file '%s' as DSK%d\n", filename, index + 1 );
		}

		return;
	}

	if( verbose >= 2 )
	{
		fprintf( stdout, "Failed to load file '%s' as DSK%d\n", filename, index + 1 );
	}
}

void cDiskDevice::UnLoadDisk( int index )
{
	FUNCTION_ENTRY( this, "cDiskDevice::UnLoadDisk", true );

	DBG_EVENT( "Removing disk: " << m_DiskMedia[ index ]->GetName( ));

	m_DiskMedia[ index ]->ClearDisk( );
}

void cDiskDevice::FlushDisk( cRefPtr<cDiskMedia> &diskMedia )
{
	FUNCTION_ENTRY( this, "cDiskDevice::FlushDisk", true );

	if( diskMedia->HasChanged( ) && ( diskMedia->SaveFile( ) == false ))
	{
		std::string homePath = GetHomePath( "disks" );

		if( homePath.compare( 0, homePath.size( ), diskMedia->GetName( ), homePath.size( )) == 0 )
		{
			CreateHomePath( "disks" );
			diskMedia->SaveFile( );
		}
	}
}

void cDiskDevice::FindSector( )
{
	FUNCTION_ENTRY( this, "cDiskDevice::FindSector", true );

	if( m_CurTrack != nullptr )
	{
		m_CurSector = m_CurTrack->GetSector( m_TrackRegister, -1, m_SectorRegister );

		if( m_CurSector == nullptr )
		{
			DBG_ERROR( "PC: " << hex << m_pCPU->GetPC( )
			                  << " ------ Unable to find sector " << m_SectorRegister
			                  << " on track " << m_TrackRegister
			                  << " side " << m_HeadSelect << "! -------" );
		}
	}
}

void cDiskDevice::CompleteCommand( )
{
	FUNCTION_ENTRY( this, "cDiskDevice::CompleteCommand", true );

	m_ClockStart = 0;

	switch( m_CmdInProgress )
	{
		case CMD_NONE :
			break;
		case CMD_READ_ADDRESS :
			break;
		case CMD_READ_TRACK :
			break;
		case CMD_READ_SECTOR :
			break;
		case CMD_WRITE_TRACK :
			if( m_BytesLeft > 0 )
			{
				m_StatusRegister |= STATUS_LOST_DATA;
				m_CurTrack->Write( track::Format::FM, m_DataBuffer );
			}
			break;
		case CMD_WRITE_SECTOR :
			if( m_BytesLeft > 0 )
			{
				m_StatusRegister |= STATUS_LOST_DATA;
				m_CurSector->Write( m_DataMark, m_DataBuffer );
			}
			break;
	}

	m_CmdInProgress = CMD_NONE;

	m_StatusRegister &= ~STATUS_BUSY;
}

UINT8 cDiskDevice::ReadByte( )
{
	FUNCTION_ENTRY( this, "cDiskDevice::ReadByte", true );

	UINT8 retVal = 0;

	if( m_ReadDataPtr != nullptr )
	{
		retVal = *m_ReadDataPtr++;

		if( --m_BytesLeft == 0 )
		{
			m_StatusRegister &= ~STATUS_BUSY;
			m_ReadDataPtr = nullptr;
		}
	}
	else
	{
		// Set error?
	}

	return retVal;
}

void cDiskDevice::WriteByte( UINT8 val )
{
	FUNCTION_ENTRY( this, "cDiskDevice::WriteByte", true );

	m_LastData = val;

	if( m_BytesLeft > 0 )
	{
		m_DataBuffer.push_back( val );

		if( --m_BytesLeft == 0 )
		{
			m_StatusRegister &= ~STATUS_BUSY;
			if( m_CmdInProgress == CMD_WRITE_TRACK )
			{
				m_CurTrack->Write( track::Format::FM, m_DataBuffer );
			}
			else
			{
				m_CurSector->Write( m_DataMark, m_DataBuffer );
			}
		}
	}
	else
	{
		// Just ignore these
	}
}

void cDiskDevice::VerifyTrack( )
{
	FUNCTION_ENTRY( this, "cDiskDevice::VerifyTrack", true );

	DBG_EVENT( "Verifing track" );

	if(( m_CurTrack == nullptr ) || ( m_CurTrack->GetSector( m_TrackRegister, -1, -1 ) == nullptr ))
	{
		DBG_WARNING( "Track verification failed" );
		m_StatusRegister |= STATUS_SEEK_ERROR;
		return;
	}

	for( auto sector : m_CurTrack->GetSectors( ))
	{
		if(( sector->LogicalCylinder( ) == m_TrackRegister ) && ( sector->ValidID( )))
		{
			return;
		}
	}

	m_StatusRegister |= STATUS_CRC_ERROR | STATUS_SEEK_ERROR;
}

void cDiskDevice::Restore( UINT8 cmd )
{
	FUNCTION_ENTRY( this, "cDiskDevice::Restore", true );

	m_TrackSelect    = 0;
	m_TrackRegister  = 0;

	m_SectorRegister = 0;

	// Set the status register
	m_StatusRegister = STATUS_TRACK_0;

	m_CurTrack = m_CurDisk ? m_CurDisk->GetTrack( m_TrackSelect, m_HeadSelect ) : nullptr;
	m_CurSector = nullptr;

	// Check for track verification
	if( cmd & 0x04 )
	{
		VerifyTrack( );
	}
}

void cDiskDevice::Seek( UINT8 cmd )
{
	FUNCTION_ENTRY( this, "cDiskDevice::Seek", true );

	// Assume the Track Register is correct and move accordingly
	m_TrackSelect   += ( m_LastData - m_TrackRegister );
	m_TrackRegister  = m_LastData;

	// Keep the hardware track within range
	if( m_TrackSelect >= MAX_TRACKS )
	{
		m_TrackSelect = MAX_TRACKS - 1;
	}

	DBG_TRACE( "Seeking to track " << m_TrackSelect );

	// Set the status register
	m_StatusRegister = ( m_TrackSelect == 0 ) ? STATUS_TRACK_0 : 0x00;

	m_CurTrack = m_CurDisk ? m_CurDisk->GetTrack( m_TrackSelect, m_HeadSelect ) : nullptr;
	m_CurSector = nullptr;

	// Check for track verification
	if( cmd & 0x04 )
	{
		VerifyTrack( );
	}
}

void cDiskDevice::Step( UINT8 cmd )
{
	FUNCTION_ENTRY( this, "cDiskDevice::Step", true );

	m_TrackSelect += m_StepDirection;

	// Keep the hardware track within range
	if( m_TrackSelect >= MAX_TRACKS )
	{
		m_TrackSelect = ( m_StepDirection > 0 ) ? MAX_TRACKS - 1 : 0;
	}

	DBG_TRACE( "Stepping to track " << m_TrackSelect );

	// Update the track register
	if( cmd & 0x10 )
	{
		DBG_EVENT( "Updating track register" );
		m_TrackRegister = m_TrackSelect;
	}

	// Set the status register
	m_StatusRegister = ( m_TrackSelect == 0 ) ? STATUS_TRACK_0 : 0x00;

	m_CurTrack = m_CurDisk ? m_CurDisk->GetTrack( m_TrackSelect, m_HeadSelect ) : nullptr;
	m_CurSector = nullptr;

	// Check for track verification
	if( cmd & 0x04 )
	{
		VerifyTrack( );
	}
}

void cDiskDevice::StepIn( UINT8 cmd )
{
	FUNCTION_ENTRY( this, "cDiskDevice::StepIn", true );

	m_StepDirection = 1;

	Step( cmd );
}

void cDiskDevice::StepOut( UINT8 cmd )
{
	FUNCTION_ENTRY( this, "cDiskDevice::StepOut", true );

	m_StepDirection = -1;

	Step( cmd );
}

void cDiskDevice::ReadSector( UINT8 cmd )
{
	FUNCTION_ENTRY( this, "cDiskDevice::ReadSector", true );

	FindSector( );

	if(( m_CurSector != nullptr ) && ( m_CurSector->ValidID( )))
	{
		DBG_EVENT( " C:" << m_CurSector->LogicalCylinder( )
		        << " H:" << m_CurSector->LogicalHead( )
		        << " S:" << m_CurSector->LogicalSector( )
		        << " L:" << m_CurSector->LogicalSize( )
		        << " - " << dec << m_CurSector->Size( ));

		m_BytesExpected   = m_CurSector->Size( );
		m_BytesLeft       = m_CurSector->Size( );
		m_DataBuffer      = m_CurSector->Read( );
		m_ReadDataPtr     = &m_DataBuffer[ 0 ];

		m_StatusRegister |= STATUS_BUSY;
		m_StatusRegister &= ~STATUS_NOT_FOUND;
		// Handle special Data Address Marks
		m_StatusRegister &= 0x60;
		if( m_IsFD1771 == true )
		{
			m_StatusRegister |= ( 0x03 ^ ( m_CurSector->DataMark( ) & 0x03 )) << 5;
		}
		else
		{
			m_StatusRegister |= ( 0x02 ^ ( m_CurSector->DataMark( ) & 0x02 )) << 4;
		}
		if( !m_CurSector->ValidData( ))
		{
			m_StatusRegister |= STATUS_CRC_ERROR;
		}
		m_CmdInProgress = CMD_READ_SECTOR;
	}
	else
	{
		// ?? set up a dummy buffer & error status ??
		m_StatusRegister |= STATUS_NOT_FOUND;
	}

	if( cmd & 0x10 )
	{
		DBG_WARNING( "Multi-sector read requested - feature not implemented" );
	}
}

void cDiskDevice::WriteSector( UINT8 cmd )
{
	FUNCTION_ENTRY( this, "cDiskDevice::WriteSector", true );

	if( m_CurDisk->IsWriteProtected( ))
	{
		m_StatusRegister |= STATUS_WRITE_PROTECTED;
		return;
	}

	FindSector( );

	if( m_CurSector != nullptr )
	{
		DBG_EVENT( " C:" << m_CurSector->LogicalCylinder( )
		        << " H:" << m_CurSector->LogicalHead( )
		        << " S:" << m_CurSector->LogicalSector( )
		        << " L:" << m_CurSector->LogicalSize( )
		        << " - " << dec << m_CurSector->Size( ));

		m_BytesExpected   = m_CurSector->Size( );
		m_BytesLeft       = m_CurSector->Size( );
		m_DataBuffer.clear( );
		m_StatusRegister |= STATUS_BUSY;
		m_StatusRegister &= ~STATUS_NOT_FOUND;
		// Set special Data Address Marks
		if( m_IsFD1771 == true )
		{
			m_DataMark = 0xFB - ( cmd & 0x03 );
		}
		else
		{
			m_DataMark = ( cmd & 0x01 ) ? 0xF8 : 0xFB;
		}
		m_CmdInProgress = CMD_WRITE_SECTOR;
	}
	else
	{
		m_StatusRegister |= STATUS_NOT_FOUND;
	}

	if( cmd & 0x10 )
	{
		DBG_WARNING( "Multi-sector write requested - feature not implemented" );
	}
}

void cDiskDevice::ReadAddress( UINT8 )
{
	FUNCTION_ENTRY( this, "cDiskDevice::ReadAddress", true );

	FindSector( );

	if( m_CurSector != nullptr )
	{
		m_BytesExpected   = 6;
		m_BytesLeft       = 6;
		m_DataBuffer      = { m_CurSector->GetID( ), m_CurSector->GetID( ) + 6 };
		m_ReadDataPtr     = &m_DataBuffer[ 0 ];
		m_StatusRegister |= STATUS_BUSY;
		m_StatusRegister &= ~STATUS_NOT_FOUND;

		m_CmdInProgress   = CMD_READ_ADDRESS;
	}
	else
	{
		m_StatusRegister |= STATUS_NOT_FOUND;
	}
}

void cDiskDevice::ReadTrack( UINT8 )
{
	FUNCTION_ENTRY( this, "cDiskDevice::ReadTrack", true );

	m_ClockStart = m_pCPU->GetClocks( );

	m_CurTrack = m_CurDisk ? m_CurDisk->GetTrack( m_TrackSelect, m_HeadSelect ) : nullptr;
	m_CurSector = nullptr;

	m_DataBuffer.clear( );
	if( m_CurTrack != nullptr )
	{
		m_DataBuffer = m_CurTrack->Read( );
	}

	if( !m_DataBuffer.empty( ))
	{
		DBG_EVENT( " T:" << m_TrackSelect << " H:" << m_HeadSelect );
		m_BytesExpected   = m_DataBuffer.size( );
		m_BytesLeft       = m_DataBuffer.size( );
		m_ReadDataPtr     = &m_DataBuffer[ 0 ];
		m_StatusRegister |= STATUS_BUSY;
		m_StatusRegister &= ~STATUS_NOT_FOUND;
		m_CmdInProgress   = CMD_READ_TRACK;
	}
	else
	{
		m_BytesExpected   = 0;
		m_BytesLeft       = 0;
		m_ReadDataPtr     = nullptr;
		m_StatusRegister |= STATUS_NOT_FOUND;
	}
}

void cDiskDevice::WriteTrack( UINT8 )
{
	FUNCTION_ENTRY( this, "cDiskDevice::WriteTrack", true );

	if( m_CurDisk->IsWriteProtected( ))
	{
		return;
	}

	m_ClockStart = m_pCPU->GetClocks( );

	m_CurTrack = m_CurDisk ? m_CurDisk->GetTrack( m_TrackSelect, m_HeadSelect ) : nullptr;
	m_CurSector = nullptr;

	if( m_CurTrack != nullptr )
	{
		DBG_EVENT( " T:" << m_TrackSelect << " H:" << m_HeadSelect );
		m_BytesExpected   = TRACK_SIZE_FM;	// TODO - Investigate 'expected' size
		m_BytesLeft       = TRACK_SIZE_FM;
		m_DataBuffer.clear( );
		m_StatusRegister |= STATUS_BUSY;
		m_StatusRegister &= ~STATUS_NOT_FOUND;
		m_CmdInProgress   = CMD_WRITE_TRACK;
	}
	else
	{
		m_BytesExpected   = 0;
		m_BytesLeft       = 0;
		m_StatusRegister |= STATUS_NOT_FOUND;
	}
}

void cDiskDevice::ForceInterrupt( UINT8 )
{
	FUNCTION_ENTRY( this, "cDiskDevice::ForceInterrupt", true );

	m_StatusRegister &= ~STATUS_BUSY;
}

void cDiskDevice::HandleCommand( UINT8 cmd )
{
	FUNCTION_ENTRY( this, "cDiskDevice::HandleCommand", true );

	// Make sure the previous command has completed
	CompleteCommand( );

	switch( cmd & 0xF0 )
	{
		// Type I commands
		case 0x00 :                     // CMD_RESTORE
			DBG_EVENT( "PC: " << hex << m_pCPU->GetPC( ) << " => " << "CMD_RESTORE" );
			Restore( cmd );
			break;
		case 0x10 :                     // CMD_SEEK
			DBG_EVENT( "PC: " << hex << m_pCPU->GetPC( ) << " => " << "CMD_SEEK" );
			Seek( cmd );
			break;
		case 0x20 :                     // CMD_STEP
		case 0x30 :
			DBG_EVENT( "PC: " << hex << m_pCPU->GetPC( ) << " => " << "CMD_STEP" );
			Step( cmd );
			break;
		case 0x40 :                     // CMD_STEP_IN
		case 0x50 :
			DBG_EVENT( "PC: " << hex << m_pCPU->GetPC( ) << " => " << "CMD_STEP_IN" );
			StepIn( cmd );
			break;
		case 0x60 :                     // CMD_STEP_OUT
		case 0x70 :
			DBG_EVENT( "PC: " << hex << m_pCPU->GetPC( ) << " => " << "CMD_STEP_OUT" );
			StepOut( cmd );
			break;

		// Type II commands
		case 0x80 :                     // CMD_READ
		case 0x90 :
			DBG_EVENT( "PC: " << hex << m_pCPU->GetPC( ) << " => " << "CMD_READ" );
			ReadSector( cmd );
			break;
		case 0xA0 :                     // CMD_WRITE
		case 0xB0 :                     // CMD_WRITE
			DBG_EVENT( "PC: " << hex << m_pCPU->GetPC( ) << " => " << "CMD_WRITE" );
			WriteSector( cmd );
			break;

		// Type III commands
		case 0xC0 :                     // CMD_READ_ADDRESS
			DBG_EVENT( "PC: " << hex << m_pCPU->GetPC( ) << " => " << "CMD_READ_ADDRESS" );
			ReadAddress( cmd );
			break;
		case 0xE0 :                     // CMD_READ_TRACK
			DBG_EVENT( "PC: " << hex << m_pCPU->GetPC( ) << " => " << "CMD_READ_TRACK" );
			ReadTrack( cmd );
			break;
		case 0xF0 :                     // CMD_WRITE_TRACK
			DBG_EVENT( "PC: " << hex << m_pCPU->GetPC( ) << " => " << "CMD_WRITE_TRACK" );
			WriteTrack( cmd );
			break;

		// Type IV commands
		case 0xD0 :                     // CMD_INTERRUPT
			DBG_EVENT( "PC: " << hex << m_pCPU->GetPC( ) << " => " << "CMD_INTERRUPT" );
			ForceInterrupt( cmd );
			break;

		default :
			DBG_ERROR( "PC: " << hex << m_pCPU->GetPC( ) << " Unknown command: " << hex << ( UINT8 ) ( cmd & 0xF0 ));
			return;
	}
}

//----------------------------------------------------------------------------
// cDevice methods
//----------------------------------------------------------------------------

void cDiskDevice::ActivateInternal( )
{
	FUNCTION_ENTRY( this, "cDiskDevice::ActivateInternal", true );

	m_pCPU->SetTrap( 0x5FF0, ( UINT8 ) MEMFLG_TRAP_READ,  m_TrapIndex );
	m_pCPU->SetTrap( 0x5FF2, ( UINT8 ) ( MEMFLG_TRAP_READ | MEMFLG_TRAP_WRITE ), m_TrapIndex );
	m_pCPU->SetTrap( 0x5FF4, ( UINT8 ) MEMFLG_TRAP_READ,  m_TrapIndex );
	m_pCPU->SetTrap( 0x5FF6, ( UINT8 ) MEMFLG_TRAP_READ,  m_TrapIndex );
	m_pCPU->SetTrap( 0x5FF8, ( UINT8 ) MEMFLG_TRAP_WRITE, m_TrapIndex );
	m_pCPU->SetTrap( 0x5FFA, ( UINT8 ) MEMFLG_TRAP_WRITE, m_TrapIndex );
	m_pCPU->SetTrap( 0x5FFC, ( UINT8 ) MEMFLG_TRAP_WRITE, m_TrapIndex );
	m_pCPU->SetTrap( 0x5FFE, ( UINT8 ) MEMFLG_TRAP_WRITE, m_TrapIndex );
}

UINT8 cDiskDevice::WriteMemory( ADDRESS address, UINT8 val )
{
	FUNCTION_ENTRY( this, "cDiskDevice::WriteMemory", true );

	val ^= 0xFF;

	switch( address )
	{
		case REG_COMMAND :
			HandleCommand( val );
			break;
		case REG_WR_TRACK :
			m_TrackRegister = val;
			DBG_EVENT( "PC: " << hex << m_pCPU->GetPC( ) << " Set Track = " << hex << val );
			break;
		case REG_WR_SECTOR :
			m_SectorRegister = val;
			DBG_EVENT( "PC: " << hex << m_pCPU->GetPC( ) << " Set Sector = " << hex << val );
			break;
		case REG_WR_DATA :
			WriteByte( val );
			break;
		default :
			DBG_ERROR( "PC: " << hex << m_pCPU->GetPC( ) << " *** Unexpected MEM Wr: " << hex << address << " => " << val );
	}

	return ( UINT8 ) ( val ^ 0xFF );
}

UINT8 cDiskDevice::ReadMemory( ADDRESS address, UINT8 )
{
	FUNCTION_ENTRY( this, "cDiskDevice::ReadMemory", true );

	UINT8 retVal = 0xFF;

	switch( address )
	{
		case REG_STATUS :
			if(( m_ClockStart != 0 ) && ( m_pCPU->GetClocks( ) - m_ClockStart > m_ClocksPerRev ))
			{
				CompleteCommand( );
			}
			retVal = m_StatusRegister;
			if(( m_CurDisk != nullptr ) && ( m_CurDisk->IsWriteProtected( )))
			{
				retVal |= STATUS_WRITE_PROTECTED;
			}
			if(( m_pCPU->GetClocks( ) % m_ClocksPerRev ) < 10 * m_ClocksPerRev / 360 )
			{
				retVal |= STATUS_INDEX_PULSE;
			}
//			DBG_TRACE ( "Left: " << m_BytesLeft << " Expected: " << m_BytesExpected );
			DBG_EVENT( "PC: " << hex << m_pCPU->GetPC( ) << " Get Status = " << hex << ( UINT8 ) retVal );
			break;
		case REG_RD_TRACK :
			retVal = m_TrackSelect;
			DBG_EVENT( "PC: " << hex << m_pCPU->GetPC( ) << " Get Track = " << hex << ( UINT8 ) retVal );
			break;
		case REG_RD_DATA :
			retVal = ReadByte( );
			break;
		default :
			DBG_ERROR( "PC: " << hex << m_pCPU->GetPC( ) << " *** Unexpected MEM Rd: " << hex << address );
	}

	return ( UINT8 ) ( retVal ^ 0xFF );
}
