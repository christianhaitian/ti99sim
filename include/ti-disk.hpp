
//----------------------------------------------------------------------------
//
// File:		ti-disk.hpp
// Date:		27-Mar-1998
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

#ifndef TIDISK_HPP_
#define TIDISK_HPP_

//#include "stateobject.hpp"
#include "device.hpp"
#include "disk-media.hpp"

#define REG_STATUS			0x5FF0
#define REG_RD_TRACK		0x5FF2
#define REG_RD_DATA			0x5FF6
#define REG_COMMAND			0x5FF8
#define REG_WR_TRACK		0x5FFA
#define REG_WR_SECTOR		0x5FFC
#define REG_WR_DATA			0x5FFE

// Not Ready
// Seek Error( track not found )
// Record Not Found( track/sector/side not found )
// Lost Data
// CRC Error
// Write protect

#define STATUS_NOT_READY		0x80		// No Diskette/Drive or Bad H/S
#define STATUS_WRITE_PROTECTED	0x40		// Write Protected Disk
#define STATUS_HEAD_LOADED		0x20
#define STATUS_WRITE_FAULT		0x20
#define STATUS_SEEK_ERROR		0x10		// Seek error or Bad H/S
#define STATUS_RECORD_NOT_FOUND	0x10
#define STATUS_CRC_ERROR		0x08		// CRC Error on a read
#define STATUS_LOST_DATA		0x04		// Lost data on a read
#define STATUS_TRACK_0			0x04		// Head positioned to track 0
#define STATUS_INDEX_PULSE		0x02
#define STATUS_DATA_REQUEST		0x02
#define STATUS_BUSY				0x01

#define STATUS_NOT_FOUND		( STATUS_CRC_ERROR | STATUS_LOST_DATA )

class cDiskDevice :
	public virtual cStateObject,
	public virtual cDevice
{

	enum TRAP_TYPE_E
	{
		TRAP_DISK
	};

	enum CMD_STATE_E
	{
		CMD_NONE,
		CMD_READ_ADDRESS,
		CMD_READ_TRACK,
		CMD_READ_SECTOR,
		CMD_WRITE_TRACK,
		CMD_WRITE_SECTOR
	};

	int                 m_StepDirection;
	UINT32              m_ClocksPerRev;
	UINT32              m_ClockStart;

	// CRU bits
	int                 m_HardwareBits;
	UINT8               m_DriveSelect;
	UINT8               m_HeadSelect;
	UINT8               m_TrackSelect;			// Actual position of the hardware

	bool                m_IsFD1771;
	bool                m_TransferEnabled;

	cRefPtr<cDiskMedia> m_DiskMedia[ 3 ];
	cDiskMedia         *m_CurDisk;
	iDiskTrack         *m_CurTrack;
	iDiskSector        *m_CurSector;

	UINT8               m_DataMark;

	// Hardware registers
	UINT8               m_StatusRegister;
	UINT8               m_TrackRegister;
	UINT8               m_SectorRegister;

	// The following are used to virtualize the Data Register
	UINT8               m_LastData;
	size_t              m_BytesExpected;
	size_t              m_BytesLeft;
	const UINT8        *m_ReadDataPtr;
	sDataBuffer         m_DataBuffer;

	CMD_STATE_E         m_CmdInProgress;

public:

	static std::string  DiskImage[ 3 ];

public:

	cDiskDevice( iCartridge * );

	// iBaseObject Methods
	virtual const void *GetInterface( const std::string &name ) const override;

	// iStateObject Methods
	virtual std::string GetIdentifier( ) override;
	virtual std::optional<sStateSection> SaveState( ) override;
	virtual bool ParseState( const sStateSection &state ) override;

	// iDevice methods
	virtual const char *GetName( ) override;
	virtual bool Initialize( iComputer *computer ) override;
	virtual void WriteCRU( ADDRESS, int ) override;
	virtual int ReadCRU( ADDRESS ) override;

	void LoadDisk( int, const char * );
	void UnLoadDisk( int );

private:

	void FlushDisk( cRefPtr<cDiskMedia> & );
	void FindSector( );

	void CompleteCommand( );

	UINT8 ReadByte( );
	void  WriteByte( UINT8 );

	void VerifyTrack( );

	void Restore( UINT8 );
	void Seek( UINT8 );
	void Step( UINT8 );
	void StepIn( UINT8 );
	void StepOut( UINT8 );
	void ReadSector( UINT8 );
	void WriteSector( UINT8 );
	void ReadAddress( UINT8 );
	void ReadTrack( UINT8 );
	void WriteTrack( UINT8 );
	void ForceInterrupt( UINT8 );
	void WriteTrackComplete( );

	void HandleCommand( UINT8 );

	// cDevice Methods
	virtual void ActivateInternal( ) override;
	virtual UINT8 WriteMemory( ADDRESS, UINT8 ) override;
	virtual UINT8 ReadMemory( ADDRESS, UINT8 ) override;

private:

	virtual ~cDiskDevice( ) override;

	// Disable the copy constructor and assignment operator defaults
	cDiskDevice( const cDiskDevice & ) = delete;		// no implementation
	void operator =( const cDiskDevice & ) = delete;	// no implementation

};

#endif
