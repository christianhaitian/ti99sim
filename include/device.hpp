
//----------------------------------------------------------------------------
//
// File:		device.hpp
// Date:		27-Mar-1998
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

#ifndef DEVICE_HPP_
#define DEVICE_HPP_

#include "cBaseObject.hpp"
#include "istateobject.hpp"
#include "idevice.hpp"
#include <set>
#include <map>

class cDevice :
	public virtual cBaseObject,
//	public virtual iStateObject,
	public virtual iDevice
{
protected:

	cRefPtr<iCartridge> m_pROM;
	iComputer          *m_pComputer;
	iTMS9900           *m_pCPU;
	UINT16              m_CRU;
	bool                m_IsValid;
	bool                m_IsActive;
	bool                m_IsRecognized;
	UINT8               m_TrapIndex;

	typedef std::map<ADDRESS,std::set<ADDRESS>> DataPool;

	DataPool            m_mapMemRead;
	DataPool            m_mapMemWrite;
	DataPool            m_mapIORead;
	DataPool            m_mapIOWrite;

public:

	cDevice( iCartridge *rom );

	// iBaseObject Methods
	virtual const void *GetInterface( const std::string &name ) const override;

	// iStateObject Methods
//	virtual std::string GetIdentifier( ) override;
//	virtual std::optional<sStateSection> SaveState( ) override;
//	virtual bool ParseState( const sStateSection &state ) override;

	// iDevice Methods
	virtual bool Initialize( iComputer *computer ) override;
	virtual int GetCRU( ) const override			{ return m_CRU; }
	virtual iCartridge *GetROM( ) const override	{ return m_pROM; }
	virtual const char *GetName( ) override			{ return "Unknown"; }
	virtual void WriteCRU( ADDRESS, int ) override;
	virtual int ReadCRU( ADDRESS ) override;

protected:

	static void DumpDataPool( const DataPool &pool, const std::string &header );
	static UINT8 TrapFunction( void *, int, bool, ADDRESS, UINT8 );

	bool RegisterTrapHandler( );
	void DeRegisterTrapHandler( );

	void EnableDevice( bool enable );
	void Activate( );
	void DeActivate( );

	virtual void ActivateInternal( );
	virtual void DeActivateInternal( );

	virtual UINT8 WriteMemory( ADDRESS, UINT8 );
	virtual UINT8 ReadMemory( ADDRESS, UINT8 );

	virtual ~cDevice( ) override;

private:

	cDevice( const cDevice & ) = delete;			// no implementation
	void operator =( const cDevice & ) = delete;	// no implementation

};

#endif
