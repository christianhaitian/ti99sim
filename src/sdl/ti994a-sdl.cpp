//----------------------------------------------------------------------------
//
// File:        ti994a-sdl.cpp
// Date:        18-Apr-2000
// Programmer:  Marc Rousseau
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
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307, USA.
//
// Revision History:
//
//----------------------------------------------------------------------------

#include <SDL.h>
#include <SDL_thread.h>
#include "common.hpp"
#include "logger.hpp"
#include "cartridge.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "tms9918a-sdl.hpp"
#include "ti994a-sdl.hpp"
#include "tms9901.hpp"
#include "support.hpp"

DBG_REGISTER( __FILE__ );

const char SAVE_IMAGE[ ] = "ti-994a.img";

extern int verbose;

cSdlTI994A::cSdlTI994A( iCartridge *ctg, iTMS9918A *vdp, iTMS9919 *sound, iTMS5220 *speech ) :
	cBaseObject( "cSdlTI994A" ),
	cTI994AGK( ctg, vdp, sound, speech ),
	m_StartTime{ 0 },
	m_StartClock{ 0 },
	m_VideoUpdateEvent( SDL_RegisterEvents( 1 )),
	m_RefreshCount{ 0 },
	m_pThread{ nullptr },
	m_SleepSem{ nullptr },
	m_WaitSem{ nullptr },
	m_JoystickMap{ -1, -1 },
	m_JoystickPosX{ 0, 0 },
	m_JoystickPosY{ 0, 0 },
	m_ActiveKeycode{ }
{
	FUNCTION_ENTRY( this, "cSdlTI994A ctor", true );

	m_SleepSem = SDL_CreateSemaphore( 0 );
	m_WaitSem  = SDL_CreateSemaphore( 0 );

	if(( m_SleepSem == nullptr ) || ( m_WaitSem == nullptr ))
	{
		DBG_WARNING( "Unable to create SDL semaphore" );
	}
}

cSdlTI994A::~cSdlTI994A( )
{
	FUNCTION_ENTRY( this, "cSdlTI994A dtor", true );

	if( m_SleepSem != nullptr )
	{
		SDL_DestroySemaphore( m_SleepSem );
	}
	if( m_WaitSem != nullptr )
	{
		SDL_DestroySemaphore( m_WaitSem );
	}
}

bool cSdlTI994A::Sleep( int cycles, UINT32 timeout )
{
	FUNCTION_ENTRY( this, "cSdlTI994A::Sleep", false );

	cTI994A::Sleep( cycles, timeout );

	// Don't do anything fancy if we don't have both semaphores
	if(( m_SleepSem == nullptr ) || ( m_WaitSem == nullptr ))
	{
		SDL_Delay( timeout );
		return false;
	}

	// Signal to any waiting threads that we've run
	if( SDL_SemValue( m_WaitSem ) <= 0 )
	{
		SDL_SemPost( m_WaitSem );
	}

	// Put the CPU to sleep a bit, but wakeup up as soon as someone else starts to wait
	if( SDL_SemWaitTimeout( m_SleepSem, timeout ) == 0 )
	{
		DBG_TRACE( "CPU woken up early" );
		return true;
	}

	return false;
}

bool cSdlTI994A::WakeCPU( UINT32 timeout )
{
	FUNCTION_ENTRY( this, "cSdlTI994A::WakeCPU", true );

	// Don't do anything fancy if we don't have both semaphores
	if(( m_SleepSem == nullptr ) || ( m_WaitSem == nullptr ))
	{
		SDL_Delay( timeout );
		return false;
	}

	// Tell the CPU to wake up if it's sleeping
	if( SDL_SemValue( m_SleepSem ) <= 0 )
	{
		DBG_TRACE("Waking up CPU");
		SDL_SemPost( m_SleepSem );
	}

	// Wait for the CPU to start running
	if( SDL_SemWaitTimeout( m_WaitSem, timeout ) == 0 )
	{
		DBG_TRACE( "CPU has run - woke up early" );
		return true;
	}

	return false;
}

void cSdlTI994A::Reset( )
{
	FUNCTION_ENTRY( this, "cSdlTI994A::Reset", true );

	bool isRunning = m_CPU->IsRunning( );

	if( isRunning )
	{
		StopThread( );
	}

	cTI994A::Reset( );

	if( isRunning )
	{
		StartThread( );
	}
}

void cSdlTI994A::Run( )
{
	FUNCTION_ENTRY( this, "cSdlTI994A::Run", true );

	StartThread( );

	int joystick;

	iTMS9901 *pic = m_PIC;
	cSdlTMS9918A *vdp = dynamic_cast<cSdlTMS9918A *>( m_VDP.get( ));

	// Set the initial state for CAPS lock
	if(( SDL_GetModState( ) & KMOD_CAPS ) != 0 )
	{
		pic->VKeyDown( SDL_SCANCODE_CAPSLOCK, VK_CAPSLOCK );
	}

	// Loop waiting for SDL_QUIT
	SDL_Event event{ };
	while( SDL_WaitEvent( &event ) >= 0 )
	{
		switch( event.type )
		{
			case SDL_WINDOWEVENT :
			{
				switch( event.window.event )
				{
					case SDL_WINDOWEVENT_FOCUS_GAINED:
						// Track changes in CAPS lock when we regain focus
						if(( pic->GetKeyState( VK_CAPSLOCK ) != 0 ) && (( SDL_GetModState( ) & KMOD_CAPS ) == 0 ))
						{
							pic->VKeyUp( SDL_SCANCODE_CAPSLOCK );
						}
						if(( pic->GetKeyState( VK_CAPSLOCK ) == 0 ) && (( SDL_GetModState( ) & KMOD_CAPS ) != 0 ))
						{
							pic->VKeyDown( SDL_SCANCODE_CAPSLOCK, VK_CAPSLOCK );
						}
						break;
					case SDL_WINDOWEVENT_RESIZED :
						vdp->ResizeWindow( event.window.data1, event.window.data2 );
						break;
					case SDL_WINDOWEVENT_EXPOSED :
						vdp->Render( );
						break;
				}
				break;
			}
			case SDL_JOYAXISMOTION :
				joystick = FindJoystick( event.jaxis.which );
				if( joystick != -1 )
				{
					int state = 0;
					int mag = ( event.jaxis.value < 0 ) ? -event.jaxis.value : event.jaxis.value;
					if( event.jaxis.value < -8192 )
					{
						state = -1;
					}
					if( event.jaxis.value >  8192 )
					{
						state = 1;
					}
					switch( event.jaxis.axis )
					{
						case 0 :
							if(( state == 0 ) || ( 2 * mag > m_JoystickPosY[ joystick ] ))
							{
								pic->SetJoystickX( joystick, state );
							}
							m_JoystickPosX[ joystick ] = mag;
							break;
						case 1 :
							if(( state == 0 ) || ( 2 * mag > m_JoystickPosX[ joystick ] ))
							{
								pic->SetJoystickY( joystick, -state );
							}
							m_JoystickPosY[ joystick ] = mag;
							break;
					}
				}
				break;
			case SDL_JOYBUTTONDOWN :
				joystick = FindJoystick( event.jbutton.which );
				if( joystick != -1 )
				{
					pic->SetJoystickButton( joystick, true );
					if( event.jbutton.button > 0 )
					{
						auto vkey = ( VIRTUAL_KEY_E ) ( VK_0 + std::clamp(( int ) event.jbutton.button, 1, 9 ));
						pic->VKeysDown( 0, vkey );
					}
				}
				break;
			case SDL_JOYBUTTONUP :
				joystick = FindJoystick( event.jbutton.which );
				if( joystick != -1 )
				{
					pic->SetJoystickButton( joystick, false );
					if( event.jbutton.button > 0 )
					{
						pic->VKeyUp( 0 );
					}
				}
				break;
			case SDL_KEYDOWN :
				m_ActiveKeycode[ event.key.keysym.scancode ] = event.key.keysym.sym;
				{
					SDL_Event peek;
					if( SDL_PeepEvents( &peek, 1, SDL_GETEVENT, SDL_TEXTINPUT, SDL_TEXTINPUT ) == 1 )
					{
						if( ( peek.text.text[ 0 ] & 0x80 ) == 0 )
						{
							m_ActiveKeycode[ event.key.keysym.scancode ] = peek.text.text[ 0 ];
						}
					}
				}
				if( event.key.keysym.sym == SDLK_ESCAPE )
				{
					goto done;
				}
				if(( SDL_GetModState( ) & KMOD_CTRL ) != 0 )
				{
					switch( event.key.keysym.sym )
					{
						case SDLK_F1 :
							GK_SetEnabled( ! m_GK_Enabled );
							break;
						case SDLK_F2 :
							GK_SetGRAM0( m_GK_OpSys );
							break;
						case SDLK_F3 :
							GK_SetGRAM12( m_GK_BASIC );
							break;
						case SDLK_F4 :
							GK_SetWriteProtect( WRITE_PROTECT::BANK1 );
							break;
						case SDLK_F5 :
							GK_SetWriteProtect( WRITE_PROTECT::ENABLED );
							break;
						case SDLK_F6 :
							GK_SetWriteProtect( WRITE_PROTECT::BANK2 );
							break;
						case SDLK_F7 :
							GK_SetLoader( ! m_GK_LoaderOn );
							break;
						default :
							KeyPressed( event.key.keysym );
							break;
					}
				}
				else
				{
					switch( event.key.keysym.sym )
					{
						case SDLK_F2 :
							SaveImage( SAVE_IMAGE );
							break;
						case SDLK_F3 :
							LoadImage( SAVE_IMAGE );
							break;
						case SDLK_F10 :
							Reset( );
							break;
						default :
							KeyPressed( event.key.keysym );
							break;
					}
				}
				break;
			case SDL_KEYUP :
				KeyReleased( event.key.keysym );
				m_ActiveKeycode[ event.key.keysym.scancode ] = SDLK_UNKNOWN;
				break;
			case SDL_QUIT :
				goto done;
			default :
				if( event.type == m_VideoUpdateEvent )
				{
					m_RefreshCount--;
					vdp->Render( );
				}
				break;
		}
	}

done:

	StopThread( );
}

bool cSdlTI994A::SaveImage( const char *filename )
{
	FUNCTION_ENTRY( this, "cSdlTI994A::SaveImage", true );

	bool isRunning = m_CPU->IsRunning( );

	if( isRunning )
	{
		StopThread( );
	}

	bool retVal = cTI994A::SaveImage( filename );

	if( isRunning )
	{
		StartThread( );
	}

	return retVal;
}

bool cSdlTI994A::LoadImage( const char *filename )
{
	FUNCTION_ENTRY( this, "cSdlTI994A::LoadImage", true );

	bool isRunning = m_CPU->IsRunning( );

	if( isRunning )
	{
		StopThread( );
	}

	bool retVal = cTI994A::LoadImage( filename );

	m_StartClock = m_CPU->GetClocks( );

	if( isRunning )
	{
		StartThread( );
	}

	return retVal;
}

void cSdlTI994A::SetJoystick( int index, SDL_Joystick *joystick )
{
	m_JoystickMap[ index ] = SDL_JoystickInstanceID( joystick );
}

int cSdlTI994A::FindJoystick( int index )
{
	FUNCTION_ENTRY( this, "cSdlTI994A::FindJoystick", true );

	if( index == m_JoystickMap[ 0 ] )
	{
		return 0;
	}
	if( index == m_JoystickMap[ 1 ] )
	{
		return 1;
	}

	DBG_WARNING( "Joystick " << index << " is not mapped" );

	return -1;
}

void cSdlTI994A::KeyPressed( SDL_Keysym keysym )
{
	FUNCTION_ENTRY( this, "cSdlTI994A::KeyPressed", false );

	iTMS9901 *pic = m_PIC;

	auto keycode = m_ActiveKeycode[ keysym.scancode ];

	switch( keycode )
	{
		case SDLK_QUOTE          : pic->VKeysDown( keysym.scancode, VK_FCTN,  VK_O );			break;
		case SDLK_COMMA          : pic->HideShiftKey( );
								   pic->VKeysDown( keysym.scancode, VK_COMMA );					break;
		case SDLK_LESS           : pic->VKeysDown( keysym.scancode, VK_SHIFT, VK_COMMA );		break;
		case SDLK_PERIOD         : pic->HideShiftKey( );
								   pic->VKeysDown( keysym.scancode, VK_PERIOD );				break;
		case SDLK_GREATER        : pic->VKeysDown( keysym.scancode, VK_SHIFT, VK_PERIOD );		break;
		case SDLK_SEMICOLON      : pic->HideShiftKey( );
								   pic->VKeysDown( keysym.scancode, VK_SEMICOLON );				break;
		case SDLK_COLON          : pic->VKeysDown( keysym.scancode, VK_SHIFT, VK_SEMICOLON );	break;
		case SDLK_UNDERSCORE     : pic->VKeysDown( keysym.scancode, VK_FCTN,  VK_U );			break;
		case SDLK_KP_VERTICALBAR : pic->VKeysDown( keysym.scancode, VK_FCTN,  VK_A );			break;
		case SDLK_EQUALS         : pic->HideShiftKey( );
								   pic->VKeysDown( keysym.scancode, VK_EQUALS );				break;
		case SDLK_PLUS           : pic->VKeysDown( keysym.scancode, VK_SHIFT, VK_EQUALS );		break;
		case '~'                 : pic->VKeysDown( keysym.scancode, VK_FCTN,  VK_W );			break;
		case '|'                 : pic->VKeysDown( keysym.scancode, VK_FCTN,  VK_A );			break;
		case SDLK_QUOTEDBL       : pic->VKeysDown( keysym.scancode, VK_FCTN,  VK_P );			break;
		case SDLK_QUESTION       : pic->VKeysDown( keysym.scancode, VK_FCTN,  VK_I );			break;
		case SDLK_SLASH          : pic->HideShiftKey( );
								   pic->VKeysDown( keysym.scancode, VK_DIVIDE );				break;
		case SDLK_MINUS          : pic->VKeysDown( keysym.scancode, VK_SHIFT, VK_DIVIDE );		break;
		case SDLK_LEFTBRACKET    : pic->VKeysDown( keysym.scancode, VK_FCTN,  VK_R );			break;
		case SDLK_RIGHTBRACKET   : pic->VKeysDown( keysym.scancode, VK_FCTN,  VK_T );			break;
		case '{'                 : pic->VKeysDown( keysym.scancode, VK_FCTN,  VK_F );			break;
		case '}'                 : pic->VKeysDown( keysym.scancode, VK_FCTN,  VK_G );			break;
		case SDLK_SPACE          : pic->VKeysDown( keysym.scancode, VK_SPACE );					break;
		case SDLK_EXCLAIM        : pic->VKeysDown( keysym.scancode, VK_SHIFT, VK_1 );			break;
		case SDLK_AT             : pic->VKeysDown( keysym.scancode, VK_SHIFT, VK_2 );			break;
		case SDLK_HASH           : pic->VKeysDown( keysym.scancode, VK_SHIFT, VK_3 );			break;
		case SDLK_DOLLAR         : pic->VKeysDown( keysym.scancode, VK_SHIFT, VK_4 );			break;
		case SDLK_PERCENT        : pic->VKeysDown( keysym.scancode, VK_SHIFT, VK_5 );			break;
		case SDLK_CARET          : pic->VKeysDown( keysym.scancode, VK_SHIFT, VK_6 );			break;
		case SDLK_AMPERSAND      : pic->VKeysDown( keysym.scancode, VK_SHIFT, VK_7 );			break;
		case SDLK_ASTERISK       : pic->VKeysDown( keysym.scancode, VK_SHIFT, VK_8 );			break;
		case SDLK_LEFTPAREN      : pic->VKeysDown( keysym.scancode, VK_SHIFT, VK_9 );			break;
		case SDLK_RIGHTPAREN     : pic->VKeysDown( keysym.scancode, VK_SHIFT, VK_0 );			break;
		case SDLK_BACKSLASH      : pic->VKeysDown( keysym.scancode, VK_FCTN,  VK_Z );			break;
		case SDLK_BACKQUOTE      : pic->VKeysDown( keysym.scancode, VK_FCTN,  VK_C );			break;
		case SDLK_TAB            : pic->VKeysDown( keysym.scancode, VK_FCTN,  VK_7 );			break;
		case SDLK_BACKSPACE      :
		case SDLK_LEFT           : pic->VKeysDown( keysym.scancode, VK_FCTN, VK_S );			break;
		case SDLK_RIGHT          : pic->VKeysDown( keysym.scancode, VK_FCTN, VK_D );			break;
		case SDLK_UP             : pic->VKeysDown( keysym.scancode, VK_FCTN, VK_E );			break;
		case SDLK_DOWN           : pic->VKeysDown( keysym.scancode, VK_FCTN, VK_X );			break;
		case SDLK_DELETE         : pic->VKeysDown( keysym.scancode, VK_FCTN, VK_1 );			break;
		case SDLK_KP_ENTER       :
		case SDLK_RETURN         : pic->VKeysDown( keysym.scancode, VK_ENTER );					break;
		case SDLK_LSHIFT         :
		case SDLK_RSHIFT         : pic->VKeysDown( keysym.scancode, VK_SHIFT );					break;
		case SDLK_LALT           :
		case SDLK_RALT           : pic->VKeysDown( keysym.scancode, VK_FCTN );					break;
		case SDLK_LGUI           :
		case SDLK_RGUI           : pic->VKeysDown( keysym.scancode, VK_FCTN );					break;
		case SDLK_LCTRL          :
		case SDLK_RCTRL          : pic->VKeysDown( keysym.scancode, VK_CTRL );					break;
		case SDLK_CAPSLOCK       :
			if(( SDL_GetModState( ) & KMOD_CAPS ) != 0 )
			{
				pic->VKeysDown( keysym.scancode, VK_CAPSLOCK );
			}
			break;
		default :
			if( keycode < 256 )
			{
				if( isalpha( keycode ))
				{
					pic->VKeysDown( keysym.scancode, ( VIRTUAL_KEY_E ) ( VK_A + ( tolower( keycode ) - SDLK_a )));
				}
				else if( isdigit( keycode ))
				{
					pic->VKeysDown( keysym.scancode, ( VIRTUAL_KEY_E ) ( VK_0 + ( keycode - SDLK_0 )));
				}
			}
			break;
	}

	// Emulate the joystick
	switch( keycode )
	{
		case SDLK_KP_0  : pic->SetJoystickButton( 0, true );	break;
		case SDLK_KP_7  : pic->SetJoystickY( 0,  1 );
		case SDLK_LEFT  :
		case SDLK_KP_4  : pic->SetJoystickX( 0, -1 );			break;
		case SDLK_KP_3  : pic->SetJoystickY( 0, -1 );
		case SDLK_RIGHT :
		case SDLK_KP_6  : pic->SetJoystickX( 0,  1 );			break;
		case SDLK_KP_1  : pic->SetJoystickX( 0, -1 );
		case SDLK_DOWN  :
		case SDLK_KP_2  : pic->SetJoystickY( 0, -1 );			break;
		case SDLK_KP_9  : pic->SetJoystickX( 0,  1 );
		case SDLK_UP    :
		case SDLK_KP_8  : pic->SetJoystickY( 0,  1 );			break;
		default : ;
	}
}

void cSdlTI994A::KeyReleased( SDL_Keysym keysym )
{
	iTMS9901 *pic = m_PIC;

	auto keycode = m_ActiveKeycode[ keysym.scancode ];

	switch( keycode )
	{
		case SDLK_CAPSLOCK       :
			if(( SDL_GetModState( ) & KMOD_CAPS ) == 0 )
			{
				pic->VKeyUp( keysym.scancode );
			}
			break;
		case SDLK_COMMA          :
		case SDLK_PERIOD         :
		case SDLK_SEMICOLON      :
		case SDLK_EQUALS         :
		case SDLK_SLASH          :
			pic->UnHideShiftKey( );
			// fallthrough
		default : ;
			pic->VKeyUp( keysym.scancode );
	}

	switch( keycode )
	{
		case SDLK_KP_0  : pic->SetJoystickButton( 0, false );	break;
		case SDLK_KP_7  :
		case SDLK_KP_1  :
		case SDLK_KP_9  :
		case SDLK_KP_3  : pic->SetJoystickY( 0, 0 );
		case SDLK_LEFT  :
		case SDLK_KP_4  :
		case SDLK_RIGHT :
		case SDLK_KP_6  : pic->SetJoystickX( 0, 0 );			break;
		case SDLK_DOWN  :
		case SDLK_KP_2  :
		case SDLK_UP    :
		case SDLK_KP_8  : pic->SetJoystickY( 0, 0 );			break;
		default : ;
	}
}

void cSdlTI994A::StartThread( )
{
	FUNCTION_ENTRY( this, "cSdlTI994A::StartThread", true );

	if( m_CPU->IsRunning( ) == true )
	{
		return;
	}

	m_StartClock = m_CPU->GetClocks( );

	m_StartTime = SDL_GetTicks( );

	m_pThread = SDL_CreateThread( _RunThreadProc, nullptr, this );
}

void cSdlTI994A::StopThread( )
{
	FUNCTION_ENTRY( this, "cSdlTI994A::StopThread", true );

	if( m_CPU->IsRunning( ) == false )
	{
		return;
	}

	m_CPU->Stop( );

	SDL_WaitThread( m_pThread, nullptr );
}

void cSdlTI994A::TimerHookProc( UINT32 clockCycles )
{
	FUNCTION_ENTRY( this, "cSdlTI994A::TimerHookProc", false );

	UINT32 ellapsedCycles = clockCycles - m_StartClock;
	UINT32 estimatedTime  = m_StartTime + ellapsedCycles / ( m_ClockSpeed / 1000 );

	// Limit the emulated speed to 3.0MHz
	while( SDL_GetTicks( ) < estimatedTime )
	{
		if( Sleep( 0, 5 ))
		{
			break;
		}
	}

	UINT32 ellapsedTime  = SDL_GetTicks( ) - m_StartTime;

	// Reset base time/clocks every 5 minutes to avoid wrap
	if( ellapsedTime > 5 * 60 * 1000 )
	{
		m_StartClock = clockCycles;
		m_StartTime  = SDL_GetTicks( );
	}

	// Let the base class simulate a 50/60Hz VDP interrupt
	cTI994A::TimerHookProc( clockCycles );
}

bool cSdlTI994A::VideoRetrace( )
{
	bool bScreenChanged = cTI994A::VideoRetrace( );

	// Schedule a render event (unless we're falling behind)
	if(( bScreenChanged == true ) && ( m_RefreshCount == 0 ))
	{
		m_RefreshCount++;

		SDL_Event event;

		memset( &event, 0, sizeof( event ));

		event.type       = m_VideoUpdateEvent;
		event.user.code  = 0;
		event.user.data1 = nullptr;
		event.user.data2 = nullptr;

		SDL_PushEvent( &event );

		return true;
	}

	return false;
}

int cSdlTI994A::_RunThreadProc( void *ptr )
{
	FUNCTION_ENTRY( nullptr, "cSdlTI994A::_RunThreadProc", true );

	cSdlTI994A *pThis = ( cSdlTI994A * ) ptr;

	return pThis->RunThreadProc( );
}

int cSdlTI994A::RunThreadProc( )
{
	FUNCTION_ENTRY( this, "cSdlTI994A::RunThreadProc", true );

	m_CPU->Run( );

	return 0;
}
