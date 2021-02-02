//----------------------------------------------------------------------------
//
// File:		idevice.hpp
// Date:		22-Sepr-2011
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

#ifndef IDEVICE_HPP_
#define IDEVICE_HPP_

#include "iBaseObject.hpp"
#include "tms9900.hpp"

struct iCartridge;
struct iComputer;

struct iDevice : virtual iBaseObject
{
	virtual bool Initialize( iComputer * ) = 0;

	virtual int GetCRU( ) const = 0;
	virtual iCartridge *GetROM( ) const = 0;
	virtual const char *GetName( ) = 0;

	virtual void WriteCRU( ADDRESS, int ) = 0;
	virtual int ReadCRU( ADDRESS ) = 0;

protected:

	virtual ~iDevice( ) {}
};

#endif
