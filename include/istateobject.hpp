
//----------------------------------------------------------------------------
//
// File:		istateobject.hpp
// Date:		24-Aug-2011
// Programmer:	Marc Rousseau
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
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
//
// Revision History:
//
//----------------------------------------------------------------------------

#ifndef ISTATEOBJECT_HPP_
#define ISTATEOBJECT_HPP_

#include <array>
#include <filesystem>
#include <map>
#include <optional>
#include <vector>
#include "iBaseObject.hpp"

enum class SaveFormat
{
	BINARY = 2,
	DECIMAL = 10,
	HEXADECIMAL = 16
};

struct sStateSection
{
	std::string								name;
	std::map<std::string,std::string>		data;
	std::vector<sStateSection>				subsections;

	static std::optional<sStateSection> LoadImage( std::filesystem::path path );
	void SaveImage( std::filesystem::path path );

	void addSubSection( iBaseObject * );
	void loadSubSection( iBaseObject * ) const;

	void store( const std::string &name, bool value );
	void store( const std::string &name, const std::string &value );
	template <typename T> void store( const std::string &name, const T *values, size_t size );
	template <typename T> void store( const std::string &name, const T &value, SaveFormat format );

	bool hasValue( const std::string &name ) const;
	bool hasSubsection( const std::string &name ) const;
	const std::string &getValue( const std::string &name ) const;
	const sStateSection &getSubsection( const std::string &name ) const;

	void load( const std::string &name, bool &value ) const;
	void load( const std::string &name, std::string &value ) const;
	template <typename T> void load( const std::string &name, T *values, size_t size ) const;
	template <typename T> void load( const std::string &name, T &value, SaveFormat format ) const;
};

struct iStateObject :
	virtual iBaseObject
{
	virtual std::string GetIdentifier( ) = 0;
	virtual std::optional<sStateSection> SaveState( ) = 0;
	virtual bool ParseState( const sStateSection &state ) = 0;

protected:

	virtual ~iStateObject( ) = default;
};

#endif
