//----------------------------------------------------------------------------
//
// File:        support.cpp
// Date:        15-Jan-2003
// Programmer:  Marc Rousseau
//
// Description:
//
// Copyright (c) 2003-2004 Marc Rousseau, All Rights Reserved.
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
//   15-Jan-2003    Renamed from original fileio.cpp
//
//----------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <sys/stat.h>
#include <map>
#if defined( __GNUC__ )
	#include <unistd.h>
#endif
#if defined( OS_WINDOWS )
	#include <array>
	#include <userenv.h>
	#include <windows.h>
#endif
#include "common.hpp"
#include "logger.hpp"
#include "cartridge.hpp"
#include "support.hpp"

DBG_REGISTER( __FILE__ );

constexpr char HOME_DIR[ ] = ".ti99sim";

#if defined( OS_OS2 ) || defined( OS_WINDOWS )
	#include <direct.h>
#endif

std::filesystem::path GetCommonPath( )
{
	static std::filesystem::path common{ };

	if( common.empty( ))
	{
		#if defined( TI_DATA_DIR )

			common = TI_DATA_DIR;

		#elif defined( OS_LINUX ) || defined( OS_MACOSX )

			static const char *self[] =
			{
				"/proc/self/exe",		// path for Linux
				"/proc/curproc/file",	// path for some BSD variants
				"/proc/curproc/exe"		// path for NetBSD
			};

			common = "/opt/ti99sim";

			for( auto &path : self )
			{
				if( std::filesystem::is_symlink( path ))
				{
					auto exe = std::filesystem::read_symlink( path ).parent_path( );
					auto base = ( exe.filename( ) == "bin" ) ? exe.parent_path( ) : exe;
					common = base.lexically_normal( );
					break;
				}
			}

		#elif defined( OS_WINDOWS )

			static std::array<char,MAX_PATH> buffer;

			GetModuleFileName( nullptr, buffer.data( ), buffer.size( ));
			auto base = std::filesystem::path{ buffer }.parent_path( );
			common = base.lexically_normal( );

		#else

			common = "./";

		#endif
	}

	return common;
}

std::filesystem::path GetHomePath( )
{
	static std::filesystem::path home{ };

	if( home.empty( ))
	{
		const char *ptr = getenv( "HOME" );

		#if defined( OS_WINDOWS )

			std::array<char,MAX_PATH> fileName;

			if( ptr == nullptr )
			{
				HANDLE hProcessToken = NULL;

				OpenProcessToken( GetCurrentProcess( ), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hProcessToken );

				DWORD nCount = MAX_PATH;

				GetUserProfileDirectory( hProcessToken, fileName.data( ), &nCount );

				ptr = fileName.data( );
			}

		#endif

		home = ptr ? ptr : ".";
		home /= HOME_DIR;
	}

	return home;
}

std::filesystem::path GetHomePath( const char *path )
{
	auto homePath = GetHomePath( );

	return path ? homePath / path : homePath;
}

std::filesystem::path CreateHomePath( const char *path )
{
	auto homePath = GetHomePath( );
	auto basePath = homePath / "..";

	std::filesystem::create_directory( homePath, basePath );

	if( path )
	{
		homePath /= path;

		std::filesystem::create_directory( homePath, basePath );
	}

	return homePath;
}

bool IsWriteable( const std::filesystem::path &filename )
{
	FUNCTION_ENTRY( nullptr, "IsWriteable", true );

	auto permissions = std::filesystem::status( filename ).permissions( );

	if(( permissions & std::filesystem::perms::others_write ) != std::filesystem::perms::none )
	{
		return true;
	}

#if defined( __GNUC__ )

	struct stat info;
	if( stat( filename.c_str( ), &info ) == 0 )
	{
		if( getuid( ) == info.st_uid )
		{
			if( info.st_mode & S_IWUSR )
			{
				return true;
			}
		}
		if( getgid( ) == info.st_gid )
		{
			if( info.st_mode & S_IWGRP )
			{
				return true;
			}
		}
	}

#endif

	return false;
}

static bool TryPath( const std::string &path, const std::string &filename )
{
	FUNCTION_ENTRY( nullptr, "TryPath", true );

	auto buffer = std::filesystem::path{ path } / filename;

	FILE *file = fopen( buffer.c_str( ), "rb" );
	if( file != nullptr )
	{
		fclose( file );
		return true;
	}

	return false;
}

std::list<std::filesystem::path> GetFiles( std::filesystem::path directory, const std::string &extension, bool recurse )
{
	std::list<std::filesystem::path> list;

	auto addFile = [&]( auto &entry )
	{
		if( entry.is_regular_file( ))
		{
			auto path = entry.path( );
			if( extension.empty( ) || ( path.extension( ) == extension ))
			{
				list.push_back( path );
			}
		}
	};

	if( std::filesystem::exists( directory ))
	{
		if( recurse )
		{
			for( auto &entry : std::filesystem::recursive_directory_iterator( directory ))
			{
				addFile( entry );
			}
		}
		else
		{
			for( auto &entry : std::filesystem::directory_iterator( directory ))
			{
				addFile( entry );
			}
		}

		list.sort( );
	}

	return list;
}

std::filesystem::path LocateFile( const std::filesystem::path &directory, const std::filesystem::path &filename )
{
	FUNCTION_ENTRY( nullptr, "LocateFile", true );

	if( filename.empty( ))
	{
		return filename;
	}

	// If we were given an absolute path, see if it's valid or not
	if( filename.is_absolute( ))
	{
		return std::filesystem::exists( filename ) ? filename : "";
	}

	// Try: CWD
	if( TryPath( ".", filename ) == true )
	{
		return filename;
	}

	std::filesystem::path fullname = directory.empty( ) ? filename : directory / filename;
	std::filesystem::path fullpath;

	// Try: ~/.ti99sim/path, /opt/ti99sim/path
	if(( TryPath( fullpath = GetHomePath( ), fullname )   == true ) ||
	   ( TryPath( fullpath = GetCommonPath( ), fullname ) == true ))
	{
		return fullpath / fullname;
	}

	return "";
}

std::list<std::filesystem::path> LocateFiles( const std::filesystem::path &directory, const std::string &extension )
{
	static std::filesystem::path paths[ ] =
	{
		GetCommonPath( ),	// Try: <install-dir>/path
		GetHomePath( ),		// Try: ~/.ti99sim/path
		"."					// Try: CWD
	};

	std::map<std::filesystem::path,std::filesystem::path> files;

	auto addFile = [&]( auto &entry )
	{
		if( entry.is_regular_file( ))
		{
			auto path = entry.path( );
			if( extension.empty( ) || ( path.extension( ) == extension ))
			{
				files[ path.filename( ) ] = path;
			}
		}
	};

	for( auto path : paths )
	{
		auto testDir = path / directory;

		if( std::filesystem::exists( testDir ))
		{
			for( auto &entry : std::filesystem::directory_iterator( testDir ))
			{
				addFile( entry );
			}
		}
	}

	std::list<std::filesystem::path> list;

	for( auto iter : files )
	{
		list.push_back( iter.second );
	}

	list.sort( );

	return list;
}

std::filesystem::path LocateCartridge( const std::string &folder, const std::string &sha1 )
{
	auto roms = LocateFiles( folder, ".ctg" );

	for( auto &name : roms )
	{
		cRefPtr<cCartridge> ctg = new cCartridge( name );
		if( ctg->sha1( ) == sha1 )
		{
			return name;
		}
	}

	return "";
}

std::filesystem::path LocateCartridge( const std::string &folder, const std::string &name, const std::list<std::string> &signatures )
{
	// Try to locate the cartridge by filename (legacy support)
	std::string romFile = LocateFile( folder, name );

	if( romFile.empty( ))
	{
		// Try to find the cartridge using the supplied signatures
		for( auto &sha1 : signatures )
		{
			romFile = LocateCartridge( folder, sha1 );
			if( !romFile.empty( ))
			{
				return romFile;
			}
		}
	}

	return romFile;
}

static std::string digest2string( const UINT8 digest[ ], size_t size )
{
	constexpr char hexDigits[ ] = "0123456789abcdef";

	std::string sum( size * 2, '0' );

	for( size_t i = 0; i < size; i++ )
	{
		sum[ 2 * i + 0 ] = hexDigits[ digest[ i ] >> 4 ];
		sum[ 2 * i + 1 ] = hexDigits[ digest[ i ] & 0x0F ];
	}

	return sum;
}

bool Is6K( const UINT8 *data, int size )
{
	if( size != 0x2000 )
	{
		return false;
	}

	for( int i = 0; i < 0x0800; i++ )
	{
		if( data[ 0x1800 + i ] != ( data[ 0x0800 + i ] | data[ 0x1000 + i ] ))
		{
			return false;
		}
	}

	return true;
}

std::string sha1( const void *data, size_t length )
{
	SHA1Context sha1;

	sha1.Update( data, length );

	return sha1.Digest( );
}

#if defined( OS_LINUX )

	#include <openssl/sha.h>

	void *CreateSHA1Context( )
	{
		SHA_CTX *context = new SHA_CTX;

		SHA1_Init( context );

		return context;
	}

	bool UpdateSHA1Context( void *context, const void *data, size_t length )
	{
		return SHA1_Update( static_cast<SHA_CTX*>( context ), data, length );
	}

	std::string GetDigest( void *context )
	{
		UINT8 digest[ SHA_DIGEST_LENGTH ] = { 0 };

		SHA1_Final( digest, static_cast<SHA_CTX*>( context ));

		return digest2string( digest, SHA_DIGEST_LENGTH );
	}

	void ReleaseSHA1Context( void *context )
	{
		delete static_cast<SHA_CTX*>( context );
	}

#endif

#if defined( OS_WINDOWS )

	#include <wincrypt.h>

	#define SHA_DIGEST_LENGTH 20

	struct SHA_CTX
	{
		HCRYPTPROV	hProv{ 0 };
		HCRYPTHASH	hHash{ 0 };
	};

	void *CreateSHA1Context( )
	{
		SHA_CTX *context = new SHA_CTX;

		if( CryptAcquireContext( &context->hProv, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT ))
		{
			if( CryptCreateHash( context->hProv, CALG_SHA1, 0, 0, &context->hHash ))
			{
				return context;
			}
			CryptReleaseContext( context->hProv, 0 );
		}

		delete context;

		return nullptr;
	}

	bool UpdateSHA1Context( void *context, const void *data, size_t length )
	{
		if( context )
		{
			return CryptHashData( static_cast<SHA_CTX*>( context )->hHash, static_cast<const BYTE*>( data ), length, 0 ) ? true : false;
		}

		return false;
	}

	std::string GetDigest( void *context )
	{
		UINT8 digest[ SHA_DIGEST_LENGTH ] = { 0 };

		if( context )
		{
			DWORD cbHash = SHA_DIGEST_LENGTH;

			if( !CryptGetHashParam( static_cast<SHA_CTX*>( context )->hHash, HP_HASHVAL, digest, &cbHash, 0 ))
			{
				memset( digest, 0, sizeof( digest ));
			}
		}

		return digest2string( digest, SHA_DIGEST_LENGTH );
	}

	void ReleaseSHA1Context( void *context )
	{
		if( context )
		{
			CryptDestroyHash( static_cast<SHA_CTX*>( context )->hHash );
			CryptReleaseContext( static_cast<SHA_CTX*>( context )->hProv, 0 );

			delete static_cast<SHA_CTX*>( context );
		}
	}

#endif
