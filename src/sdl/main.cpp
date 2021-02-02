//----------------------------------------------------------------------------
//
// File:        main.cpp
// Date:        02-Apr-2000
// Programmer:  Marc Rousseau
//
// Description: This file contains starup code for Linux/SDL
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

#include <cstdio>
#include <cmath>
#include <SDL.h>
#include "common.hpp"
#include "logger.hpp"
#include "cartridge.hpp"
#include "tms9900.hpp"
#include "ti994a-sdl.hpp"
#include "tms9918a-sdl.hpp"
#include "tms9919-sdl.hpp"
#include "tms5220.hpp"
#include "disk-media.hpp"
#include "device-support.hpp"
#include "ti-disk.hpp"
#include "cf7+.hpp"
#include "support.hpp"
#include "option.hpp"

DBG_REGISTER( __FILE__ );

static sRGBQUAD ColorTable[ 3 ][ 17 ] =
{
	{
		{ 0x00, 0x00, 0x00, 0xFF },             // 0x00 TI_TRANSPARENT - Background
		{ 0x00, 0x00, 0x00, 0xFF },             // 0x01 TI_BLACK
		{ 0x48, 0x9C, 0x08, 0xFF },             // 0x02 TI_MEDIUM_GREEN *
		{ 0x70, 0xBF, 0x88, 0xFF },             // 0x03 TI_LIGHT_GREEN *
		{ 0x28, 0x3C, 0x8A, 0xFF },             // 0x04 TI_DARK_BLUE *
		{ 0x50, 0x6C, 0xCF, 0xFF },             // 0x05 TI_LIGHT_BLUE *
		{ 0xD0, 0x48, 0x00, 0xFF },             // 0x06 TI_DARK_RED *
		{ 0x00, 0xCC, 0xFF, 0xFF },             // 0x07 TI_CYAN *
		{ 0xD0, 0x58, 0x28, 0xFF },             // 0x08 TI_MEDIUM_RED *
		{ 0xFF, 0xA0, 0x40, 0xFF },             // 0x09 TI_LIGHT_RED *
		{ 0xFC, 0xF0, 0x50, 0xFF },             // 0x0A TI_DARK_YELLOW *
		{ 0xFF, 0xFF, 0x80, 0xFF },             // 0x0B TI_LIGHT_YELLOW *
		{ 0x00, 0x80, 0x00, 0xFF },             // 0x0C TI_DARK_GREEN
		{ 0xCD, 0x58, 0xCD, 0xFF },             // 0x0D TI_MAGENTA *
		{ 0xE0, 0xE0, 0xE0, 0xFF },             // 0x0E TI_GRAY *
		{ 0xFF, 0xFF, 0xFF, 0xFF },             // 0x0F TI_WHITE
		{ 0xFF, 0xFF, 0xFF, 0xFF }              // 0x10 - Text Mode Foreground
	}, {
		{ 0x00, 0x00, 0x00, 0xFF },             // 0x00 TI_TRANSPARENT - Background
		{ 0x00, 0x00, 0x00, 0xFF },             // 0x01 TI_BLACK
		{   33,  200,   66, 0xFF },             // 0x02 TI_MEDIUM_GREEN *
		{   94,  220,  120, 0xFF },             // 0x03 TI_LIGHT_GREEN *
		{   84,   85,  237, 0xFF },             // 0x04 TI_DARK_BLUE *
		{  125,  118,  252, 0xFF },             // 0x05 TI_LIGHT_BLUE *
		{  212,   82,   77, 0xFF },             // 0x06 TI_DARK_RED *
		{   66,  235,  245, 0xFF },             // 0x07 TI_CYAN *
		{  252,   85,   84, 0xFF },             // 0x08 TI_MEDIUM_RED *
		{  255,  121,  120, 0xFF },             // 0x09 TI_LIGHT_RED *
		{  212,  193,   84, 0xFF },             // 0x0A TI_DARK_YELLOW *
		{  230,  206,  128, 0xFF },             // 0x0B TI_LIGHT_YELLOW *
		{   33,  176,   59, 0xFF },             // 0x0C TI_DARK_GREEN
		{  201,   91,  186, 0xFF },             // 0x0D TI_MAGENTA *
		{  204,  204,  204, 0xFF },             // 0x0E TI_GRAY *
		{ 0xFF, 0xFF, 0xFF, 0xFF },             // 0x0F TI_WHITE
		{ 0xFF, 0xFF, 0xFF, 0xFF }              // 0x10 - Text Mode Foreground
	}, {
		{ 0x00, 0x00, 0x00, 0xFF },             // 0x00 TI_TRANSPARENT - Background
		{ 0x00, 0x00, 0x00, 0xFF },             // 0x01 TI_BLACK
		{ 0x00, 0xCC, 0x00, 0xFF },             // 0x02 TI_MEDIUM_GREEN
		{ 0x00, 0xFF, 0x00, 0xFF },             // 0x03 TI_LIGHT_GREEN
		{ 0x00, 0x00, 0x80, 0xFF },             // 0x04 TI_DARK_BLUE
		{ 0x00, 0x00, 0xFF, 0xFF },             // 0x05 TI_LIGHT_BLUE
		{ 0x80, 0x00, 0x00, 0xFF },             // 0x06 TI_DARK_RED
		{ 0x00, 0xFF, 0xFF, 0xFF },             // 0x07 TI_CYAN
		{ 0xCC, 0x00, 0x00, 0xFF },             // 0x08 TI_MEDIUM_RED
		{ 0xFF, 0x00, 0x00, 0xFF },             // 0x09 TI_LIGHT_RED
		{ 0xB0, 0xB0, 0x00, 0xFF },             // 0x0A TI_DARK_YELLOW
		{ 0xFF, 0xFF, 0x00, 0xFF },             // 0x0B TI_LIGHT_YELLOW
		{ 0x00, 0x80, 0x00, 0xFF },             // 0x0C TI_DARK_GREEN
		{ 0xB0, 0x00, 0xB0, 0xFF },             // 0x0D TI_MAGENTA
		{ 0xCC, 0xCC, 0xCC, 0xFF },             // 0x0E TI_GRAY
		{ 0xFF, 0xFF, 0xFF, 0xFF },             // 0x0F TI_WHITE
		{ 0xFF, 0xFF, 0xFF, 0xFF }              // 0x10 - Text Mode Foreground
	}
};

static bool        joystickInitialized  = false;
static int         joystickIndex[ 2 ]   = { 0, 1 };
static int         framesOn             = 1;
static int         framesOff            = 0;
static std::string consoleFile { };

bool ListJoysticks( const char *, void * )
{
	FUNCTION_ENTRY( nullptr, "ListJoysticks", true );

	if(( joystickInitialized == false ) && ( SDL_InitSubSystem( SDL_INIT_JOYSTICK ) < 0 ))
	{
		fprintf( stderr, "Error initializing joystick subsystem: %s\n", SDL_GetError( ));
		return false;
	}

	joystickInitialized = true;

	int num = SDL_NumJoysticks( );
	fprintf( stdout, "joysticks found: %d\n", num );

	if( num > 0 )
	{
		fprintf( stdout, "\nThe names of the joysticks are:\n" );
		for( int i = 0; i < num; i++ )
		{
			fprintf( stdout, "  %d) %s\n", i + 1, SDL_JoystickNameForIndex( i ));
		}
	}

	exit( 0 );
}

bool ParseJoystick( const char *arg, void * )
{
	FUNCTION_ENTRY( nullptr, "ParseJoystick", true );

	arg += strlen( "joystick" );

	int joy_num = arg[ 0 ] - '1';
	if(( joy_num < 0 ) || ( joy_num > 1 ) || ( arg[ 1 ] != '=' ))
	{
		fprintf( stderr, "Joystick must be either 1 or 2\n" );
		return false;
	}

	if(( joystickInitialized == false ) && ( SDL_InitSubSystem( SDL_INIT_JOYSTICK ) < 0 ))
	{
		fprintf( stderr, "Error initializing joystick subsystem: %s\n", SDL_GetError( ));
		return false;
	}

	joystickInitialized = true;

	int joy_index = atoi( arg + 2 ) - 1;
	if(( joy_index < 0 ) || ( joy_index >= SDL_NumJoysticks( )))
	{
		if( SDL_NumJoysticks( ) == 0 )
		{
			fprintf( stderr, "No joysticks were detected\n" );
		}
		else
		{
			fprintf( stderr, "Joystick %s is invalid (range=%d-%d)\n", arg + 2, 1, SDL_NumJoysticks( ));
		}
		return false;
	}
	joystickIndex[ joy_num ] = joy_index;

	return true;
}

bool ParseCF7( const char *arg, void * )
{
	FUNCTION_ENTRY( nullptr, "ParseCF7", true );

	cCF7::DiskImage = arg + 4;

	return true;
}

bool ParseConsole( const char *arg, void * )
{
	FUNCTION_ENTRY( nullptr, "ParseConsole", true );

	consoleFile = LocateFile( "console", arg + 8 );

	if( consoleFile.empty( ))
	{
		fprintf( stderr, "Unable to locate console file '%s'\n", arg + 8 );
	}

	return true;
}

bool ParseDisk( const char *arg, void * )
{
	FUNCTION_ENTRY( nullptr, "ParseDisk", true );

	arg += strlen( "dsk" );

	int disk = arg[ 0 ] - '1';
	if(( disk < 0 ) || ( disk > 2 ) || ( arg[ 1 ] != '=' ))
	{
		fprintf( stderr, "Disk must be either 1, 2, or 3\n" );
		return false;
	}

	cDiskDevice::DiskImage[ disk ] = arg + 2;

	return true;
}

bool ParseSampleRate( const char *arg, void *ptr )
{
	FUNCTION_ENTRY( nullptr, "ParseSampleRate", true );

	int freq = 0;

	arg = strchr( arg, '=' ) + 1;

	if( sscanf( arg, "%d", &freq ) != 1 )
	{
		fprintf( stderr, "Invalid sampling rate '%s'\n", arg );
		return false;
	}

	if(( freq > 44100 ) || ( freq < 8000 ))
	{
		fprintf( stderr, "Sampling rate must be between 8000 and 44100\n" );
		return false;
	}

	*( int * ) ptr = freq;

	return true;
}

bool ParseFrameRate( const char *arg, void * )
{
	FUNCTION_ENTRY( nullptr, "ParseFrameRate", true );

	int num = 0, den = 0;

	arg = strchr( arg, '=' ) + 1;

	switch( sscanf( arg, "%d/%d", &num, &den ))
	{
		case 1 :
			den = 100;
			break;
		case 2 :
			break;
		default :
			fprintf( stderr, "Invalid framerate '%s'\n", arg );
			return false;
	}

	if(( num > den ) || ( num * den == 0 ))
	{
		fprintf( stderr, "Invalid framerate specified\n" );
		return false;
	}

	framesOn  = num;
	framesOff = den - num;

	return true;
}

bool IsType( const char *filename, const char *type )
{
	FUNCTION_ENTRY( nullptr, "IsType", true );

	size_t len = strlen( filename );
	const char *ptr = filename + len - 4;
	return ( strcmp( ptr, type ) == 0 ) ? true : false;
}

void PrintUsage( )
{
	FUNCTION_ENTRY( nullptr, "PrintUsage", true );

	fprintf( stdout, "Usage: ti99sim-sdl [options] [cartridge.ctg] [image.img]\n" );
	fprintf( stdout, "\n" );
}

int main( int argc, char *argv[] )
{
	FUNCTION_ENTRY( nullptr, "main", true );

	int flagScale       = -1;
	bool flagSound      = true;
	bool flagSpeech     = true;
	bool flagJoystick   = true;
	bool flagMonochrome = false;
	int colorTableIndex = 0;
	bool fullScreenMode = false;
	int refreshRate     = 60;
	int samplingRate    = 44100;
	bool useCF7         = true;
	bool useUCSD        = false;
	bool useScale2x     = false;
	int volume          = 50;

	sOption optList[ ] =
	{
		{ '4', nullptr,              OPT_VALUE_SET | OPT_SIZE_INT,  2,     &flagScale,       nullptr,         "Double width/height window" },
		{  0,  "cf7=*<filename>",    OPT_NONE,                      0,     nullptr,          ParseCF7,        "Use <filename> for CF7+ disk image" },
		{  0,  "console=*<filename>", OPT_NONE,                     0,     nullptr,          ParseConsole,    "Use <filename> for system ROM image" },
		{  0,  "dsk*n=<filename>",   OPT_NONE,                      0,     nullptr,          ParseDisk,       "Use <filename> disk image for DSKn" },
		{  0,  "framerate=*{n/d|p}", OPT_NONE,                      0,     nullptr,          ParseFrameRate,  "Reduce frame rate to fraction n/d or percentage p" },
		{ 'f', "fullscreen",         OPT_VALUE_SET | OPT_SIZE_BOOL, true,  &fullScreenMode,  nullptr,         "Fullscreen" },
		{  0,  "joystick*n=i",       OPT_NONE,                      0,     nullptr,          ParseJoystick,   "Use system joystick i as TI joystick n" },
		{  0,  "list-joysticks",     OPT_NONE,                      0,     nullptr,          ListJoysticks,   "Print a list of all detected joysticks" },
		{  0,  "no-cf7",             OPT_VALUE_SET | OPT_SIZE_BOOL, false, &useCF7,          nullptr,         "Disable CF7+ disk support" },
		{  0,  "no-joystick",        OPT_VALUE_SET | OPT_SIZE_BOOL, false, &flagJoystick,    nullptr,         "Disable hardware joystick support" },
		{ 'q', "no-sound",           OPT_VALUE_SET | OPT_SIZE_BOOL, false, &flagSound,       nullptr,         "Turn off all sound/speech" },
		{  0,  "no-speech",          OPT_VALUE_SET | OPT_SIZE_BOOL, false, &flagSpeech,      nullptr,         "Disable speech synthesis" },
		{  0,  "NTSC",               OPT_VALUE_SET | OPT_SIZE_INT,  60,    &refreshRate,     nullptr,         "Emulate a NTSC display (60Hz)" },
		{  0,  "PAL",                OPT_VALUE_SET | OPT_SIZE_INT,  50,    &refreshRate,     nullptr,         "Emulate a PAL display (50Hz)" },
		{ 'p', "palette=*n",         OPT_VALUE_PARSE_INT,           0,     &colorTableIndex, nullptr,         "Select a color palette (1-3)" },
		{  0,  "bw",                 OPT_VALUE_SET | OPT_SIZE_BOOL, true , &flagMonochrome,  nullptr,         "Display black & white video" },
		{ 's', "sample=*<freq>",     OPT_NONE,                      0,     &samplingRate,    ParseSampleRate, "Select sampling frequency for audio playback" },
		{  0,  "scale=*n",           OPT_VALUE_PARSE_INT,           2,     &flagScale,       nullptr,         "Scale the window width & height by scale" },
		{  0,  "scale2x",            OPT_VALUE_SET | OPT_SIZE_BOOL, true,  &useScale2x,      nullptr,         "Use the Scale2x algorithm to scale display" },
		{  0,  "ucsd",               OPT_VALUE_SET | OPT_SIZE_BOOL, true,  &useUCSD,         nullptr,         "Enable the UCSD p-System device if present" },
		{ 'v', "verbose*=n",         OPT_VALUE_PARSE_INT,           1,     &verbose,         nullptr,         "Display extra information" },
		{  0,  "volume=*n",          OPT_VALUE_PARSE_INT,           50,    &volume,          nullptr,         "Set the audio volume" }
	};

	// Initialize the SDL library (starts the event loop)
	if( SDL_Init( SDL_INIT_VIDEO ) < 0 )
	{
		fprintf( stderr, "Couldn't initialize SDL: %s\n", SDL_GetError( ));
		return -1;
	}

	// Clean up on exit, exit on window close and interrupt
	atexit( SDL_Quit );

	std::string ctgFile;
	std::string imgFile;

	int index = 1;

	bool isValid = true;

	while( index < argc )
	{
		int newIndex = ParseArgs( index, argc, argv, SIZE( optList ), optList );

		if( index == newIndex )
		{
			if( IsType( argv[ index ], ".ctg" ))
			{
				std::string filename = LocateFile( "cartridges", argv[ index ] );
				if( !filename.empty( ))
				{
					ctgFile = filename;
				}
				else
				{
					isValid = false;
					fprintf( stderr, "Unable to locate cartridge \"%s\"\n", argv[ index ] );
				}
			}
			else if( IsType( argv[ index ], ".img" ))
			{
				std::string filename = LocateFile( ".", argv[ index ] );
				if( !filename.empty( ))
				{
					isValid = true;
					imgFile = filename;
				}
				else
				{
					isValid = false;
					fprintf( stderr, "Unable to locate image \"%s\"\n", argv[ index ] );
				}
			}
			else
			{
				isValid = false;
				fprintf( stderr, "Unrecognized argument \"%s\"\n", argv[ index ] );
			}

			index++;
		}
		else
		{
			index = newIndex;
		}
	}

	if( isValid == false )
	{
		return 0;
	}

	if( colorTableIndex > 0 )
	{
		if( colorTableIndex > 3 )
		{
			fprintf( stderr, "Invalid palette selected - must be 1 or 2\n" );
			return -1;
		}
		colorTableIndex--;
	}

	sRGBQUAD *colorTable = ColorTable[ colorTableIndex ];

	if( flagMonochrome == true )
	{
		static sRGBQUAD bwTable[ 17 ];

		for( int i = 0; i < 17; i++ )
		{
			int y = lrint( 0.299 * colorTable[ i ].r + 0.587 * colorTable[ i ].g + 0.114 * colorTable[ i ].b );
			bwTable[ i ].r = y;
			bwTable[ i ].g = y;
			bwTable[ i ].b = y;
		}

		colorTable = bwTable;
	}

	SDL_Joystick *joy1 = nullptr;
	SDL_Joystick *joy2 = nullptr;

	if( flagJoystick == true )
	{
		// Try to initialize the joystick subsystem and allocate them
		if(( joystickInitialized == true ) || ( SDL_InitSubSystem( SDL_INIT_JOYSTICK ) >= 0 ))
		{
			// NOTE: If we called SDL_InitSubSystem here, the user didn't explicitly request joystick support
			//   and we'll silently ignore the failure and disable joysticks (MAC OS X seems to have a
			//   problem with joysticks).  If they asked for them, and it failed it would have been reported
			//   to them in ParseJoystick.
			joystickInitialized = true;
			joy1 = SDL_JoystickOpen( joystickIndex[ 0 ]);
			joy2 = SDL_JoystickOpen( joystickIndex[ 1 ]);
		}
	}

	cRefPtr<cCartridge> consoleROM = consoleFile.empty( ) ? nullptr : new cCartridge( consoleFile );

	cRefPtr<cSdlTMS9918A> vdp = new cSdlTMS9918A( colorTable, refreshRate, useScale2x, fullScreenMode, flagScale );

	cRefPtr<iTMS9919> sound = nullptr;
	cRefPtr<iTMS5220> speech = nullptr;

	if( flagSound )
	{
		SDL_InitSubSystem( SDL_INIT_AUDIO );
		cSdlTMS9919 *sdlSound = new cSdlTMS9919( samplingRate );
		sdlSound->SetMasterVolume( volume );
		sound = sdlSound;
		if( flagSpeech )
		{
			speech = new cTMS5220;
		}
	}

	vdp->SetFrameRate( framesOn, framesOff );

	cRefPtr<cSdlTI994A> computer = new cSdlTI994A( consoleROM, vdp, sound, speech );

	if( auto console = computer->GetConsole( ))
	{
		if( verbose >= 2 )
		{
			fprintf( stdout, "Using system ROM \"%s\" - \"%s\"\n", console->GetFileName( ), console->GetTitle( ));
		}
	}
	else
	{
		fprintf( stderr, "Unable to locate console ROMs!\n" );
		return -1;
	}

	LoadDevices( computer, [=]( auto name )
	{
		if( strcmp( name, "cf7+.ctg" ) == 0 ) return useCF7;
		if( strcmp( name, "ti-pcard.ctg" ) == 0 ) return useUCSD;
		return true;
	});

	if( joy1 != nullptr )
	{
		computer->SetJoystick( 0, joy1 );
	}
	if( joy2 != nullptr )
	{
		computer->SetJoystick( 1, joy2 );
	}

	if( !ctgFile.empty( ))
	{
		cRefPtr<cCartridge> ctg = new cCartridge( ctgFile );
		if( verbose >= 1 )
		{
			fprintf( stdout, "Loading cartridge \"%s\" (%s)\n", ctg->GetFileName( ), ctg->GetTitle( ));
		}
		computer->InsertCartridge( ctg );
	}

	if( !imgFile.empty( ))
	{
		if( verbose >= 1 )
		{
			fprintf( stdout, "Loading image \"%s\"\n", imgFile.c_str( ));
		}
		computer->LoadImage( imgFile.c_str( ));
	}

	if( verbose >= 1 )
	{
		fprintf( stdout, " Video refresh rate: %d Hz\n", refreshRate );
		if( flagSound == true )
		{
			fprintf( stdout, "Audio sampling rate: %d Hz\n", samplingRate );
		}
	}

	computer->Run( );

	if( joy1 != nullptr )
	{
		SDL_JoystickClose( joy1 );
	}
	if( joy2 != nullptr )
	{
		SDL_JoystickClose( joy2 );
	}

	return 0;
}
