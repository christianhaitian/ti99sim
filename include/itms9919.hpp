//----------------------------------------------------------------------------
//
// File:		tms9919.hpp
// Date:		22-Sep-2011
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

#ifndef ITMS9919_HPP_
#define ITMS9919_HPP_

#include "common.hpp"
#include "iBaseObject.hpp"

struct iTMS5220;

struct iTMS9919 :
	virtual iBaseObject
{
	virtual void SetSpeechSynthesizer( iTMS5220 * ) = 0;
	virtual void WriteData( UINT8 data ) = 0;
	virtual int GetPlaybackFrequency( ) = 0;

protected:

	virtual ~iTMS9919( ) {}
};

#endif

