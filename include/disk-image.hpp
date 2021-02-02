//----------------------------------------------------------------------------
//
// File:		disk-image.hpp
// Date:		05-May-2015
// Programmer:	Marc Rousseau
//
// Description: A class to manipulate disk images
//
// Copyright (c) 2001-2004 Marc Rousseau, All Rights Reserved.
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

#ifndef DISK_IMAGE_HPP_
#define DISK_IMAGE_HPP_

#include <memory>
#include <vector>

#include "disk-track.hpp"
#include "idisk-serializer.hpp"

struct sDiskGapPair
{
	int                         count;
	int                         byte;
};

struct sDiskGap
{
	sDiskGapPair                filler;
	sDiskGapPair                sync;
	sDiskGapPair                clock;
};

struct sSectorInfo
{
	int                         LogicalCylinder;
	int                         LogicalHead;
	int                         LogicalSector;
	int                         Size;
	int                         DataMark;
};

class cDiskImage
{
	size_t                      NumTracks;
	size_t                      NumHeads;
	cRefPtr<iDiskSerializer>    Serializer;
	std::vector<
	  std::vector<cDiskTrack>>  Track;

public:

	static auto FormatTrack( track::Format format, size_t tIndex, size_t hIndex, size_t noSectors, size_t interleave ) -> sDataBuffer;
	static auto FormatTrack( track::Format format, const std::vector<sSectorInfo> &sectorInfo ) -> sDataBuffer;
	static auto FormatTrack( const sDiskGap gapInfo[ 5 ], const std::vector<sSectorInfo> &sectorInfo ) -> sDataBuffer;

	auto GetNumHeads( ) const -> size_t;

	auto GetNumTracks( ) const -> size_t;

	auto AllocateTracks( size_t, size_t ) -> bool;

	auto FormatDisk( size_t, size_t, size_t, track::Format ) -> bool;

	auto WriteTrack( size_t, size_t, track::Format, sDataBuffer ) -> bool;

	auto GetTrack( size_t, size_t ) -> iDiskTrack *;

	auto GetTrack( size_t, size_t ) const -> const iDiskTrack *;

	auto HasChanged( ) const -> bool;
	auto ClearChanged( ) -> void;

	auto SetLoadOnDemand( iDiskSerializer * ) -> void;

	auto CompleteLoad( ) const -> void;
};

#endif
