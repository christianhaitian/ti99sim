//----------------------------------------------------------------------------
//
// File:		file-system-disk.hpp
// Date:		15-Jan-2003
// Programmer:	Marc Rousseau
//
// Description: A class to manage the filesystem information on a TI disk.

// Copyright (c) 2003-2004 Marc Rousseau, All Rights Reserved.
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

#ifndef DISKFS_HPP_
#define DISKFS_HPP_

#include "file-system.hpp"
#include "disk-image.hpp"
#include "idisk-serializer.hpp"

class cDiskMedia;

class cDiskFileSystem :
	public cFileSystem
{
	cRefPtr<cDiskMedia>     m_Media;

	sDataBuffer             m_VIBdata;
	VIB                    *m_VIB;

protected:

	cDiskFileSystem( const char * );
	virtual ~cDiskFileSystem( ) override;

	int FindFreeSector( int start = 0 ) const;
	void SetSectorAllocation( int index, bool bUsed );

	int FindLastSector( const sFileDescriptorRecord *fdr ) const;
	bool AddFileSector( sFileDescriptorRecord *fdr, int );

	int FindFileDescriptorIndex( const char *name, int dir );
	int GetFileDescriptorIndex( const sFileDescriptorRecord *fdr, int dir ) const;

	int AddFileDescriptor( const sFileDescriptorRecord *fdr, int dir );

	int GetDirSector( int ) const;
	int GetDirIndex( const char * ) const;

	// cFileSystem non-public methods
	virtual int DirectoryCount( ) const override;
	virtual const char *DirectoryName( int ) const override;
	virtual int FileCount( int ) const override;
	virtual const sFileDescriptorRecord * GetFileDescriptor( int, int ) const override;
	virtual iSector *GetFileSector( const sFileDescriptorRecord *fdr, int index ) override;
	virtual int ExtendFile( sFileDescriptorRecord *fdr, int count ) override;
	virtual bool TruncateFile( sFileDescriptorRecord *fdr, int limit ) override;
	virtual int DefaultRecordLength( ) override;

public:

	cDiskFileSystem( cDiskMedia * );

	static cRefPtr<cDiskFileSystem> Open( const std::string &, eDiskFormat );

	cRefPtr<cDiskMedia> GetMedia( ) const;

	const iSector *FindSector( int index ) const;
	iSector *FindSector( int index );

	// cFileSystem public methods
	virtual bool CheckDisk( bool ) const override;
	virtual std::string GetPath( ) const override;
	virtual std::string GetName( ) const override;
	virtual bool IsValid( ) const override;
	virtual bool IsCollection( ) const override;
	virtual cRefPtr<cFile> OpenFile( const char *, int ) override;
	virtual cRefPtr<cFile> CreateFile( const char *, UINT8, int, int ) override;
	virtual bool AddFile( cFile *, int ) override;
	virtual bool DeleteFile( const char *, int ) override;

	// cFileSystem listing helper functions
	virtual int AllocationSize( ) const override;
	virtual int FreeSectors( ) const override;
	virtual int TotalSectors( ) const override;
	virtual void ListingHeader( int, std::list<std::string> & ) const override;
	virtual void ListingData( cFile *, int, int, std::list<std::string> & ) const override;

private:

	cDiskFileSystem( const cDiskFileSystem & ) = delete; // no implementation
	void operator =( const cDiskFileSystem & ) = delete; // no implementation

};

#endif
