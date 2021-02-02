
//----------------------------------------------------------------------------
//
// File:        tms9918a.cpp
// Date:        28-Sep-2011
// Programmer:  Marc Rousseau
//
// Description:
//
// Copyright (c) 2011 Marc Rousseau, All Rights Reserved.
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

#include <bitset>
#include <cstdio>
#include <iomanip>
#include <fstream>
#include "common.hpp"
#include "logger.hpp"
#include "stateobject.hpp"
#include "support.hpp"

DBG_REGISTER( __FILE__ );

static std::optional<std::tuple<size_t, std::string, std::string>> readLine( std::istream &stream )
{
	std::string line;
	if( std::getline( stream, line ).eof( ))
	{
		return { };
	}

	size_t indent = line.find_first_not_of( ' ' );
	line = line.substr( indent );

	std::istringstream lstream( line );

	std::string key;
	std::getline( lstream, key, ':' );

	std::string value;
	std::getline( lstream >> std::ws, value );

	return {{ indent, key, value }};
}

sStateSection LoadImageFileSection( std::istream &stream )
{
	sStateSection section;

	if( auto base = readLine( stream ))
	{
		auto [ baseIndent, baseKey, baseValue ] = *base;
		if( !baseKey.empty( ) && baseKey.front( ) == '[' )
		{
			section.name = baseKey;
			section.name = section.name.substr( 1 ).substr( 0, section.name.size( ) - 2 );

			auto pos = stream.tellg( );
			while( auto line = readLine( stream ))
			{
				auto [ lineIndent, lineKey, lineValue ] = *line;
				if( lineIndent <= baseIndent )
				{
					stream.seekg( pos );
					break;
				}
				if( !lineKey.empty( ) && lineKey.front( ) == '[' )
				{
					stream.seekg( pos );
					section.subsections.push_back( LoadImageFileSection( stream ));
				}
				else
				{
					section.data.insert({ lineKey, lineValue });
				}

				pos = stream.tellg( );
			}
		}
	}

	return section;
}

void SaveImageFileSection( std::ostream &stream, const sStateSection &contents, int indent )
{
	if( !contents.data.empty( ) || !contents.subsections.empty( ))
	{
		stream << std::setw( indent ) << "" << "[" << contents.name << "]\n";

		indent += 2;

		for( auto &data : contents.data )
		{
			stream << std::setw( indent ) << "" << data.first << ": " << data.second  << "\n";
		}
		for( auto &section : contents.subsections )
		{
			SaveImageFileSection( stream, section, indent );
		}
	}
}

std::optional<sStateSection> sStateSection::LoadImage( std::filesystem::path path )
{
	if( std::filesystem::exists( path ))
	{
		std::ifstream stream{ path };
		auto image = LoadImageFileSection( stream );
		if( !image.name.empty( ) && ( !image.data.empty( ) || !image.subsections.empty( )))
		{
			return image;
		}
	}

	return { };
}

void sStateSection::SaveImage( std::filesystem::path path )
{
	if( std::filesystem::exists( path.parent_path( )))
	{
		std::ofstream stream{ path };
		SaveImageFileSection( stream, *this, 0 );
	}
}

void sStateSection::addSubSection( iBaseObject *base )
{
	if( base != nullptr )
	{
		if( auto object = static_cast< iStateObject * >( base->GetInterface( "iStateObject" )))
		{
			if( auto state = object->SaveState( ))
			{
				subsections.push_back( *state );
			}
		}
	}
}

void sStateSection::loadSubSection( iBaseObject *base ) const
{
	if( base != nullptr )
	{
		if( auto object = static_cast< iStateObject * >( base->GetInterface( "iStateObject" )))
		{
			for( auto &section : subsections )
			{
				if( section.name == object->GetIdentifier( ))
				{
					object->ParseState( section );
					return;
				}
			}
		}
	}
}

void sStateSection::store( const std::string &name, bool value )
{
	data.insert({ name,  value ? "true" : "false" });
}

void sStateSection::store( const std::string &name, const std::string &value )
{
	data.insert({ name,  value });
}

template <typename T>
void sStateSection::store( const std::string &name, const T *values, size_t size )
{
	auto width = 16 / sizeof( T );
	if( size <= width )
	{
		data.insert({ name, util::toHexArray( values, size )});
	}
	else
	{
		sStateSection array;
		array.name = name;
		array.data = util::toHexDump( values,  size );
		if( !array.data.empty( ))
		{
			subsections.push_back( array );
		}
	}
}

template void sStateSection::store( const std::string &name, const uint8_t *values, size_t );
template void sStateSection::store( const std::string &name, const uint16_t *values, size_t );

template <typename T>
void sStateSection::store( const std::string &name, const T &value, SaveFormat format )
{
	switch( format )
	{
		case SaveFormat::BINARY:
			data.insert({ name, std::bitset<sizeof(T)*8>( value ).to_string( )});
			break;
		case SaveFormat::DECIMAL:
			data.insert({ name, std::to_string( value )});
			break;
		case SaveFormat::HEXADECIMAL:
			data.insert({ name, util::toHex( value )});
			break;
	}
}

template void sStateSection::store( const std::string &name, const uint8_t &value, SaveFormat format );
template void sStateSection::store( const std::string &name, const uint16_t &value, SaveFormat format );
template void sStateSection::store( const std::string &name, const uint32_t &value, SaveFormat format );
template void sStateSection::store( const std::string &name, const uint64_t &value, SaveFormat format );
template void sStateSection::store( const std::string &name, const int &value, SaveFormat format );

bool sStateSection::hasValue( const std::string &name ) const
{
	auto match = data.find( name );

	return match != data.end( );
}

bool sStateSection::hasSubsection( const std::string &name ) const
{
	for( auto &section : subsections )
	{
		if( section.name == name )
		{
			return true;
		}
	}

	return false;
}

const std::string &sStateSection::getValue( const std::string &name ) const
{
	if( auto match = data.find( name ); match != data.end( ))
	{
		return match->second;
	}

	throw name + " not found in section [" + this->name + "] of save file data";
}

const sStateSection &sStateSection::getSubsection( const std::string &name ) const
{
	for( auto &section : subsections )
	{
		if( section.name == name )
		{
			return section;
		}
	}

	throw name + " not found in section [" + this->name + "] of save file subsections";
}

void sStateSection::load( const std::string &name, bool &value ) const
{
	value = getValue( name ) == "true";
}

void sStateSection::load( const std::string &name, std::string &value ) const
{
	value = getValue( name );
}

template <typename T>
void sStateSection::load( const std::string &name, T *values, size_t size ) const
{
	std::fill( values, values + size, 0 );

	if( hasValue( name ))
	{
		auto data = util::fromHex<T>( getValue( name ));
		std::copy( data.begin( ), data.end( ), values );
	}
	else if( hasSubsection( name ))
	{
		auto data = util::fromHexDump<uint8_t>( getSubsection( name ).data );
		std::copy( data.begin( ), data.end( ), values );
	}
}

template void sStateSection::load( const std::string &name, uint8_t *values, size_t size ) const;
template void sStateSection::load( const std::string &name, uint16_t *values, size_t size ) const;

template <typename T>
void sStateSection::load( const std::string &name, T &value, SaveFormat format ) const
{
	auto chars = getValue( name );
	auto result = std::from_chars( &*chars.begin( ), &*chars.end( ), value, static_cast<int>( format ));
	if( result.ec != std::errc{ })
	{
		throw std::make_error_code( result.ec).message( ) + " when reading " + name + " from [" + this->name + "]";
	}
}

template void sStateSection::load( const std::string &name, uint8_t &value, SaveFormat format ) const;
template void sStateSection::load( const std::string &name, uint16_t &value, SaveFormat format ) const;
template void sStateSection::load( const std::string &name, uint32_t &value, SaveFormat format ) const;
template void sStateSection::load( const std::string &name, uint64_t &value, SaveFormat format ) const;
template void sStateSection::load( const std::string &name, int &value, SaveFormat format ) const;

cStateObject::cStateObject( )
{
}

//----------------------------------------------------------------------------
// iBaseObject Methods
//----------------------------------------------------------------------------

const void *cStateObject::GetInterface( const std::string &iName ) const
{
	FUNCTION_ENTRY( this, "cStateObject::GetInterface", false );

	if( iName == "iStateObject" )
	{
		return static_cast< const iStateObject * >( this );
	}

	return cBaseObject::GetInterface( iName );
}
