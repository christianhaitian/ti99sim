//----------------------------------------------------------------------------
//
// File:		itms9918a.hpp
// Date:		24-Aug-2011
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

#ifndef ITMS9918A_HPP_
#define ITMS9918A_HPP_

#include "common.hpp"
#include "iBaseObject.hpp"

struct iTMS9901;

struct iTMS9918A :
	virtual iBaseObject
{
	virtual void SetMemory( UINT8 * ) = 0;

	virtual void SetPIC( iTMS9901 *pic, int level ) = 0;

	virtual void Reset( ) = 0;

	virtual void SetAddress( UINT8 address ) = 0;
	virtual UINT16 GetAddress( ) = 0;

	virtual void WriteData( UINT8 data ) = 0;
	virtual void WriteRegister( size_t reg, UINT8 value ) = 0;

	virtual UINT8 ReadData( ) = 0;
	virtual UINT8 ReadRegister( size_t reg ) = 0;

	virtual UINT8 ReadStatus( ) = 0;

	virtual bool Retrace( ) = 0;
	virtual void Render( ) = 0;

protected:

	virtual ~iTMS9918A( ) {}
};

#endif
