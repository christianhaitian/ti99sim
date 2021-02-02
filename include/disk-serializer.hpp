//----------------------------------------------------------------------------
//
// File:        disk-serializer.hpp
// Date:        04-May-2015
// Programmer:  Marc Rousseau
//
// Description: A class to load/save disk images
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

#ifndef DISK_SERIALIZER_HPP_
#define DISK_SERIALIZER_HPP_

#include "cBaseObject.hpp"
#include "idisk-serializer.hpp"

class cDiskSerializer :
	public virtual cBaseObject,
	public virtual iDiskSerializer
{
protected:

	FILE *m_DemandLoadFile;

public:

	cDiskSerializer( );
	virtual ~cDiskSerializer( ) override;

	// iDiskSerializer methods
	virtual bool SupportsFeatures( const cDiskImage &image ) override;
	virtual eDiskFormat GetFormat( ) const override = 0;
	virtual size_t GetVolume( ) const override;
	virtual size_t MaxVolume( ) const override;
	virtual std::string RawFileName( const char *filename ) const override;
	virtual bool LoadFile( const char *filename, cDiskImage *image ) override;
	virtual bool SaveFile( const cDiskImage &image, const char *filename ) override;
	virtual bool LoadTrack( size_t cylinder, size_t head, iDiskTrack *track ) override;
	virtual void LoadComplete( ) override;

protected:

	virtual FILE *OpenForRead( const std::string &name );
	virtual FILE *OpenForWrite( const std::string &name );

	virtual bool ReadFile( FILE *file, cDiskImage *image ) = 0;
	virtual bool WriteFile( const cDiskImage &image, FILE *file ) = 0;
};

#endif
