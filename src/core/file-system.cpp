//----------------------------------------------------------------------------
//
// File:        file-system.cpp
// Date:        05-Sep-2003
// Programmer:  Marc Rousseau
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

#include <filesystem>
#include "common.hpp"
#include "logger.hpp"
#include "file-system.hpp"
#include "file-system-disk.hpp"
#include "file-system-arc.hpp"
#include "file-system-pseudo.hpp"
#include "fileio.hpp"

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

//------------------------------------------------------------------------------
// Procedure:   cFileSystem::Open
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cRefPtr<cFileSystem> cFileSystem::Open( const std::string &filename )
{
	FUNCTION_ENTRY( nullptr, "cFileSystem::Open", true );

	cRefPtr<cFileSystem> fallback;
	cRefPtr<cFileSystem> disk;

	disk = cDiskFileSystem::Open( filename, FORMAT_UNKNOWN );

	if( disk == nullptr )
	{
		fallback = disk;
		disk = cArchiveFileSystem::Open( filename );
		if(( disk == nullptr ) || ( disk->IsValid( ) == false ))
		{
			if( fallback == nullptr )
			{
				fallback = disk;
			}
			disk = cPseudoFileSystem::Open( filename );
		}
	}

	if( disk == nullptr )
	{
		return fallback;
	}

	return disk;
}

//------------------------------------------------------------------------------
// Procedure:   cFileSystem::GetCleanName
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
void cFileSystem::GetCleanName( const sFileDescriptorRecord *fdr, char *name )
{
	for( int i = 0; i < 10; i++ )
	{
		name[ i ] = isprint( fdr->FileName[ i ] ) ? fdr->FileName[ i ] : '?';
	}

	name[ 10 ] = '\0';
}

//------------------------------------------------------------------------------
// Procedure:   cFileSystem::EscapeName
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
std::string cFileSystem::EscapeName( std::string name )
{
	FUNCTION_ENTRY( nullptr, "cFileSystem::EscapeName", true );

	size_t last = name.find_last_not_of( ' ' );

	if( last != std::string::npos )
	{
		name.resize( last + 1 );
	}

	const char valid[] = { "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_." };

	std::string converted;

	size_t start = 0;

	while( start < name.size( ))
	{
		size_t end = name.find_first_not_of( valid, start );

		if( start != end )
		{
			converted += name.substr( start, end - start );
			start = end;
		}
		else
		{
			char ch = name[ end ];
			converted += "#";
			converted += valid[ ch / 16 ];
			converted += valid[ ch % 16 ];
			converted += ";";
			start++;
		}
	}

	return converted;
}

//------------------------------------------------------------------------------
// Procedure:   cFileSystem::UnEscapeName
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
std::string cFileSystem::UnEscapeName( std::string name )
{
	FUNCTION_ENTRY( nullptr, "cFileSystem::UnEscapeName", true );

	auto ptr = name.begin( );
	auto end = name.begin( ) + name.length( );

	std::string converted;

	while( ptr < end )
	{
		char ch = *ptr++;
		if(( ch == '#' ) && ( ptr + 2 < end ) && isxdigit( ptr[ 0 ] ) && isxdigit( ptr[ 1 ] ) && ( ptr[ 2 ] == ';' ))
		{
			ch = std::stoi( { &*ptr, 2 }, nullptr, 16 );
			ptr += 3;
		}
		converted += ch;
	}

	return converted;
}

//------------------------------------------------------------------------------
// Procedure:   cFileSystem::IsValidName
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cFileSystem::IsValidName( const char *name )
{
	FUNCTION_ENTRY( nullptr, "cFileSystem::IsValidName", true );

	if( name == nullptr )
	{
		return false;
	}

	int i = 0;

	// Allow a single leading space (e.g. Miller's Graphics serial #s)
	if( name[ 0 ] == ' ' )
	{
		i += 1;
	}

	bool good = false;

	for( ; i < MAX_FILENAME; i++ )
	{
		if( name[ i ] == '.' )
		{
			return false;
		}
		if( name[ i ] == '\0' )
		{
			return false;
		}
		if( name[ i ] == ' ' )
		{
			break;
		}
		if( isprint( name[ i ] & 0xFF ))
		{
			good = true;
		}
	}

	for( i++; i < MAX_FILENAME; i++ )
	{
		if( name[ i ] != ' ' )
		{
			return false;
		}
	}

	return good;
}

//------------------------------------------------------------------------------
// Procedure:   cFileSystem::IsValidFDR
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cFileSystem::IsValidFDR( const sFileDescriptorRecord *fdr )
{
	FUNCTION_ENTRY( nullptr, "cFileSystem::IsValidFDR", true );

	if( fdr == nullptr )
	{
		return false;
	}

	// Make sure the filename valid
	if( IsValidName( fdr->FileName ) == false )
	{
		return false;
	}

	int totalSectors = GetUINT16( &fdr->TotalSectors );

	// Make sure totalSectors is not greater than the maximum number of sectors possible
	if( totalSectors > 80 * 18 * 2 )
	{
		return false;
	}

	// Simple sanity checks
	if(( fdr->FileStatus & PROGRAM_TYPE ) != 0 )
	{
		if( fdr->RecordsPerSector != 0 )
		{
			return false;
		}
	}
	else
	{
		// NOTE: This isn't reliable for a RecordLength of 0 (could be 80 or 64)
		if( fdr->RecordsPerSector * fdr->RecordLength > DEFAULT_SECTOR_SIZE )
		{
			return false;
		}

		if(( fdr->FileStatus & VARIABLE_TYPE ) != 0 )
		{
			if( GetUINT16_LE( &fdr->NoFixedRecords ) > totalSectors )
			{
				return false;
			}
		}
	}

	return true;
}

//------------------------------------------------------------------------------
// Procedure:   cFileSystem::IsProgram
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cFileSystem::IsProgram( const sFileDescriptorRecord *fdr )
{
	return (( fdr->FileStatus & ( PROGRAM_TYPE | INTERNAL_TYPE | VARIABLE_TYPE )) == PROGRAM_TYPE ) ? true : false;
}

//------------------------------------------------------------------------------
// Procedure:   cFileSystem::IsDisplay
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cFileSystem::IsDisplay( const sFileDescriptorRecord *fdr )
{
	return (( fdr->FileStatus & ( PROGRAM_TYPE | INTERNAL_TYPE )) == DISPLAY_TYPE ) ? true : false;
}

//------------------------------------------------------------------------------
// Procedure:   cFileSystem::IsInternal
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cFileSystem::IsInternal( const sFileDescriptorRecord *fdr )
{
	return (( fdr->FileStatus & ( PROGRAM_TYPE | INTERNAL_TYPE )) == INTERNAL_TYPE ) ? true : false;
}

//------------------------------------------------------------------------------
// Procedure:   cFileSystem::IsFixed
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cFileSystem::IsFixed( const sFileDescriptorRecord *fdr )
{
	return (( fdr->FileStatus & ( PROGRAM_TYPE | VARIABLE_TYPE )) == FIXED_TYPE ) ? true : false;
}

//------------------------------------------------------------------------------
// Procedure:   cFileSystem::IsVariable
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cFileSystem::IsVariable( const sFileDescriptorRecord *fdr )
{
	return (( fdr->FileStatus & ( PROGRAM_TYPE | VARIABLE_TYPE )) == VARIABLE_TYPE ) ? true : false;
}

//------------------------------------------------------------------------------
// Procedure:   cFileSystem::FormatTimestamp
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
void cFileSystem::FormatTimestamp( const UINT8 *ptr, std::string &date, std::string &time )
{
	FUNCTION_ENTRY( nullptr, "cFileSystem::FormatTimestamp", true );

	if( *( const UINT32 * ) ptr != 0 )
	{
		char buffer[ 32 ];

		int year    = ( ptr[ 2 ] >> 1 ) & 0x7F;
		int month   = (( ptr[ 2 ] << 3 ) | ( ptr[ 3 ] >> 5 )) & 0x0F;
		int day     = ( ptr[ 3 ] ) & 0x1F;

		sprintf( buffer, "%2d/%02d/%4d", month, day, year + (( year < 80 ) ? 2000 : 1900 ));
		date = buffer;

		int hour    = ( ptr[ 0 ] >> 3 ) & 0x1F;
		int minutes = (( ptr[ 0 ] << 3 ) | ( ptr[ 1 ] >> 5 )) & 0x3F;
		int seconds = (( ptr[ 1 ] ) & 0x1F ) * 2;

		sprintf( buffer, "%02d:%02d:%02d", hour, minutes, seconds );
		time = buffer;
	}
	else
	{
		date = "          ";
		time = "        ";
	}
}

//------------------------------------------------------------------------------
// Procedure:   cFileSystem::CreateFile
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cRefPtr<cFile> cFileSystem::CreateFile( const sFileDescriptorRecord *fdr )
{
	FUNCTION_ENTRY( this, "cFileSystem::CreateFile", true );

	return new cFile( this, fdr );
}

//------------------------------------------------------------------------------
// Procedure:   cFileSystem::CheckDisk
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cFileSystem::CheckDisk( bool ) const
{
	FUNCTION_ENTRY( this, "cFileSystem::CheckDisk", true );

	return true;
}

//------------------------------------------------------------------------------
// Procedure:   cFileSystem::DirectoryCount
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cFileSystem::DirectoryCount( ) const
{
	FUNCTION_ENTRY( this, "cFileSystem::DirectoryCount", true );

	return 0;
}

//------------------------------------------------------------------------------
// Procedure:   cFileSystem::DirectoryName
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
const char *cFileSystem::DirectoryName( int ) const
{
	FUNCTION_ENTRY( this, "cFileSystem::DirectoryName", true );

	return nullptr;
}

//------------------------------------------------------------------------------
// Procedure:   cFileSystem::AllocationSize
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cFileSystem::AllocationSize( ) const
{
	FUNCTION_ENTRY( this, "cFileSystem::AllocationSize", true );

	return 1;
}

//------------------------------------------------------------------------------
// Procedure:   cFileSystem::ListingHeader
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
void cFileSystem::ListingHeader( int flags, std::list<std::string> &headers ) const
{
	FUNCTION_ENTRY( this, "cFileSystem::ListingHeader", false );

	if( flags & LISTING_FLAG_SHA1 )
	{
		headers.push_back( "              SHA1 Checksum             " );
	}

	headers.push_back( " Filename " );
	headers.push_back( "Size" );
	headers.push_back( "   Type    " );
	headers.push_back( "P" );

	if( flags & LISTING_FLAG_TIMESTAMPS )
	{
		headers.push_back( "Created   " );
		headers.push_back( "        " );
		headers.push_back( "Modified  " );
		headers.push_back( "        " );
	}
}

//------------------------------------------------------------------------------
// Procedure:   cFileSystem::ListingData
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
void cFileSystem::ListingData( cFile *file, int dir, int flags, std::list<std::string> &data ) const
{
	FUNCTION_ENTRY( this, "cFileSystem::ListingData", false );

	if( flags & LISTING_FLAG_SHA1 )
	{
		data.push_back( file->sha1( ));
	}

	char buffer[ 32 ];

	const sFileDescriptorRecord *fdr = file->GetFDR( );

	// Print Filename
	GetCleanName( fdr, buffer );
	data.push_back( IsValidName( buffer ) ? buffer : "??????????" );

	// Print Size
	int size = GetUINT16( &fdr->TotalSectors ) + 1;
	sprintf( buffer, "%4d", size );
	data.push_back( size <= TotalSectors( ) ? buffer : "????" );

	// Print Type
	if( cFileSystem::IsProgram( fdr ))
	{
		data.push_back( "PROGRAM    " );
	}
	else
	{
		sprintf( buffer, "%s/%s %3d", cFileSystem::IsInternal( fdr ) ? "INT" : "DIS",
				 cFileSystem::IsVariable( fdr ) ? "VAR" : "FIX", fdr->RecordLength );
		data.push_back( buffer );
	}

	data.push_back(( fdr->FileStatus & WRITE_PROTECTED_TYPE ) ? "Y" : " " );

	if( flags & LISTING_FLAG_TIMESTAMPS )
	{
		std::string date;
		std::string time;

		FormatTimestamp( fdr->reserved2, date, time );
		data.push_back( date );
		data.push_back( time );

		FormatTimestamp( fdr->reserved2 + 4, date, time );
		data.push_back( date );
		data.push_back( time );
	}
}

//------------------------------------------------------------------------------
// Procedure:   cFileSystem::GetFilenames
// Purpose:     Return a list of all files on the disk
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
void cFileSystem::GetFilenames( std::list<std::string>& names, int dir ) const
{
	FUNCTION_ENTRY( this, "cFileSystem::GetFilenames", true );

	int fileCount = FileCount( dir );

	for( int i = 0; i < fileCount; i++ )
	{
		if( const sFileDescriptorRecord *fdr = GetFileDescriptor( i, dir ))
		{
			std::string name( fdr->FileName, 10 );

			size_t end = name.find_last_not_of( ' ' );
			if( end != std::string::npos )
			{
				name.erase( end + 1 );
			}
			names.push_back( name );

			DBG_TRACE( name );
		}
	}
}
