//----------------------------------------------------------------------------
//
// File:		fileio.hpp
// Date:		19-Sep-2000
// Programmer:	Marc Rousseau
//
// Description: This file contains startup code for Linux/SDL
//
// Copyright (c) 2000-2004 Marc Rousseau, All Rights Reserved.
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

#ifndef FILEIO_HPP_
#define FILEIO_HPP_

#include "file-system-disk.hpp"
#include "cBaseObject.hpp"

class cFile :
	public virtual cBaseObject
{
	friend class cFileSystem;

	cRefPtr<cFileSystem>            m_FileSystem;
	const sFileDescriptorRecord    *m_FDR;
	int                             m_TotalRecordsLeft;
	int                             m_RecordsLeft;
	int                             m_SectorIndex;
	UINT8                           m_SectorBuffer[ 256 ];
	UINT8                          *m_RecordPtr;

private:

	// Disable the copy constructor and assignment operator defaults
	cFile( const cFile & ) = delete;				// no implementation
	void operator =( const cFile & ) = delete;		// no implementation

protected:

	cFile( cFileSystem *, const sFileDescriptorRecord * );
	virtual ~cFile( ) override;

	bool ReadNextSector( );

public:

	static cRefPtr<cFile> Open( const std::string &filename, const std::string &path = "" );

	int FileSize( ) const;
	int RecordLength( ) const;
	int TotalSectors( ) const;
	bool IsProgram( ) const;
	bool IsDisplay( ) const;
	bool IsInternal( ) const;
	bool IsFixed( ) const;
	bool IsVariable( ) const;

	std::string GetPath( ) const;
	std::string GetName( ) const;

	const sFileDescriptorRecord *GetFDR( ) const;

	bool SeekRecord( int );
	int ReadRecord( void *, int );
	int WriteRecord( const void *, int );

	int ReadSector( int, void * );
	int WriteSector( int, const void * );

	std::string sha1( );

};

inline const sFileDescriptorRecord *cFile::GetFDR( ) const	{ return m_FDR; }
inline bool cFile::IsProgram( ) const						{ return cFileSystem::IsProgram( m_FDR ); }
inline bool cFile::IsDisplay( ) const						{ return cFileSystem::IsDisplay( m_FDR ); }
inline bool cFile::IsInternal( ) const						{ return cFileSystem::IsInternal( m_FDR ); }
inline bool cFile::IsFixed( ) const							{ return cFileSystem::IsFixed( m_FDR ); }
inline bool cFile::IsVariable( ) const						{ return cFileSystem::IsVariable( m_FDR ); }

#endif
