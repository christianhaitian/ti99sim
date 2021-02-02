//----------------------------------------------------------------------------
//
// File:		isector.hpp
// Date:		01-Jul-2015
// Programmer:	Marc Rousseau
//
// Description: Default class for the TMS5220 Speech Synthesizer Chip
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

#ifndef ISECTOR_HPP_
#define ISECTOR_HPP_

#include "common.hpp"
#include <vector>

constexpr int DEFAULT_SECTOR_SIZE = 256;

using sDataBuffer = std::vector<UINT8>;

struct iSector
{
	virtual auto GetData( ) const -> const UINT8 * = 0;

	virtual auto Size( ) const -> size_t = 0;

	virtual auto Read( ) const -> sDataBuffer = 0;

	virtual auto Write( const sDataBuffer &data ) -> bool = 0;

protected:

	iSector( ) = default;
	iSector( const iSector & ) = default;

	iSector &operator =( const iSector & ) = default;

	virtual ~iSector( ) {}
};

#endif
