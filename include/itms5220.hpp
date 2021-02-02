//----------------------------------------------------------------------------
//
// File:		itms5220.hpp
// Date:		22-Sep-2011
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

#ifndef ITMS5220_HPP_
#define ITMS5220_HPP_

#include "common.hpp"
#include "iBaseObject.hpp"

struct iTMS9919;
struct iComputer;

struct iTMS5220 :
	virtual iBaseObject
{
	virtual void SetSoundChip( iTMS9919 * ) = 0;
	virtual void SetComputer( iComputer * ) = 0;

	virtual bool AudioCallback( INT16 *, int ) = 0;

	virtual void Reset( ) = 0;

	virtual UINT8 WriteData( UINT8 ) = 0;
	virtual UINT8 ReadData( UINT8 ) = 0;

protected:

	virtual ~iTMS5220( ) {}
};

#endif
