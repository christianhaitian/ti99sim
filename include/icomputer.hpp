//----------------------------------------------------------------------------
//
// File:		icomputer.hpp
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

#ifndef ICOMPUTER_HPP_
#define ICOMPUTER_HPP_

#include "common.hpp"
#include "iBaseObject.hpp"

struct iTMS9900;
struct iTMS9918A;
struct iTMS9919;
struct iTMS5220;
struct iDevice;
struct iCartridge;

struct iComputer :
	virtual iBaseObject
{
	virtual iCartridge *GetConsole( ) const = 0;
	virtual iTMS9900 *GetCPU( ) const = 0;
	virtual iTMS9918A *GetVDP( ) const = 0;
	virtual iTMS9919 *GetSoundGenerator( ) const = 0;
	virtual iTMS5220 *GetSynthesizer( ) const = 0;

	virtual UINT8 *GetVideoMemory( ) const = 0;

	virtual void SetGromAddress( UINT16 addr ) = 0;
	virtual UINT16 GetGromAddress( ) const = 0;

	virtual int ReadCRU( UINT16 ) = 0;
	virtual void WriteCRU( UINT16, UINT16 ) = 0;

	virtual bool RegisterDevice( iDevice * ) = 0;
	virtual bool EnableDevice( iDevice * ) = 0;
	virtual bool DisableDevice( iDevice * ) = 0;

	virtual bool InsertCartridge( iCartridge * ) = 0;
	virtual void RemoveCartridge( ) = 0;

	virtual void Reset( ) = 0;

	virtual bool Sleep( int, UINT32 ) = 0;
	virtual bool WakeCPU( UINT32 ) = 0;

	virtual void Run( ) = 0;
	virtual void Stop( ) = 0;
	virtual bool Step( ) = 0;
	virtual bool IsRunning( ) = 0;

	virtual bool SaveImage( const char * ) = 0;
	virtual bool LoadImage( const char * ) = 0;

protected:

	virtual ~iComputer( ) {}
};

#endif
