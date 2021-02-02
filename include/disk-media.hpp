//----------------------------------------------------------------------------
//
// File:		disk-media.hpp
// Date:		14-Jul-2001
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

#ifndef DISK_MEDIA_HPP_
#define DISK_MEDIA_HPP_

#include "cBaseObject.hpp"
#include "disk-image.hpp"
#include "idisk-serializer.hpp"

#define MAX_SECTORS				36
#define MAX_TRACKS				80
#define MAX_TRACKS_LO			40
#define MAX_TRACKS_HI			80

#define TRACK_SIZE_FM			3236
#define TRACK_SIZE_MFM			6450

#define MAX_TRACK_SIZE			15000

class cDiskMedia :
	public virtual cBaseObject
{
	bool                        m_IsWriteProtected;
	std::string                 m_FileName;
	cRefPtr<iDiskSerializer>    m_Serializer;

	std::unique_ptr<cDiskImage> m_Image;

public:

	cDiskMedia( cDiskImage * );
	cDiskMedia( std::nullptr_t );
	cDiskMedia( const char *, eDiskFormat );

	bool HasChanged( ) const		{ return m_Image->HasChanged( ); }
	bool IsWriteProtected( ) const	{ return m_IsWriteProtected; }

	eDiskFormat GetFormat( ) const;
	int GetVolume( ) const;
	int MaxVolume( ) const;

	int NumTracks( ) const			{ return m_Image->GetNumTracks( ); }
	int NumSides( ) const			{ return m_Image->GetNumHeads( ); }

	const char *GetName( ) const	{ return m_FileName.c_str( ); }

	void ClearDisk( );

	bool LoadFile( const char *, eDiskFormat );
	bool SaveFile( bool = false );
	bool SaveFileAs( const char *, eDiskFormat );

	iDiskTrack *GetTrack( size_t, size_t );
	iSector *GetSector( int, int, int );

protected:

	virtual ~cDiskMedia( ) override;

	static iDiskSerializer *CreateSerializer( eDiskFormat );
	static iDiskSerializer *FindSerializer( const char * );

	void LoadFile( );

private:

	cDiskMedia( const cDiskMedia & ) = delete;		// no implementation
	void operator =( const cDiskMedia & ) = delete;	// no implementation

};

#endif
