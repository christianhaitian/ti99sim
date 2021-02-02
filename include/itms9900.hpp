//----------------------------------------------------------------------------
//
// File:		itms9900.hpp
// Date:		22-Sep-2011
// Programmer:	Marc Rousseau
//
// Description:
//
// Copyright (c) 2011 Marc Rousseau, All Rights Reserved.
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

#ifndef ITMS9900_HPP_
#define ITMS9900_HPP_

#include "common.hpp"
#include "iBaseObject.hpp"

struct sTrapInfo;

typedef UINT16 ADDRESS;
typedef UINT8(*TRAP_FUNCTION)( void *, int, bool, ADDRESS, UINT8 );
typedef UINT16(*BREAKPOINT_FUNCTION)( void *, ADDRESS, bool, UINT16, bool, bool );

struct sTrapInfo
{
	void          *ptr;
	int            data;
	TRAP_FUNCTION  function;
};

// Flags for traps/breakpoints

const UINT8 MEMFLG_8BIT          = 0x04;
const UINT8 MEMFLG_FETCH         = 0x08;
const UINT8 MEMFLG_READ          = 0x10;
const UINT8 MEMFLG_WRITE         = 0x20;
const UINT8 MEMFLG_DEBUG         = 0x38;
const UINT8 MEMFLG_TRAP_READ     = 0x40;
const UINT8 MEMFLG_TRAP_WRITE    = 0x80;
const UINT8 MEMFLG_TRAP_ACCESS   = 0xC0;

struct iTMS9900 :
	virtual iBaseObject
{
	virtual void SetPC( ADDRESS address ) = 0;
	virtual void SetWP( ADDRESS address ) = 0;
	virtual void SetST( UINT16 address ) = 0;

	virtual ADDRESS GetPC( ) = 0;
	virtual ADDRESS GetWP( ) = 0;
	virtual UINT16 GetST( ) = 0;

	virtual void Run( ) = 0;
	virtual void Stop( ) = 0;
	virtual bool Step( ) = 0;

	virtual bool IsRunning( ) = 0;

	virtual void Reset( ) = 0;
	virtual void SignalInterrupt( UINT8 ) = 0;
	virtual void ClearInterrupt( UINT8 ) = 0;

	virtual UINT32 GetClocks( ) = 0;
	virtual void AddClocks( int ) = 0;
	virtual void ResetClocks( ) = 0;

	virtual UINT32 GetCounter( ) = 0;
	virtual void ResetCounter( ) = 0;

	virtual UINT8 RegisterTrapHandler( TRAP_FUNCTION, void *, int ) = 0;
	virtual void DeRegisterTrapHandler( UINT8 ) = 0;

	virtual UINT8 GetTrapIndex( TRAP_FUNCTION, int ) = 0;
	virtual bool SetTrap( ADDRESS, UINT8, UINT8 ) = 0;

	virtual void ClearTrap( UINT8, ADDRESS, int ) = 0;

	virtual void RegisterDebugHandler( BREAKPOINT_FUNCTION, void * ) = 0;
	virtual void DeRegisterDebugHandler( ) = 0;
	virtual bool SetBreakpoint( ADDRESS, UINT8 ) = 0;
	virtual bool ClearBreakpoint( ADDRESS, UINT8 ) = 0;

protected:

	virtual ~iTMS9900( ) {}
};

#endif
