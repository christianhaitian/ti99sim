//----------------------------------------------------------------------------
//
// File:        file-system-disk.cpp
// Date:        15-Jan-2003
// Programmer:  Marc Rousseau
//
// Description: A class to manage the filesystem information on a TI disk.
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
#include "disk-media.hpp"
#include "idisk-sector.hpp"
#include "file-system-disk.hpp"
#include "fileio.hpp"

DBG_REGISTER( __FILE__ );

static inline void SetUINT16( void *_ptr, UINT16 value )
{
	FUNCTION_ENTRY( nullptr, "SetUINT16", true );

	UINT8 *ptr = ( UINT8 * ) _ptr;

	ptr[ 0 ] = value >> 8;
	ptr[ 1 ] = value & 0x0FF;
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

static void PrintSequence( FILE *file, const std::vector<int> &data )
{
	for( size_t i = 0; i < data.size( ); i++ )
	{
		fprintf( file, "%s%03X", ( i == 0 ) ? "" : ",", data[ i ]);

		if((( i + 1 ) < data.size( )) && ( data[ i + 1 ] == data[ i ] + 1 ))
		{
			do
			{
				i += 1;
			}
			while((( i + 1 ) < data.size( )) && ( data[ i + 1 ] == data[ i ] + 1 ));

			fprintf( file, "-%03X", data[ i ]);
		}
	}
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::Open
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cRefPtr<cDiskFileSystem> cDiskFileSystem::Open( const std::string &filename, eDiskFormat format )
{
	FUNCTION_ENTRY( nullptr, "cDiskFileSystem::Open", true );

	cRefPtr<cDiskMedia> media = new cDiskMedia( filename.c_str( ), format );
	if( media->GetFormat( ) != FORMAT_UNKNOWN )
	{
		return new cDiskFileSystem( media );
	}

	return nullptr;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem ctor
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cDiskFileSystem::cDiskFileSystem( cDiskMedia *media ) :
	cBaseObject( "cDiskFileSystem" ),
	m_Media( media ),
	m_VIB( nullptr )
{
	FUNCTION_ENTRY( this, "cDiskFileSystem ctor", true );

	if( m_Media != nullptr )
	{
		if( iSector *sec = m_Media->GetSector( 0, 0, 0 ))
		{
			m_VIBdata = sec->Read( );
			m_VIB = reinterpret_cast<VIB *>( &m_VIBdata[ 0 ] );
		}
	}
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem dtor
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cDiskFileSystem::~cDiskFileSystem( )
{
	FUNCTION_ENTRY( this, "cDiskFileSystem dtor", true );

	if( m_VIB != nullptr )
	{
		m_Media->GetSector( 0, 0, 0 )->Write( m_VIBdata );
	}
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::GetMedia
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cRefPtr<cDiskMedia> cDiskFileSystem::GetMedia( ) const
{
	return m_Media;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::FindSector
// Purpose:     Return a pointer to the requested sector
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
iSector *cDiskFileSystem::FindSector( int index )
{
	FUNCTION_ENTRY( this, "cDiskFileSystem::FindSector", true );

	int trackSize = (( m_VIB != nullptr ) && ( m_VIB->SectorsPerTrack != 0 )) ? m_VIB->SectorsPerTrack : 9;

	int t = index / trackSize;
	int s = index % trackSize;
	int h = 0;

	if( t >= m_Media->NumTracks( ))
	{
		t = 2 * m_Media->NumTracks( ) - t - 1;
		h = 1;
	}

	if( t >= m_Media->NumTracks( ))
	{
		DBG_WARNING( "Invalid sector index (" << index << ")" );
		return nullptr;
	}

	return m_Media->GetSector( t, h, s );
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::FindSector
// Purpose:     Return a pointer to the requested sector
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
const iSector *cDiskFileSystem::FindSector( int index ) const
{
	FUNCTION_ENTRY( this, "cDiskFileSystem::FindSector", true );

	return const_cast<cDiskFileSystem *>( this )->FindSector( index );
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::FindFreeSector
// Purpose:     Find a free sector on the disk beginning at sector 'start'
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cDiskFileSystem::FindFreeSector( int start ) const
{
	FUNCTION_ENTRY( this, "cDiskFileSystem::FindFreeSector", true );

	int auSize = AllocationSize( );

	start /= auSize;

	int index = ( start / 8 ) * 8;

	for( size_t i = ( start / 8 ); i < sizeof( VIB::AllocationMap ); i++ )
	{
		UINT8 bits = m_VIB->AllocationMap[ i ];
		for( int j = 0; j < 8; j++ )
		{
			if((( bits & 1 ) == 0 ) && ( index >= start ))
			{
				return index * auSize;
			}
			index++;
			bits >>= 1;
		}
	}

	return -1;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::SetSectorAllocation
// Purpose:     Update the allocation bitmap in the VIB for the indicated sector
// Parameters:
// Returns:
// Notes:       This routine assumes that only the owning FDR will free a sector
//              and that it will free the remaining sectors in an AU
//------------------------------------------------------------------------------
void cDiskFileSystem::SetSectorAllocation( int index, bool bUsed )
{
	FUNCTION_ENTRY( this, "cDiskFileSystem::SetSectorAllocation", true );

	int auSize = AllocationSize( );

	// Only mark an AU as free if we're releasing the first sector in the AU
	if(( bUsed == false ) && ( index % auSize != 0 ))
	{
		return;
	}

	index /= auSize;

	int i   = index / 8;
	int bit = index % 8;

	if( bUsed == true )
	{
		m_VIB->AllocationMap[ i ] |= 1 << bit;
	}
	else
	{
		m_VIB->AllocationMap[ i ] &= ~( 1 << bit );
	}
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::FindLastSector
// Purpose:     Return the index of the last sector of the file
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cDiskFileSystem::FindLastSector( const sFileDescriptorRecord *fdr ) const
{
	FUNCTION_ENTRY( this, "cDiskFileSystem::FindLastSector", true );

	int totalSectors = GetUINT16( &fdr->TotalSectors );

	const CHAIN *chain = fdr->DataChain;

	// Keep track of how many sectors we've already seen
	int count = 0;
	int last = 0;

	while( count < totalSectors )
	{
		if( chain >= fdr->DataChain + MAX_CHAINS )
		{
			return -1;
		}

		int start = chain->start + (( int ) ( chain->start_offset & 0x0F ) << 8 );
		int offset = (( int ) chain->offset << 4 ) + ( chain->start_offset >> 4 ) + 1;

		if( offset <= count )
		{
			return -1;
		}

		last = start + ( offset - count ) - 1;

		count = offset;
		chain++;
	}

	return last;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::AddFileSector
// Purpose:     Add the sector to the file's sector chain
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cDiskFileSystem::AddFileSector( sFileDescriptorRecord *fdr, int index )
{
	FUNCTION_ENTRY( this, "cDiskFileSystem::AddFileSector", true );

	int totalSectors = GetUINT16( &fdr->TotalSectors );

	CHAIN *chain = fdr->DataChain;

	// Keep track of how many sectors we've already seen
	int count      = 0;
	int lastOffset = 0;

	// Walk the chain to find the last entry
	while( count < totalSectors )
	{
		lastOffset = count;
		if( chain >= fdr->DataChain + MAX_CHAINS ) return false;
		int offset = (( int ) chain->offset << 4 ) + ( chain->start_offset >> 4 ) + 1;
		if( offset <= count ) return false;
		count      = offset;
		chain++;
	}

	// See if we can append to the last chain
	if( count > 0 )
	{
		int start  = chain[ -1 ].start + (( int ) ( chain[ -1 ].start_offset & 0x0F ) << 8 );
		int offset = (( int ) chain[ -1 ].offset << 4 ) + ( chain[ -1 ].start_offset >> 4 ) + 1;
		if( index == start + offset - lastOffset )
		{
			chain[ -1 ].start_offset = (( totalSectors & 0x0F ) << 4 ) | (( start >> 8 ) & 0x0F );
			chain[ -1 ].offset       = ( totalSectors >> 4 ) & 0xFF;
			totalSectors++;
			fdr->TotalSectors = GetUINT16( &totalSectors );
			return true;
		}
	}

	// Start a new chain if there is room
	if( chain < fdr->DataChain + MAX_CHAINS )
	{
		chain->start        = index & 0xFF;
		chain->start_offset = (( totalSectors & 0x0F ) << 4 ) | (( index >> 8 ) & 0x0F );
		chain->offset       = ( totalSectors >> 4 ) & 0xFF;
		totalSectors++;
		fdr->TotalSectors = GetUINT16( &totalSectors );
		return true;
	}

	DBG_WARNING( "Not enough room in CHAIN list" );

	return false;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::FindFileDescriptorIndex
// Purpose:     Return the sector index of the file descriptor with the given filename
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cDiskFileSystem::FindFileDescriptorIndex( const char *name, int dir )
{
	FUNCTION_ENTRY( this, "cDiskFileSystem::FindFileDescriptorIndex", true );

	if( name != nullptr )
	{
		if( auto dirSector = FindSector( GetDirSector( dir )))
		{
			auto sectorData = dirSector->Read( );
			UINT16 *dirIndex = reinterpret_cast<UINT16 *>( &sectorData[ 0 ] );

			int start  = ( dirIndex[ 0 ] == 0 ) ? 1 : 0;

			char target[ MAX_FILENAME ] = { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ' };
			for( size_t i = 0; i < MAX_FILENAME; i++ )
			{
				if( name[ i ] == '\0' )
				{
					break;
				}
				target[ i ] = name[ i ];
			}

			for( int i = start; i < 128; i++ )
			{
				if( dirIndex[ i ] == 0 )
				{
					break;
				}

				int index = GetUINT16( &dirIndex[ i ] );
				if( auto fdrSector = FindSector( index ))
				{
					auto fdrData = fdrSector->Read( );
					const sFileDescriptorRecord *fdr = reinterpret_cast<const sFileDescriptorRecord *>( &fdrData[ 0 ] );

					if( memcmp( fdr->FileName, target, MAX_FILENAME ) == 0 )
					{
						return index;
					}
				}
			}
		}
	}

	return -1;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::GetFileDescriptorIndex
// Purpose:     Return the sector index of the given FDR
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cDiskFileSystem::GetFileDescriptorIndex( const sFileDescriptorRecord *fdr, int dir ) const
{
	FUNCTION_ENTRY( this, "cDiskFileSystem::GetFileDescriptorIndex", true );

	if( auto dirSector = FindSector( GetDirSector( dir )))
	{
		const UINT16 *dirIndex = reinterpret_cast<const UINT16 *>( dirSector->GetData( ));

		int start  = ( dirIndex[ 0 ] == 0 ) ? 1 : 0;

		for( int i = start; i < 128; i++ )
		{
			if( dirIndex[ i ] == 0 )
			{
				break;
			}
			int index = GetUINT16( &dirIndex[ i ] );
			if( auto fdrSector = FindSector( index ))
			{
				const sFileDescriptorRecord *sector = reinterpret_cast<const sFileDescriptorRecord *>( fdrSector->GetData( ));
				if( fdr == sector )
				{
					return index;
				}
			}
		}
	}

	return -1;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::AddFileDescriptor
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cDiskFileSystem::AddFileDescriptor( const sFileDescriptorRecord *fdr, int dir )
{
	FUNCTION_ENTRY( this, "cDiskFileSystem::AddFileDescriptor", true );

	auto sortDirectoryIndex = [=,this]( UINT16 arg1, UINT16 arg2 )
	{
		if( arg1 == arg2 )
		{
			return false;
		}

		auto fdrSector1 = FindSector( GetUINT16( &arg1 ));
		auto fdrSector2 = FindSector( GetUINT16( &arg2 ));

		if(( fdrSector1 != nullptr ) && ( fdrSector2 != nullptr ))
		{
			const sFileDescriptorRecord *fdr1 = reinterpret_cast<const sFileDescriptorRecord *>( fdrSector1->GetData( ));
			const sFileDescriptorRecord *fdr2 = reinterpret_cast<const sFileDescriptorRecord *>( fdrSector2->GetData( ));

			return strncmp( fdr1->FileName, fdr2->FileName, sizeof( sFileDescriptorRecord::FileName )) < 0;
		}

		return (( fdrSector1 != fdrSector2 ) ? ( fdrSector1 == nullptr ) ? -1 : 1 : arg1 - arg2 ) < 0;
	};

	int fdrIndex = -1;

	if( auto *fdiSector = FindSector( GetDirSector( dir )))
	{
		std::vector<UINT8> fdiData = fdiSector->Read( );
		UINT16 *FDI = reinterpret_cast<UINT16*>( &fdiData[ 0 ] );

		// Preserve the 'visibility' of files
		int start = (( FDI[ 0 ] == 0 ) && ( FDI[ 1 ] != 0 )) ? 1 : 0;

		// Look for a free slot in the file descriptor index
		for( int i = start; i < 127; i++ )
		{
			if( FDI[ i ] == 0 )
			{
				// Find a place to put the FDR
				fdrIndex = FindFreeSector( 0 );
				if( fdrIndex == -1 )
				{
					DBG_WARNING( "Out of disk space" );
					return -1;
				}

				// Copy the FDR to the sector on m_Media
				if( auto fdrSector = FindSector( fdrIndex ))
				{
					// Mark the new FDR's sector as used
					SetSectorAllocation( fdrIndex, true );

					// Copy the FDR to the sector on m_Media
					sDataBuffer fdrData( (const UINT8*) fdr, (const UINT8*) fdr + DEFAULT_SECTOR_SIZE );
					sFileDescriptorRecord *newFDR = reinterpret_cast<sFileDescriptorRecord *>( &fdrData[ 0 ] );

					// Make sure the new name is padded with spaces
					for( size_t j = strlen( newFDR->FileName ); j < sizeof( sFileDescriptorRecord::FileName ); j++ )
					{
						newFDR->FileName[ j ] = ' ';
					}

					// Zero out the CHAIN list
					newFDR->TotalSectors = 0;
					memset( newFDR->DataChain, 0, sizeof( CHAIN ) * MAX_CHAINS );

					fdrSector->Write( fdrData );

					// Add the name and resort the directory
					SetUINT16( &FDI[ i ], fdrIndex );
					std::sort( FDI + start, FDI + i + 1, sortDirectoryIndex );

					fdiSector->Write( fdiData );
				}

				break;
			}
		}
	}

	return fdrIndex;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::GetDirSector
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cDiskFileSystem::GetDirSector( int index ) const
{
	FUNCTION_ENTRY( this, "cDiskFileSystem::GetDirSector", true );

	if( index == -1 )
	{
		return 1;
	}

	const Directory *dir = &m_VIB->directory[ index ];

	return GetUINT16( &dir->FDIRecord );
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::GetDirIndex
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cDiskFileSystem::GetDirIndex( const char *filename ) const
{
	for( int i = 0; i < DirectoryCount( ); i++ )
	{
		const char *dirName = DirectoryName( i );

		const char *space = strchr( dirName, ' ' );

		size_t length = space ? space - dirName : strlen( dirName );

		if( strncmp( filename, dirName, length ) == 0 )
		{
			if(( strlen( filename ) == length ) || ( filename[ length ] == '.' ))
			{
				return i;
			}
		}
	}

	return -1;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::DirectoryCount
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cDiskFileSystem::DirectoryCount( ) const
{
	FUNCTION_ENTRY( this, "cDiskFileSystem::DirectoryCount", true );

	int count = 0;

	while( count < 3 )
	{
		if( !IsValidName(( const char * ) &m_VIB->directory[ count ].Name ))
		{
			break;
		}
		count++;
	}

	return count;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::DirectoryName
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
const char *cDiskFileSystem::DirectoryName( int dir ) const
{
	FUNCTION_ENTRY( this, "cDiskFileSystem::DirectoryName", true );

	static char name[ MAX_FILENAME + 1 ];

	if( dir == -1 )
	{
		return nullptr;
	}

	memcpy( name, m_VIB->directory[ dir ].Name, MAX_FILENAME );
	name[ MAX_FILENAME ] = '\0';

	return name;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::FileCount
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cDiskFileSystem::FileCount( int dir ) const
{
	FUNCTION_ENTRY( this, "cDiskFileSystem::FileCount", true );

	if( auto dirSector = FindSector( GetDirSector( dir )))
	{
		const UINT16 *dirIndex = ( const UINT16 * ) dirSector->GetData( );

		int start = ( dirIndex[ 0 ] == 0 ) ? 1 : 0;

		for( int i = start; i < 128; i++ )
		{
			if( dirIndex[ i ] == 0 )
			{
				return i - start;
			}
		}

		return 127;
	}

	return 0;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::GetFileDescriptor
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
const sFileDescriptorRecord *cDiskFileSystem::GetFileDescriptor( int index, int dir ) const
{
	FUNCTION_ENTRY( this, "cDiskFileSystem::GetFileDescriptor", false );

	if( auto dirSector = FindSector( GetDirSector( dir )))
	{
		const UINT16 *dirIndex = reinterpret_cast<const UINT16 *>( dirSector->GetData( ));

		int start = ( dirIndex[ 0 ] == 0 ) ? 1 : 0;

		if( auto sector = FindSector( GetUINT16( &dirIndex[ start + index ] )))
		{
			return reinterpret_cast<const sFileDescriptorRecord *>( sector->GetData( ));
		}
	}

	return nullptr;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::AllocationSize
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cDiskFileSystem::AllocationSize( ) const
{
	FUNCTION_ENTRY( this, "cDiskFileSystem::AllocationSize", true );

	int formattedSectors = GetUINT16( &m_VIB->FormattedSectors );

	int auSize = 1;

	while( auSize * 200 * 8 < formattedSectors )
	{
		auSize *= 2;
	}

	return auSize;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::FreeSectors
// Purpose:     Count the number of free sectors on the disk as indicated by the
//              allocation bitmap in the VIB
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cDiskFileSystem::FreeSectors( ) const
{
	FUNCTION_ENTRY( this, "cDiskFileSystem::FreeSectors", true );

	int free = 0;
	int index = 0;

	int auSize = AllocationSize( );

	for( UINT8 bits : m_VIB->AllocationMap )
	{
		for( int j = 0; j < 8; j++ )
		{
			if(( bits & 1 ) == 0 )
			{
				free++;
			}
			bits >>= 1;
			index += auSize;
			if( index >= TotalSectors( ) + 2 )
			{
				goto done;
			}

		}
	}

done:

	return free * auSize;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::TotalSectors
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cDiskFileSystem::TotalSectors( ) const
{
	FUNCTION_ENTRY( this, "cDiskFileSystem::TotalSectors", true );

	return GetUINT16( &m_VIB->FormattedSectors ) - 2;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::GetFileSector
// Purpose:     Get then Nth sector for this file.
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
iSector *cDiskFileSystem::GetFileSector( const sFileDescriptorRecord *fdr, int index )
{
	FUNCTION_ENTRY( this, "cDiskFileSystem::GetFileSector", true );

	int totalSectors = GetUINT16( &fdr->TotalSectors );

	if( index >= totalSectors )
	{
		DBG_WARNING( "Requested index (" << index << ") exceeds totalSectors (" << totalSectors << ")" );
		return nullptr;
	}

	const CHAIN *chain = fdr->DataChain;

	// Keep track of how many sectors we've already seen
	int count = 0;

	while( count < totalSectors )
	{
		if( chain >= fdr->DataChain + MAX_CHAINS )
		{
			DBG_FATAL( "Internal error: Invalid file CHAIN" );
			return nullptr;
		}

		int start  = chain->start + (( int ) ( chain->start_offset & 0x0F ) << 8 );
		int offset = (( int ) chain->offset << 4 ) + ( chain->start_offset >> 4 ) + 1;

		if( offset <= count )
		{
			DBG_ERROR( "Internal error: Invalid file CHAIN" );
			return nullptr;
		}

		// Is it in this chain?
		if( index < offset )
		{
			int sector = start + ( index - count );
			return FindSector( sector );
		}

		count = offset;
		chain++;
	}

	DBG_FATAL( "Internal error: Error traversing file CHAIN" );

	return nullptr;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::ExtendFile
// Purpose:     Increase the sector allocation for this file by 'count'.
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cDiskFileSystem::ExtendFile( sFileDescriptorRecord *fdr, int count )
{
	FUNCTION_ENTRY( this, "cDiskFileSystem::ExtendFile", true );

	int auSize = AllocationSize( );

	// Try to extend the file without fragmenting
	int last = FindLastSector( fdr );

	if( last == -1 )
	{
		return -1;
	}

	// Try to start at the next sector if we've already allocated sectors to this file
	int index = ( last == 0 ) ? ( 34 / auSize ) * auSize : last + 1;

	for( int i = 0; i < count; i++, index++ )
	{
		// If we reached the end of the last AU, look for another
		if( index % auSize == 0 )
		{
			index = FindFreeSector( index );
			if( index == -1 )
			{
				// Can't stay contiguous, try any available sector - start at sector 34 (from TI-DSR)
				index = FindFreeSector( 34 );
				if( index == -1 )
				{
					// No 'normal' sectors left - try FDI sector range
					index = FindFreeSector( 0 );
					if( index == -1 )
					{
						DBG_WARNING( "Disk is full" );
						return i;
					}
				}
			}
		}

		// Add this sector to the file chain and mark it in use
		if( AddFileSector( fdr, index ) == false )
		{
			DBG_WARNING( "File is too fragmented" );
			return i;
		}

		iSector *sector = FindSector( index );
		if( sector == nullptr )
		{
			DBG_WARNING( "Error reading from disk" );
			return i;
		}

		std::vector<UINT8> empty( DEFAULT_SECTOR_SIZE, 0 );
		sector->Write( empty );

		SetSectorAllocation( index, true );
	}

	return count;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::TruncateFile
// Purpose:     Free all sectors beyond the indicated sector count limit
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cDiskFileSystem::TruncateFile( sFileDescriptorRecord *fdr, int limit )
{
	FUNCTION_ENTRY( this, "cDiskFileSystem::TruncateFile", true );

	int totalSectors = GetUINT16( &fdr->TotalSectors );

	if( limit > totalSectors )
	{
		DBG_WARNING( "limit (" << limit << ") exceeds totalSectors (" << totalSectors << ")" );
		return false;
	}

	CHAIN *chain = fdr->DataChain;

	// Keep track of how many sectors we've already seen
	int count = 0;

	while( count < limit )
	{
		if( chain >= fdr->DataChain + MAX_CHAINS )
		{
			return false;
		}

		int start  = chain->start + (( int ) ( chain->start_offset & 0x0F ) << 8 );
		int offset = (( int ) chain->offset << 4 ) + ( chain->start_offset >> 4 ) + 1;

		if( offset <= count )
		{
			return false;
		}

		if( limit < offset )
		{
			// Mark the excess sectors as free
			for( int i = limit - count; i < offset - count; i++ )
			{
				SetSectorAllocation( start + i, false );
			}

			// Update the chain
			chain->start        = start & 0xFF;
			chain->start_offset = (( limit & 0x0F ) << 4 ) | (( start >> 8 ) & 0x0F );
			chain->offset       = ( limit >> 4 ) & 0xFF;
		}

		count = offset;
		chain++;
	}

	// Zero out the rest of the chain entries & free their sectors
	while( count < totalSectors )
	{
		int start  = chain->start + (( int ) ( chain->start_offset & 0x0F ) << 8 );
		int offset = (( int ) chain->offset << 4 ) + ( chain->start_offset >> 4 ) + 1;

		// Mark the sectors as free
		for( int i = 0; i < offset - count; i++ )
		{
			SetSectorAllocation( start + i, false );
		}

		chain->start        = 0;
		chain->start_offset = 0;
		chain->offset       = 0;

		count = offset;

		chain++;
	}

	fdr->TotalSectors = GetUINT16( &limit );

	return true;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::DefaultRecordLength
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cDiskFileSystem::DefaultRecordLength( )
{
	FUNCTION_ENTRY( this, "cDiskFileSystem::DefaultRecordLength", true );

	return DEFAULT_RECORD_LENGTH_DISK;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::CheckDisk
// Purpose:     Check the integrity of the data structures on the disk
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cDiskFileSystem::CheckDisk( bool verbose ) const
{
	FUNCTION_ENTRY( this, "cDiskFileSystem::CheckDisk", true );

	if( m_VIB == nullptr )
	{
		return false;
	}

	printf( "Checking disk '%s'\n", m_Media->GetName( ));
	printf( "\n" );

	int auSize = AllocationSize( );

	size_t allocSize = sizeof( VIB::AllocationMap ) * 8 * auSize;

	std::vector<const sFileDescriptorRecord *> allocationTable{ allocSize };

	auto MarkAllocated = [ & ]( int index, const sFileDescriptorRecord *fdr )
	{
		int count = auSize - ( index % auSize );
		for( int au = 0; au < count; au++ )
		{
			allocationTable[ index + au ] = fdr;
		}
	};

	sFileDescriptorRecord reserved[ 5 ] = { };

	memcpy( reserved[ 0 ].FileName, "VIB       ", sizeof( sFileDescriptorRecord::FileName ));
	memcpy( reserved[ 1 ].FileName, "Directory0", sizeof( sFileDescriptorRecord::FileName ));
	memcpy( reserved[ 2 ].FileName, "Directory1", sizeof( sFileDescriptorRecord::FileName ));
	memcpy( reserved[ 3 ].FileName, "Directory2", sizeof( sFileDescriptorRecord::FileName ));
	memcpy( reserved[ 4 ].FileName, "Directory3", sizeof( sFileDescriptorRecord::FileName ));

	MarkAllocated( 0, &reserved[ 0 ] );
	MarkAllocated( 1, &reserved[ 1 ] );

	for( int dir = 0; dir < DirectoryCount( ); dir++ )
	{
		int index = GetDirSector( dir );
		MarkAllocated( index, &reserved[ 2 + dir ] );
	}

	bool isOK = true;

	fflush( stdout );

	for( int dir = -1; dir < DirectoryCount( ); dir++ )
	{
		int fileCount = FileCount( dir );

		// Create a list of which files are on which sectors
		for( int i = 0; i < fileCount; i++ )
		{
			if( const sFileDescriptorRecord *fdr = GetFileDescriptor( i, dir ))
			{
				if( IsValidFDR( fdr ) == false )
				{
					fprintf( stderr, "  Invalid File Descriptor found (file index %d)\n", i );
					isOK = false;
					continue;
				}

				if( IsFixed( fdr ) && ( fdr->EOF_Offset != 0 ))
				{
					fprintf( stderr, "  File '%10.10s' is a FIXED length file but has a non-zero EOF offset\n", fdr->FileName );
					isOK = false;
				}

				int dindex = GetFileDescriptorIndex( fdr, dir );
				if(( allocationTable[ dindex ] != nullptr ) && ( allocationTable[ dindex ] != fdr ))
				{
					fprintf( stderr, "  File '%10.10s' is cross-linked with file '%10.10s' on sector %d (%03X)\n", fdr->FileName, allocationTable[ dindex ]->FileName, dindex, dindex );
					isOK = false;
				}
				MarkAllocated( dindex, fdr );

				const CHAIN *chain = fdr->DataChain;
				int totalSectors   = GetUINT16( &fdr->TotalSectors );

				// Keep track of how many sectors we've already seen
				int count = 0;

				while( count < totalSectors )
				{
					if( chain >= fdr->DataChain + MAX_CHAINS )
					{
						fprintf( stderr, "  File '%10.10s' data chain exceeds maximum length\n", fdr->FileName );
						isOK = false;
						break;
					}

					int start  = chain->start + (( int ) ( chain->start_offset & 0x0F ) << 8 );
					int offset = (( int ) chain->offset << 4 ) + ( chain->start_offset >> 4 ) + 1;

					if( offset <= count )
					{
						fprintf( stderr, "  File '%10.10s' invalid data chain\n", fdr->FileName );
						isOK = false;
						break;
					}

					for( int findex = start; findex < start + offset - count; findex++ )
					{
						if(( allocationTable[ findex ] != nullptr ) && ( allocationTable[ findex ] != fdr ))
						{
							fprintf( stderr, "  File '%10.10s' is cross-linked with file '%10.10s' on sector %d (%03X)\n", fdr->FileName, allocationTable[ findex ]->FileName, findex, findex );
							isOK = false;
						}
						MarkAllocated( findex, fdr );
					}

					count = offset;
					chain++;
				}
			}
		}
	}

	int index = 0;

	std::vector<int> bad;

	for( UINT8 bits : m_VIB->AllocationMap )
	{
		for( int j = 0; j < 8; j++ )
		{
			for( int au = 0; au < auSize; au++ )
			{
				if(( bits & 1 ) == 0 )
				{
					if( allocationTable[ index ] != nullptr )
					{
						fprintf( stderr, "  Sector %d (%03X) is marked as free but is used by file '%10.10s'\n", index, index, allocationTable[ index ]->FileName );
						isOK = false;
					}
				}
				else
				{
					if( allocationTable[ index ] == nullptr )
					{
						bad.push_back( index );
						isOK = false;
					}
				}
				index++;
				if( index >= TotalSectors( ) + 2 )
				{
					goto done;
				}
			}

			bits >>= 1;
		}
	}

done:

	if( !bad.empty( ))
	{
		fprintf( stderr, "  %d %s (", ( int ) bad.size( ), ( bad.size( ) > 1 ) ? "sectors" : "sector" );
		PrintSequence( stderr, bad );
		fprintf( stderr, ") marked as in use but not used by any file\n" );
	}

	fflush( stderr );

	if( isOK )
	{
		printf( "  No errors detected\n" );
	}

	if( verbose )
	{
		index = 0;
		int lastIndex = 0;
		const sFileDescriptorRecord *activeRecord = nullptr;

		fprintf( stdout, "\nSector Usage:\n\n" );

		while( index < TotalSectors( ) + 2 )
		{
			if( allocationTable[ index ] != activeRecord )
			{
				if( index )
				{
					if( activeRecord != nullptr )
					{
						char name[ 11 ];
						GetCleanName( activeRecord, name );
						if( index == lastIndex + 1 )
						{
							fprintf( stdout, "     - %s\n", name );
						}
						else
						{
							fprintf( stdout, "-%03X - %s\n", index - 1, name );
						}
					}
					else
					{
						fprintf( stdout, "-%03X - <%d unused sectors>\n", index - 1, index - lastIndex );
					}
				}

				fprintf( stdout, "  %03X", index );

				activeRecord = allocationTable[ index ];
				lastIndex = index;
			}

			index++;
		}

		if( activeRecord != nullptr )
		{
			char name[ 11 ];
			GetCleanName( activeRecord, name );
			if( index == lastIndex + 1 )
			{
				fprintf( stdout, "     - %s\n", name );
			}
			else
			{
				fprintf( stdout, "-%03X - %s\n", index - 1, name );
			}
		}
		else
		{
			fprintf( stdout, "-%03X - <%d unused sectors>\n", index - 1, index - lastIndex );
		}

		fprintf( stdout, "\n" );
	}

	return isOK;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::GetPath
// Purpose:     Return the filename of the disk
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
std::string cDiskFileSystem::GetPath( ) const
{
	FUNCTION_ENTRY( this, "cDiskFileSystem::GetPath", true );

	return ( m_Media == nullptr ) ? "" : m_Media->GetName( );
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::GetName
// Purpose:     Return the filename of the disk
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
std::string cDiskFileSystem::GetName( ) const
{
	FUNCTION_ENTRY( this, "cDiskFileSystem::GetName", true );

	if( m_VIB == nullptr )
	{
		return "";
	}

	size_t length = MAX_FILENAME;

	while(( length > 0 ) && ( m_VIB->VolumeName[ length - 1 ] == ' ' ))
	{
		length--;
	}

	return std::string( m_VIB->VolumeName, length );
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::IsValid
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cDiskFileSystem::IsValid( ) const
{
	FUNCTION_ENTRY( this, "cDiskFileSystem::IsValid", true );

	if(( m_Media == nullptr ) || ( m_VIB == nullptr ))
	{
		return false;
	}
	if( memcmp( m_VIB->DSK, "DSK", 3 ) != 0 )
	{
		return false;
	}

	return true;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::IsCollection
// Purpose:     Return the filename of the disk
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cDiskFileSystem::IsCollection( ) const
{
	FUNCTION_ENTRY( this, "cDiskFileSystem::IsCollection", true );

	return true;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::OpenFile
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cRefPtr<cFile> cDiskFileSystem::OpenFile( const char *filename, int dir )
{
	FUNCTION_ENTRY( this, "cDiskFileSystem::OpenFile", true );

	if( m_VIB == nullptr )
	{
		return nullptr;
	}

	const char *separator = strchr( filename, '.' );

	if(( dir == -1 ) && ( separator != nullptr ))
	{
		dir = GetDirIndex( filename );
		if( dir != -1 )
		{
			filename = separator + 1;
		}
	}

	int index = FindFileDescriptorIndex( filename, dir );

	if( index != -1 )
	{
		if( auto fdrSector = FindSector( index ))
		{
			return cFileSystem::CreateFile( reinterpret_cast<const sFileDescriptorRecord *>( fdrSector->GetData( )));
		}
	}

	return nullptr;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::CreateFile
// Purpose:     Create a new file on the disk (deletes any pre-existing file)
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cRefPtr<cFile> cDiskFileSystem::CreateFile( const char *filename, UINT8 type, int recordLength, int dir )
{
	FUNCTION_ENTRY( this, "cDiskFileSystem::CreateFile", true );

	if( m_VIB == nullptr )
	{
		return nullptr;
	}

	// Get rid of any existing file by this name
	int index = FindFileDescriptorIndex( filename, dir );
	if( index != -1 )
	{
		DeleteFile( filename, dir );
	}

	sFileDescriptorRecord fdr;
	memset( &fdr, 0, sizeof( fdr ));

	for( int i = 0; i < MAX_FILENAME; i++ )
	{
		fdr.FileName[ i ] = ( *filename != '\0' ) ? *filename++ : ' ';
	}

	fdr.FileStatus       = type;
	fdr.RecordsPerSector = IsVariable( &fdr ) ? ( 255 / ( recordLength + 1 )) : 256 / recordLength;
	fdr.RecordLength     = recordLength;

	DBG_FATAL( "Function not implemented yet" );

	return nullptr;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::AddFile
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cDiskFileSystem::AddFile( cFile *file, int dir )
{
	FUNCTION_ENTRY( this, "cDiskFileSystem::AddFile", true );

	if( m_VIB == nullptr )
	{
		return false;
	}

	const sFileDescriptorRecord *fdr = file->GetFDR( );

//	DBG_TRACE ( "Adding file " << file->GetName () << " to disk " << GetName ());

	// Look for a file on this disk with the same name
	int index = FindFileDescriptorIndex( fdr->FileName, dir );
	if( index != -1 )
	{
		if( auto fdrSector = FindSector( index ))
		{
			// See if the file we found is the one we're trying to add
			if(( const void * ) fdr == ( const void * ) fdrSector->GetData( ))
			{
				DBG_WARNING( "File already exists on this disk" );
				return true;
			}
		}
		else
		{
			DBG_WARNING( "Error reading from disk" );
			return false;
		}
		// Remove the existing file
		DeleteFile( fdr->FileName, dir );
	}

	// First, make sure the disk has enough room for the file
	int totalSectors = GetUINT16( &fdr->TotalSectors );
	if( FreeSectors( ) < totalSectors + 1 )
	{
		DBG_WARNING( "Not enough room on disk for file" );
		return false;
	}

	// Next, Try to add another filename
	int fdrIndex = AddFileDescriptor( fdr, dir );
	if( fdrIndex == -1 )
	{
		DBG_WARNING( "No room left in the file descriptor table" );
		return false;
	}

	// Get a pointer to the new FDR so we can update the file chain
	if( auto fdrSector = FindSector( fdrIndex ))
	{
		auto fdrData = fdrSector->Read( );

		sFileDescriptorRecord *newFDR = reinterpret_cast<sFileDescriptorRecord *>( &fdrData[ 0 ] );

		// This shouldn't fail since we've already checked for free space
		if( ExtendFile( newFDR, totalSectors ) != totalSectors )
		{
			DBG_FATAL( "Internal error: Unable to extend file" );
			DeleteFile( fdr->FileName, dir );
			return false;
		}

		// Copy the data to the new data sectors
		for( int i = 0; i < totalSectors; i++ )
		{
			UINT8 data[ DEFAULT_SECTOR_SIZE ];
			file->ReadSector( i, data );
			if( auto sector = GetFileSector( newFDR, i ))
			{
				sector->Write( { data, data + DEFAULT_SECTOR_SIZE } );
			}
			else
			{
				return false;
			}
		}

		fdrSector->Write( fdrData );

		return true;
	}

	return false;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::DeleteFile
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cDiskFileSystem::DeleteFile( const char *fileName, int dir )
{
	FUNCTION_ENTRY( this, "cDiskFileSystem::DeleteFile", true );

	if( m_VIB == nullptr )
	{
		return false;
	}

	// See if we have a file with this name
	int index = FindFileDescriptorIndex( fileName, dir );
	if( index == -1 )
	{
		return false;
	}

//	DBG_TRACE ( "Removing file " << fileName << " from disk " << GetName ());

	if( auto fdiSector = FindSector( GetDirSector( dir )))
	{
		auto fdiData = fdiSector->Read( );

		UINT16 *FDI = reinterpret_cast<UINT16 *>( &fdiData[ 0 ] );

		int start = ( FDI[ 0 ] == 0 ) ? 1 : 0;

		// Remove the pointer to the FDR from the FDI
		int fdrIndex = GetUINT16( &index );
		for( int i = start; i < 127; i++ )
		{
			if( FDI[ i ] == fdrIndex )
			{
				memcpy( FDI + i, FDI + i + 1, ( 127 - i ) * sizeof( UINT16 ));
				FDI[ 127 ] = 0;
				break;
			}
		}

		if( auto fdrSector = FindSector( index ))
		{
			auto fdrData = fdrSector->Read( );

			sFileDescriptorRecord *fdr = reinterpret_cast<sFileDescriptorRecord *>( &fdrData[ 0 ] );

			// Release all of the sectors owned by the file itself
			if( TruncateFile( fdr, 0 ))
			{
				// Release the FDR's sector
				SetSectorAllocation( index, false );

				fdiSector->Write( fdiData );
				fdrSector->Write( fdrData );

				return true;
			}
		}
	}

	return false;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::ListingHeader
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
void cDiskFileSystem::ListingHeader( int flags, std::list<std::string> &headers ) const
{
	FUNCTION_ENTRY( this, "cDiskFileSystem::ListingHeader", false );

	cFileSystem::ListingHeader( flags, headers );

	if( flags & LISTING_FLAG_VERBOSE )
	{
		headers.push_back( " ST" );
		headers.push_back( "R/S" );
		headers.push_back( "#SEC" );
		headers.push_back( "EOF" );
		headers.push_back( " RL" );
		headers.push_back( "#FIX" );
		headers.push_back( "FDI" );
		headers.push_back( " Chains" );
	}
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::ListingData
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
void cDiskFileSystem::ListingData( cFile *file, int dir, int flags, std::list<std::string> &data ) const
{
	FUNCTION_ENTRY( this, "cDiskFileSystem::ListingData", false );

	cFileSystem::ListingData( file, dir, flags, data );

	const sFileDescriptorRecord *fdr = file->GetFDR( );

	if( flags & LISTING_FLAG_VERBOSE )
	{
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

		sprintf( buffer, "%03X", GetFileDescriptorIndex( fdr, dir ));
		data.push_back( buffer );

		std::string sectorChain;

		const CHAIN *chain = fdr->DataChain;
		int totalSectors   = GetUINT16( &fdr->TotalSectors );

		// Keep track of how many sectors we've already seen
		int count = 0;

		while( count < totalSectors )
		{
			if( chain >= fdr->DataChain + MAX_CHAINS )
			{
				sectorChain += " ** Invalid data chain **";
				break;
			}

			int start  = chain->start + (( int ) ( chain->start_offset & 0x0F ) << 8 );
			int offset = (( int ) chain->offset << 4 ) + ( chain->start_offset >> 4 ) + 1;

			if( offset <= count )
			{
				sectorChain += " ** Invalid data chain";
				break;
			}

			sprintf( buffer, "%s%03X/%03X", count ? " " : "", start, offset - count );

			sectorChain += buffer;

			count = offset;
			chain++;
		}

		if( ! IsValidFDR( fdr ))
		{
			sectorChain += " ** Bad FDR";
		}

		data.push_back( sectorChain );
	}
}
