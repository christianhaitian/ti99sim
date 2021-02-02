//----------------------------------------------------------------------------
//
// File:		tms9918a-console.hpp
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

#ifndef TMS9918A_CONSOLE_HPP_
#define TMS9918A_CONSOLE_HPP_

#include "tms9918a.hpp"

class cConsoleTMS9918A :
	public cTMS9918A
{
protected:

	UINT8               m_Bias;
	int                 m_Width;

	// cTMS9918A protected methods
	virtual bool SetMode( int ) override;
	virtual void FlipAddressing( ) override;

public:

	cConsoleTMS9918A( int );

	void SetBias( UINT8 );

	// cTMS9918A public methods
	virtual void Reset( ) override;
	virtual void WriteData( UINT8 ) override;
	virtual void WriteRegister( size_t, UINT8 ) override;
	virtual void Render( ) override;

protected:

	virtual ~cConsoleTMS9918A( ) override;

private:

	cConsoleTMS9918A( const cConsoleTMS9918A & ) = delete;	// no implementation
	void operator =( const cConsoleTMS9918A & ) = delete;	// no implementation

};

#endif
