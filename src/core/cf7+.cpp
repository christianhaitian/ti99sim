//----------------------------------------------------------------------------
//
// File:        cf7+.cpp
// Date:        27-Jun-2013
// Programmer:  Marc Rousseau
//
// Description:
//
// Copyright (c) 2013 Marc Rousseau, All Rights Reserved.
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
#include "cf7+.hpp"
#include "support.hpp"

DBG_REGISTER( __FILE__ );

extern int verbose;

std::string cCF7::DiskImage = "image.cf7";

#define STATUS_BSY	0x80	// Controller is busy executing a command. No register should be accessed
							//   (except the digital output register) while this bit is set
#define STATUS_RDY	0x40	// Controller is ready to accept a command, and the drive is spinning at correct speed
#define STATUS_WFT	0x20	// Controller detected a write fault
#define STATUS_SKC	0x10	// Read/write head is in position (seek completed)
#define STATUS_DRQ	0x08	// Controller is expecting data (for a write) or is sending data (for a read). Don't
							//   access the data register while this bit is 0
#define STATUS_COR	0x04	// Controller had to correct data, by using the ECC bytes (error correction code: extra
							//   bytes at the end of the sector that allows to verify its integrity and, sometimes,
							//   to correct errors)
#define STATUS_IDX	0x02	// Controller detected the index mark (which is not a hole on hard-drives)
#define STATUS_ERR	0x01	// Error occurred. An error code has been placed in the error register


// Error register
//
//   This is a read only register. Its content is only meaningful when the ERR bit is set in the status register.
//
#define ERROR_BBK	0x80	// Bad block. Sector marked as bad by host
#define ERROR_UNC	0x40	// Uncorrectable data error
#define ERROR_MC	0x20	// Medium changed (e.g. CD-ROM. Enhanced IDE only)
#define ERROR_NID	0x10	// No ID mark found
#define ERROR_MCR	0x08	// Medium change required (Enhanced IDE only)
#define ERROR_ABT	0x04	// Command aborted
#define ERROR_NT0	0x02	// No track 0 found
#define ERROR_NDM	0x01	// No data mark found

#pragma pack( 1 )

struct IDE_IDENTIFY
{
	UINT16	GeneralInformation;
	UINT16	LogicalCylinders;
	UINT16	reserved1;
	UINT16	LogicalHeads;
	UINT16	UnformattedBytesPerTrack;
	UINT16	UnformattedBytesPerSector;
	UINT16	LogicalSectors;
	UINT32	SectorsPerCard;
	UINT16	VendorStatusWords;
	char	SerialNumber[ 20 ];
	UINT16	ControllerType;
	UINT16	BufferSize;
	UINT16	ECCBytes;
	char	FirmwareRevision[ 8 ];
	char	ModelNumber[ 40 ];
	UINT16	RWMultiplesImplemented;
	UINT16	SupportDoubleWordIO;
	UINT16	reserved3;
	UINT16	reserved4;
	UINT16	MinimumPIOcycle;
	UINT16	MinimumDMAcycle;
};

#pragma pack( )

static inline void SetUINT16( void *_ptr, UINT16 value )
{
	FUNCTION_ENTRY( nullptr, "SetUINT16", true );

	UINT8 *ptr = ( UINT8 * ) _ptr;

	ptr[ 0 ] = value >> 8;
	ptr[ 1 ] = value & 0x0FF;
}

static inline void SetUINT32( void *_ptr, UINT32 value )
{
	FUNCTION_ENTRY( nullptr, "SetUINT32", true );

	UINT8 *ptr = ( UINT8 * ) _ptr;

	ptr[ 0 ] = ( UINT8 ) ( value >> 24 );
	ptr[ 1 ] = ( UINT8 ) ( value >> 16 );
	ptr[ 2 ] = ( UINT8 ) ( value >>  8 );
	ptr[ 3 ] = ( UINT8 ) ( value >>  0 );
}

cCF7::cCF7( iCartridge *rom ) :
	cBaseObject( "cCF7" ),
	cStateObject( ),
	cDevice( rom ),
	m_LBA( 0xA0000000 ),
	m_SectorCount( 0 ),
	m_Feature( 0 ),
	m_StatusRegister( STATUS_RDY | STATUS_SKC ),
	m_ErrorRegister( 0 ),
	m_TransferMode( XFER_MODE_NONE ),
	m_BytesTransferred( 0 ),
	m_DataBuffer( ),
	m_File( nullptr ),
	m_FileName( ),
	m_FileSectors( 0 ),
	m_CmdInProgress( CMD_NONE )
{
	FUNCTION_ENTRY( this, "cCF7::cCF7", true );

	if( m_IsValid && ( m_CRU + 1 == 0 ))
	{
		DBG_ERROR( "Cartridge does not appear to be a valid USCD p-System device" );
		m_IsValid = false;
	}
}

cCF7::~cCF7( )
{
	FUNCTION_ENTRY( this, "cCF7::~cCF7", true );

	if( m_File )
	{
		fclose( m_File );
	}
}

//----------------------------------------------------------------------------
// iBaseObject Methods
//----------------------------------------------------------------------------

const void *cCF7::GetInterface( const std::string &iName ) const
{
	FUNCTION_ENTRY( this, "cCF7::GetInterface", false );

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

std::string cCF7::GetIdentifier( )
{
	FUNCTION_ENTRY( this, "cCF7::GetIdentifier", true );

	return "CF7+";
}

bool cCF7::ParseState( const sStateSection &state )
{
	FUNCTION_ENTRY( this, "cCF7::ParseState", true );

	m_pROM = cCartridge::LoadCartridge( state.getValue( "ROM" ), "console" );
	state.loadSubSection( m_pROM );

	state.load( "LBA", m_LBA, SaveFormat::HEXADECIMAL );
	state.load( "SectorCount", m_SectorCount, SaveFormat::HEXADECIMAL );
	state.load( "Feature", m_Feature, SaveFormat::HEXADECIMAL );
	state.load( "StatusRegister", m_StatusRegister, SaveFormat::HEXADECIMAL );
	state.load( "ErrorRegister", m_ErrorRegister, SaveFormat::HEXADECIMAL );

	state.load( "TransferMode", reinterpret_cast<int&>( m_TransferMode ), SaveFormat::DECIMAL );
	state.load( "BytesTransferred", m_BytesTransferred, SaveFormat::DECIMAL );

	state.load( "Buffer", m_DataBuffer, sizeof( m_DataBuffer ));

	std::string filename;
	state.load( "FieName", filename );

	if( !std::filesystem::exists( filename ))
	{
		return false;
	}

	UnLoadDisk( );
	LoadDisk( filename.c_str( ));

	state.load( "CmdInProgress", reinterpret_cast<int&>( m_CmdInProgress ), SaveFormat::DECIMAL );

	return true;
}

std::optional<sStateSection> cCF7::SaveState( )
{
	FUNCTION_ENTRY( this, "cCF7::SaveState", true );

	sStateSection save;

	save.name = "CF7+";

	save.store( "ROM", m_pROM->GetDescriptor( ));
	save.addSubSection( m_pROM );

	save.store( "LBA", m_LBA, SaveFormat::HEXADECIMAL );
	save.store( "SectorCount", m_SectorCount, SaveFormat::HEXADECIMAL );
	save.store( "Feature", m_Feature, SaveFormat::HEXADECIMAL );
	save.store( "StatusRegister", m_StatusRegister, SaveFormat::HEXADECIMAL );
	save.store( "ErrorRegister", m_ErrorRegister, SaveFormat::HEXADECIMAL );

	save.store( "TransferMode", static_cast<int>( m_TransferMode ), SaveFormat::DECIMAL );
	save.store( "BytesTransferred", m_BytesTransferred, SaveFormat::DECIMAL );

	save.store( "Buffer", m_DataBuffer, sizeof( m_DataBuffer ));

	save.store( "FieName", m_FileName );

	save.store( "CmdInProgress", static_cast<int>( m_CmdInProgress ), SaveFormat::DECIMAL );

	return save;
}

//----------------------------------------------------------------------------
// iDevice methods
//----------------------------------------------------------------------------

const char *cCF7::GetName( )
{
	return "CF7+ Disk System";
}

bool cCF7::Initialize( iComputer *computer )
{
	FUNCTION_ENTRY( this, "cCF7::Initialize", true );

	std::string validName = LocateFile( "disks", DiskImage );

	if( ! validName.empty( ))
	{
		LoadDisk( validName.c_str( ));
	}
	else if( verbose >= 2 )
	{
		fprintf( stdout, "Unable to locate file '%s' for CF7+ device\n", DiskImage.c_str( ));
	}

	return cDevice::Initialize( computer );
}

void cCF7::WriteCRU( ADDRESS address, int val )
{
	FUNCTION_ENTRY( this, "cCF7::WriteCRU", true );

	switch( address )
	{
		case 0 :
			EnableDevice( val );
			break;
		default :
			DBG_ERROR( "Unexpected Address - " << address );
			break;
	}
}

int cCF7::ReadCRU( ADDRESS address )
{
	FUNCTION_ENTRY( this, "cCF7::ReadCRU", true );

	int retVal = 1;

	switch( address )
	{
		default :
			DBG_ERROR( "Unexpected Address - " << address );
			break;
	}

	return retVal;
}

void cCF7::LoadDisk( const char *filename )
{
	FUNCTION_ENTRY( this, "cCF7::LoadDisk", true );

	FILE *file = fopen( filename, "r+" );
	if( file == nullptr )
	{
		DBG_ERROR( "Unable to open file " << filename );
		return;
	}

	if( verbose >= 2 )
	{
		fprintf( stdout, "Loaded file '%s' as CF7+ device\n", filename );
	}

	if( m_File != nullptr )
	{
		DBG_WARNING( "Disk already loaded - closing" );
		fclose( m_File );
	}

	fseek( file, 0, SEEK_END );

	m_File        = file;
	m_FileName    = filename;
	m_FileSectors = ftell( m_File ) / 512;
}

void cCF7::UnLoadDisk( )
{
	FUNCTION_ENTRY( this, "cCF7::UnLoadDisk", true );

	if( m_File != nullptr )
	{
		fclose( m_File );
		m_File = nullptr;
		m_FileName.clear( );
	}
}

void cCF7::CompleteCommand( )
{
	FUNCTION_ENTRY( this, "cCF7::CompleteCommand", true );

	// TODO

	m_StatusRegister = STATUS_RDY | STATUS_SKC;
}

UINT8 cCF7::ReadByte( )
{
	FUNCTION_ENTRY( this, "cCF7::ReadByte", true );

	UINT8 value = 0;

	if( m_BytesTransferred < 512 )
	{
		if( m_TransferMode == XFER_MODE_16BIT )
		{
			value = m_DataBuffer[ m_BytesTransferred ];
			m_BytesTransferred += 2;
		}
		else
		{
			value = m_DataBuffer[ m_BytesTransferred++ ^ 1 ];
		}

		if( m_BytesTransferred == 512 )
		{
			m_SectorCount--;

			// TODO - Handle multiple sector transfers
			memset( m_DataBuffer, 0, sizeof( m_DataBuffer ));

			CompleteCommand( );
		}
	}
	else
	{
		DBG_ERROR( "ReadByte request failure - sector buffer is empty" );
	}

	return value;
}

void cCF7::WriteByte( UINT8 value )
{
	FUNCTION_ENTRY( this, "cCF7::WriteByte", true );

	if( m_BytesTransferred < 512 )
	{
		if( m_TransferMode == XFER_MODE_16BIT )
		{
			m_DataBuffer[ m_BytesTransferred ] = value;
			m_BytesTransferred += 2;
		}
		else
		{
			m_DataBuffer[ m_BytesTransferred++ ^ 1 ] = value;
		}

		if( m_BytesTransferred == 512 )
		{
			UINT32 index = m_LBA & 0x0FFFFFFF;

			fseek( m_File, index * 512, SEEK_SET );
			fwrite( m_DataBuffer, 512, 1, m_File );
			fflush( m_File );

			m_SectorCount--;

			// TODO - Handle multiple sector transfers
			memset( m_DataBuffer, 0, sizeof( m_DataBuffer ));

			CompleteCommand( );
		}
	}
	else
	{
		DBG_ERROR( "WriteByte request failure - sector buffer is full" );
	}
}

void cCF7::ReadSector( UINT8 command )
{
	FUNCTION_ENTRY( this, "cCF7::ReadSector", true );

	// For this command you have to load LBA first. When the command completes (DRQ goes active)
	//  you can read 256 words (16-bits) from the data register. Commands 0x22 and 0x23 will pass
	//  the four error correction code bytes after the data bytes

	UINT32 index = m_LBA & 0x0FFFFFFF;

	if( index >= m_FileSectors )
	{
		m_StatusRegister |= STATUS_ERR;
		m_ErrorRegister   = ERROR_NID;
		return;
	}

	m_BytesTransferred = 0;

	fseek( m_File, index * 512, SEEK_SET );
	if( fread( m_DataBuffer, 512, 1, m_File ) != 1 )
	{
		m_StatusRegister |= STATUS_ERR;
		m_ErrorRegister   = ERROR_BBK;
		return;
	}

	m_StatusRegister &= ~STATUS_RDY;
	m_StatusRegister |=  STATUS_DRQ;

	m_CmdInProgress = CMD_READ_SECTOR;
}

void cCF7::WriteSector( UINT8 command )
{
	FUNCTION_ENTRY( this, "cCF7::WriteSector", true );

	// Here too you have to load cylinder/head/sector. Then wait for DRQ to become active.
	//  Feed the disk 256 words of data in the data register. Next the disk starts writing.
	//  When BSY goes not active you can read the status from the status register. With
	//  commands 0x30 and 0x31, the controller calculates the four ECC bytes (error correction
	//  codes), with commands 0x32 and 0x33 you should provides these four bytes after the data bytes.

	UINT32 index = m_LBA & 0x0FFFFFFF;

	if( index >= m_FileSectors )
	{
		m_StatusRegister |= STATUS_ERR;
		m_ErrorRegister   = ERROR_NID;
		return;
	}

	m_BytesTransferred = 0;

	memset( m_DataBuffer, 0, sizeof( m_DataBuffer ));

	m_StatusRegister &= ~STATUS_RDY;
	m_StatusRegister |=  STATUS_DRQ;

	m_CmdInProgress = CMD_WRITE_SECTOR;
}

void cCF7::VerifySector( UINT8 command )
{
	FUNCTION_ENTRY( this, "cCF7::VerifySector", true );

	// Reads sector(s) and checks if the ECC matches, but doesn't transfer data.
	// 0x40	Verify sector with retry
	// 0x41	Verify sector without retry
}

void cCF7::Diagnostic( UINT8 command )
{
	FUNCTION_ENTRY( this, "cCF7::Diagnostic", true );

	// Tells the controller to perform a self test and to test the drives.
	//    Results are passed in the error register:
	//      0x01 = Master OK
	//      0x02 = Format circuit error in master
	//      0x03 = Buffer error in master
	//      0x04 = ECC logic error in master
	//      0x05 = Microprocessor error in master
	//      0x06 = Interface error in master
	//      0x80 = At least one error in slave
}

void cCF7::IdentifyDrive( UINT8 command )
{
	FUNCTION_ENTRY( this, "cCF7::IdentifyDrive", true );

	m_BytesTransferred = 0;

	IDE_IDENTIFY identify;

	memset( &identify, 0, sizeof( identify ));

	SetUINT16( &identify.LogicalCylinders, m_FileSectors / 64 / 62 );
	SetUINT16( &identify.LogicalHeads,     64 );
	SetUINT16( &identify.LogicalSectors,   62 );
	SetUINT32( &identify.SectorsPerCard,   m_FileSectors );

	memcpy( identify.SerialNumber,        "00000000000000000000", 20 );
	memcpy( identify.FirmwareRevision,    "Rev 1.00", 8 );
	memcpy( identify.ModelNumber,         "TI-99/Sim CF7+ 1.0                      ", 40 );

	memset( m_DataBuffer, 0, sizeof( m_DataBuffer ));
	memcpy( m_DataBuffer, &identify, sizeof( identify ));

	m_StatusRegister |=  STATUS_DRQ;

	m_CmdInProgress = CMD_IDENTIFY_DRIVE;
}

void cCF7::SetFeature( UINT8 )
{
	FUNCTION_ENTRY( this, "cCF7::SetFeature", true );

	switch( m_Feature )
	{
		case 0x01 :
			m_TransferMode = XFER_MODE_8BIT;
			break;
		case 0x81 :
			m_TransferMode = XFER_MODE_16BIT;
			break;
		default :
			DBG_ERROR( "Request for unimplemented feature: " << hex << m_Feature );
	}
}

void cCF7::HandleCommand( UINT8 command )
{
	FUNCTION_ENTRY( this, "cCF7::HandleCommand", true );

	if( m_StatusRegister & STATUS_DRQ )
	{
		DBG_ERROR( "DRQ set before new command (last transfer " << m_BytesTransferred << " bytes)" );
		m_StatusRegister &= ~STATUS_DRQ;
	}

	switch( command )
	{
		case 0x20 : // Read sector with retry
		case 0x21 : // Read sector without retry
		case 0x22 : // Read sector also pass ECC bytes
		case 0x23 : // Read sector with retry and pass ECC bytes
			ReadSector( command );
			break;

		case 0x30 : // Write sector with retry
		case 0x31 : // Write sector without retry
		case 0x32 : // ECC bytes will be passed by host
		case 0x33 : // Write sector with retry and ECC bytes will be passed by host
			WriteSector( command );
			break;

		case 0xEC : // Identify drive
			IdentifyDrive( command );
			break;

		case 0xEF : // Set features
			SetFeature( command );
			break;

		case 0x40 : // Verify sector with retry
		case 0x41 : // Verify sector without retry
//			VerifySector( command );
//			break;

		case 0x50 : // Format track
		case 0x70 : // Seek
//			break;

		case 0x90 : // Diagnostic
//			Diagnostic( command );
//			break;

		default :
			DBG_ERROR( "PC: " << hex << m_pCPU->GetPC( ) << " Unhandled IDE command : " << hex << command );
	}
}

//----------------------------------------------------------------------------
// cDevice methods
//----------------------------------------------------------------------------

void cCF7::ActivateInternal( )
{
	FUNCTION_ENTRY( this, "cCF7::ActivateInternal", true );

	// These are based on the DSR in ROM
	m_pCPU->SetTrap( 0x5E01, ( UINT8 ) MEMFLG_TRAP_READ ,  m_TrapIndex );
	m_pCPU->SetTrap( 0x5E03, ( UINT8 ) MEMFLG_TRAP_READ,   m_TrapIndex );
	m_pCPU->SetTrap( 0x5E05, ( UINT8 ) MEMFLG_TRAP_READ,   m_TrapIndex );
	m_pCPU->SetTrap( 0x5E07, ( UINT8 ) MEMFLG_TRAP_READ,   m_TrapIndex );
	m_pCPU->SetTrap( 0x5E09, ( UINT8 ) MEMFLG_TRAP_READ,   m_TrapIndex );
	m_pCPU->SetTrap( 0x5E0B, ( UINT8 ) MEMFLG_TRAP_READ,   m_TrapIndex );
	m_pCPU->SetTrap( 0x5E0D, ( UINT8 ) MEMFLG_TRAP_READ,   m_TrapIndex );
	m_pCPU->SetTrap( 0x5E0F, ( UINT8 ) MEMFLG_TRAP_READ,   m_TrapIndex );

	m_pCPU->SetTrap( 0x5F01, ( UINT8 ) MEMFLG_TRAP_WRITE,  m_TrapIndex );
	m_pCPU->SetTrap( 0x5F03, ( UINT8 ) MEMFLG_TRAP_WRITE,  m_TrapIndex );
	m_pCPU->SetTrap( 0x5F05, ( UINT8 ) MEMFLG_TRAP_WRITE,  m_TrapIndex );
	m_pCPU->SetTrap( 0x5F07, ( UINT8 ) MEMFLG_TRAP_WRITE,  m_TrapIndex );
	m_pCPU->SetTrap( 0x5F09, ( UINT8 ) MEMFLG_TRAP_WRITE,  m_TrapIndex );
	m_pCPU->SetTrap( 0x5F0B, ( UINT8 ) MEMFLG_TRAP_WRITE,  m_TrapIndex );
	m_pCPU->SetTrap( 0x5F0D, ( UINT8 ) MEMFLG_TRAP_WRITE,  m_TrapIndex );
	m_pCPU->SetTrap( 0x5F0F, ( UINT8 ) MEMFLG_TRAP_WRITE,  m_TrapIndex );
	m_pCPU->SetTrap( 0x5F1D, ( UINT8 ) MEMFLG_TRAP_WRITE,  m_TrapIndex );

	// These are based on CFMGR
	m_pCPU->SetTrap( 0x5F81, ( UINT8 ) MEMFLG_TRAP_READ,   m_TrapIndex );
	m_pCPU->SetTrap( 0x5F8F, ( UINT8 ) MEMFLG_TRAP_READ,   m_TrapIndex );

	m_pCPU->SetTrap( 0x5FC1, ( UINT8 ) MEMFLG_TRAP_WRITE,  m_TrapIndex );
	m_pCPU->SetTrap( 0x5FC3, ( UINT8 ) MEMFLG_TRAP_WRITE,  m_TrapIndex );
	m_pCPU->SetTrap( 0x5FCF, ( UINT8 ) MEMFLG_TRAP_WRITE,  m_TrapIndex );
}

UINT8 cCF7::WriteMemory( ADDRESS address, UINT8 value )
{
	FUNCTION_ENTRY( this, "cCF7::WriteMemory", true );

	switch( address & 0xFF1F )
	{
		case 0x5F01 :	// Data (written 256 times)
			WriteByte( value );
			break;
		case 0x5F03 :	// Feature / Precomp register
			m_Feature = value;
			break;
		case 0x5F05 :	// Number of sectors
			m_SectorCount = value;
			break;
		case 0x5F07 :	// LBA – bits 7..0 or sector
			m_LBA = ( m_LBA & 0xFFFFFF00 ) | static_cast<UINT32>( value << 0 );
			break;
		case 0x5F09 :	// LBA – bits 15..8 or cylinder low
			m_LBA = ( m_LBA & 0xFFFF00FF ) | static_cast<UINT32>( value << 8 );
			break;
		case 0x5F0B :	// LBA – bits 23..16 or cylinder high
			m_LBA = ( m_LBA & 0xFF00FFFF ) | static_cast<UINT32>( value << 16 );
			break;
		case 0x5F0D :	// LBA – bits 27-24 or head
			m_LBA = ( m_LBA & 0x00FFFFFF ) | static_cast<UINT32>( value << 24 );
			break;
		case 0x5F0F :	// Command
			HandleCommand( value );
			break;
		case 0x5F1D :	// Control
			break;
		default :
			DBG_ERROR( "PC: " << hex << m_pCPU->GetPC( ) << " *** Unexpected MEM Wr: " << hex << address << " => " << value );
	}

	return value;
}

UINT8 cCF7::ReadMemory( ADDRESS address, UINT8 value )
{
	FUNCTION_ENTRY( this, "cCF7::ReadMemory", true );

	switch( address & 0xFE0F )
	{
		case 0x5E01 :	// Data (read 256 times)
			value = ReadByte( );
			break;
		case 0x5E03 :	// Error code
			value = m_ErrorRegister;
			m_StatusRegister &= ~STATUS_ERR;
			break;
		case 0x5E05 :	// Number of sectors
			value = m_SectorCount;
			break;
		case 0x5E07 :	// LBA – bits 7..0 or sector
			value = ( UINT8 ) ( m_LBA >> 0 );
			break;
		case 0x5E09 :	// LBA – bits 15..8 or cylinder low
			value = ( UINT8 ) ( m_LBA >> 8 );
			break;
		case 0x5E0B :	// LBA – bits 23..16 or cylinder high
			value = ( UINT8 ) ( m_LBA >> 16 );
			break;
		case 0x5E0D :	// LBA – bits 27-24 or head
			value = ( UINT8 ) ( m_LBA >> 24 );
			break;
		case 0x5E0F :	// Status
			value = ( m_File == nullptr ) ? m_StatusRegister | STATUS_DRQ | STATUS_ERR : m_StatusRegister;
			break;
		default :
			DBG_ERROR( "PC: " << hex << m_pCPU->GetPC( ) << " *** Unexpected MEM Rd: " << hex << address );
	}

	return value;
}
