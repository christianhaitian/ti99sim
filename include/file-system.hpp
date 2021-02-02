//----------------------------------------------------------------------------
//
// File:		file-system.hpp
// Date:		05-Sep-2003
// Programmer:	Marc Rousseau
//
// Description: A base class for TI filesystem classes
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
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
//
// Revision History:
//
//----------------------------------------------------------------------------

#ifndef FS_HPP_
#define FS_HPP_

#include <list>
#include <string>
#include "cBaseObject.hpp"

// Flags used by TI to indicate file types

#define DATA_TYPE						0x00
#define PROGRAM_TYPE					0x01

#define DISPLAY_TYPE					0x00
#define INTERNAL_TYPE					0x02

#define WRITE_PROTECTED_TYPE			0x08

#define FIXED_TYPE						0x00
#define VARIABLE_TYPE					0x80

#define DEFAULT_RECORD_LENGTH_DISK		80
#define DEFAULT_RECORD_LENGTH_CASSETTE	64

#define LISTING_FLAG_TIMESTAMPS			0x01
#define LISTING_FLAG_VERBOSE			0x02
#define LISTING_FLAG_SHA1				0x04

const int MAX_FILENAME					= 10;
const int MAX_CHAINS					= 76;
const int MAX_FILES						= 127;		// Maximum number of files that can fit on a disk(DEFAULT_SECTOR_SIZE/2-1)

// On-Disk structures used by TI

struct Directory
{
	char      Name[ MAX_FILENAME ];
	UINT16    FDIRecord;
};

struct VIB
{
	char      VolumeName[ MAX_FILENAME ];
	UINT16    FormattedSectors;
	UINT8     SectorsPerTrack;
	char      DSK[ 3 ];
	UINT8     reserved;
	UINT8     TracksPerSide;
	UINT8     Sides;
	UINT8     Density;
	union
	{
		UINT8     reserved2[ 36 ];
		Directory directory[ 3 ];
	};
	UINT8     AllocationMap[ 200 ];
};

static_assert( sizeof ( VIB ) == 256, "Volume Information Block size is incorrect" );

struct CHAIN
{
	UINT8     start;
	UINT8     start_offset;
	UINT8     offset;
};

static_assert( sizeof ( CHAIN ) == 3, "Allocation chain size is incorrect" );

struct sFileDescriptorRecord
{
	char      FileName[ MAX_FILENAME ];
	UINT8     reserved1[ 2 ];
	UINT8     FileStatus;
	UINT8     RecordsPerSector;
	UINT16    TotalSectors;
	UINT8     EOF_Offset;
	UINT8     RecordLength;
	UINT16    NoFixedRecords;				// For some strange reason, this is little-endian!
	UINT8     reserved2[ 8 ];
	CHAIN     DataChain[ MAX_CHAINS ];
};

static_assert( sizeof ( sFileDescriptorRecord ) == 256, "File Descriptor Record size is incorrect" );

struct iSector;

class cFile;

class cFileSystem :
	public virtual cBaseObject
{
public:

	static cRefPtr<cFileSystem> Open( const std::string & );

	static void GetCleanName( const sFileDescriptorRecord *fdr, char *name );
	static std::string EscapeName( std::string name );
	static std::string UnEscapeName( std::string name );

	static bool IsValidName( const char *name );
	static bool IsValidFDR( const sFileDescriptorRecord *fdr );

	static bool IsProgram( const sFileDescriptorRecord *fdr );
	static bool IsDisplay( const sFileDescriptorRecord *fdr );
	static bool IsInternal( const sFileDescriptorRecord *fdr );
	static bool IsFixed( const sFileDescriptorRecord *fdr );
	static bool IsVariable( const sFileDescriptorRecord *fdr );

	static void FormatTimestamp( const UINT8 *ptr, std::string &date, std::string &time );

	virtual bool CheckDisk( bool verbose ) const;
	virtual void GetFilenames( std::list<std::string>& names, int = -1 ) const;

	// Functions used by cFile
	virtual iSector *GetFileSector( const sFileDescriptorRecord *fdr, int index ) = 0;
	virtual int ExtendFile( sFileDescriptorRecord *fdr, int count ) = 0;
	virtual bool TruncateFile( sFileDescriptorRecord *fdr, int limit ) = 0;
	virtual int DefaultRecordLength( ) = 0;

	// Generic file system functions
	virtual std::string GetPath( ) const = 0;
	virtual std::string GetName( ) const = 0;
	virtual bool IsValid( ) const = 0;
	virtual bool IsCollection( ) const = 0;
	virtual cRefPtr<cFile> OpenFile( const char *, int = -1 ) = 0;
	virtual cRefPtr<cFile> CreateFile( const char *, UINT8, int, int = -1 ) = 0;
	virtual bool AddFile( cFile *, int = -1 ) = 0;
	virtual bool DeleteFile( const char *, int = -1 ) = 0;

	// Listing helper functions
	virtual int DirectoryCount( ) const;
	virtual const char *DirectoryName( int ) const;
	virtual int AllocationSize( ) const;
	virtual int FreeSectors( ) const = 0;
	virtual int TotalSectors( ) const = 0;
	virtual void ListingHeader( int, std::list<std::string> & ) const;
	virtual void ListingData( cFile *, int, int, std::list<std::string> & ) const;

protected:

	cFileSystem( ) : cBaseObject( "cFileSystem" ) {}
	cFileSystem( const char *name ) : cBaseObject( name ) {}
	~cFileSystem( ) {}

	cRefPtr<cFile> CreateFile( const sFileDescriptorRecord *fdr );

	virtual int FileCount( int ) const = 0;
	virtual const sFileDescriptorRecord * GetFileDescriptor( int, int ) const = 0;

private:

	// Disable the copy constructor and assignment operator defaults
	cFileSystem( const cFileSystem & ) = delete;		// no implementation
	void operator =( const cFileSystem & ) = delete;	// no implementation

};

#endif
