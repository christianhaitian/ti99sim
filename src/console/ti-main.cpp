//----------------------------------------------------------------------------
//
// File:        ti-main.cpp
// Date:        23-Feb-1998
// Programmer:  Marc Rousseau
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
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307, USA.
//
// Revision History:
//
//----------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "common.hpp"
#include "logger.hpp"
#include "tms9900.hpp"
#include "cartridge.hpp"
#include "ti994a-console.hpp"
#include "tms9918a-console.hpp"
#include "device-support.hpp"
#include "ti-disk.hpp"
#include "cf7+.hpp"
#include "screenio.hpp"
#include "option.hpp"
#include "support.hpp"

DBG_REGISTER( __FILE__ );

static std::string consoleFile { };

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

	fprintf( stdout, "Usage: ti99sim-console [options] [cartridge.ctg] [image.img]\n" );
	fprintf( stdout, "\n" );
}

int main( int argc, char *argv[] )
{
	FUNCTION_ENTRY( nullptr, "main", true );

	int refreshRate = 60;
	bool useCF7     = true;
	bool useUCSD    = false;

	sOption optList[ ] =
	{
		{  0,  "cf7=*<filename>",     OPT_NONE,                      0,     nullptr,         ParseCF7,       "Use <filename> for CF7+ disk image" },
		{  0,  "console=*<filename>", OPT_NONE,                      0,     nullptr,         ParseConsole,   "Use <filename> for system ROM image" },
		{  0,  "dsk*n=<filename>",    OPT_NONE,                      0,     nullptr,         ParseDisk,      "Use <filename> disk image for DSKn" },
		{  0,  "no-cf7",              OPT_VALUE_SET | OPT_SIZE_BOOL, false, &useCF7,         nullptr,        "Disable CF7+ disk support" },
		{  0,  "NTSC",                OPT_VALUE_SET | OPT_SIZE_INT,  60,    &refreshRate,    nullptr,        "Emulate a NTSC display (60Hz)" },
		{  0,  "PAL",                 OPT_VALUE_SET | OPT_SIZE_INT,  50,    &refreshRate,    nullptr,        "Emulate a PAL display (50Hz)" },
		{  0,  "ucsd",                OPT_VALUE_SET | OPT_SIZE_BOOL, true,  &useUCSD,        nullptr,        "Enable the UCSD p-System device if present" },
		{ 'v', "verbose*=n",          OPT_VALUE_PARSE_INT,           1,     &verbose,        nullptr,        "Display extra information" },
	};

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

	SaveConsoleSettings( );

	HideCursor( );
	ClearScreen( );

	cRefPtr<cCartridge> consoleROM = consoleFile.empty( ) ? nullptr : new cCartridge( consoleFile );

	cRefPtr<cTMS9918A> vdp = new cConsoleTMS9918A( refreshRate );

	cRefPtr<iComputer> computer = new cConsoleTI994A( consoleROM, vdp );

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

	computer->Run( );

	ClearScreen( );
	ShowCursor( );

	RestoreConsoleSettings( );

	return 0;
}
