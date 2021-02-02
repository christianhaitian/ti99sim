
//----------------------------------------------------------------------------
//
// File:		tms9900.hpp
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

#ifndef TMS9900_HPP_
#define TMS9900_HPP_

#include "cBaseObject.hpp"
#include "stateobject.hpp"
#include "itms9900.hpp"

const int TMS_LOGICAL          = 0x8000;
const int TMS_ARITHMETIC       = 0x4000;
const int TMS_EQUAL            = 0x2000;
const int TMS_CARRY            = 0x1000;
const int TMS_OVERFLOW         = 0x0800;
const int TMS_PARITY           = 0x0400;
const int TMS_XOP              = 0x0200;

class cTMS9900 :
	public virtual cBaseObject,
	public virtual cStateObject,
	public virtual iTMS9900
{
public:

	cTMS9900( );

	// iBaseObject Methods
	virtual const void *GetInterface( const std::string &name ) const override;

	// iStateObject Methods
	virtual std::string GetIdentifier( ) override;
	virtual std::optional<sStateSection> SaveState( ) override;
	virtual bool ParseState( const sStateSection &state ) override;

	// iTMS9900 Methods
	virtual void SetPC( ADDRESS address ) override;
	virtual void SetWP( ADDRESS address ) override;
	virtual void SetST( UINT16 address ) override;
	virtual ADDRESS GetPC( ) override;
	virtual ADDRESS GetWP( ) override;
	virtual UINT16 GetST( ) override;
	virtual void Run( ) override;
	virtual void Stop( ) override;
	virtual bool Step( ) override;
	virtual bool IsRunning( ) override;
	virtual void Reset( ) override;
	virtual void SignalInterrupt( UINT8 ) override;
	virtual void ClearInterrupt( UINT8 ) override;
	virtual UINT32 GetClocks( ) override;
	virtual void AddClocks( int ) override;
	virtual void ResetClocks( ) override;
	virtual UINT32 GetCounter( ) override;
	virtual void ResetCounter( ) override;
	virtual UINT8 RegisterTrapHandler( TRAP_FUNCTION, void *, int ) override;
	virtual void DeRegisterTrapHandler( UINT8 ) override;
	virtual UINT8 GetTrapIndex( TRAP_FUNCTION, int ) override;
	virtual bool SetTrap( ADDRESS, UINT8, UINT8 ) override;
	virtual void ClearTrap( UINT8, ADDRESS, int ) override;
	virtual void RegisterDebugHandler( BREAKPOINT_FUNCTION, void * ) override;
	virtual void DeRegisterDebugHandler( ) override;
	virtual bool SetBreakpoint( ADDRESS, UINT8 ) override;
	virtual bool ClearBreakpoint( ADDRESS, UINT8 ) override;

protected:

	virtual ~cTMS9900( ) override;

private:

	cTMS9900( const cTMS9900 & ) = delete;				// no implementation
	cTMS9900 &operator =( const cTMS9900 & ) = delete;	// no implementation

};

#endif
