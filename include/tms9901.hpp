
//----------------------------------------------------------------------------
//
// File:		tms9901.hpp
// Date:		18-Dec-2001
// Programmer:	Marc Rousseau
//
// Description:
//
// Copyright (c) 2001-2004 Marc Rousseau, All Rights Reserved.
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

#ifndef TMS9901_HPP_
#define TMS9901_HPP_

#include "cBaseObject.hpp"
#include "device.hpp"
#include "itms9901.hpp"

class cTMS9901 :
	public virtual cBaseObject,
	public virtual cStateObject,
	public virtual cDevice,
	public virtual iTMS9901
{
	struct sJoystickInfo
	{
		bool     isPressed;
		int      x_Axis;
		int      y_Axis;
	};

	bool                m_TimerActive;
	int                 m_ReadRegister;
	int                 m_Decrementer;
	int                 m_ClockRegister;
	uint8_t             m_PinState[ 32 ][ 2 ];
	int                 m_InterruptRequested;
	int                 m_ActiveInterrupts;

	int                 m_LastDelta;
	UINT32              m_DecrementClock;

	bool                m_CapsLock;
	int                 m_ColumnSelect;
	int                 m_HideShift;
	UINT8               m_StateTable[ VK_MAX ];
	VIRTUAL_KEY_E       m_KSLinkTable[ 512 ][ 2 ];
	sJoystickInfo       m_Joystick[ 2 ];

public:

	cTMS9901( );

	// iBaseObject Methods
	virtual const void *GetInterface( const std::string &name ) const override;

	// iStateObject Methods
	virtual std::string GetIdentifier( ) override;
	virtual std::optional<sStateSection> SaveState( ) override;
	virtual bool ParseState( const sStateSection &state ) override;

	// iDevice methods
	virtual const char *GetName( ) override;
	virtual void WriteCRU( ADDRESS, int ) override;
	virtual int ReadCRU( ADDRESS ) override;

	// iTMS9901 methods
	virtual void UpdateTimer( UINT32 ) override;
	virtual void HardwareReset( ) override;
	virtual void SoftwareReset( ) override;
	virtual void SignalInterrupt( int ) override;
	virtual void ClearInterrupt( int ) override;
	virtual void VKeyUp( int sym ) override;
	virtual void VKeyDown( int sym, VIRTUAL_KEY_E vkey ) override;
	virtual void VKeysDown(int sym, VIRTUAL_KEY_E, VIRTUAL_KEY_E = VK_NONE ) override;
	virtual void HideShiftKey( ) override;
	virtual void UnHideShiftKey( ) override;
	virtual UINT8 GetKeyState( VIRTUAL_KEY_E ) override;
	virtual void SetJoystickX( int, int ) override;
	virtual void SetJoystickY( int, int ) override;
	virtual void SetJoystickButton( int, bool ) override;

protected:

	virtual ~cTMS9901( ) override;

private:

	cTMS9901( const cTMS9901 & ) = delete;				// no implementation
	cTMS9901 &operator =( const cTMS9901 & ) = delete;	// no implementation

};

#endif
