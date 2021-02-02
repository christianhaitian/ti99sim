//----------------------------------------------------------------------------
//
// File:        ti994a-gk.hpp
// Date:        28-Mar-2016
// Programmer:  Marc Rousseau
//
// Description:
//
// Copyright (c) 2016 Marc Rousseau, All Rights Reserved.
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

#ifndef TI99SIM_GK_H
#define TI99SIM_GK_H

#include "ti994a.hpp"

enum class WRITE_PROTECT: char
{
	BANK1,
	ENABLED,
	BANK2
};

class cTI994AGK :
	public cTI994A
{
protected:

	cRefPtr<iCartridge> m_GK_Cartridge;

	WRITE_PROTECT       m_GK_WriteProtect;		// Gram Kracker Switch 4 (Bank1/WP/Bank2)

	bool                m_GK_Enabled;			// Gram Kracker Switch 1 (Off/Normal/Reset)
	bool                m_GK_OpSys;				// Gram Kracker Switch 2 (Gram0/OpSys)
	bool                m_GK_BASIC;				// Gram Kracker Switch 3 (Gram12/TI BASIC)
	bool                m_GK_LoaderOn;			// Gram Kracker Switch 5 (Loader On/Loader Off)

public:

	cTI994AGK( iCartridge *, iTMS9918A * = nullptr, iTMS9919 * = nullptr, iTMS5220 * = nullptr );
	virtual ~cTI994AGK( ) override;

	// iComputer methods
	virtual bool InsertCartridge( iCartridge * ) override;
	virtual void RemoveCartridge( ) override;

protected:

	// cTI994A methods
	virtual std::optional<sStateSection> SaveState( ) override;
	virtual bool ParseState( const sStateSection & ) override;

	using cTI994A::RemoveCartridge;

	// Gram Kracker methods
	virtual void GK_SetEnabled( bool );
	virtual void GK_SetGRAM0( bool );
	virtual void GK_SetGRAM12( bool );
	virtual void GK_SetLoader( bool );
	virtual void GK_SetWriteProtect( WRITE_PROTECT );

private:

	// Internal Gram Kracker methods
	void SetEnabled( bool );
	void SetGRAM0( bool );
	void SetGRAM12( bool );
	void SetLoader( bool );
	void SetWriteProtect( WRITE_PROTECT );

	cTI994AGK( const cTI994AGK & ) = delete;      // no implementation
	void operator =( const cTI994AGK & ) = delete; // no implementation
};

#endif // TI99SIM_GK_H
