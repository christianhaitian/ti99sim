//----------------------------------------------------------------------------
//
// File:		tms9901.hpp
// Date:		22-Seo-2011
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

#ifndef ITMS9901_HPP_
#define ITMS9901_HPP_

#include "common.hpp"
#include "iBaseObject.hpp"

//
// Virtual keys
//

enum VIRTUAL_KEY_E
{
	VK_NONE,
	VK_ENTER, VK_SPACE, VK_COMMA, VK_PERIOD, VK_DIVIDE, VK_SEMICOLON, VK_EQUALS,
	VK_CAPSLOCK, VK_SHIFT, VK_CTRL, VK_FCTN,
	VK_0, VK_1, VK_2, VK_3, VK_4, VK_5, VK_6, VK_7, VK_8, VK_9,
	VK_A, VK_B, VK_C, VK_D, VK_E, VK_F, VK_G, VK_H, VK_I, VK_J, VK_K, VK_L, VK_M,
	VK_N, VK_O, VK_P, VK_Q, VK_R, VK_S, VK_T, VK_U, VK_V, VK_W, VK_X, VK_Y, VK_Z,
	VK_MAX
};

struct iTMS9901 :
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

	//
	// Methods to handle the keyboard/joysticks
	//
	virtual void VKeyUp( int sym ) = 0;
	virtual void VKeyDown( int sym, VIRTUAL_KEY_E vkey ) = 0;
	virtual void VKeysDown(int sym, VIRTUAL_KEY_E, VIRTUAL_KEY_E = VK_NONE ) = 0;

	virtual void HideShiftKey( ) = 0;
	virtual void UnHideShiftKey( ) = 0;

	virtual UINT8 GetKeyState( VIRTUAL_KEY_E ) = 0;

	virtual void SetJoystickX( int, int ) = 0;
	virtual void SetJoystickY( int, int ) = 0;
	virtual void SetJoystickButton( int, bool ) = 0;

protected:

	virtual ~iTMS9901( ) {}
};

#endif
