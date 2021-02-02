//----------------------------------------------------------------------------
//
// File:        disk-serializer.cpp
// Date:        04-May-2015
// Programmer:  Marc Rousseau
//
// Description: A class to load/save AnaDisk disk images
//
// Copyright (c) 2015 Marc Rousseau, All Rights Reserved.
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

#include "common.hpp"
#include "logger.hpp"
#include "support.hpp"
#include "disk-media.hpp"
#include "disk-serializer.hpp"

DBG_REGISTER( __FILE__ );

cDiskSerializer::cDiskSerializer( ) :
	m_DemandLoadFile( nullptr )
{
}

cDiskSerializer::~cDiskSerializer( )
{
	if( m_DemandLoadFile )
	{
		fclose( m_DemandLoadFile );
	}
}

bool cDiskSerializer::SupportsFeatures( const cDiskImage &image )
{
	FUNCTION_ENTRY( this, "cDiskSerializer::SupportsFeatures", true );

	return true;
}

size_t cDiskSerializer::GetVolume( ) const
{
	FUNCTION_ENTRY( this, "cDiskSerializer::GetVolume", true );

	return 0;
}

size_t cDiskSerializer::MaxVolume( ) const
{
	FUNCTION_ENTRY( this, "cDiskSerializer::MaxVolume", true );

	return 0;
}

std::string cDiskSerializer::RawFileName( const char *filename ) const
{
	FUNCTION_ENTRY( this, "cDiskSerializer::RawFileName", true );

	return filename;
}

bool cDiskSerializer::LoadFile( const char *filename, cDiskImage *image )
{
	FUNCTION_ENTRY( this, "cDiskSerializer::LoadFile", true );

	std::string validName = LocateFile( "disks", RawFileName( filename ));

	if( ! validName.empty( ))
	{
		if( FILE *file = OpenForRead( validName ))
		{
			bool status = ReadFile( file, image );

			image->ClearChanged( );

			if( m_DemandLoadFile != file )
			{
				fclose( file );
			}

			return status;
		}
	}

	return false;
}

bool cDiskSerializer::SaveFile( const cDiskImage &image, const char *filename )
{
	FUNCTION_ENTRY( this, "cDiskSerializer::SaveFile", true );

	std::string validName = LocateFile( "disks", RawFileName( filename ));

	if( validName.empty( ))
	{
		validName = RawFileName( filename );
	}

	image.CompleteLoad( );

	if( FILE *file = OpenForWrite( validName ))
	{
		bool status = WriteFile( image, file );

		fclose( file );

		return status;
	}

	return false;
}

bool cDiskSerializer::LoadTrack( size_t cylinder, size_t head, iDiskTrack *track )
{
	return false;
}

FILE *cDiskSerializer::OpenForRead( const std::string &name )
{
	FUNCTION_ENTRY( this, "cDiskSerializer::OpenForRead", true );

	return fopen( name.c_str( ), "rb" );
}

FILE *cDiskSerializer::OpenForWrite( const std::string &name )
{
	FUNCTION_ENTRY( this, "cDiskSerializer::OpenForWrite", true );

	return fopen( name.c_str( ), "wb" );
}

void cDiskSerializer::LoadComplete( )
{
	if( m_DemandLoadFile )
	{
		fclose( m_DemandLoadFile );
		m_DemandLoadFile = nullptr;
	}
}
