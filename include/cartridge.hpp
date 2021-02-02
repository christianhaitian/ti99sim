//----------------------------------------------------------------------------
//
// File:		cartridge.hpp
// Date:		23-Feb-1998
// Programmer:	Marc Rousseau
//
// Description:
//
// Copyright (c) 1998-2004 Marc Rousseau, All Rights Reserved.
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

#ifndef CARTRIDGE_HPP_
#define CARTRIDGE_HPP_

#include "cBaseObject.hpp"
#include "istateobject.hpp"
#include "icartridge.hpp"

class cCartridge :
	public virtual cBaseObject,
	public virtual iCartridge,
	public virtual iStateObject
{
	static const std::string sm_Banner;

	std::filesystem::path   m_FileName;
	std::filesystem::path   m_RamFileName;
	std::string             m_Title;
	UINT16                  m_BaseCRU;

	sMemoryRegion           m_CpuMemory[ NUM_ROM_BANKS ];
	sMemoryRegion           m_GromMemory[ NUM_GROM_BANKS ];

	std::map<std::string,std::string>  m_Features;

// Manufacturer
// Copyright/date
// Catalog Number
// Icon/Image

public:

	cCartridge( const std::string & );

	static cRefPtr<cCartridge> LoadCartridge( const std::string &description, const std::string &folder );

	// iBaseObject Methods
	virtual const void *GetInterface( const std::string &name ) const override;

	// iCartridge Methods
	virtual const char *GetFileName( ) const override;
	virtual void SetTitle( const char * ) override;
	virtual const char *GetTitle( ) const override;
	virtual void SetCRU( UINT16 cru ) override;
	virtual UINT16 GetCRU( ) const override;
	virtual std::string sha1( ) const override;
	virtual std::string GetDescriptor( ) const override;
	virtual void SetFeature( const char *, const char * ) override;
	virtual const char *GetFeature( const char * ) const override;
	virtual std::list<std::string> GetFeatures( ) const override;
	virtual sMemoryRegion *GetCpuMemory( size_t ) override;
	virtual sMemoryRegion *GetGromMemory( size_t ) override;
	virtual bool IsValid( ) const override;
	virtual void PrintInfo( FILE * ) const override;
	virtual bool LoadImage( const char * ) override;
	virtual bool SaveImage( const char * ) override;

	// iStateObject Methods
	virtual std::string GetIdentifier( ) override;
	virtual std::optional<sStateSection> SaveState( ) override;
	virtual bool ParseState( const sStateSection &state ) override;

protected:

	virtual ~cCartridge( ) override;

private:

	cCartridge( const cCartridge & ) = delete;		// no implementation
	void operator =( const cCartridge & ) = delete;	// no implementation

	void SetFileName( const char * );

	void DumpRegion( FILE *, const char *, const sMemoryRegion *, size_t, size_t, BANK_TYPE_E, bool ) const;

	bool LoadRAM( ) const;
	bool SaveRAM( ) const;

	static bool EncodeCallback( void *, size_t, void * );
	static bool DecodeCallback( void *, size_t, void * );

	static bool SaveBufferLZW( const void *, size_t, FILE * );

	static bool LoadBufferLZW( void *, size_t, FILE * );
	static bool LoadBufferRLE( void *, size_t, FILE * );

	bool LoadImageV0( FILE * );
	bool LoadImageV1( FILE * );
	bool LoadImageV2( FILE * );

};

#endif
