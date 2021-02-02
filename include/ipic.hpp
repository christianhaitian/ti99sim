//----------------------------------------------------------------------------
//
// File:		ipic.hpp
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

#ifndef IPIC_HPP_
#define IPIC_HPP_

#include "common.hpp"
#include "iBaseObject.hpp"

struct iPIC :
	virtual iBaseObject
{
	virtual void UpdateTimer( UINT32 ) = 0;
	virtual void HardwareReset( ) = 0;
	virtual void SoftwareReset( ) = 0;

	//
	// Methods used by devices to signal an interrupt to the CPU
	//
	virtual void SignalInterrupt( int ) = 0;
	virtual void ClearInterrupt( int ) = 0;

/*
	//
	// Methods to handle the keyboard/joysticks
	//
	virtual void VKeyUp( int sym ) = 0;
	virtual void VKeyDown( int sym, VIRTUAL_KEY_E vkey ) = 0;
	virtual void VKeysDown(int sym, VIRTUAL_KEY_E, VIRTUAL_KEY_E = VK_NONE ) = 0;

	virtual UINT8 GetKeyState( VIRTUAL_KEY_E ) = 0;

	virtual void SetJoystickX( int, int ) = 0;
	virtual void SetJoystickY( int, int ) = 0;
	virtual void SetJoystickButton( int, bool ) = 0;
*/

protected:

	virtual ~iPIC( ) {}
};

#endif
