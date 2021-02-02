//----------------------------------------------------------------------------
//
// File:		disk-track.hpp
// Date:		01-Jul-2015
// Programmer:	Marc Rousseau
//
// Description: A class to manipulate disk tracks
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

#ifndef DISK_TRACK_HPP_
#define DISK_TRACK_HPP_

#include <vector>

#include "idisk-track.hpp"
#include "disk-sector.hpp"

class cDiskTrack :
	public iDiskTrack
{
	bool                        Dirty;
	track::Format               Format;						// Encoding used for this track
	std::vector<size_t>         Clock;						// List of all clock patterns on the track
	sDataBuffer                 Data;						// Byte aligned image of complete track
	std::vector<cDiskSector>    Sector;						// List of all sectors on the track

public:

	cDiskTrack( );

	virtual auto HasChanged( ) const -> bool override;

	virtual auto ClearChanged( ) -> void override;

	virtual auto GetFormat( ) const -> track::Format override;

	virtual auto Read( ) const -> sDataBuffer override;

	virtual auto Write( track::Format format, sDataBuffer data ) -> bool override;

	virtual auto RawWrite( track::Format format, std::vector<size_t> clock, sDataBuffer data ) -> bool override;

	virtual auto GetClockLocations( ) const -> std::vector<size_t> override;

	virtual auto GetSectors( ) const -> std::vector<iDiskSector*> override;

	virtual auto GetSector( int logicalCylinder, int logicalHead, int logicalSector ) -> iDiskSector * override;

	virtual auto GetSector( int logicalCylinder, int logicalHead, int logicalSector ) const -> const iDiskSector * override;

	auto Erase( ) -> void;
	auto IsEmpty( ) const -> bool;

	auto VerifyID( const UINT8 *id ) const -> bool;
	auto VerifyData( const UINT8 *data, size_t size ) const -> bool;
	auto DataModified( const UINT8 *data, size_t size ) -> void;

protected:

	auto LocateSectors( ) -> void;

};

#endif
