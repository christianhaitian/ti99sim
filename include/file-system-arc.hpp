//----------------------------------------------------------------------------
//
// File:		file-system-arc.hpp
// Date:		15-Sep-2003
// Programmer:	Marc Rousseau
//
// Description: A class to simulate a filesystem for an ARC file(Barry Boone's Archive format)
//
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

#ifndef ARCFS_HPP_
#define ARCFS_HPP_

#include "file-system.hpp"

class cPseudoFileSystem;
class cDecodeLZW;

class cArchiveFileSystem :
	public cFileSystem
{
	// 18-byte structure used to hold file descriptors in .ark files
	struct sArcFileDescriptorRecord
	{
		char                   FileName[ MAX_FILENAME ];
		UINT8                  FileStatus;
		UINT8                  RecordsPerSector;
		UINT16                 TotalSectors;
		UINT8                  EOF_Offset;
		UINT8                  RecordLength;
		UINT16                 NoFixedRecords;				 // For some strange reason, this is little-endian!
	};

	struct sArchiveSector;

	struct sFileInfo
	{
		sFileDescriptorRecord  fdr;
		sArchiveSector        *sector;
		UINT8                 *fileData;
		UINT32                 bytesUsed;
	};

	cRefPtr<cPseudoFileSystem> m_Container;
	sFileInfo                  m_Directory[ 128 ];
	int                        m_FileCount;
	int                        m_TotalSectors;

protected:

	cArchiveFileSystem( cPseudoFileSystem * );
	virtual ~cArchiveFileSystem( ) override;

	static bool IsValidDescriptor( const sArcFileDescriptorRecord * );
	static bool DirectoryCallback( void *, size_t, void * );
	static bool DataCallback( void *, size_t, void * );

	void LoadFile( );

	const sFileInfo *GetFileInfo( const sFileDescriptorRecord * ) const;

	// cFileSystem non-public methods
	virtual int FileCount( int ) const override;
	virtual const sFileDescriptorRecord * GetFileDescriptor( int, int ) const override;
	virtual iSector *GetFileSector( const sFileDescriptorRecord *fdr, int index ) override;
	virtual int ExtendFile( sFileDescriptorRecord *fdr, int count ) override;
	virtual bool TruncateFile( sFileDescriptorRecord *fdr, int limit ) override;
	virtual int DefaultRecordLength( ) override;

public:

	static cRefPtr<cArchiveFileSystem> Open( const std::string & );

	// cFileSystem public methods
	virtual std::string GetPath( ) const override;
	virtual std::string GetName( ) const override;
	virtual bool IsValid( ) const override;
	virtual bool IsCollection( ) const override;
	virtual cRefPtr<cFile> OpenFile( const char *, int ) override;
	virtual cRefPtr<cFile> CreateFile( const char *, UINT8, int, int ) override;
	virtual bool AddFile( cFile *, int ) override;
	virtual bool DeleteFile( const char *, int ) override;

	// cFileSystem listing helper functions
	virtual int FreeSectors( ) const override;
	virtual int TotalSectors( ) const override;
	virtual void ListingHeader( int, std::list<std::string> & ) const override;
	virtual void ListingData( cFile *, int, int, std::list<std::string> & ) const override;

private:

	cArchiveFileSystem( const cArchiveFileSystem & ) = delete; // no implementation
	void operator =( const cArchiveFileSystem & ) = delete;    // no implementation

};

#endif
