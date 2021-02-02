//----------------------------------------------------------------------------
//
// File:        fileio.cpp
// Date:        19-Sep-2000
// Programmer:  Marc Rousseau
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
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307, USA.
//
// Revision History:
//
//   15-Jan-2003    Moved old code to support.cpp and added new cFile classes
//
//----------------------------------------------------------------------------

#include <cstring>
#include "common.hpp"
#include "logger.hpp"
#include "fileio.hpp"
#include "support.hpp"

DBG_REGISTER( __FILE__ );

static inline UINT16 GetUINT16( const void *_ptr )
{
	FUNCTION_ENTRY( nullptr, "GetUINT16", true );

	const UINT8 *ptr = ( const UINT8 * ) _ptr;
	return ( UINT16 ) (( ptr[ 0 ] << 8 ) | ptr[ 1 ] );
}

static inline UINT16 GetUINT16_LE( const void *_ptr )
{
	FUNCTION_ENTRY( nullptr, "GetUINT16_LE", true );

	const UINT8 *ptr = ( const UINT8 * ) _ptr;
	return ( UINT16 ) (( ptr[ 1 ] << 8 ) | ptr[ 0 ] );
}

cRefPtr<cFile> cFile::Open( const std::string &filename, const std::string &path )
{
	FUNCTION_ENTRY( nullptr, "cFile::Open", true );

	std::string embeddedFileName;

	size_t sep = filename.find_last_of( ':' );
	if( sep != std::string::npos )
	{
		embeddedFileName = filename.substr( sep + 1 );
	}

	cRefPtr<cFileSystem> disk = cFileSystem::Open( filename );

	if(( disk == nullptr ) && ( sep != std::string::npos ))
	{
		disk = cFileSystem::Open( filename.substr( 0, sep ));
	}

	if( disk != nullptr )
	{
		return disk->OpenFile( embeddedFileName.c_str( ));
	}

	DBG_ERROR( "-- Function OpenFile is not implemented yet! --" );

	return nullptr;
}

cFile::cFile( cFileSystem *disk, const sFileDescriptorRecord *fdr ) :
	cBaseObject( "cFile" ),
	m_FileSystem( disk ),
	m_FDR( fdr ),
	m_TotalRecordsLeft( 0 ),
	m_RecordsLeft( 0 ),
	m_SectorIndex( -1 ),
	m_RecordPtr( nullptr )
{
	FUNCTION_ENTRY( this, "cFile ctor", true );

	DBG_ASSERT( disk != nullptr );
	DBG_ASSERT( fdr != nullptr );

	if( cFileSystem::IsValidFDR( fdr ))
	{
		m_TotalRecordsLeft = IsProgram( ) ? TotalSectors( ) : GetUINT16_LE( &m_FDR->NoFixedRecords );

		int recordsPerSector = m_FDR->RecordsPerSector ? m_FDR->RecordsPerSector : 1;

		if( m_TotalRecordsLeft > TotalSectors( ) * recordsPerSector )
		{
			m_TotalRecordsLeft = GetUINT16( &m_FDR->NoFixedRecords );
		}
	}

	memset( m_SectorBuffer, 0, sizeof( m_SectorBuffer ));
}

cFile::~cFile( )
{
	FUNCTION_ENTRY( this, "cFile dtor", true );
}

bool cFile::ReadNextSector( )
{
	FUNCTION_ENTRY( this, "cFile::ReadNextSector", true );

	int totalSectors = TotalSectors( );

	if( m_SectorIndex >= totalSectors - 1 )
	{
		return false;
	}

	if( ReadSector( ++m_SectorIndex, m_SectorBuffer ) < 0 )
	{
		return false;
	}

	m_RecordPtr   = m_SectorBuffer;
	m_RecordsLeft = m_FDR->RecordsPerSector;

	return true;
}

int cFile::FileSize( ) const
{
	FUNCTION_ENTRY( this, "cFile::FileSize", true );

	const sFileDescriptorRecord *fdr = GetFDR( );

	int totalSectors = TotalSectors( );
	int fileSize     = ( totalSectors - 1 ) * DEFAULT_SECTOR_SIZE;
	fileSize        += ( fdr->EOF_Offset != 0 ) ? fdr->EOF_Offset : DEFAULT_SECTOR_SIZE;

	return fileSize;
}

int cFile::RecordLength( ) const
{
	FUNCTION_ENTRY( this, "cFile::RecordLength", true );

	const sFileDescriptorRecord *fdr = GetFDR( );

	return fdr->RecordLength ? fdr->RecordLength : m_FileSystem->DefaultRecordLength( );
}

int cFile::TotalSectors( ) const
{
	FUNCTION_ENTRY( this, "cFile::TotalSectors", true );

	const sFileDescriptorRecord *fdr = GetFDR( );

	return GetUINT16( &fdr->TotalSectors );
}

std::string cFile::GetPath( ) const
{
	FUNCTION_ENTRY( this, "cFile::GetPath", true );

	// Get the base path from the underlying filesystem
	std::string path = m_FileSystem->GetPath( );

	if( !path.empty( ))
	{
		// Do we need to add the filename?
		if( m_FileSystem->IsCollection( ) == true )
		{
			return path + ":" + GetName( );
		}
	}

	return path;
}

std::string cFile::GetName( ) const
{
	FUNCTION_ENTRY( this, "cFile::GetName", true );

	size_t length = MAX_FILENAME;

	while(( length > 0 ) && ( m_FDR->FileName[ length - 1 ] == ' ' ))
	{
		length--;
	}

	return std::string( m_FDR->FileName, length );
}

bool cFile::SeekRecord( int index )
{
	FUNCTION_ENTRY( this, "cFile::SeekRecord", true );

	if( ! IsFixed( ))
	{
		return false;
	}

	if( index >= GetUINT16_LE( &m_FDR->NoFixedRecords ))
	{
		return false;
	}

	int sector = index / m_FDR->RecordsPerSector;
	if( ReadSector( sector, m_SectorBuffer ) < 0 )
	{
		return false;
	}

	int record = index - sector * m_FDR->RecordsPerSector;
	m_RecordPtr   = m_SectorBuffer + record * RecordLength( );
	m_RecordsLeft = m_FDR->RecordsPerSector - record;

	m_TotalRecordsLeft = GetUINT16_LE( &m_FDR->NoFixedRecords ) - index;

	return true;
}

int cFile::ReadRecord( void *buffer, int maxLen )
{
	FUNCTION_ENTRY( this, "cFile::ReadRecord", true );

	if( m_TotalRecordsLeft == 0 )
	{
		return -1;
	}

	if( m_RecordsLeft == 0 )
	{
		if( ReadNextSector( ) == false )
		{
			m_TotalRecordsLeft = 0;
			return -2;
		}
	}

	int count = 0;

	if( ! IsProgram( ))
	{
		int length = 0;

		if( IsFixed( ))
		{
			length = RecordLength( );
			m_TotalRecordsLeft--;
			m_RecordsLeft--;
		}
		else
		{
			length = *m_RecordPtr++;
			if(( length > RecordLength( )) || ( m_RecordPtr + length >= m_SectorBuffer + 256 ))
			{
				m_TotalRecordsLeft = 0;
				return -3;
			}
			if( m_RecordPtr[ length ] == 0xFF )
			{
				m_TotalRecordsLeft--;
				m_RecordsLeft = 0;
			}
		}

		count = ( length < maxLen ) ? length : maxLen;
		memcpy( buffer, m_RecordPtr, count );
		m_RecordPtr += length;
	}
	else
	{
		count = (( m_TotalRecordsLeft == 1 ) && ( m_FDR->EOF_Offset != 0 )) ? m_FDR->EOF_Offset : DEFAULT_SECTOR_SIZE;
		count = ( count < maxLen) ? count : maxLen;
		memcpy( buffer, m_RecordPtr, count );
		m_TotalRecordsLeft--;
		m_RecordsLeft = 0;
	}

	return count;
}

int cFile::WriteRecord( const void *, int )
{
	FUNCTION_ENTRY( this, "cFile::WriteRecord", true );

	// TBD

	return 0;
}

int cFile::ReadSector( int index, void *buffer )
{
	FUNCTION_ENTRY( this, "cFile::ReadSector", true );

	if( index >= TotalSectors( ))
	{
		return -1;
	}

	const iSector *sector = m_FileSystem->GetFileSector( m_FDR, index );
	if( sector == nullptr )
	{
		return -2;
	}

	memcpy( buffer, sector->Read( ).data( ), DEFAULT_SECTOR_SIZE );

	return 0;
}

int cFile::WriteSector( int index, const void *buffer )
{
	FUNCTION_ENTRY( this, "cFile::WriteSector", true );

	int totalSectors = TotalSectors( );
	if( index >= totalSectors )
	{
		int count = ( index - totalSectors ) + 1;
		// TODO - Fix this!
		if( m_FileSystem->ExtendFile( const_cast<sFileDescriptorRecord*>( m_FDR ), count ) != count )
		{
			return -1;
		}
	}

	iSector *sector = m_FileSystem->GetFileSector( m_FDR, index );
	if( sector == nullptr )
	{
		return -2;
	}

	sector->Write( { (const UINT8*) buffer, (const UINT8*) buffer + DEFAULT_SECTOR_SIZE } );

	return 0;
}

std::string cFile::sha1( )
{
	SHA1Context context;

	int count;
	do
	{
		char data[ DEFAULT_SECTOR_SIZE ];
		count = ReadRecord( data, DEFAULT_SECTOR_SIZE );
		if( count <= -2 )
		{
			return SHA1Context( ).Digest( );
		}
		if( count >= 0 )
		{
			if( IsVariable( ))
			{
				UINT8 size = ( UINT8 ) count;
				context.Update( &size, 1 );
			}
			context.Update( data, count );
		}
	}
	while( count >= 0 );

	return context.Digest( );
}
