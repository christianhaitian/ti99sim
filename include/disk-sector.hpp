//----------------------------------------------------------------------------
//
// File:		disk-sector.hpp
// Date:		01-Jul-2015
// Programmer:	Marc Rousseau
//
// Description: A class to manipulate disk sectors
//
// Copyright (c) 2015 Marc Rousseau, All Rights Reserved.
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

#ifndef DISK_SECTOR_HPP_
#define DISK_SECTOR_HPP_

#include "idisk-sector.hpp"

class cDiskTrack;

class cDiskSector :
	public iDiskSector
{
	cDiskTrack             *Track;
	UINT8                  *ID;
	UINT8                  *Data;

public:

	cDiskSector( cDiskTrack *track, UINT8 *id, UINT8 *data );

	virtual auto Size( ) const -> size_t override				{ return 128u << ( ID[ 4 ] & 0x03 ); }

	virtual auto Read( ) const -> sDataBuffer override;

	virtual auto Write( const sDataBuffer &data ) -> bool override;
	virtual auto Write( UINT8 dataMark, const sDataBuffer &data ) -> bool override;

	virtual auto LogicalCylinder( ) const -> int override		{ return ID[ 1 ]; }
	virtual auto LogicalHead( ) const -> int override			{ return ID[ 2 ]; }
	virtual auto LogicalSector( ) const -> int override			{ return ID[ 3 ]; }
	virtual auto LogicalSize( ) const -> int override			{ return ID[ 4 ]; }

	virtual auto GetID( ) const -> const UINT8 * override		{ return ID ? ID + 1 : nullptr; }
	virtual auto GetData( ) const -> const UINT8 * override		{ return Data ? Data + 1 : nullptr; }

	virtual auto IsDeletedData( ) const -> bool override		{ return Data ? Data[ 0 ] == MARK_DDAM : false; }
	virtual auto DataMark( ) const -> UINT8 override			{ return Data ? Data[ 0 ] : MARK_DAM; }

	virtual auto ValidID( ) const -> bool override;
	virtual auto ValidData( ) const -> bool override;

	virtual auto Matches( int logicalCylinder, int logicalHead, int logicalSector ) const -> bool override;

};

#endif
