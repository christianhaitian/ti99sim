//----------------------------------------------------------------------------
//
// File:		disk-sector.cpp
// Date:		01-Jul-2015
// Programmer:	Marc Rousseau
//
// Description: A class to manipulate disk sectors
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
#include "disk-sector.hpp"
#include "disk-track.hpp"

DBG_REGISTER( __FILE__ );

//----------------------------------------------------------------------------
// cDiskSector
//----------------------------------------------------------------------------

cDiskSector::cDiskSector( cDiskTrack *track, UINT8 *id, UINT8 *data ) :
	Track( track ),
	ID( id ),
	Data( data )
{
}

sDataBuffer cDiskSector::Read( ) const
{
	return { Data + 1, Data + Size( ) + 1 };
}

bool cDiskSector::Write( const sDataBuffer &data )
{
	FUNCTION_ENTRY( this, "cDiskSector::Write", true );

	return Write( MARK_DAM, data );
}

bool cDiskSector::Write( UINT8 dataMark, const sDataBuffer &data )
{
	FUNCTION_ENTRY( this, "cDiskSector::Write", true );

	bool isDirty = false;

	UINT8 *ptr = Data;

	auto writeByte = [&]( UINT8 byte )
	{
		if( *ptr != byte )
		{
			isDirty = true;
		}
		*ptr++ = byte;
	};

	writeByte( dataMark );

	for( size_t i = 0; i < Size( ); i++ )
	{
		writeByte(( i < data.size( )) ? data[ i ] : 0xFF );
	}

	if( isDirty )
	{
		Track->DataModified( Data, Size( ));
	}

	return true;
}

bool cDiskSector::ValidID( ) const
{
	FUNCTION_ENTRY( this, "cDiskSector::ValidID", true );

	return Track->VerifyID( ID );
}

bool cDiskSector::ValidData( ) const
{
	FUNCTION_ENTRY( this, "cDiskSector::ValidData", true );

	return Track->VerifyData( Data, Size( ));
}

bool cDiskSector::Matches( int logicalCylinder, int logicalHead, int logicalSector ) const
{
	return ((( logicalCylinder == -1 ) || ( LogicalCylinder( ) == logicalCylinder )) &&
	        (( logicalHead     == -1 ) || ( LogicalHead( )     == logicalHead     )) &&
	        (( logicalSector   == -1 ) || ( LogicalSector( )   == logicalSector   ))) ? true : false;
}
