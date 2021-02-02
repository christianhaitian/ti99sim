//----------------------------------------------------------------------------
//
// File:        device-support.cpp
// Date:        29-Feb-2020
// Programmer:  Marc Rousseau
//
// Description:
//
// Copyright (c) 2020 Marc Rousseau, All Rights Reserved.
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

#include <regex>
#include "idevice.hpp"
#include "icomputer.hpp"
#include "cartridge.hpp"
#include "option.hpp"
#include "support.hpp"
#include "device.hpp"
#include "ti-disk.hpp"
#include "ti-pcard.hpp"
#include "cf7+.hpp"

using FactoryFunction = iDevice*(*)(iCartridge *);

struct sFactoryInfo
{
	const char *		sha1;
	const char *		filename;
	FactoryFunction		factory;
};

std::array deviceMap =
{
	sFactoryInfo{ "4d26e5ef0997ed2f3a56eb8104778bfe719b38f2",	"cf7+.ctg",			[]( iCartridge *rom ) { return static_cast<iDevice*>( new cCF7( rom )); }},
	sFactoryInfo{ "ed91d48c1eaa8ca37d5055bcf67127ea51c4cad5",	"ti-disk.ctg",		[]( iCartridge *rom ) { return static_cast<iDevice*>( new cDiskDevice( rom )); }},
	sFactoryInfo{ "27aceb956262d3e3f97d938602dfaa91b53da59e",	"ti-pcard.ctg",		[]( iCartridge *rom ) { return static_cast<iDevice*>( new cUCSDDevice( rom )); }},
};

cRefPtr<iDevice> LoadDevice( const std::string &description, const std::string &folder )
{
	std::cmatch what;

	std::regex pattern( R"((.*) - (.*))", std::regex_constants::ECMAScript );
	if( std::regex_match( description.c_str( ), what, pattern ))
	{
		auto hash = std::string( what[ 1 ].first, what[ 1 ].second );
		auto path = std::string( what[ 2 ].first, what[ 2 ].second );

		if( !std::filesystem::exists( path ))
		{
			path = LocateCartridge( folder, hash );
		}

		if( !path.empty( ))
		{
			auto rom = new cCartridge( path );
			auto sha1 = rom->sha1( );
			for( auto &entry : deviceMap )
			{
				if( sha1 == entry.sha1 )
				{
					return cRefPtr<iDevice>( entry.factory( rom ));
				}
			}
		}
	}

	return { };
}

static void LoadDevice( iComputer *computer, const std::filesystem::path &name, const sFactoryInfo &info )
{
	cRefPtr<cCartridge> ctg = new cCartridge( name );

	cRefPtr<iDevice> device = info.factory( ctg );

	bool status = computer->RegisterDevice( device );

	if( verbose >= 1 )
	{
		int padding = 25 - strlen( device->GetName( ));
		if( verbose >= 2 )
		{
			fprintf( stdout, "Using device ROM \"%s\" - \"%s\"\n", ctg->GetFileName( ), ctg->GetTitle( ));
		}
		fprintf( stdout, "Loading device: >%04X - \"%-s\"%*.*s - %s\n", device->GetCRU( ), device->GetName( ),
			padding, padding, "", status ? "OK" : "** Failed to add device **" );
	}
}

void LoadDevices( iComputer *computer, std::function<bool(const char *)> filter )
{
	auto roms = LocateFiles( "console", ".ctg" );

	for( auto &rom : roms )
	{
		for( auto &entry : deviceMap )
		{
			if(( rom.filename( ) == entry.filename ) && ( filter( entry.filename )))
			{
				LoadDevice( computer, rom, entry );
				break;
			}
		}
	}
}
