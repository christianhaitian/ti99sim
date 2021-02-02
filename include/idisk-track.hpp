//----------------------------------------------------------------------------
//
// File:		idisk-track.hpp
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

#ifndef IDISK_TRACK_HPP_
#define IDISK_TRACK_HPP_

#include "idisk-sector.hpp"

namespace track
{

	enum class Format
	{
		Unknown,	// Unformatted
		FM,			// FM format
		MFM			// MFM format
	};

}

struct iDiskTrack
{
	virtual auto HasChanged( ) const -> bool = 0;

	virtual auto ClearChanged( ) -> void = 0;

	virtual auto GetFormat( ) const -> track::Format = 0;

	virtual auto Read( ) const -> sDataBuffer = 0;

	virtual auto Write( track::Format format, sDataBuffer data ) -> bool = 0;

	virtual auto RawWrite( track::Format format, std::vector<size_t> clock, sDataBuffer data ) -> bool = 0;

	virtual auto GetClockLocations( ) const -> std::vector<size_t> = 0;

	virtual auto GetSectors( ) const -> std::vector<iDiskSector*> = 0;

	virtual auto GetSector( int logicalCylinder, int logicalHead, int logicalSector ) -> iDiskSector * = 0;

	virtual auto GetSector( int logicalCylinder, int logicalHead, int logicalSector ) const -> const iDiskSector * = 0;

protected:

	iDiskTrack( ) = default;
	iDiskTrack( const iDiskTrack & ) = default;

	iDiskTrack &operator =( const iDiskTrack & ) = default;

	virtual ~iDiskTrack( ) {}

};

#endif
