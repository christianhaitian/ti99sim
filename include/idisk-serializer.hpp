//----------------------------------------------------------------------------
//
// File:        idisk-serializer.hpp
// Date:        04-May-2015
// Programmer:  Marc Rousseau
//
// Description: An interface for loading/saving disk images
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

#ifndef IDISK_SERIALIZER_HPP_
#define IDISK_SERIALIZER_HPP_

#include "iBaseObject.hpp"

enum eDiskFormat
{
	FORMAT_UNKNOWN,			// Image format cannot be detected
	FORMAT_RAW_TRACK,		// Image is an array of tracks - PC99
	FORMAT_RAW_SECTOR,		// Image is an array of sectors - v9t9
	FORMAT_ANADISK,			// Image is an array of headers+sectors
	FORMAT_CF7,				// Image is a Compact Flash disk from CF7+/nanoPEB
	FORMAT_HFE,				// Image is a HxC Floppy Emulator disk (HFE)
	FORMAT_MAX
};

struct iDiskTrack;

class cDiskImage;

struct iDiskSerializer : virtual iBaseObject
{
	virtual ~iDiskSerializer( ) {}

	virtual auto SupportsFeatures( const cDiskImage &image ) -> bool = 0;

	virtual auto GetFormat( ) const-> eDiskFormat = 0;

	virtual auto GetVolume( ) const -> size_t = 0;
	virtual auto MaxVolume( ) const -> size_t = 0;

	virtual auto RawFileName( const char *filename ) const ->std::string = 0;

	virtual auto LoadFile( const char *filename, cDiskImage *image ) -> bool = 0;
	virtual auto SaveFile( const cDiskImage &image, const char *filename ) -> bool = 0;

	virtual auto LoadTrack( size_t cylinder, size_t head, iDiskTrack *track ) -> bool = 0;
	virtual auto LoadComplete( ) -> void = 0;
};

#endif
