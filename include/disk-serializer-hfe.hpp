//----------------------------------------------------------------------------
//
// File:        disk-serializer-hfe.cpp
// Date:        06-July-2015
// Programmer:  Marc Rousseau
//
// Description: A class to load/save AnaDisk disk images
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

#ifndef DISK_SERIALIZER_HFE_HPP_
#define DISK_SERIALIZER_HFE_HPP_

#include "common.hpp"

namespace HxC
{
	// 0x0000-0x0200 (512 bytes) : File header

	struct FileHeader
	{
		UINT8		signature[ 8 ];				// “HXCPICFE”
		UINT8		revision;					// Revision 0
		UINT8		numTracks;					// Number of track in the file
		UINT8		numSides;					// Number of valid side (Not used by the emulator)
		UINT8		trackEncoding;				// Track Encoding mode
		UINT16		bitRate;					// Bitrate in Kbit/s. Ex : 250=250000bits/s (Max value : 500)
		UINT16		floppyRPM;					// Rotation per minute (Not used by the emulator)
		UINT8		floppyInterfaceMode;		// Floppy interface mode. (Please see the list above)
		UINT8		_unused;					// Free
		UINT16		trackListOffset;			// Offset of the track list LUT in block of 512 bytes (Ex: 1=0x200)
		UINT8		writeAllowed;				// The Floppy image is write protected ?
		UINT8		singleStep;					// 0xFF : Single Step – 0x00 Double Step mode
		UINT8		t0s0AltEncoding;			// 0x00 : Use an alternate track_encoding for track 0 Side 0
		UINT8		t0s0Encoding;				// alternate track_encoding for track 0 Side 0
		UINT8		t0s1AltEncoding;			// 0x00 : Use an alternate track_encoding for track 0 Side 1
		UINT8		t0s1Encoding;				// alternate track_encoding for track 0 Side 1
	};

	constexpr auto HEADER_SIGNATURE			= "HXCPICFE";

	// Floppy Interface modes

	enum class FLOPPYMODE : UINT8
	{
		IBMPC_DD				= 0x00,
		IBMPC_HD				= 0x01,
		ATARIST_DD				= 0x02,
		ATARIST_HD				= 0x03,
		AMIGA_DD				= 0x04,
		AMIGA_HD				= 0x05,
		CPC_DD					= 0x06,
		GENERIC_SHUGGART_DD		= 0x07,
		IBMPC_ED				= 0x08,
		MSX2_DD					= 0x09,
		C64_DD					= 0x0A,
		EMU_SHUGART				= 0x0B,
		S950_DD					= 0x0C,
		S950_HD					= 0x0D,
		DISABLE					= 0xFE
	};

	// Track Encoding types

	enum class ENCODING : UINT8
	{
		ISOIBM_MFM				= 0x00,
		AMIGA_MFM				= 0x01,
		ISOIBM_FM				= 0x02,
		EMU_FM					= 0x03,
		TYCOM_FM				= 0x04,
		MEMBRAIN_MFM			= 0x05,
		APPLEII_GCR1			= 0x06,
		APPLEII_GCR2			= 0x07,
		APPLEII_HDDD_A2_GCR1	= 0x08,
		APPLEII_HDDD_A2_GCR2	= 0x09,
		ARBURGDAT				= 0x0A,
		ARBURGSYS				= 0x0B,
		AED6200P_MFM			= 0x0C,
		UNKNOWN					= 0xFF
	};

	// Note :
	//   If track0s0_altencoding is set to 0xFF, track0s0_encoding is ignored and track_encoding is used for track 0 side 0
	//   If track0s1_altencoding is set to 0xFF, track0s1_encoding is ignored and track_encoding is used for track 0 side 1

	// (up to 1024 bytes) : Track offset LUT

	struct pictrack
	{
		UINT16		offset;					// Offset of the track data in blocks of 512 bytes (Ex: 2=0x400)
		UINT16		trackLength;			// Length of the track data in bytes
	};

}

#include "disk-serializer.hpp"

class cDiskSerializerHFE :
	public cDiskSerializer
{
	std::vector<UINT8> FileBuffer;

public:

	cDiskSerializerHFE( );
	virtual ~cDiskSerializerHFE( ) override;

	static bool MatchesFormat( FILE *file );

	// iDiskSerializer methods
	virtual bool SupportsFeatures( const cDiskImage &image ) override;
	virtual eDiskFormat GetFormat( ) const override;
	virtual bool LoadTrack( size_t cylinder, size_t head, iDiskTrack *track ) override;
	virtual void LoadComplete( ) override;

	// cDiskSerializer methods
	virtual bool ReadFile( FILE *file, cDiskImage *image ) override;
	virtual bool WriteFile( const cDiskImage &image, FILE *file ) override;

protected:

};

#endif
