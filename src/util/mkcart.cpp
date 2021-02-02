//----------------------------------------------------------------------------
//
// File:        mkcart.cpp
// Date:        12-Apr-2016
// Programmer:  Marc Rousseau
//
// Description: This program will create a set of cartridges from files in a specified directory
//
// Copyright (c) 2016 Marc Rousseau, All Rights Reserved.
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
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fnmatch.h>
#include <list>
#include <optional>
#include <string>
#include "common.hpp"
#include "logger.hpp"
#include "cartridge.hpp"
#include "option.hpp"
#include "disk-media.hpp"
#include "file-system-disk.hpp"
#include "fileio.hpp"
#include "support.hpp"

DBG_REGISTER( __FILE__ );

struct sTOSEC
{
	std::string							filename;
	std::string							extension;
	std::string							title_version;
	std::string							title;
	std::string							version;
	std::string							demo;
	std::string							date;
	std::string							publisher;
	std::list<std::string>				fields;
	std::list<std::string>				flags;
};

struct sCartridgeInfo
{
	std::string							filename;
	std::string							grom;
	std::string							rom0;
	std::string							rom1;
	std::string							title;
	std::map<std::string,std::string>	features;
};

static inline UINT16 GetUINT16( const void *_ptr )
{
	FUNCTION_ENTRY( nullptr, "GetUINT16", true );

	const UINT8 *ptr = ( const UINT8 * ) _ptr;
	return ( UINT16 ) (( ptr[ 0 ] << 8 ) | ptr[ 1 ] );
}

void AddFile( sCartridgeInfo &info, const std::filesystem::path &name, char type )
{
	switch( tolower( type ))
	{
		case 'g' :
			info.grom = name;
			break;
		case 'c' :
			info.rom0 = name;
			break;
		case 'd' :
			info.rom1 = name;
			break;
		default :
			fprintf( stderr, "Unexpected filename '%s'\n", name.c_str( ));
			break;
	}
}

void ParseTitleVersion( std::string TitleVersion, sTOSEC *tosec )
{
	std::cmatch what;

	// Look for a version (e.g. Rev 1.0, v1.0)
	std::regex version( R"((.*?) ((?:(?:(?:.*?[vV]) )|[vV])\d.*?))", std::regex_constants::ECMAScript );
	if( std::regex_match( TitleVersion.c_str( ), what, version ))
	{
		tosec->version = std::string( what[ 2 ].first, what[ 2 ].second );
		TitleVersion   = std::string( what[ 1 ].first, what[ 1 ].second );
	}

	// Look for a comma and fix title if necessary (e.g. Attack, The -> The Attack)
	std::regex comma( R"((.*?), (.*?))", std::regex_constants::ECMAScript );
	if( std::regex_match( TitleVersion.c_str( ), what, comma ))
	{
		TitleVersion = std::string( what[ 2 ].first, what[ 2 ].second ) + " " + std::string( what[ 1 ].first, what[ 1 ].second );
	}

	tosec->title = TitleVersion;
}

std::optional<sTOSEC> ParseTOSEC( const std::string &filename )
{
	// Parse a TOSEC name:
	//   Title version (demo) (Date)(Publisher)(System)(Video)(Country)(Language)(Copyright status)(Development status)(Media Type)(Media Label)[Dump info flags][more info]
	// Minimum required name:
	//   Title (Date)(Publisher)

	// A regular expression to extract the Title version (demo) (........) and extension
	std::regex TitleVersion( R"(.*/(.*?) (\(.*?\) )?(\(.*)\.(.*))", std::regex_constants::ECMAScript );

	std::cmatch what;
	if( std::regex_match( filename.c_str( ), what, TitleVersion ))
	{
		sTOSEC tosec;

		tosec.filename      = filename;
		tosec.title_version = std::string( what[ 1 ].first, what[ 1 ].second );
		tosec.demo          = std::string( what[ 2 ].first, what[ 2 ].second );
		tosec.extension     = std::string( what[ 4 ].first, what[ 4 ].second );

		ParseTitleVersion( tosec.title_version, &tosec );

		std::string remainder( what[ 3 ].first, what[ 3 ].second );

		// A regular expression to extract the next field (field)(.......)
		std::regex NextParen( R"(\(([^)]*)\)(.*))", std::regex_constants::ECMAScript );

		while( std::regex_match( remainder.c_str( ), what, NextParen ))
		{
			int length = ( int ) ( what[1].second - what[1].first );
			tosec.fields.push_back( std::string( what[ 1 ].first, what[ 1 ].second ));
			remainder.erase( 0, length + 2 );
		}

		// A regular expression to extract the next flag [flag](.......)
		std::regex NextFlag( R"(\[([^\]]*)\](.*))", std::regex_constants::ECMAScript );

		while( std::regex_match( remainder.c_str( ), what, NextFlag ))
		{
			int length = ( int ) ( what[1].second - what[1].first );
			tosec.flags.push_back( std::string( what[ 1 ].first, what[ 1 ].second ));
			remainder.erase( 0, length + 2 );
		}

		// (Date)(Publisher) are manditory...
		tosec.date      = tosec.fields.front( ); tosec.fields.pop_front( );
		tosec.publisher = tosec.fields.front( ); tosec.fields.pop_front( );

		if( tosec.publisher == "-" )
		{
			tosec.publisher.clear( );
		}

		return tosec;
	}

	return { };
}

int AddTOSECFiles( sCartridgeInfo &info, const sTOSEC &file1, const sTOSEC &file2, const sTOSEC &file3 )
{
	int count = 0;

	auto &name1 = file1.filename;
	auto &name2 = file2.filename;
	auto &name3 = file3.filename;

	size_t pos = name1.rfind( '.' );

	if( pos == name1.length( ) - 2 )
	{
		pos += 1;

		info.filename = file1.title + ".ctg";

		count += 1;
		AddFile( info, name1, name1[ pos ]);
		if( name1.compare( 0, pos, name2, 0, pos ) == 0 )
		{
			count += 1;
			AddFile( info, name2, name2[ pos ]);
			if( name1.compare( 0, pos, name3, 0, pos ) == 0 )
			{
				count += 1;
				AddFile( info, name3, name1[ pos ]);
			}
		}
	}
	else
	{
		size_t part = name1.rfind( "ile 1 of" );
		if( part != std::string::npos )
		{
			pos -= 2;

			size_t start = name1.rfind( '(' );
			info.filename = name1.substr( start + 1, pos - start - 1 ) + ".ctg";

			count += 1;
			AddFile( info, name1, name1[ pos ]);
			if( name1.compare( 0, part, name2, 0, part ) == 0 )
			{
				count += 1;
				AddFile( info, name2, name2[ pos ]);
				if( name1.compare( 0, part, name3, 0, part ) == 0 )
				{
					count += 1;
					AddFile( info, name3, name3[ pos ]);
				}
			}
		}
		else
		{
			count = 1;
			info.filename = file1.title + ".ctg";
			info.grom = name1;
		}
	}

	info.title = file1.title;

	std::transform( info.filename.begin( ), info.filename.end( ), info.filename.begin( ), ::tolower );
	std::transform( info.filename.begin( ), info.filename.end( ), info.filename.begin( ),
		[]( unsigned char c ) { return ( c == ' ' ) ? '-' : c; } );

	// Clean up an duplicate hyphens we might have added
	for( auto match = info.filename.find( "--" ); match != std::string::npos; match = info.filename.find( "--" ))
	{
		info.filename = info.filename.substr( 0, match ) + info.filename.substr( match + 1 );
	}

	info.features.insert( { "date", file1.date } );
	info.features.insert( { "publisher", file1.publisher } );
	info.features.insert( { "version", file1.version } );

	return count;
}

void FindTOSECCartridges( std::list<std::filesystem::path> &files, std::list<sCartridgeInfo> &infoList )
{
	std::list<sTOSEC> tosecList;

	for( auto next = files.begin( ); next != files.end( ); )
	{
		auto file = next++;

		if( auto tosec = ParseTOSEC( *file ))
		{
			tosecList.push_back( *tosec );
			files.erase( file );
		}
	}

	while( !tosecList.empty( ))
	{
		auto it = tosecList.begin( );

		// Get 3 potential files for the next cartridge
		auto file1 = ( it != tosecList.end( )) ? *it++ : sTOSEC{ };
		auto file2 = ( it != tosecList.end( )) ? *it++ : sTOSEC{ };
		auto file3 = ( it != tosecList.end( )) ? *it++ : sTOSEC{ };

		sCartridgeInfo info{ "", "", "", "", "", { }};

		for( int matches = AddTOSECFiles( info, file1, file2, file3 ); matches > 0; --matches )
		{
			tosecList.pop_front( );
		}

		infoList.push_back( info );
	}
}

std::optional<size_t> FilesRelated( const std::filesystem::path &file1, const std::filesystem::path &file2 )
{
	if( file1.native( ).size( ) == file2.native( ).size( ))
	{
		if( file1.stem( ) == file2.stem( ))
		{
			auto ext1 = file1.extension( ).native( );
			auto ext2 = file2.extension( ).native( );

			if(( ext1.size( ) == 2 ) && ( ext2.size( ) == 2 ))
			{
				return file1.native( ).rfind( '.' ) + 1;
			}
		}

		if( file1.extension( ) == file2.extension( ))
		{
			auto stem1 = file1.stem( ).native( );
			auto stem2 = file2.stem( ).native( );

			if( stem1.back( ) != stem2.back( ))
			{
				stem1.pop_back( );
				stem2.pop_back( );

				if( stem1 == stem2 )
				{
					return file1.native( ).rfind( '.' ) - 1;
				}
			}
		}
	}

	return { };
}

void FindCartridges( std::list<std::filesystem::path> &files, std::list<sCartridgeInfo> &infoList )
{
	while( !files.empty( ))
	{
		auto it = files.begin( );

		// Get 3 potential files for the next cartridge
		auto file1 = ( it != files.end( )) ? *it++ : "";
		auto file2 = ( it != files.end( )) ? *it++ : "";
		auto file3 = ( it != files.end( )) ? *it++ : "";

		sCartridgeInfo info{ "", "", "", "", "", { }};

		auto filename = file1.stem( ).native( );

		if( auto index = FilesRelated( file1, file2 ))
		{
			if( *index < file1.native( ).rfind( '.' ))
			{
				filename.pop_back( );
			}

			AddFile( info, file1, file1.native( )[ *index ]);
			files.pop_front( );

			AddFile( info, file2, file2.native( )[ *index ] );
			files.pop_front( );

			if( auto index = FilesRelated( file1, file3 ))
			{
				AddFile( info, file3, file3.native( )[ *index ] );
				files.pop_front( );
			}
		}
		else
		{
			auto stem = file1.stem( ).native( );
			auto extension = file1.extension( ).native( );
			char type = ( extension.size( ) == 2 ) ? extension.back( ) : ( filename.pop_back( ), stem.back( ));

			AddFile( info, file1, type );
			files.pop_front( );
		}

		info.filename = filename + ".ctg";

		infoList.push_back( info );
	}
}

struct sGromHeader
{
	// This is only valid AFTER bytes have been swapped !!!
	UINT8    Valid;
	UINT8    Version;
	UINT8    NoApplications;
	UINT8    reserved1;
	UINT16   PowerUpHeader;
	UINT16   ApplicationHeader;
	UINT16   DSR_Header;
	UINT16   SubprogramHeader;
	UINT16   InterruptHeader;
	UINT16   reserved2;

	sGromHeader( UINT8 *ptr ) :
		Valid( 0 ),
		Version( 0 ),
		NoApplications( 0 ),
		reserved1( 0 ),
		PowerUpHeader( 0 ),
		ApplicationHeader( 0 ),
		DSR_Header( 0 ),
		SubprogramHeader( 0 ),
		InterruptHeader( 0 ),
		reserved2( 0 )
	{
		FUNCTION_ENTRY( this, "sGromHeader ctor", true );

		sGromHeader *rawHdr = reinterpret_cast<sGromHeader *>( ptr );

		Valid             = rawHdr->Valid;
		Version           = rawHdr->Version;
		NoApplications    = rawHdr->NoApplications;
		reserved1         = rawHdr->reserved1;

		PowerUpHeader     = GetUINT16( &rawHdr->PowerUpHeader );
		ApplicationHeader = GetUINT16( &rawHdr->ApplicationHeader );
		DSR_Header        = GetUINT16( &rawHdr->DSR_Header );
		SubprogramHeader  = GetUINT16( &rawHdr->SubprogramHeader );
		InterruptHeader   = GetUINT16( &rawHdr->InterruptHeader );
		reserved2         = GetUINT16( &rawHdr->reserved2 );
	}
};

void FindName( cCartridge *cartridge )
{
	FUNCTION_ENTRY( nullptr, "FindName", true );

	if( cartridge->GetTitle( ) != nullptr )
	{
		return;
	}

	std::string title = "Unknown";

	auto searchGROM = [&]( UINT8 *data, int index, int size )
	{
		if( data != nullptr )
		{
			sGromHeader hdr( data );
			if(( hdr.Valid == 0xAA ) && ( hdr.ApplicationHeader != 0 ))
			{
				int base = ( index * size ) & 0xE000;
				int appHdr = hdr.ApplicationHeader;
				while( appHdr )
				{
					appHdr -= base;
					if(( appHdr < 0 ) || ( appHdr > GROM_BANK_SIZE ))
					{
						break;
					}

					UINT8 length = data[ appHdr + 4 ];
					UINT8 *name = &data[ appHdr + 5 ];

					if( length != 0 )
					{
						for( int i = 0; i < length; i ++ )
						{
							if( !isprint( name[ i ] ))
							{
								goto skip;
							}
						}
						if( *name == '"' )
						{
							name += 1;
							length -= 1;
						}
						while(( length > 0 ) && ( name[ length - 1 ] == ' ' ))
						{
							length -= 1;
						}
						if(( length > 0 ) && ( name[ length - 1 ] == '"' ))
						{
							length -= 1;
						}
						if( length > 0 )
						{
							title = { (char *) name, length };
							return true;
						}
					}
				skip:
					appHdr = ( data[ appHdr ] << 8 ) + data[ appHdr + 1 ];
				}
			}
		}

		return false;
	};

	// Search GROM for headers
	for( unsigned i = 0; i < NUM_GROM_BANKS; i++ )
	{
		sMemoryRegion *memory = cartridge->GetGromMemory( i );
		if( searchGROM( memory->Bank[ 0 ].Data, i, GROM_BANK_SIZE ) )
		{
			goto done;
		}
	}

	// Search ROM for headers
	for( unsigned i = 0; i < NUM_ROM_BANKS; i++ )
	{
		sMemoryRegion *memory = cartridge->GetCpuMemory( i );
		if( searchGROM( memory->Bank[ 0 ].Data, i, ROM_BANK_SIZE ) )
		{
			goto done;
		}
	}

done:

	cartridge->SetTitle( title.c_str( ));
}

cRefPtr<cCartridge> MakeCart( const sCartridgeInfo &info, bool force6K )
{
	cRefPtr<cCartridge> cartridge = new cCartridge ( "" );

	int validBanks = 0;

	auto ReadMemoryRegion = [&]( bool isGROM, int index, int bank, FILE *file, const char *filename )
	{
		sMemoryRegion *memory = isGROM ? cartridge->GetGromMemory( index ) : cartridge->GetCpuMemory( index );

		if( memory->NumBanks != bank )
		{
			fprintf( stderr, "WARNING: Unable to add bank %d to %s without a valid bank %d\n", bank, isGROM ? "GROM" : "ROM", bank - 1 );
			return;
		}

		validBanks += 1;

		memory->NumBanks = bank + 1;

		size_t size = isGROM ? GROM_BANK_SIZE : ROM_BANK_SIZE;

		UINT8 *data = new UINT8[ size ];

		size_t read = fread( data, 1, size, file );

		if( read != size )
		{
			memset( data + read, 0, size - read );
			if(( isGROM == false ) || ( read != 0x1800 ))
			{
				fprintf( stderr, "WARNING: Error reading from %s file %s (read %04X bytes, expecting %04X bytes)\n", isGROM ? "GROM" : "ROM", filename, ( int ) read, ( int ) size );
			}
		}

		// Fill out 6K GROM banks
		if( isGROM && ( force6K || ( read == 0x1800 )))
		{
			for( int i = 0; i < 0x0800; i++ )
			{
				data[ 0x1800 + i ] = data[ 0x0800 + i ] | data[ 0x1000 + i ];
			}
		}

		memory->Bank[ bank ].Type = BANK_ROM;
		memory->Bank[ bank ].Data = data;
	};

	// Read GROM
	if( ! info.grom.empty( ))
	{
		const char *filename = info.grom.c_str( );
		if( FILE *file = fopen( filename, "rb" ))
		{
			int region = 3;
			int ch = getc( file );
			while( !feof( file ))
			{
				ungetc( ch, file );
				ReadMemoryRegion( true, region++, 0, file, filename );
				ch = getc( file );
			}
			fclose( file );
		}
	}

	// Read RAM 0
	if( ! info.rom0.empty( ))
	{
		const char *filename = info.rom0.c_str( );
		if( FILE *file = fopen( filename, "rb" ))
		{
			ReadMemoryRegion( false, 6, 0, file, filename );
			ReadMemoryRegion( false, 7, 0, file, filename );
			fclose( file );
		}
	}

	// Read RAM 1
	if( ! info.rom1.empty( ))
	{
		const char *filename = info.rom1.c_str( );
		if( FILE *file = fopen( filename, "rb" ))
		{
			ReadMemoryRegion( false, 6, 1, file, filename );
			ReadMemoryRegion( false, 7, 1, file, filename );
			fclose( file );
		}
	}

	if( validBanks == 0 )
	{
		return nullptr;
	}

	if( info.title.empty( ))
	{
		FindName( cartridge );
	}
	else
	{
		cartridge->SetTitle( info.title.c_str( ));
	}

	for( auto &feature : info.features )
	{
		cartridge->SetFeature( feature.first.c_str( ), feature.second.c_str( ));
	}

	if( ! info.filename.empty( ))
	{
		cartridge->SaveImage( info.filename.c_str( ));
	}

	return cartridge;
}

std::list<cRefPtr<cCartridge>> CreateCartridges( std::list<std::filesystem::path> &files, bool force6K )
{
	std::list<sCartridgeInfo> infoList;

	FindTOSECCartridges( files, infoList );
	FindCartridges( files, infoList );

	std::list<cRefPtr<cCartridge>> cartridges;

	for( auto &info : infoList )
	{
		cRefPtr<cCartridge> cartridge = MakeCart( info, force6K );
		if( cartridge != nullptr )
		{
			cartridges.push_back( cartridge );
		}
	}

	return cartridges;
}

void PrintUsage( )
{
	FUNCTION_ENTRY( nullptr, "PrintUsage", true );

	fprintf( stdout, "Usage: mkcart [options] directory\n" );
	fprintf( stdout, "\n" );
}

int main( int argc, char *argv[] )
{
	FUNCTION_ENTRY( nullptr, "main", true );

	bool force6K = false;
	bool create= true;
	bool recurse = false;

	sOption optList[ ] =
	{
		{ '6', "force6K",    OPT_VALUE_SET | OPT_SIZE_BOOL, true,  &force6K,      nullptr,  "Treat all GROM banks as 6K when creating new cartridges" },
		{ 'r', "recurse",    OPT_VALUE_SET | OPT_SIZE_BOOL, true,  &recurse,      nullptr,  "Recursively descend into subdirectories when searching for files" },
		{ 'v', "verbose*=n", OPT_VALUE_PARSE_INT,           1,     &verbose,      nullptr,  "Display extra information" }
	};

	if( argc == 1 )
	{
		PrintHelp( SIZE( optList ), optList );
		return 0;
	}

	printf( "TI-99/Sim Bulk Cartridge Converter\n\n" );

	int index = 1;
	index = ParseArgs( index, argc, argv, SIZE( optList ), optList );

	if( index >= argc )
	{
		fprintf( stderr, "No input file specified\n" );
		return -1;
	}

	std::list<cRefPtr<cCartridge>> cartridges;

	if( create )
	{
		auto files = GetFiles( argv[ index ], ".bin", recurse );

		// Handle alternate TOSEC filenames
		files.splice( files.end( ), GetFiles( argv[ index ], ".g", recurse ));
		files.splice( files.end( ), GetFiles( argv[ index ], ".c", recurse ));
		files.splice( files.end( ), GetFiles( argv[ index ], ".d", recurse ));

		// Sort the list (makes sure File 1 of x comes before File 2 of x)
		files.sort( );

		size_t start = cartridges.size( );

		cartridges = CreateCartridges( files, force6K );

		printf( "Created %d new cartridges\n", (int) ( cartridges.size( ) - start ));
	}

	if( cartridges.empty( ))
	{
		fprintf( stderr, "No files found\n" );
		return 0;
	}

	return 0;
}
