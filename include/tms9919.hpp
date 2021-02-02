
//----------------------------------------------------------------------------
//
// File:		tms9919.hpp
// Date:		23-Feb-1998
// Programmer:	Marc Rousseau
//
// Description:
//
// Copyright (c) 1998-2004 Marc Rousseau, All Rights Reserved.
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

#ifndef TMS9919_HPP_
#define TMS9919_HPP_

#include "common.hpp"
#include "cBaseObject.hpp"
#include "itms9919.hpp"
#include "istateobject.hpp"

struct iTMS5220;

class cTMS9919 :
	public virtual cBaseObject,
	public virtual iTMS9919,
	public virtual iStateObject
{
protected:

	enum NOISE_COLOR_E
	{
		NOISE_PERIODIC,
		NOISE_WHITE
	};

	cRefPtr<iTMS5220>   m_pSpeechSynthesizer;

	uint8_t             m_LastData;

	uint16_t            m_Frequency[ 4 ];
	uint8_t             m_Attenuation[ 4 ];
	uint8_t             m_NoiseColor;
	uint8_t             m_NoiseType;

	virtual void SetNoise( uint8_t, uint8_t );
	virtual void SetFrequency( uint8_t, uint16_t );
	virtual void SetAttenuation( uint8_t, uint8_t );

public:

	cTMS9919( );

	// iBaseObject Methods
	virtual const void *GetInterface( const std::string &name ) const override;

	// iTMS9919 Methods
	virtual void SetSpeechSynthesizer( iTMS5220 * ) override;
	virtual void WriteData( UINT8 data ) override;
	virtual int GetPlaybackFrequency( ) override;

	// iStateObject Methods
	virtual std::string GetIdentifier( ) override;
	virtual std::optional<sStateSection> SaveState( ) override;
	virtual bool ParseState( const sStateSection &state ) override;

protected:

	virtual ~cTMS9919( ) override;

private:

	cTMS9919( const cTMS9919 & ) = delete;			// no implementation
	void operator =( const cTMS9919 & ) = delete;	// no implementation

};

#endif
