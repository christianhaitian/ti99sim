//----------------------------------------------------------------------------
//
// File:        file-system-arc.cpp
// Date:        15-Sep-2003
// Programmer:  Marc Rousseau
//
// Description: A class to simulate a filesystem for an ARC file (Barry Boone's Archive format)
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
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307, USA.
//
// Revision History:
//
//----------------------------------------------------------------------------

#include <algorithm>
#include <cstring>
#include "common.hpp"
#include "logger.hpp"
#include "file-system-arc.hpp"
#include "file-system-pseudo.hpp"
#include "fileio.hpp"
#include "decode-lzw.hpp"

DBG_REGISTER( __FILE__ );

struct cArchiveFileSystem::sArchiveSector :
	public iSector
{
	sDataBuffer buffer;

	virtual auto GetData( ) const -> const UINT8 * override			{ return buffer.data( ); }

	virtual auto Size( ) const -> size_t override					{ return DEFAULT_SECTOR_SIZE; }

	virtual auto Read( ) const -> sDataBuffer override				{ return buffer; }

	virtual auto Write( const sDataBuffer &data ) -> bool override	{ return false; }
};

struct sDecodeInfo
{
	cArchiveFileSystem *FileSystem;
	cDecodeLZW         *pDecoder;
	int FileIndex;
};

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

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::Open
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cRefPtr<cArchiveFileSystem> cArchiveFileSystem::Open( const std::string &filename )
{
	FUNCTION_ENTRY( nullptr, "cArchiveFileSystem::Open", true );

	cRefPtr<cPseudoFileSystem> container = cPseudoFileSystem::Open( filename );

	if(( container != nullptr ) && container->IsValid( ))
	{
		cArchiveFileSystem *disk = new cArchiveFileSystem( container );

		if( disk->IsValid( ))
		{
			return disk;
		}

		delete disk;
	}

	return nullptr;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::cArchiveFileSystem
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cArchiveFileSystem::cArchiveFileSystem( cPseudoFileSystem *container ) :
	cBaseObject( "cArchiveFileSystem" ),
	m_Container( container ),
	m_FileCount( 0 ),
	m_TotalSectors( 0 )
{
	FUNCTION_ENTRY( this, "cArchiveFileSystem ctor", true );

	memset( m_Directory, 0, sizeof( m_Directory ));

	LoadFile( );
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::~cArchiveFileSystem
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cArchiveFileSystem::~cArchiveFileSystem( )
{
	FUNCTION_ENTRY( this, "cArchiveFileSystem dtor", true );

	for( int i = 0; i < m_FileCount; i++ )
	{
		delete m_Directory[ i ].sector;
		delete [] m_Directory[ i ].fileData;
	}
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::IsValidDescriptor
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cArchiveFileSystem::IsValidDescriptor( const sArcFileDescriptorRecord *arc )
{
	if( IsValidName( arc->FileName ) == false )
	{
		return false;
	}

	int totalSectors = GetUINT16( &arc->TotalSectors );
	int rps = ( arc->FileStatus & VARIABLE_TYPE ) ? 1 : arc->RecordsPerSector;

	if(( arc->RecordLength != 0 ) && ( 256 / arc->RecordLength != arc->RecordsPerSector ))
	{
		return false;
	}
	if(( arc->NoFixedRecords < ( totalSectors - 1 ) * rps ) || ( arc->NoFixedRecords > totalSectors * rps ))
	{
		return false;
	}

	return true;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::DirectoryCallback
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cArchiveFileSystem::DirectoryCallback( void *buffer, size_t size, void *token )
{
	FUNCTION_ENTRY( token, "cArchiveFileSystem::DirectoryCallback", false );

	sDecodeInfo *pData = reinterpret_cast<sDecodeInfo *>( token );

	cArchiveFileSystem *me = pData->FileSystem;

	sArcFileDescriptorRecord *arc = static_cast<sArcFileDescriptorRecord *>( buffer );

	// Populate the directory
	for( int i = 0; i < 14; i++ )
	{
		if( IsValidDescriptor( arc ) == false )
		{
			break;
		}

		sFileDescriptorRecord *fdr = &me->m_Directory[ me->m_FileCount++ ].fdr;

		// Copy the fields from the .ark directory to the FDR
		memcpy( fdr->FileName, arc->FileName, MAX_FILENAME );
		fdr->FileStatus        = arc->FileStatus;
		fdr->RecordsPerSector  = arc->RecordsPerSector;
		fdr->TotalSectors      = arc->TotalSectors;
		fdr->EOF_Offset        = arc->EOF_Offset;
		fdr->RecordLength      = arc->RecordLength;
		fdr->NoFixedRecords    = arc->NoFixedRecords;

		int totalSectors = GetUINT16( &fdr->TotalSectors );

		me->m_TotalSectors += totalSectors + 1;

		arc++;
	}

	// Look for the end of the directory and swith to the data callback
	if( memcmp(( char * ) buffer + 252, "END!", 4 ) == 0 )
	{
		// Set up the call back for the first file
		return DataCallback( buffer, size, token );
	}

	return ( me->m_FileCount > 0 );
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::DataCallback
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cArchiveFileSystem::DataCallback( void *buffer, size_t size, void *token )
{
	FUNCTION_ENTRY( token, "cArchiveFileSystem::DataCallback", false );

	sDecodeInfo *pData = reinterpret_cast<sDecodeInfo *>( token );

	cArchiveFileSystem *me = pData->FileSystem;

	// Get the FDR for the next file in line
	sFileInfo *info = &me->m_Directory[ pData->FileIndex++ ];

	unsigned int bytesLeft = pData->pDecoder->BytesLeft( );

	if( pData->FileIndex > 1 )
	{
		// Update bytesUsed to reflect the actual number used by the last file
		info[ -1 ].bytesUsed -= bytesLeft;

		// Allocate a fake sector for use by this file
		sArchiveSector *sector = new sArchiveSector;

		info[ -1 ].sector = sector;
	}

	// There's nothing more to do for the last call
	if( pData->FileIndex == me->m_FileCount + 1 )
	{
		return true;
	}

	int totalSectors = GetUINT16( &info->fdr.TotalSectors );

	// Allocate space for the file's data and store the pointer in a reserved portion of the FDR
	size   = DEFAULT_SECTOR_SIZE * totalSectors;
	buffer = new UINT8[ DEFAULT_SECTOR_SIZE * totalSectors ];

	info->fileData = ( UINT8 * ) buffer;
	info->bytesUsed = bytesLeft;

	pData->pDecoder->SetWriteCallback( DataCallback, buffer, size, token );

	return true;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::LoadFile
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
void cArchiveFileSystem::LoadFile( )
{
	FUNCTION_ENTRY( this, "cArchiveFileSystem::LoadFile", true );

	cRefPtr<cFile> file = m_Container->OpenFile( nullptr, -1 );
	if( file == nullptr )
	{
		return;
	}

	const sFileDescriptorRecord *fdr = file->GetFDR( );

	int recLen   = fdr->RecordLength;
	int recCount = GetUINT16_LE( &fdr->NoFixedRecords );
	int size     = recLen * recCount;

	// Handle files that aren't proper archive files (may randomly have 0 for recLen or recCount)
	if( size == 0 )
	{
		return;
	}

	UINT8 *inputBuffer = new UINT8[ size ];
	for( int i = 0; i < recCount; i++ )
	{
		file->ReadRecord( inputBuffer + (( long ) i * recLen ), recLen );
	}

	cDecodeLZW *pDecoder = new cDecodeLZW(( inputBuffer[ 0 ] == 0x80 ) ? 12 : 8 );
	char buffer[ 256 ];
	sDecodeInfo data =
	{
		this, pDecoder, 0
	};

	pDecoder->SetWriteCallback( DirectoryCallback, buffer, 256, &data );
	pDecoder->ParseBuffer( inputBuffer, size );
	delete pDecoder;

	// If there was a problem, zero out the size of the affected files
	for( int i = std::max( data.FileIndex - 1, 0 ); i < m_FileCount; i++ )
	{
		m_Directory[ i ].fdr.TotalSectors = 0;
	}

	delete [] inputBuffer;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::GetFileInfo
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
const cArchiveFileSystem::sFileInfo *cArchiveFileSystem::GetFileInfo( const sFileDescriptorRecord *fdr ) const
{
	FUNCTION_ENTRY( this, "cArchiveFileSystem::GetFileInfo", true );

	for( size_t i = 0; i < SIZE( m_Directory ); i++ )
	{
		if( fdr == &m_Directory[ i ].fdr )
		{
			return &m_Directory[ i ];
		}
	}

	return nullptr;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::FileCount
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cArchiveFileSystem::FileCount( int ) const
{
	FUNCTION_ENTRY( this, "cArchiveFileSystem::FileCount", true );

	return m_FileCount;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::GetFileDescriptor
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
const sFileDescriptorRecord *cArchiveFileSystem::GetFileDescriptor( int index, int ) const
{
	FUNCTION_ENTRY( this, "cArchiveFileSystem::GetFileDescriptor", true );

	DBG_ASSERT( index < m_FileCount );

	return &m_Directory[ index ].fdr;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::GetFileSector
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
iSector *cArchiveFileSystem::GetFileSector( const sFileDescriptorRecord *fdr, int index )
{
	FUNCTION_ENTRY( this, "cArchiveFileSystem::GetFileSector", true );

	const sFileInfo *info = GetFileInfo( fdr );

	sArchiveSector *sector = info->sector;

	UINT8 *fileBuffer = info->fileData + (( long ) index * DEFAULT_SECTOR_SIZE );

	sector->buffer = sDataBuffer{ fileBuffer, fileBuffer + DEFAULT_SECTOR_SIZE };

	return sector;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::ExtendFile
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cArchiveFileSystem::ExtendFile( sFileDescriptorRecord *, int )
{
	FUNCTION_ENTRY( this, "cArchiveFileSystem::ExtendFile", true );

	DBG_FATAL( "Function not implemented" );

	return -1;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::TruncateFile
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cArchiveFileSystem::TruncateFile( sFileDescriptorRecord *, int )
{
	FUNCTION_ENTRY( this, "cArchiveFileSystem::TruncateFile", true );

	DBG_FATAL( "Function not implemented" );

	return false;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::DefaultRecordLength
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cArchiveFileSystem::DefaultRecordLength( )
{
	FUNCTION_ENTRY( this, "cArchiveFileSystem::DefaultRecordLength", true );

	return DEFAULT_RECORD_LENGTH_DISK;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::GetPath
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
std::string cArchiveFileSystem::GetPath( ) const
{
	FUNCTION_ENTRY( this, "cArchiveFileSystem::GetPath", true );

	return m_Container->GetPath( );
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::GetName
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
std::string cArchiveFileSystem::GetName( ) const
{
	FUNCTION_ENTRY( this, "cArchiveFileSystem::GetName", true );

	return m_Container->GetName( );
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::IsValid
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cArchiveFileSystem::IsValid( ) const
{
	FUNCTION_ENTRY( this, "cArchiveFileSystem::IsValid", true );

	return ( m_FileCount > 0 ) ? true : false;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::IsCollection
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cArchiveFileSystem::IsCollection( ) const
{
	FUNCTION_ENTRY( this, "cArchiveFileSystem::IsCollection", true );

	return true;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::OpenFile
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cRefPtr<cFile> cArchiveFileSystem::OpenFile( const char *filename, int )
{
	FUNCTION_ENTRY( this, "cArchiveFileSystem::OpenFile", true );

	cRefPtr<cFile> file;

	if( filename != nullptr )
	{
		size_t len = strlen( filename );

		for( int i = 0; i < m_FileCount; i++ )
		{
			sFileDescriptorRecord *fdr = &m_Directory[ i ].fdr;
			if( strnicmp( fdr->FileName, filename, len ) == 0 )
			{
				// Watch out for bad/corrupt files
				if( fdr->TotalSectors > 0 )
				{
					file = cFileSystem::CreateFile( fdr );
				}
				break;
			}
		}
	}

	return file;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::CreateFile
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cRefPtr<cFile> cArchiveFileSystem::CreateFile( const char *, UINT8, int, int )
{
	FUNCTION_ENTRY( this, "cArchiveFileSystem::CreateFile", true );

	DBG_FATAL( "Function not supported" );

	return cRefPtr<cFile>( );
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::AddFile
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cArchiveFileSystem::AddFile( cFile *, int )
{
	FUNCTION_ENTRY( this, "cArchiveFileSystem::AddFile", true );

	DBG_FATAL( "Function not implemented" );

	return false;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::DeleteFile
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cArchiveFileSystem::DeleteFile( const char *, int )
{
	FUNCTION_ENTRY( this, "cArchiveFileSystem::DeleteFile", true );

	DBG_FATAL( "Function not implemented" );

	return false;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::FreeSectors
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cArchiveFileSystem::FreeSectors( ) const
{
	FUNCTION_ENTRY( this, "cArchiveFileSystem::FreeSectors", true );

	return 0;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::TotalSectors
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cArchiveFileSystem::TotalSectors( ) const
{
	FUNCTION_ENTRY( this, "cArchiveFileSystem::TotalSectors", true );

	return m_TotalSectors;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::ListingHeader
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
void cArchiveFileSystem::ListingHeader( int flags, std::list<std::string> &headers ) const
{
	FUNCTION_ENTRY( this, "cArchiveFileSystem::ListingHeader", false );

	cFileSystem::ListingHeader( flags, headers );

	if( flags & LISTING_FLAG_VERBOSE )
	{
		headers.push_back( " ST" );
		headers.push_back( "R/S" );
		headers.push_back( "#SEC" );
		headers.push_back( "EOF" );
		headers.push_back( " RL" );
		headers.push_back( "#FIX" );
		headers.push_back( "Ratio" );
	}
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::ListingData
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
void cArchiveFileSystem::ListingData( cFile *file, int dir, int flags, std::list<std::string> &data ) const
{
	FUNCTION_ENTRY( this, "cArchiveFileSystem::ListingData", false );

	cFileSystem::ListingData( file, dir, flags, data );

	if( flags & LISTING_FLAG_VERBOSE )
	{
		const sFileDescriptorRecord *fdr = file->GetFDR( );

		char buffer[ 16 ];

		sprintf( buffer, " %02X", fdr->FileStatus );
		data.push_back( buffer );

		sprintf( buffer, " %02X", fdr->RecordsPerSector );
		data.push_back( buffer );

		sprintf( buffer, "%04X",  GetUINT16( &fdr->TotalSectors ));
		data.push_back( buffer );

		sprintf( buffer, " %02X", fdr->EOF_Offset );
		data.push_back( buffer );

		sprintf( buffer, " %02X", fdr->RecordLength );
		data.push_back( buffer );

		sprintf( buffer, "%04X",  GetUINT16_LE( &fdr->NoFixedRecords ));
		data.push_back( buffer );

		const sFileInfo *info = GetFileInfo( fdr );

		// If we allocated an sSector for this file, it's valid
		if( info->sector != nullptr )
		{
			auto totalBytes = GetUINT16( &fdr->TotalSectors ) * DEFAULT_SECTOR_SIZE;

			auto ratio = 100.0 * info->bytesUsed / totalBytes;

			char buffer[ 32 ];
			sprintf( buffer, "%5.1f%% - %u/%u", ratio, info->bytesUsed, totalBytes );

			data.push_back( buffer );
		}
		else
		{
			data.push_back( " *BAD*" );
		}
	}
}
