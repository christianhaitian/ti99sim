//----------------------------------------------------------------------------
//
// File:        disk-media.cpp
// Date:        14-Jul-2001
// Programmer:  Marc Rousseau
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
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307, USA.
//
// Revision History:
//
//----------------------------------------------------------------------------

#include <cstring>
#include "common.hpp"
#include "logger.hpp"
#include "support.hpp"
#include "disk-media.hpp"
#include "file-system-disk.hpp"
#include "idisk-serializer.hpp"
#include "disk-serializer-anadisk.hpp"
#include "disk-serializer-v9t9.hpp"
#include "disk-serializer-pc99.hpp"
#include "disk-serializer-cf7+.hpp"
#include "disk-serializer-hfe.hpp"

DBG_REGISTER( __FILE__ );

cDiskMedia::cDiskMedia( cDiskImage *image ) :
	cBaseObject( "cDiskMedia" ),
	m_IsWriteProtected( false ),
	m_FileName( ),
	m_Serializer( nullptr ),
	m_Image( image )
{
	FUNCTION_ENTRY( this, "cDiskMedia ctor", true );
}

cDiskMedia::cDiskMedia( std::nullptr_t ) :
	cDiskMedia( new cDiskImage )
{
	FUNCTION_ENTRY( this, "cDiskMedia ctor", true );

	m_Image->AllocateTracks( MAX_TRACKS, 2 );

	m_Image->ClearChanged( );
}

cDiskMedia::cDiskMedia( const char *fileName, eDiskFormat format ) :
	cDiskMedia( nullptr )
{
	FUNCTION_ENTRY( this, "cDiskMedia ctor", true );

	LoadFile( fileName, format );
}

cDiskMedia::~cDiskMedia( )
{
	FUNCTION_ENTRY( this, "cDiskMedia dtor", true );

	if( m_Image->HasChanged( ))
	{
		SaveFile( );
	}
}

iDiskSerializer *cDiskMedia::CreateSerializer( eDiskFormat format )
{
	FUNCTION_ENTRY( nullptr, "cDiskMedia::CreateSerializer", true );

	switch( format )
	{
		case FORMAT_RAW_TRACK :
			return new cDiskSerializerPC99( );
		case FORMAT_RAW_SECTOR :
			return new cDiskSerializerV9T9( );
		case FORMAT_ANADISK :
			return new cDiskSerializerAnaDisk( );
		case FORMAT_CF7 :
			return new cDiskSerializerCF7( );
		case FORMAT_HFE :
			return new cDiskSerializerHFE( );
		default :
			break;
	}

	return nullptr;
}

iDiskSerializer *cDiskMedia::FindSerializer( const char *fileName )
{
	FUNCTION_ENTRY( nullptr, "cDiskMedia::FindSerializer", true );

	std::string validName = LocateFile( "disks", fileName );

	if( validName.empty( ))
	{
		validName = LocateFile( "disks", cDiskSerializerCF7::GetRawFileName( fileName ));

		if( validName.empty( ))
		{
			return nullptr;
		}
	}

	iDiskSerializer* serializer = nullptr;

	if( FILE *file = fopen( validName.c_str( ), "rb" ))
	{
		fseek( file, 0L, SEEK_END );
		if( ftell ( file ) == 0 )
		{
			fclose( file );
			return nullptr;
		}

		if( cDiskSerializerHFE::MatchesFormat( file ))
		{
			serializer = new cDiskSerializerHFE( );
			goto match;
		}
		if( cDiskSerializerV9T9::MatchesFormat( file ))
		{
			serializer = new cDiskSerializerV9T9( );
			goto match;
		}
		if( cDiskSerializerAnaDisk::MatchesFormat( file ))
		{
			serializer = new cDiskSerializerAnaDisk( );
			goto match;
		}
		if( cDiskSerializerPC99::MatchesFormat( file ))
		{
			serializer = new cDiskSerializerPC99( );
			goto match;
		}
		if( cDiskSerializerCF7::MatchesFormat( file, fileName ))
		{
			serializer = new cDiskSerializerCF7( );
			goto match;
		}

	match:

		fclose( file );
	}

	return serializer;
}

eDiskFormat cDiskMedia::GetFormat( ) const
{
	FUNCTION_ENTRY( this, "cDiskMedia::GetFormat", true );

	return m_Serializer ? m_Serializer->GetFormat( ) : FORMAT_UNKNOWN;
}

int cDiskMedia::GetVolume( ) const
{
	FUNCTION_ENTRY( this, "cDiskMedia::GetVolume", true );

	return m_Serializer ? m_Serializer->GetVolume( ) : 0;
}

int cDiskMedia::MaxVolume( ) const
{
	FUNCTION_ENTRY( this, "cDiskMedia::MaxVolume", true );

	return m_Serializer ? m_Serializer->MaxVolume( ) : 0;
}


void cDiskMedia::ClearDisk( )
{
	FUNCTION_ENTRY( this, "cDiskMedia::ClearDisk", true );

	m_Image->AllocateTracks( m_Image->GetNumTracks( ), m_Image->GetNumHeads( ));

	m_Image->ClearChanged( );
}

bool cDiskMedia::LoadFile( const char *name, eDiskFormat format )
{
	FUNCTION_ENTRY( this, "cDiskMedia::LoadFile", true );

	if( name != nullptr )
	{
		m_FileName = name;
	}

	if( m_FileName.empty( ))
	{
		DBG_ERROR( "No filename specified" );
		return false;
	}

	cRefPtr<iDiskSerializer> serializer = ( format != FORMAT_UNKNOWN ) ? CreateSerializer( format ) : FindSerializer( m_FileName.c_str( ));

	if( serializer == nullptr )
	{
		DBG_WARNING( "Unable to determine format of '" << m_FileName << '\'' );
		return false;
	}

	if( serializer->LoadFile( m_FileName.c_str( ), m_Image.get( )) == false )
	{
		DBG_WARNING( "Error reading '" << m_FileName << '\'' );
		return false;
	}

	m_IsWriteProtected = IsWriteable( serializer->RawFileName( m_FileName.c_str( ))) ? false : true;

	m_Serializer = serializer;

	return true;
}

bool cDiskMedia::SaveFile( bool force )
{
	FUNCTION_ENTRY( this, "cDiskMedia::SaveFile", true );

	if(( force == false ) && ( m_Image->HasChanged( ) == false ))
	{
		return true;
	}

	if( m_Serializer == nullptr )
	{
		return SaveFileAs( m_FileName.c_str( ), FORMAT_UNKNOWN );
	}

	if( m_Serializer->SaveFile( *m_Image, m_FileName.c_str( )) == true )
	{
		m_Image->ClearChanged( );
		return true;
	}

	return false;
}

bool cDiskMedia::SaveFileAs( const char *filename, eDiskFormat format )
{
	FUNCTION_ENTRY( this, "cDiskMedia::SaveFileAs", true );

	if( filename == nullptr )
	{
		DBG_ERROR( "No filename specified" );
		return false;
	}

	if( format == FORMAT_UNKNOWN )
	{
		format = ( m_Serializer == nullptr ) ? FORMAT_RAW_TRACK : m_Serializer->GetFormat( );
	}

	cRefPtr<iDiskSerializer> serializer = m_Serializer;

	// See if we can use the existing serializer to save the file
	if(( serializer == nullptr ) || ( serializer->GetFormat( ) != format ))
	{
		switch( format )
		{
			case FORMAT_RAW_TRACK :
				serializer = new cDiskSerializerPC99( );
				break;
			case FORMAT_RAW_SECTOR :
				serializer = new cDiskSerializerV9T9( );
				break;
			case FORMAT_ANADISK :
				serializer = new cDiskSerializerAnaDisk( );
				break;
			case FORMAT_CF7 :
				serializer = new cDiskSerializerCF7( );
				break;
			case FORMAT_HFE :
				serializer = new cDiskSerializerHFE( );
				break;
			default :
				DBG_ERROR( "No serializer supporting format " << format );
				return false;
		}

		if( serializer->SupportsFeatures( *m_Image ) == false )
		{
			DBG_ERROR( "Disk contains features that this format does not support" );
			return false;
		}
	}

	LoadFile( );

	if( serializer->SaveFile( *m_Image, filename ) != true )
	{
		DBG_ERROR( "Serializer reported failure saving file" );
		return false;
	}

	m_FileName = filename;
	m_Serializer = serializer;

	return true;
}

iDiskTrack *cDiskMedia::GetTrack( size_t tIndex, size_t hIndex )
{
	FUNCTION_ENTRY( this, "cDiskMedia::GetTrack", true );

	return m_Image->GetTrack( tIndex, hIndex );
}

iSector *cDiskMedia::GetSector( int logicalCylinder, int logicalHead, int logicalSector )
{
	FUNCTION_ENTRY( this, "cDiskMedia::GetSector", true );

	if( iDiskTrack *track = m_Image->GetTrack( logicalCylinder, logicalHead ))
	{
		if( iDiskSector *sector = track->GetSector( logicalCylinder, logicalHead, logicalSector ))
		{
			return sector;
		}
	}

	DBG_WARNING( "Sector " << logicalSector << " (t:" << logicalCylinder << ",h:" << logicalHead << ") not found" );

	return nullptr;
}

void cDiskMedia::LoadFile( )
{
	for( size_t h = 0; h < m_Image->GetNumHeads( ); h++ )
	{
		for( size_t t = 0; t < m_Image->GetNumTracks( ); t++ )
		{
			m_Image->GetTrack( t, h );
		}
	}
}
