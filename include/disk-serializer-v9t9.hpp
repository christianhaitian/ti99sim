//----------------------------------------------------------------------------
//
// File:        disk-serializer-v9t9.cpp
// Date:        04-May-2015
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

#ifndef DISK_SERIALIZER_V9T9_HPP_
#define DISK_SERIALIZER_V9T9_HPP_

#include "disk-serializer.hpp"

class cDiskSerializerV9T9 :
	public cDiskSerializer
{
public:

	cDiskSerializerV9T9( );
	virtual ~cDiskSerializerV9T9( ) override;

	static bool MatchesFormat( FILE *file );

	// iDiskSerializer methods
	virtual bool SupportsFeatures( const cDiskImage &image ) override;
	virtual eDiskFormat GetFormat( ) const override;
	virtual bool ReadFile( FILE *file, cDiskImage *image ) override;
	virtual bool WriteFile( const cDiskImage &image, FILE *file ) override;
};

#endif
