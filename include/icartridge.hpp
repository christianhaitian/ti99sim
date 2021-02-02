//----------------------------------------------------------------------------
//
// File:		icartridge.hpp
// Date:		22-Sep-2011
// Programmer:	Marc Rousseau
//
// Description:
//
// Copyright (c) 20114 Marc Rousseau, All Rights Reserved.
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

#ifndef ICARTRIDGE_HPP_
#define ICARTRIDGE_HPP_

#include <list>
#include <string>
#include "common.hpp"
#include "iBaseObject.hpp"

#define ROM_BANK_SIZE	0x1000
#define GROM_BANK_SIZE	0x2000

enum BANK_TYPE_E
{
	BANK_UNKNOWN,
	BANK_RAM,
	BANK_ROM,
	BANK_BATTERY_BACKED
};

constexpr UINT8 FLAG_BATTERY_BACKED = 0x01;
constexpr UINT8 FLAG_READ_ONLY      = 0x02;

struct sMemoryBank
{
	BANK_TYPE_E    Type;
	UINT8          Flags;
	UINT8         *Data;
};

struct sMemoryRegion
{
	int            NumBanks;
	sMemoryBank   *CurBank;
	sMemoryBank    Bank[ 256 ];
};

#define NUM_ROM_BANKS	16
#define NUM_GROM_BANKS	 8

/// A container class for the TI-99/4A system ROM, device ROMs, ROM/GROM cartridges, Gram Kracker, ...
/// The cCartridge class stores information about the various banks of memory in a particular device
/// that can be plugged into the TI-99/4A. Each bank can be either ROM, RAM, or battery-backed RAM.
///
struct iCartridge :
	virtual iBaseObject
{
	virtual const char *GetFileName( ) const = 0;

	virtual void SetTitle( const char * ) = 0;
	virtual const char *GetTitle( ) const = 0;

	virtual void SetCRU( UINT16 cru ) = 0;
	virtual UINT16 GetCRU( ) const = 0;

	virtual std::string sha1( ) const = 0;
	virtual std::string GetDescriptor( ) const = 0;

	virtual void SetFeature( const char *, const char * ) = 0;
	virtual const char *GetFeature( const char * ) const = 0;
	virtual std::list<std::string> GetFeatures( ) const = 0;

	virtual sMemoryRegion *GetCpuMemory( size_t ) = 0;
	virtual sMemoryRegion *GetGromMemory( size_t ) = 0;

	virtual bool IsValid( ) const = 0;
	virtual void PrintInfo( FILE * ) const = 0;

	virtual bool LoadImage( const char * ) = 0;
	virtual bool SaveImage( const char * ) = 0;

protected:

	virtual ~iCartridge( ) {}
};

#endif
