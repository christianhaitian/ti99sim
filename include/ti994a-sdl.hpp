//----------------------------------------------------------------------------
//
// File:		ti994a-sdl.hpp
// Date:		18-Apr-2000
// Programmer:	Marc Rousseau
//
// Description:
//
// Copyright (c) 2000-2004 Marc Rousseau, All Rights Reserved.
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

#ifndef TI994A_SDL_HPP_
#define TI994A_SDL_HPP_

#include <atomic>
#include "ti994a-gk.hpp"

struct SDL_Thread;

class cSdlTI994A :
	public cTI994AGK
{
	UINT32              m_StartTime;
	UINT32              m_StartClock;

	UINT32              m_VideoUpdateEvent;
	std::atomic<int>    m_RefreshCount;

	SDL_Thread         *m_pThread;
	SDL_sem            *m_SleepSem;
	SDL_sem            *m_WaitSem;

	int                 m_JoystickMap[ 2 ];
	int                 m_JoystickPosX[ 2 ];
	int                 m_JoystickPosY[ 2 ];

	SDL_Keycode         m_ActiveKeycode[ SDL_NUM_SCANCODES ];

public:

	cSdlTI994A( iCartridge *, iTMS9918A * = nullptr, iTMS9919 * = nullptr, iTMS5220 * = nullptr );

	// iComputer methods
	virtual void Reset( ) override;
	virtual bool Sleep( int, UINT32 ) override;
	virtual bool WakeCPU( UINT32 ) override;
	virtual void Run( ) override;
	virtual bool SaveImage( const char * ) override;
	virtual bool LoadImage( const char * ) override;

	void SetJoystick( int, SDL_Joystick * );

protected:

	int FindJoystick( int );

	void KeyPressed( SDL_Keysym keysym );
	void KeyReleased( SDL_Keysym keysym );

	void StartThread( );
	void StopThread( );

	// cTI994A methods
	virtual void TimerHookProc( UINT32 clockCycles ) override;
	virtual bool VideoRetrace( ) override;

	static int _RunThreadProc( void * );
	int RunThreadProc( );

private:

	virtual ~cSdlTI994A( ) override;

	cSdlTI994A( const cSdlTI994A & ) = delete;		// no implementation
	void operator =( const cSdlTI994A & ) = delete;	// no implementation

};

#endif
