//----------------------------------------------------------------------------
//
// File:        catalog.cpp
// Date:        23-Feb-2016
// Programmer:  Marc Rousseau
//
// Description: This programs will list all disks (and contents) or cartridges
//
// Copyright (c) 1995-2004 Marc Rousseau, All Rights Reserved.
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
#include <map>
#include <set>
#include "common.hpp"
#include "logger.hpp"
#include "cartridge.hpp"
#include "disk-media.hpp"
#include "fileio.hpp"
#include "file-system.hpp"
#include "option.hpp"
#include "support.hpp"

DBG_REGISTER( __FILE__ );

void PrintUsage( )
{
	FUNCTION_ENTRY( nullptr, "PrintUsage", true );

	fprintf( stdout, "Usage: catalog [options] path [path...]\n" );
	fprintf( stdout, "\n" );
}

using CartMap = std::multimap<std::string,cRefPtr<cCartridge>>;
using DiskMap = std::multimap<std::string,cRefPtr<cFileSystem>>;
using FileMap = std::multimap<std::string,cRefPtr<cFile>>;

FileMap GetFiles( cRefPtr<cFileSystem> disk )
{
	FileMap map;

	std::string diskName = disk->GetName( );

	for( int i = -1; i < disk->DirectoryCount( ); i++ )
	{
		const char *dirName = disk->DirectoryName( i );

		std::string path = diskName;
		if( dirName != nullptr )
		{
			path += ".";
			path += dirName;
		}

		std::list<std::string> filenames;
		disk->GetFilenames( filenames, i );

		for( auto &fileName : filenames )
		{
			cRefPtr<cFile> file = disk->OpenFile( fileName.c_str( ), i );
			if( file != nullptr )
			{
				map.insert( { file->sha1( ), file } );
			}
		}
	}

	return map;
}

int main( int argc, char *argv[] )
{
	FUNCTION_ENTRY( nullptr, "main", true );

	bool dumpCartridges = false;
	bool dumpDisks      = false;
	bool dumpFiles      = false;

	sOption optList[ ] =
	{
		{ 'c', "cartridges",           OPT_VALUE_SET | OPT_SIZE_BOOL, true,              &dumpCartridges, nullptr,        "List all cartridges" },
		{ 'd', "disks",                OPT_VALUE_SET | OPT_SIZE_BOOL, true,              &dumpDisks,      nullptr,        "List all disks" },
		{ 'f', "files",                OPT_VALUE_SET | OPT_SIZE_BOOL, true,              &dumpFiles,      nullptr,        "List all files on disks" }
	};

	if( argc == 1 )
	{
		PrintHelp( SIZE( optList ), optList );
		return 0;
	}

	int index = ParseArgs( 1, argc, argv, SIZE( optList ), optList );

	std::list<std::string> paths;

	while( argv[ index ] != nullptr )
	{
		paths.push_back( argv[ index++ ] );
	}

	if( dumpCartridges )
	{
		CartMap cartMap;

		for( auto &path : paths )
		{
			for( auto &file : GetFiles( path, ".ctg", true ))
			{
				cRefPtr<cCartridge> cartridge = new cCartridge( "" );
				if( cartridge->LoadImage( file.c_str( )) == false )
				{
					fprintf( stderr, "The file \"%s\" does not appear to be a proper ROM cartridge\n", file.c_str( ));
					continue;
				}
				cartMap.insert( { cartridge->sha1( ), cartridge } );
			}
		}

		if( !cartMap.empty( ))
		{
			fprintf( stdout, "List of cartridges found:\n" );

			std::set<std::string> unique;

			for( auto &entry: cartMap )
			{
				auto sha1 = entry.first;
				auto cartridge = entry.second;

				unique.insert( sha1 );
				char duplicate = ( cartMap.count( sha1 ) > 1 ) ? '*' : ' ';

				fprintf( stdout, "%c %s : '%-30.30s' - %s\n", duplicate, sha1.c_str( ), cartridge->GetTitle( ), cartridge->GetFileName( ));
			}

			fprintf( stdout, "\n" );
			fprintf( stdout, "%d cartridges found (%d unique)\n", (int) cartMap.size( ), (int) unique.size( ));
			fprintf( stdout, "\n" );
		}
		else
		{
			fprintf( stdout, "No cartridges found\n" );
			if( dumpDisks || dumpFiles )
			{
				fprintf( stdout, "\n" );
			}
		}
	}

	if( dumpDisks || dumpFiles )
	{
		DiskMap diskMap;
		FileMap fileMap;

		for( auto &path : paths )
		{
			for( auto &file : GetFiles( path, "", true ))
			{
				// Ignore any cartridge files that show up (treat all other files a possible disk image files)
				if( file.extension( ) == ".ctg" )
				{
					continue;
				}

				auto TrackDisk = [&]( cRefPtr<cFileSystem> disk )
				{
					SHA1Context context;

					auto diskFiles = GetFiles( disk );

					for( auto &entry : diskFiles )
					{
						auto sha1 = entry.first;

						context.Update( sha1.c_str( ), sha1.size( ));
					}

					diskMap.insert( { context.Digest( ), disk } );
					fileMap.insert( diskFiles.begin( ), diskFiles.end( ));
				};

				cRefPtr<cFileSystem> disk = cFileSystem::Open( file );
				if( disk == nullptr )
				{
					fprintf( stderr, "Unable to open disk image '%s'\n", file.c_str( ));
					continue;
				}
				if( disk->IsValid( ) == false )
				{
					std::string name = disk->GetPath( );
					fprintf( stderr, "File \"%s\" does not contain a recognized disk format\n", file.c_str( ));
					continue;
				}

				bool tracked = false;

				if( cDiskFileSystem* fs = dynamic_cast<cDiskFileSystem*> ( disk.get( )))
				{
					cRefPtr<cDiskMedia> media = fs->GetMedia( );

					if(( media->GetVolume( ) == 0 ) && ( media->MaxVolume( ) > 0 ))
					{
						tracked = true;

						std::string filename = disk->GetPath( );
						for( int i = 1; i <= media->MaxVolume( ); i++ )
						{
							char volumeName[ 4096 ];
							sprintf( volumeName, "%s#%d", filename.c_str( ), i );
							cRefPtr<cFileSystem> vol = cFileSystem::Open( volumeName );
							if(( vol != nullptr ) && vol->IsValid( ))
							{
								TrackDisk( vol );
							}
						}
					}
				}

				if( tracked == false )
				{
					TrackDisk( disk );
				}
			}
		}

		if( dumpFiles )
		{
			if( ! fileMap.empty( ))
			{
				fprintf( stdout, "List of files found:\n" );

				std::set<std::string> unique;

				for( auto &entry: fileMap )
				{
					auto sha1 = entry.first;
					auto file = entry.second;

					std::string filePath = file->GetPath( );

					unique.insert( sha1 );
					char duplicate = ( fileMap.count( sha1 ) > 1 ) ? '*' : ' ';

					fprintf( stdout, "%c %s : %s\n", duplicate, sha1.c_str( ), filePath.c_str( ));
				}

				fprintf( stdout, "\n" );
				fprintf( stdout, "%d files found (%d unique)\n", (int) fileMap.size( ), (int) unique.size( ));
				fprintf( stdout, "\n" );
			}
			else
			{
				fprintf( stdout, "No files found\n" );
			}

			if( dumpDisks )
			{
				fprintf( stdout, "\n" );
			}
		}

		if( dumpDisks )
		{
			if( ! diskMap.empty( ))
			{
				fprintf( stdout, "List of disks found:\n" );

				std::set<std::string> unique;

				for( auto &entry: diskMap )
				{
					auto sha1 = entry.first;
					auto disk = entry.second;

					std::string diskName = disk->GetName( );
					std::string diskPath = disk->GetPath( );

					unique.insert( sha1 );
					char duplicate = ( diskMap.count( sha1 ) > 1 ) ? '*' : ' ';

					fprintf( stdout, "%c %s : '%-10.10s' - %s\n", duplicate, sha1.c_str( ), diskName.c_str( ), diskPath.c_str( ));
				}

				fprintf( stdout, "\n" );
				fprintf( stdout, "%d disks found (%d unique)\n", (int) diskMap.size( ), (int) unique.size( ));
				fprintf( stdout, "\n" );
			}
			else
			{
				fprintf( stdout, "No disks found\n" );
			}
		}
	}

	return 0;
}
