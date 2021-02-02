//----------------------------------------------------------------------------
//
// File:        file-system-pseudo.cpp
// Date:        05-Sep-2003
// Programmer:  Marc Rousseau
//
// Description: A class to simulate a TI disk filesystem for TIFILES & FIAD files
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

#include <cstring>
#include "common.hpp"
#include "logger.hpp"
#include "support.hpp"
#include "file-system-pseudo.hpp"
#include "fileio.hpp"

DBG_REGISTER( __FILE__ );

struct cPseudoFileSystem::sPseudoSector :
	public iSector
{
	sDataBuffer buffer;

	virtual auto GetData( ) const -> const UINT8 * override			{ return buffer.data( ); }

	virtual auto Size( ) const -> size_t override					{ return DEFAULT_SECTOR_SIZE; }

	virtual auto Read( ) const -> sDataBuffer override				{ return buffer; }

	virtual auto Write( const sDataBuffer &data ) -> bool override	{ return false; }
};

static inline void SetUINT16( void *_ptr, UINT16 value )
{
	FUNCTION_ENTRY( nullptr, "SetUINT16", true );

	UINT8 *ptr = ( UINT8 * ) _ptr;

	ptr[ 0 ] = value >> 8;
	ptr[ 1 ] = value & 0x0FF;
}

static inline void SetUINT16_LE( void *_ptr, UINT16 value )
{
	FUNCTION_ENTRY( nullptr, "SetUINT16_LE", true );

	UINT8 *ptr = ( UINT8 * ) _ptr;

	ptr[ 0 ] = value & 0x0FF;
	ptr[ 1 ] = value >> 8;
}

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
// Procedure:   cPseudoFileSystem::Open
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cRefPtr<cPseudoFileSystem> cPseudoFileSystem::Open( const std::string &filename )
{
	FUNCTION_ENTRY( nullptr, "cPseudoFileSystem::Open", true );

	std::string actualFileName = LocateFile( "disks", filename );

	if( actualFileName.empty( ) == false )
	{
		cRefPtr<cPseudoFileSystem> file = new cPseudoFileSystem( actualFileName.c_str( ));
		if( file->IsValid( ))
		{
			return file;
		}
	}

	return nullptr;
}

//------------------------------------------------------------------------------
// Procedure:   cPseudoFileSystem::cPseudoFileSystem
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cPseudoFileSystem::cPseudoFileSystem( const char *filename ) :
	cBaseObject( "cPseudoFileSystem" ),
	m_PathName( filename ),
	m_FileName( m_PathName.filename( )),
	m_FileBuffer( nullptr ),
	m_File( nullptr ),
	m_FDR( ),
	m_CurrentSector( nullptr )
{
	FUNCTION_ENTRY( this, "cPseudoFileSystem ctor", true );

	m_CurrentSector = new sPseudoSector;

	m_File = fopen( filename, "rb" );

	if( m_File != nullptr )
	{
		if((( FindHeader( ) == false ) || ( LoadFileBuffer( ) == false )) &&
		   (( ConstructHeader( ) == false ) || ( ConstructFileBuffer( ) == false )))
		{
			fclose( m_File );
			m_File = nullptr;
		}
	}
}

//------------------------------------------------------------------------------
// Procedure:   cPseudoFileSystem::~cPseudoFileSystem
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cPseudoFileSystem::~cPseudoFileSystem( )
{
	FUNCTION_ENTRY( this, "cPseudoFileSystem dtor", true );

	if( m_File != nullptr )
	{
		fclose( m_File );
		m_File = nullptr;
	}

	delete [] m_FileBuffer;
	delete m_CurrentSector;
}

//------------------------------------------------------------------------------
// Procedure:   cPseudoFileSystem::FindHeader
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cPseudoFileSystem::FindHeader( )
{
	FUNCTION_ENTRY( this, "cPseudoFileSystem::FindHeader", true );

	// See if there is either a TIFILES header or a file descriptor record (FIAD)
	char buffer[ 128 ];
	fseek( m_File, 0L, SEEK_SET );
	if( fread( buffer, 1, sizeof( buffer ), m_File ) != sizeof( buffer ))
	{
		DBG_ERROR( "Unable to read header from file " << m_FileName.c_str( ));
		return false;
	}

	// Look for TIFILES (and all it's variants)
	if( ConstructFDR_TIFILES( buffer ) == false )
	{
		// See if we can detect the FIAD header
		return ConstructFDR_FIAD( buffer );
	}

	return true;
}

//------------------------------------------------------------------------------
// Procedure:   cPseudoFileSystem::ConstructHeader
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------

bool cPseudoFileSystem::ConstructHeader( )
{
	FUNCTION_ENTRY( this, "cPseudoFileSystem::ConstructHeader", true );

	auto name = cFileSystem::UnEscapeName( m_FileName.stem( ));

	for( size_t i = 0; i < MAX_FILENAME; i++ )
	{
		m_FDR.FileName[ i ] = ( i < name.length( )) ? name[ i ] : ' ';
	}

	// Look for extensions matching the decorated name used by the disk utility
	std::string type = m_FileName.extension( );

	// Look for PROGRAM files
	if(( type.compare( ".PROG" ) == 0 ) || ( type.compare( ".prog" ) == 0 ))
	{
		m_FDR.FileStatus = PROGRAM_TYPE;

		return true;
	}

	char display = 0;
	char fixed = 0;
	int recordLength = 0;
	char filler = 0;

	if(( type.find( ' ' ) == std::string::npos ) && // Make sure there is no space before the number
	   ( sscanf( type.c_str( ), "%c%c%3d%c", &display, &fixed, &recordLength, &filler ) == 3 ))
	{
		if((( toupper( display ) == 'D' ) || ( toupper( display ) == 'I' )) &&
		   (( toupper( fixed ) == 'V' ) || ( toupper( fixed ) == 'F' )) &&
		   (( recordLength > 0 ) && ( recordLength <= 256 )))
		{
			if( toupper( display ) == 'I' )
			{
				m_FDR.FileStatus |= INTERNAL_TYPE;
			}
			if( toupper( fixed ) == 'V' )
			{
				m_FDR.FileStatus |= VARIABLE_TYPE;
			}
			m_FDR.RecordLength = ( recordLength == 256 ) ? 0 : recordLength;

			return true;
		}

		return false;
	}

	// Treat anything else as a PROGRAM file
	m_FDR.FileStatus = PROGRAM_TYPE;

	return true;
}

//------------------------------------------------------------------------------
// Procedure:   cPseudoFileSystem::ConstructFDR_TIFILES
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cPseudoFileSystem::ConstructFDR_TIFILES( const char *buffer )
{
	FUNCTION_ENTRY( this, "cPseudoFileSystem::ConstructFDR_TIFILES", true );

	const sTIFILES_Header *hdr = reinterpret_cast<const sTIFILES_Header *>( buffer );

	if(( hdr->length != 7 ) || ( strncmp( hdr->Name, "TIFILES", 7 ) != 0 ))
	{
		return false;
	}

	m_FDR.FileStatus       = hdr->Status;
	m_FDR.RecordsPerSector = hdr->RecordsPerSector;
	m_FDR.TotalSectors     = hdr->SectorCount;
	m_FDR.EOF_Offset       = hdr->EOF_Offset;
	m_FDR.RecordLength     = hdr->RecordSize;
	m_FDR.NoFixedRecords   = hdr->RecordCount;

	// Do a quick sanity check (there seem to be two different ways to store RecordCount)
	int totalSectors = GetUINT16( &m_FDR.TotalSectors );
	int recCount = GetUINT16_LE( &m_FDR.NoFixedRecords );
	int rps = cFileSystem::IsVariable( &m_FDR ) ? 1 : m_FDR.RecordsPerSector;

	// Make sure things match up - if not, swap bytes
	if(( recCount < ( totalSectors - 1 ) * rps ) || ( recCount > totalSectors * rps ))
	{
		SetUINT16( &m_FDR.NoFixedRecords, recCount );
		recCount = GetUINT16_LE( &m_FDR.NoFixedRecords );
		// If the byte swap didn't correct the problem then this probably isn't a TIFILES file
		if(( recCount < ( totalSectors - 1 ) * rps ) || ( recCount > totalSectors * rps ))
		{
			return false;
		}
	}

	// Try to find a filename in the header
	if( IsValidName( buffer + 16 ) == true )
	{
		strncpy( m_FDR.FileName, buffer + 16, MAX_FILENAME );
	}
	else if( IsValidName( buffer + 26 ) == true )
	{
		strncpy( m_FDR.FileName, buffer + 26, MAX_FILENAME );
	}
	else
	{
		// Just use the first 10 characters of the real filename
		auto name = cFileSystem::UnEscapeName( m_FileName.c_str( ));
		memset( m_FDR.FileName, ' ', MAX_FILENAME );
		for( size_t i = 0; i < MAX_FILENAME; i++ )
		{
			if(( i >= name.length( )) || ( name[ i ] == '.' ))
			{
				break;
			}
			m_FDR.FileName[ i ] = name[ i ];
		}
	}

	return true;
}

//------------------------------------------------------------------------------
// Procedure:   cPseudoFileSystem::ConstructFDR_FIAD
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cPseudoFileSystem::ConstructFDR_FIAD( const char *buffer )
{
	FUNCTION_ENTRY( this, "cPseudoFileSystem::ConstructFDR_FIAD", true );

	// Assume the file starts with an sFileDescriptorRecord
	const sFileDescriptorRecord *fdr = reinterpret_cast<const sFileDescriptorRecord *>( buffer );

	// Make sure the contents of the FDR look reasonable
	if( IsValidFDR( fdr ) == false )
	{
		return false;
	}

	// See if the file size is within the size indicated by the 'FDR'
	int totalSectors = GetUINT16( &fdr->TotalSectors );
	long expectedMin = ( totalSectors - 1 ) * DEFAULT_SECTOR_SIZE + fdr->EOF_Offset;
	long expectedMax = totalSectors * DEFAULT_SECTOR_SIZE;

	fseek( m_File, 0L, SEEK_END );
	long actualSize = ftell( m_File ) - 128;

	if(( actualSize < expectedMin ) || ( actualSize > expectedMax ))
	{
		return false;
	}

	// We made it this far - it's most likely an FIAD file
	memset( &m_FDR, 0, sizeof( m_FDR ));
	memcpy( &m_FDR, buffer, 128 );

	return true;
}

//------------------------------------------------------------------------------
// Procedure:   cPseudoFileSystem::LoadFileBuffer
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cPseudoFileSystem::LoadFileBuffer( )
{
	FUNCTION_ENTRY( this, "cPseudoFileSystem::LoadFileBuffer", true );

	int totalSectors = GetUINT16( &m_FDR.TotalSectors );
	size_t maxSize = totalSectors * DEFAULT_SECTOR_SIZE;
	m_FileBuffer = new UINT8[ maxSize ];

	fseek( m_File, 128L, SEEK_SET );

	if( fread( m_FileBuffer, 1, maxSize, m_File ) != maxSize )
	{
		DBG_ERROR( "Failed to read " << maxSize << " bytes from file" );
		return false;
	}

	return true;
}

//------------------------------------------------------------------------------
// Procedure:   cPseudoFileSystem::LoadInternalRecords
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cPseudoFileSystem::RecordList cPseudoFileSystem::LoadInternalRecords( int recordLength )
{
	FUNCTION_ENTRY( this, "cPseudoFileSystem::LoadInternalRecords", true );

	RecordList records;

	auto length = fgetc( m_File );

	while( length != EOF )
	{
		char recordBuffer[ 256 ];
		length = (UINT8) length;
		if(( length > recordLength ) || ( fread( recordBuffer, length, 1, m_File ) != 1 ))
		{
			DBG_ERROR( "Failed to read record " << records.size( ) << " from file" );

			fclose( m_File );
			m_File = nullptr;

			break;
		}
		records.push_back( { recordBuffer, static_cast<size_t>( length ) } );
		length = fgetc( m_File );
	}

	return records;
}

//------------------------------------------------------------------------------
// Procedure:   cPseudoFileSystem::LoadDisplayRecords
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cPseudoFileSystem::RecordList cPseudoFileSystem::LoadDisplayRecords( int recordLength )
{
	FUNCTION_ENTRY( this, "cPseudoFileSystem::LoadDisplayRecords", true );

	RecordList records;

	char recordBuffer[ 256 ];

	memset( recordBuffer, 0, sizeof( recordBuffer ));

	while( fgets( recordBuffer, 256, m_File ) != nullptr )
	{
		size_t length = 0;
		while((( int ) length <= recordLength ) && ( recordBuffer[ length ] != '\n' ))
		{
			length++;
		}
		if(( int ) length > recordLength )
		{
			DBG_ERROR( "Failed to read record " << records.size( ) << " from file" );

			fclose( m_File );
			m_File = nullptr;

			break;
		}
		records.push_back( { recordBuffer, length } );
	}

	return records;
}

//------------------------------------------------------------------------------
// Procedure:   cPseudoFileSystem::ConstructFileBuffer
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cPseudoFileSystem::ConstructFileBuffer( )
{
	FUNCTION_ENTRY( this, "cPseudoFileSystem::ConstructFileBuffer", true );

	fseek( m_File, 0, SEEK_END );
	unsigned long fileSize = ftell( m_File );
	fseek( m_File, 0, SEEK_SET );

	if( cFileSystem::IsProgram( &m_FDR ))
	{
		int recCount = ( fileSize + 255 ) / 256;
		SetUINT16( &m_FDR.TotalSectors, recCount );
		m_FDR.EOF_Offset = fileSize % 256;

		m_FileBuffer = new UINT8[ recCount * 256 ];
		memset( m_FileBuffer, 0, recCount * 256 );

		if( fread( m_FileBuffer, 1, fileSize, m_File ) != fileSize )
		{
			DBG_ERROR( "Failed to read " << fileSize << " bytes from file" );
			return false;
		}

		return true;
	}

	int recordLength = m_FDR.RecordLength ? m_FDR.RecordLength : 256;

	if( cFileSystem::IsVariable( &m_FDR ))
	{
		RecordList records = cFileSystem::IsInternal( &m_FDR ) ? LoadInternalRecords( recordLength ) : LoadDisplayRecords( recordLength );

		m_FDR.RecordsPerSector = 255 / ( recordLength + 1 );

		int recCount     = records.size( );
		int totalSectors = ( recCount + m_FDR.RecordsPerSector - 1 ) / m_FDR.RecordsPerSector;

		m_FileBuffer = new UINT8[ totalSectors * 256 ];
		memset( m_FileBuffer, 0, totalSectors * 256 );

		auto it = records.begin( );

		for( int i = 0; i < totalSectors; i++ )
		{
			UINT8 *start = m_FileBuffer + i * 256;
			UINT8 *ptr = start;
			while( it != records.end( ))
			{
				int size = static_cast<UINT8>( it->size( ));
				if( ptr + 2 + size > start + 256 )
				{
					break;
				}

				*ptr++ = size;
				memcpy( ptr, it->data( ), it->size( ));
				ptr += it->size( );
				it++;
			}
			*ptr++ = 0xFF;

			if( it == records.end( ))
			{
				SetUINT16_LE( &m_FDR.NoFixedRecords, i + 1 );
				SetUINT16( &m_FDR.TotalSectors, i + 1 );
				m_FDR.EOF_Offset = ptr - ( m_FileBuffer + i * 256 ) - 1;
				return true;
			}
		}
	}
	else
	{
		int fileRecordLength = recordLength + ( cFileSystem::IsDisplay( &m_FDR ) ? 1 : 0 );

		int recCount = fileSize / fileRecordLength;

		m_FDR.RecordsPerSector = 256 / recordLength;

		int totalSectors = ( recCount + m_FDR.RecordsPerSector - 1 ) / m_FDR.RecordsPerSector;

		SetUINT16( &m_FDR.TotalSectors, totalSectors );
		SetUINT16_LE( &m_FDR.NoFixedRecords, recCount );

		m_FileBuffer = new UINT8[ totalSectors * 256 ];
		memset( m_FileBuffer, 0, totalSectors * 256 );

		for( int i = 0; i < totalSectors; i++ )
		{
			UINT8 *ptr = m_FileBuffer + i * 256;
			for( int r = 0; r < m_FDR.RecordsPerSector; r++ )
			{
				if( fread( ptr + r * recordLength, recordLength, 1, m_File ) != 1 )
				{
					DBG_ERROR( "Failed to read record from file" );
					return false;
				}
				if( cFileSystem::IsDisplay( &m_FDR ) && ( fgetc( m_File ) != '\n' ))
				{
					DBG_ERROR( "Record missing trailing newline" );
					return false;
				}
				if( --recCount == 0 )
				{
					return true;
				}
			}
		}
	}

	return true;
}

//------------------------------------------------------------------------------
// Procedure:   cPseudoFileSystem::FileCount
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cPseudoFileSystem::FileCount( int ) const
{
	FUNCTION_ENTRY( this, "cPseudoFileSystem::FileCount", true );

	return 1;
}

//------------------------------------------------------------------------------
// Procedure:   cPseudoFileSystem::GetFileDescriptor
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
const sFileDescriptorRecord *cPseudoFileSystem::GetFileDescriptor( int index, int ) const
{
	FUNCTION_ENTRY( this, "cPseudoFileSystem::GetFileDescriptor", true );

	DBG_ASSERT( index == 0 );

	return &m_FDR;
}

//------------------------------------------------------------------------------
// Procedure:   cPseudoFileSystem::FreeSectors
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cPseudoFileSystem::FreeSectors( ) const
{
	FUNCTION_ENTRY( this, "cPseudoFileSystem::GetFreeSectors", true );

	return 0;
}

//------------------------------------------------------------------------------
// Procedure:   cPseudoFileSystem::TotalSectors
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cPseudoFileSystem::TotalSectors( ) const
{
	FUNCTION_ENTRY( this, "cPseudoFileSystem::GetTotalSectors", true );

	// We're simulating a disk, so include a fake FDR sector in the count

	return GetUINT16( &m_FDR.TotalSectors ) + 1;
}

//------------------------------------------------------------------------------
// Procedure:   cPseudoFileSystem::GetFileSector
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
iSector *cPseudoFileSystem::GetFileSector( const sFileDescriptorRecord *fdr, int index )
{
	FUNCTION_ENTRY( this, "cPseudoFileSystem::GetFileSector", true );

	if( fdr != &m_FDR )
	{
		return nullptr;
	}

	UINT8 *fileBuffer = m_FileBuffer + (( long ) index * DEFAULT_SECTOR_SIZE );

	m_CurrentSector->buffer = sDataBuffer{ fileBuffer, fileBuffer + DEFAULT_SECTOR_SIZE };

	return m_CurrentSector;
}

//------------------------------------------------------------------------------
// Procedure:   cPseudoFileSystem::ExtendFile
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cPseudoFileSystem::ExtendFile( sFileDescriptorRecord *fdr, int )
{
	FUNCTION_ENTRY( this, "cPseudoFileSystem::ExtendFile", true );

	if( fdr != &m_FDR )
	{
		return -1;
	}

	DBG_FATAL( "Function not implemented" );

	return -1;
}

//------------------------------------------------------------------------------
// Procedure:   cPseudoFileSystem::TruncateFile
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cPseudoFileSystem::TruncateFile( sFileDescriptorRecord *fdr, int )
{
	FUNCTION_ENTRY( this, "cPseudoFileSystem::TruncateFile", true );

	if( fdr != &m_FDR )
	{
		return false;
	}

	DBG_FATAL( "Function not implemented" );

	return false;
}

//------------------------------------------------------------------------------
// Procedure:   cPseudoFileSystem::DefaultRecordLength
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cPseudoFileSystem::DefaultRecordLength( )
{
	FUNCTION_ENTRY( this, "cPseudoFileSystem::DefaultRecordLength", true );

	return DEFAULT_RECORD_LENGTH_DISK;
}

//------------------------------------------------------------------------------
// Procedure:   cPseudoFileSystem::GetPath
// Purpose:     Return the filename of the disk
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
std::string cPseudoFileSystem::GetPath( ) const
{
	FUNCTION_ENTRY( this, "cPseudoFileSystem::GetPath", true );

	return m_PathName;
}

//------------------------------------------------------------------------------
// Procedure:   cPseudoFileSystem::GetName
// Purpose:     Return the filename of the disk
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
std::string cPseudoFileSystem::GetName( ) const
{
	FUNCTION_ENTRY( this, "cPseudoFileSystem::GetName", true );

	size_t length = MAX_FILENAME;

	while(( length > 0 ) && ( m_FDR.FileName[ length - 1 ] == ' ' ))
	{
		length--;
	}

	return std::string( m_FDR.FileName, length );
}

//------------------------------------------------------------------------------
// Procedure:   cPseudoFileSystem::IsValid
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cPseudoFileSystem::IsValid( ) const
{
	FUNCTION_ENTRY( this, "cPseudoFileSystem::IsValid", true );

	return ( m_File != nullptr ) && std::filesystem::file_size( m_PathName ) ? true : false;
}

//------------------------------------------------------------------------------
// Procedure:   cPseudoFileSystem::IsCollection
// Purpose:     Return the filename of the disk
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cPseudoFileSystem::IsCollection( ) const
{
	FUNCTION_ENTRY( this, "cPseudoFileSystem::IsCollection", true );

	return false;
}

//------------------------------------------------------------------------------
// Procedure:   cPseudoFileSystem::OpenFile
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cRefPtr<cFile> cPseudoFileSystem::OpenFile( const char *, int )
{
	FUNCTION_ENTRY( this, "cPseudoFileSystem::OpenFile", true );

	return cFileSystem::CreateFile( &m_FDR );
}

//------------------------------------------------------------------------------
// Procedure:   cPseudoFileSystem::CreateFile
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cRefPtr<cFile> cPseudoFileSystem::CreateFile( const char *, UINT8, int, int )
{
	FUNCTION_ENTRY( this, "cPseudoFileSystem::CreateFile", true );

	DBG_WARNING( "Function not supported" );

	return nullptr;
}

//------------------------------------------------------------------------------
// Procedure:   cPseudoFileSystem::AddFile
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cPseudoFileSystem::AddFile( cFile *, int )
{
	FUNCTION_ENTRY( this, "cPseudoFileSystem::AddFile", true );

	DBG_WARNING( "Function not supported" );

	return false;
}

//------------------------------------------------------------------------------
// Procedure:   cPseudoFileSystem::DeleteFile
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cPseudoFileSystem::DeleteFile( const char *, int )
{
	FUNCTION_ENTRY( this, "cPseudoFileSystem::DeleteFile", true );

	DBG_WARNING( "Function not supported" );

	return false;
}
