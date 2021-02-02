//----------------------------------------------------------------------------
//
// File:        disk-serializer-pc99.cpp
// Date:        06-July-2015
// Programmer:  Marc Rousseau
//
// Description: A class to load/save AnaDisk disk images
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

#ifndef DISK_SERIALIZER_PC99_HPP_
#define DISK_SERIALIZER_PC99_HPP_

#include "idisk-sector.hpp"
#include "idisk-track.hpp"
#include "disk-serializer.hpp"

class cDiskSerializerPC99 :
	public cDiskSerializer
{
	struct sTrackInfo
	{
		std::vector<size_t> clock;
		sDataBuffer         data;
	};

public:

	cDiskSerializerPC99( );
	virtual ~cDiskSerializerPC99( ) override;

	static bool MatchesFormat( FILE *file );

	// iDiskSerializer methods
	virtual bool SupportsFeatures( const cDiskImage &image ) override;
	virtual eDiskFormat GetFormat( ) const override;
	virtual bool ReadFile( FILE *file, cDiskImage *image ) override;
	virtual bool WriteFile( const cDiskImage &image, FILE *file ) override;

	static void fuzz( const uint8_t *data, size_t size );

protected:

	static void AddClockLocations( sTrackInfo &track, track::Format format, size_t offset );
	static const UINT8 *FindAddressMark( UINT8 mask, UINT8 mark, track::Format format, const UINT8 *ptr, const UINT8 *max );
	static sTrackInfo FindTrack( track::Format format, const UINT8 *start, const UINT8 *max );
	static std::pair<size_t, size_t> DetermineSize( const std::vector<sTrackInfo> &trackData,track::Format format );
	static std::vector<sTrackInfo> ReadTrackData( FILE *file, track::Format format, size_t maxTrackSize );

};

#endif
