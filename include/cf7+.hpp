
//----------------------------------------------------------------------------
//
// File:		cf7+.hpp
// Date:		27-Jun-2013
// Programmer:	Marc Rousseau
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
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
//
// Revision History:
//
//----------------------------------------------------------------------------

#ifndef CF7P_HPP_
#define CF7P_HPP_

#include "device.hpp"

class cCF7 :
	public virtual cStateObject,
	public virtual cDevice
{
	enum CMD_STATE_E
	{
		CMD_NONE,
		CMD_IDENTIFY_DRIVE,
		CMD_READ_SECTOR,
		CMD_WRITE_SECTOR
	};

	enum XFER_MODE_E
	{
		XFER_MODE_NONE,
		XFER_MODE_8BIT,
		XFER_MODE_16BIT
	};

	// Hardware registers
	UINT32              m_LBA;
	UINT32              m_SectorCount;
	UINT8               m_Feature;
	UINT8               m_StatusRegister;
	UINT8               m_ErrorRegister;

	XFER_MODE_E         m_TransferMode;
	size_t              m_BytesTransferred;
	UINT8               m_DataBuffer[ 512 ];

	FILE               *m_File;
	std::string         m_FileName;
	UINT32              m_FileSectors;

	CMD_STATE_E         m_CmdInProgress;

public:

	static std::string  DiskImage;

public:

	cCF7( iCartridge * );

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

	void LoadDisk( const char * );
	void UnLoadDisk( );

private:

	void CompleteCommand( );

	UINT8 ReadByte( );
	void  WriteByte( UINT8 );

	void ReadSector( UINT8 );
	void WriteSector( UINT8 );
	void VerifySector( UINT8 );
	void Diagnostic( UINT8 );
	void IdentifyDrive( UINT8 );
	void SetFeature( UINT8 );

	void HandleCommand( UINT8 );

	// cDevice Methods
	virtual void ActivateInternal( ) override;
	virtual UINT8 WriteMemory( ADDRESS, UINT8 ) override;
	virtual UINT8 ReadMemory( ADDRESS, UINT8 ) override;

private:

	virtual ~cCF7( ) override;

	// Disable the copy constructor and assignment operator defaults
	cCF7( const cCF7 & ) = delete;		// no implementation
	void operator =( const cCF7 & ) = delete;	// no implementation

};

#endif
