//----------------------------------------------------------------------------
//
// File:		idisk-sector.hpp
// Date:		01-Jul-2015
// Programmer:	Marc Rousseau
//
// Description: Default class for the TMS5220 Speech Synthesizer Chip
//
// Copyright (c) 2011 Marc Rousseau, All Rights Reserved.
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

#ifndef IDISK_SECTOR_HPP_
#define IDISK_SECTOR_HPP_

#include "isector.hpp"

constexpr int MARK_DDAM	= 0xF8;		// Deleted Data Address Mark
constexpr int MARK_DAMx	= 0xF9;		// Data Address Mark (alternate)
constexpr int MARK_DAMy	= 0xFA;		// Data Address Mark (alternate)
constexpr int MARK_DAM	= 0xFB;		// Data Address Mark
constexpr int MARK_IAM	= 0xFC;		// Index Address Mark
constexpr int MARK_IDAM	= 0xFE;		// ID Address Mark

struct iDiskSector : iSector
{
	using iSector::Write;

	virtual auto Write( UINT8 dataMark, const sDataBuffer &data ) -> bool = 0;

	virtual auto LogicalCylinder( ) const -> int = 0;
	virtual auto LogicalHead( ) const -> int = 0;
	virtual auto LogicalSector( ) const -> int = 0;
	virtual auto LogicalSize( ) const -> int = 0;

	virtual auto GetID( ) const -> const UINT8 * = 0;
	virtual auto GetData( ) const -> const UINT8 * = 0;

	virtual auto IsDeletedData( ) const -> bool = 0;
	virtual auto DataMark( ) const -> UINT8 = 0;

	virtual auto ValidID( ) const -> bool = 0;
	virtual auto ValidData( ) const -> bool = 0;

	virtual auto Matches( int logicalCylinder, int logicalHead, int logicalSector ) const -> bool = 0;
};

#endif
