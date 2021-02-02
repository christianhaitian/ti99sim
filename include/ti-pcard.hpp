
//----------------------------------------------------------------------------
//
// File:        ti-pcard.hpp
// Date:		26-Jun-2013
// Programmer:  Marc Rousseau
//
// Description:
//
// Copyright (c) 2013-2016 Marc Rousseau, All Rights Reserved.
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

#ifndef TIPCARD_HPP_
#define TIPCARD_HPP_

#include "device.hpp"
#include "icartridge.hpp"

class cUCSDDevice :
	public virtual cStateObject,
	public virtual cDevice
{
private:

	// CRU bits
	bool                m_BankSwapped;

	// Grom Information
	ADDRESS             m_GromAddress;
	int                 m_GromReadShift;
	int                 m_GromWriteShift;

	sMemoryRegion       m_GromMemory[ NUM_GROM_BANKS ];

public:

	cUCSDDevice( iCartridge * );

	// iBaseObject Methods
	virtual const void *GetInterface( const std::string &name ) const override;

	// iStateObject Methods
	virtual std::string GetIdentifier( ) override;
	virtual std::optional<sStateSection> SaveState( ) override;
	virtual bool ParseState( const sStateSection &state ) override;

	// iDevice methods
	virtual const char *GetName( ) override;
	virtual bool Initialize( iComputer *computer ) override;
	virtual void WriteCRU( ADDRESS, int ) override;
	virtual int ReadCRU( ADDRESS ) override;

private:

	// cDevice Methods
	virtual void ActivateInternal( ) override;
	virtual UINT8 WriteMemory( ADDRESS, UINT8 ) override;
	virtual UINT8 ReadMemory( ADDRESS, UINT8 ) override;

private:

	virtual ~cUCSDDevice( ) override;

	// Disable the copy constructor and assignment operator defaults
	cUCSDDevice( const cUCSDDevice & ) = delete;		// no implementation
	void operator =( const cUCSDDevice & ) = delete;	// no implementation

};

#endif
