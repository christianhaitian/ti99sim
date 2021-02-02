//----------------------------------------------------------------------------
//
// File:		support.hpp
// Date:		15-Jan-2003
// Programmer:	Marc Rousseau
//
// Description: This file contains startup code for Linux/SDL
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

#ifndef SUPPORT_HPP_
#define SUPPORT_HPP_

#include <filesystem>
#include <string>
#include <list>
#include "common.hpp"

std::filesystem::path GetCommonPath( );
std::filesystem::path GetHomePath( );

std::filesystem::path GetHomePath( const char *path );
std::filesystem::path CreateHomePath( const char *path = nullptr );

bool IsAbsolutePath( const std::filesystem::path &filename );
bool IsWriteable( const std::filesystem::path &filename );

std::list<std::filesystem::path> GetFiles( std::filesystem::path directory, const std::string &extension, bool recurse );

// File location utilities - all LocateXYZ functions search the CWD, ~/.ti99sim/<path>, and <install-dir>/<path> for files

std::list<std::filesystem::path> LocateFiles( const std::filesystem::path &path, const std::string &extension );
std::filesystem::path LocateFile( const std::filesystem::path &path, const std::filesystem::path &filename );
std::filesystem::path LocateCartridge( const std::string &folder, const std::string &sha1 );
std::filesystem::path LocateCartridge( const std::string &folder, const std::string &name, const std::list<std::string> &signatures );

bool Is6K( const UINT8 *data, int size );

std::string sha1( const void *data, size_t length );

// OS-specific SHA1 Methods

void *CreateSHA1Context( );
bool UpdateSHA1Context( void *context, const void *data, size_t length );
std::string GetDigest( void *context );
void ReleaseSHA1Context( void *context );

class SHA1Context
{
	void	*context{ nullptr };

public:

	SHA1Context( )
	{
		context = CreateSHA1Context( );
	}

	~SHA1Context( )
	{
		ReleaseSHA1Context( context );
	}

	bool Update( const void *data, size_t length )
	{
		return UpdateSHA1Context( context, data, length );
	}

	std::string Digest( )
	{
		return GetDigest( context );
	}
};

#include <algorithm>
#include <cctype>
#include <charconv>
#include <map>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace util
{
	static inline char TOHEX( int x )
	{
		return ( x > 9 ) ? x + 'A' - 10 : x + '0';
	}

	template<typename T>
	auto toHex( T value )
	{
		std::string buffer( sizeof( value ) * 2, '0' );

		char *ptr = &buffer.back( );

		for( size_t i = buffer.length( ); value && i; i-- )
		{
			*ptr-- = TOHEX( value & 0x0F );
			value >>= 4;
		}

		return buffer;
	}

	template<typename T>
	auto fromHex( const std::string_view &text )
	{
		std::vector<T> data;

		for( auto begin = text.begin( ), end = text.end( ); begin != end; )
		{
			T value = 0;
			while( std::isspace( *begin ) && ( begin < end )) begin++;
			auto [ptr, err] = std::from_chars( begin, end, value, 16 );
			if( err != std::errc( ))
			{
				break;
			}
			data.push_back( value );
			begin = ptr;
		}

		return data;
	}

	template<typename T>
	auto toHexArray( const T *data, size_t length ) -> std::string
	{
		std::string line = length ? toHex( *data ) : "";

		for( auto begin = data + 1, end = data + length; begin != end; )
		{
			line += ' ';
			line += toHex( *begin++ );
		}

		return line;
	}

	template<typename T>
	auto toHexDump( const T *data, size_t length, size_t width = 16 / sizeof( T ), bool skip = true, T fill = 0 ) -> std::map<std::string,std::string>
	{
		std::map<std::string,std::string> hexdump;

		for( size_t offset = 0; offset < length; )
		{
			auto begin = data + offset;
			auto end = begin + std::min( length - offset, width );

			if( !skip || !std::all_of( begin, end, [=]( T byte ){ return byte == fill; }))
			{
				hexdump.insert({ toHex( static_cast<uint16_t>( offset )), toHexArray( begin, end - begin ) });
			}
			offset += width;
		}

		return hexdump;
	}

	template<typename T>
	auto fromHexDump( const std::map<std::string,std::string> &dump, T fill = 0 ) -> std::vector<T>
	{
		std::vector<T> data;

		for( auto &line : dump )
		{
			size_t offset = fromHex<uint16_t>( line.first )[0];
			auto values = fromHex<T>( line.second );
			if( data.size( ) < offset + values.size( ))
			{
				data.resize( offset + values.size( ), fill );
			}
			std::copy( values.begin( ), values.end( ), data.begin( ) + offset );
		}

		return data;
	}
}

#endif
