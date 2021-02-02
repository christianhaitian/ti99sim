//----------------------------------------------------------------------------
//
// File:		ti994a-console.hpp
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

#ifndef TI994A_CONSOLE_HPP_
#define TI994A_CONSOLE_HPP_

#include "ti994a.hpp"

#define FCTN_KEY        0x0100
#define SHIFT_KEY       0x0200
#define CTRL_KEY        0x0400
#define CAPS_LOCK_KEY   0x0800

class cConsoleTI994A :
	public cTI994A
{
	bool                m_CapsLock;
	int                 m_ColumnSelect;
	int                 m_KeyHead;
	int                 m_KeyTail;
	int                 m_KeyBuffer[ 50 ];
	int                 m_GromCounter;

public:

	cConsoleTI994A( iCartridge *ctg, iTMS9918A * = nullptr );

	// iComputer methods
	virtual int ReadCRU( ADDRESS ) override;
	virtual void WriteCRU( ADDRESS, UINT16 ) override;
	virtual void Run( ) override;
	virtual bool Step( ) override;
	virtual bool LoadImage( const char * ) override;

protected:

	void Refresh( bool );

	void KeyPressed( int ch );
	void EditRegisters( );

	// cTI994A virtual functions
	virtual UINT8 VideoReadBreakPoint( ADDRESS address, UINT8 data ) override;
	virtual UINT8 VideoWriteBreakPoint( ADDRESS address, UINT8 data ) override;
	virtual UINT8 GromReadBreakPoint( ADDRESS address, UINT8 data ) override;
	virtual UINT8 GromWriteBreakPoint( ADDRESS address, UINT8 data ) override;

	UINT16 DebugHandler( ADDRESS address, bool isWord, UINT16 data, bool read, bool fetch );

	static UINT16 _DebugHandler( void *, ADDRESS address, bool isWord, UINT16 data, bool read, bool fetch );

protected:

	virtual ~cConsoleTI994A( ) override;

private:

	cConsoleTI994A( const cConsoleTI994A & ) = delete;				// no implementation
	cConsoleTI994A & operator =( const cConsoleTI994A & ) = delete;	// no implementation

};

#endif
