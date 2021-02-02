//----------------------------------------------------------------------------
//
// File:		disk-util.hpp
// Date:		25-Jul-2015
// Programmer:	Marc Rousseau
//
// Description: A set of utility functions for disk functions
//
// Copyright (c) 2015 Marc Rousseau, All Rights Reserved.
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

#ifndef DISK_UTIL_HPP_
#define DISK_UTIL_HPP_

#include <cstddef>
#include <list>
#include <vector>
#include "common.hpp"

class cBitStream;

struct sDataFragment
{
	size_t						bitOffsetStart	{ 0 };
	size_t						bitOffsetEnd	{ 0 };
	int							clock			{ 0 };
	std::vector<unsigned char>	byteData		{ };
};

extern std::list<sDataFragment> DecodeDataFM( cBitStream &stream );
extern std::list<sDataFragment> DecodeDataMFM( cBitStream &stream );

extern std::vector<UINT8> EncodeDataFM( const std::list<sDataFragment> &fragments, bool lsb );
extern std::vector<UINT8> EncodeDataMFM( const std::list<sDataFragment> &fragments, bool lsb );

#endif
