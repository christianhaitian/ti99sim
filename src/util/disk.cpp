//----------------------------------------------------------------------------
//
// File:        disk.cpp
// Date:        23-Feb-1998
// Programmer:  Marc Rousseau
//
// Description: A simple program to display the contents of a TI-Disk image
//              created by AnaDisk.  It also supports extracting files from
//              the disk and optionally converting them to 'standard' PC type
//              files.
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
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307, USA.
//
// Revision History:
//
//   15-Jan-2003    Created classes to encapsulate TI file & disks
//
//----------------------------------------------------------------------------

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include "common.hpp"
#include "logger.hpp"
#include "disk-media.hpp"
#include "file-system.hpp"
#include "file-system-arc.hpp"
#include "fileio.hpp"
#include "option.hpp"
#include "support.hpp"

#if defined ( OS_OS2 )
	#include <conio.h>
#elif defined ( OS_WINDOWS )
	#include <conio.h>
#elif defined ( OS_LINUX )
	#include <unistd.h>
#endif

DBG_REGISTER( __FILE__ );

struct sCreateDescriptor
{
	int		cylinders;
	int		heads;
	int		sectors;
};

const char *GetDiskFormatString( eDiskFormat format )
{
	const char *strFormat = "<Unknown>";
	switch( format )
	{
		case FORMAT_RAW_TRACK :
			strFormat = "PC99";
			break;
		case FORMAT_RAW_SECTOR :
			strFormat = "v9t9";
			break;
		case FORMAT_ANADISK :
			strFormat = "AnaDisk";
			break;
		case FORMAT_CF7 :
			strFormat = "CF7+";
			break;
		case FORMAT_HFE :
			strFormat = "HFE";
			break;
		default :
			break;
	}

	return strFormat;
}

const char *GetTrackFormatString( track::Format format )
{
	const char *txtDensity = "<Unknown>";

	switch( format )
	{
		case track::Format::Unknown :
			txtDensity = "<Unknown>";
			break;
		case track::Format::FM :
			txtDensity = "Single";
			break;
		case track::Format::MFM :
			txtDensity = "Double";
			break;
	}

	return txtDensity;
}

static inline void SetUINT16( void *_ptr, UINT16 value )
{
	FUNCTION_ENTRY( nullptr, "SetUINT16", true );

	UINT8 *ptr = ( UINT8 * ) _ptr;

	ptr[ 0 ] = value >> 8;
	ptr[ 1 ] = value & 0x0FF;
}

static inline UINT16 GetUINT16( const void *_ptr )
{
	FUNCTION_ENTRY( nullptr, "GetUINT16", true );

	const UINT8 *ptr = ( const UINT8 * ) _ptr;
	return ( UINT16 ) (( ptr[ 0 ] << 8 ) | ptr[ 1 ] );
}

void DumpFile( cFile *file, bool convertFiles )
{
	FUNCTION_ENTRY( nullptr, "DumpFile", true );

	const sFileDescriptorRecord *fdr = file->GetFDR( );

	if( cFileSystem::IsValidFDR( fdr ) == false )
	{
		return;
	}

	std::string base = cFileSystem::EscapeName( { fdr->FileName, 10 } );
	std::string ext;

	// Decorate the name to match the file's type
	if( convertFiles == true )
	{
		if( file->IsProgram( ))
		{
			ext = ".PROG";
		}
		else
		{
			ext += ".";
			ext += file->IsInternal( ) ? 'I' : 'D';
			ext += file->IsVariable( ) ? 'V' : 'F';
			ext += std::to_string( file->RecordLength( ));
		}
	}

	// Find a unique filename to use
	char filename[ 80 ];
	sprintf( filename, "%s%s", base.c_str( ), ext.c_str( ));
	for( int i = 0; i < 255; i++ )
	{
		FILE *testFile = fopen( filename, "rb" );
		if( testFile == nullptr )
		{
			break;
		}
		fclose( testFile );
		sprintf( filename, "%s.%03d%s", base.c_str( ), i, ext.c_str( ));
	}

	const char *mode = ( convertFiles && file->IsDisplay( )) ? "wt" : "wb";

	// Open the file
	FILE *outFile = fopen( filename, mode );
	if( outFile == nullptr )
	{
		return;
	}

	// Write the file header (v9t9 FIAD)
	if( convertFiles == false )
	{
		char header[ 128 ];
		memset( header, 0, 128 );
		memcpy( header, fdr, 28 );
		fwrite( header, 128, 1, outFile );
	}

	if( convertFiles == true )
	{
		bool errors = false;

		int count;
		do
		{
			char sector[ DEFAULT_SECTOR_SIZE ];
			count = file->ReadRecord( sector, DEFAULT_SECTOR_SIZE );
			if( count >= 0 )
			{
				// Handle variable length, internal type files
				if( file->IsVariable( ) && file->IsInternal( ))
				{
					fputc( count, outFile );
				}
				fwrite( sector, count, 1, outFile );
				// Handle display type files
				if( file->IsDisplay( ))
				{
					for( int i = 0; i < count; i++ )
					{
						errors |= ( sector[ i ] == '\n' );
					}
					fprintf( outFile, "\n" );
				}
			}
		}
		while( count >= 0 );

		if( errors )
		{
			fprintf( stderr, "File '%s' contains newline characters and will not recoverable\n", filename );
		}
	}
	else
	{
		int totalSectors = file->TotalSectors( );
		for( int i = 0; i < totalSectors; i++ )
		{
			char sector[ DEFAULT_SECTOR_SIZE ];
			if( file->ReadSector( i, sector ) < 0 )
			{
				fprintf( stderr, "I/O Error reading file %10.19s\n", fdr->FileName );
				break;
			}
			fwrite( sector, DEFAULT_SECTOR_SIZE, 1, outFile );
		}
	}

	fclose( outFile );
}

void PrintDiskLayout( cDiskMedia *media )
{
	FUNCTION_ENTRY( nullptr, "PrintDiskLayout", true );

	track::Format format     = track::Format::Unknown;
	track::Format lastFormat = track::Format::Unknown;

	printf( "\n" );

	for( int h = 0; h < media->NumSides( ); h++ )
	{
		for( int t = 0; t < media->NumTracks( ); t++ )
		{
			iDiskTrack *track = media->GetTrack( t, h );
			if( track == nullptr )
			{
				continue;
			}
			if( format == track::Format::Unknown )
			{
				format = track->GetFormat( );
			}
			printf( "Track: %2d  Side: %d - ", t, h );
			if( track->GetFormat( ) == track::Format::Unknown )
			{
				printf( "is not formatted\n" );
				continue;
			}
			if( lastFormat != track->GetFormat( ))
			{
				if( lastFormat != track::Format::Unknown )
				{
					printf( "format changed -" );
				}
				lastFormat = track->GetFormat( );
			}
			int noSectors = 0;
			for( auto &sector : track->GetSectors( ))
			{
				printf( " %d", sector->LogicalSector( ));
				noSectors++;
			}
			printf( "\n" );
			for( auto &sector : track->GetSectors( ))
			{
				if( sector->GetData( ) == nullptr )
				{
					printf( "  sector %d does not contain any data\n", sector->LogicalSector( ));
				}
				if( sector->DataMark( ) != MARK_DAM )
				{
					printf( "  contains deleted sector %d\n", sector->LogicalSector( ));
				}
				if( sector->LogicalCylinder( ) != t )
				{
					printf( "  sector %d has incorrect cylinder %d\n", sector->LogicalSector( ), sector->LogicalCylinder( ));
				}
				if( sector->LogicalHead( ) != h )
				{
					printf( "  sector %d has incorrect side %d\n", sector->LogicalSector( ), sector->LogicalHead( ));
				}
				if( sector->LogicalSize( ) != 1 )
				{
					printf( "  sector %d size is %d bytes\n", sector->LogicalSector( ), ( int ) sector->Size( ));
				}
			}
		}
	}
}

void PrintDiskInfo( cDiskMedia *media )
{
	FUNCTION_ENTRY( nullptr, "PrintDiskInfo", true );

	printf( "\n" );

	bool mixed = false;
	track::Format format = track::Format::Unknown;

	for( int h = 0; h < media->NumSides( ); h++ )
	{
		for( int t = 0; t < media->NumTracks( ); t++ )
		{
			iDiskTrack *track = media->GetTrack( t, h );
			if( track == nullptr )
			{
				continue;
			}
			if( format == track::Format::Unknown )
			{
				format = track->GetFormat( );
			}
			else if(( format != track->GetFormat( )) && ( track->GetFormat( ) != track::Format::Unknown ))
			{
				mixed = true;
			}
		}
	}

	printf( "     File: %s", media->GetName( ));
	if( media->GetFormat( ) == FORMAT_CF7 )
	{
		printf( " - Volume %d", media->GetVolume( ));
	}
	printf( "\n" );
	printf( "   Format: %s\n", GetDiskFormatString( media->GetFormat( )) );
	printf( "   Tracks: %d\n", media->NumTracks( ));
	printf( "    Sides: %d\n", media->NumSides( ));
	printf( "  Density: %s\n", mixed ? "Mixed" : GetTrackFormatString( format ));
}

bool ParseCreate( const char *arg, void *base )
{
	FUNCTION_ENTRY( nullptr, "ParseCreate", true );

	const char *ptr = strchr( arg, '=' ) + 1;

	sCreateDescriptor *info = static_cast<sCreateDescriptor*>( base );

	char trash[ 8 ];

	if( strlen( ptr ) > 8 )
	{
		fprintf( stderr, "Disk image type '%s' too long.\n", arg );
		return false;
	}

	if( sscanf( ptr, "%d:%d:%d%s", &info->cylinders, &info->heads, &info->sectors, trash ) == 3 )
	{
		if(( info->cylinders != 35 ) && ( info->cylinders != 40 ) && ( info->cylinders != 80 ))
		{
			fprintf( stderr, "Invalid number of tracks (%d) for disk image\n", info->cylinders );
			return false;
		}
		if(( info->heads != 1 ) && ( info->heads != 2 ))
		{
			fprintf( stderr, "Invalid number of sides (%d) for disk image\n", info->heads );
			return false;
		}
		if(( info->sectors != 9 ) && ( info->sectors != 16 ) && ( info->sectors != 18 ) && ( info->sectors != 36 ))
		{
			fprintf( stderr, "Invalid number of sectors (%d) for disk image\n", info->sectors );
			return false;
		}
		if(( info->sectors == 16 ) && ( info->cylinders != 40 ))
		{
			fprintf( stderr, "Unrecognized disk configuration %d:%d:%d\n", info->cylinders, info->heads, info->sectors );
			return false;
		}

		return true;
	}

	std::map<std::string,sCreateDescriptor> validValues =
	{
		{   "90K",	{ 40, 1,  9 }},
		{  "160K",	{ 40, 1, 16 }},
		{  "180K",	{ 40, 2,  9 }},
		{  "320K",	{ 40, 2, 16 }},
		{  "360K",	{ 40, 2, 18 }},
		{  "400K",	{ 40, 2, 20 }},
		{  "640K",	{ 80, 2, 16 }},
		{  "720K",	{ 80, 2, 18 }},
		{ "1.44M",	{ 80, 2, 36 }},
		{  "SSSD",	{ 40, 1,  9 }},
		{  "SSDD",	{ 40, 1, 18 }},
		{  "DSSD",	{ 40, 2,  9 }},
		{  "DSDD",	{ 40, 2, 18 }},
		{  "CF7+",	{ 40, 2, 20 }},
		{  "DSHD",	{ 80, 2, 36 }}
	};

	std::string input( ptr );

	for( auto &ch : input ) ch = toupper( ch );

	if( validValues.count( input ) == 0 )
	{
		fprintf( stderr, "Unrecognized format (%s) for disk image\n", ptr );
		return false;
	}

	*info = validValues[ input ];

	return true;
}

bool ParseFormat( const char *arg, void *base )
{
	FUNCTION_ENTRY( nullptr, "ParseFormat", true );

	const char *ptr = strchr( arg, '=' ) + 1;

	eDiskFormat *info = static_cast<eDiskFormat*>( base );

	std::map<std::string,eDiskFormat> validValues =
	{
		{  "PC99",    FORMAT_RAW_TRACK  },
		{  "V9T9",    FORMAT_RAW_SECTOR },
		{  "ANADISK", FORMAT_ANADISK    },
		{  "CF7+",    FORMAT_CF7        },
		{  "HFE",     FORMAT_HFE        }
	};

	std::string input( ptr );

	for( auto &ch : input ) ch = toupper( ch );

	if( validValues.count( input ) == 0 )
	{
		fprintf( stderr, "Unrecognized format (%s) for disk format\n", ptr );
		return false;
	}

	*info = validValues[ input ];

	return true;
}

bool ParseFileName( const char *arg, void *base )
{
	FUNCTION_ENTRY( nullptr, "ParseFileName", true );

	const char *ptr = strchr( arg, '=' );

	// Handle special case of extract all
	if( ptr == nullptr )
	{
		fprintf( stderr, "A filename needs to be specified: '%s'\n", arg );
		return false;
	}

	std::string *name = static_cast<std::string*>( base );

	*name = ptr + 1;

	return true;
}

bool ParseFileNames( const char *arg, void *base )
{
	FUNCTION_ENTRY( nullptr, "ParseFileNames", true );

	const char *ptr = strchr( arg, '=' );

	// Handle special case of extract all
	if( ptr == nullptr )
	{
		fprintf( stderr, "A filename needs to be specified: '%s'\n", arg );
		return false;
	}

	std::list<std::string> *list = static_cast<std::list<std::string>*>( base );

	list->push_back( ptr + 1 );

	return true;
}

void PrintUsage( )
{
	FUNCTION_ENTRY( nullptr, "PrintUsage", true );

	fprintf( stdout, "Usage: disk [options] file\n" );
	fprintf( stdout, "\n" );
}

cFileSystem *CreateFileSystem( const sCreateDescriptor &diskLayout, const char *filename )
{
	if( FILE *file = fopen( filename, "r" ))
	{
		fclose( file );
		return nullptr;
	}

	track::Format format = ( diskLayout.sectors > 9 ) ? track::Format::MFM : track::Format::FM;

	sDataBuffer VIBdata( 256 );
	VIB *vib = reinterpret_cast<VIB*>( &VIBdata[ 0 ] );

	int formattedSectors = diskLayout.cylinders * diskLayout.heads * diskLayout.sectors;

	SetUINT16( &vib->FormattedSectors, formattedSectors );

	memcpy( vib->VolumeName, "BLANK     ", 10 );
	memcpy( vib->DSK, "DSK", 3 );

	vib->SectorsPerTrack    = diskLayout.sectors;
	vib->TracksPerSide      = diskLayout.cylinders;
	vib->Sides              = diskLayout.heads;
	vib->Density            = ( diskLayout.sectors <= 9 ) ? 1 : 2;
	vib->AllocationMap[ 0 ] = ( formattedSectors <= 200 * 8 ) ? 0x03 : 0x01;

	int mapIndex = formattedSectors / 8;

	while( mapIndex > 200 ) mapIndex /= 2;

	// Mark sectors that don't exist as in use
	memset( vib->AllocationMap + mapIndex, 0xFF, 200 - mapIndex );

	cDiskImage *image = new cDiskImage;

	image->FormatDisk( diskLayout.cylinders, diskLayout.heads, diskLayout.sectors, format );

	// Write out the Volume Information Block
	image->GetTrack( 0, 0 )->GetSector( 0, 0, 0 )->Write( VIBdata );

	memset( vib, 0, 256 );

	// Clear out the directory
	image->GetTrack( 0, 0 )->GetSector( 0, 0, 1 )->Write( VIBdata );

	return new cDiskFileSystem( new cDiskMedia( image ) );
}

bool HasTimestamp( const sFileDescriptorRecord *fdr )
{
	for( int j = 0; j < 8; j++ )
	{
		if( fdr->reserved2[ j ] != 0 )
		{
			return true;
		}
	}

	return false;
}

void ListFiles( FILE *outFile, cRefPtr<cFileSystem> disk )
{
	for( int i = -1; i < disk->DirectoryCount( ); i++ )
	{
		std::string directory;

		if( const char *dirName = disk->DirectoryName( i ))
		{
			directory = dirName;
			directory = directory.substr( 0, directory.find_last_not_of( ' ' ) + 1 );
			directory += ".";
		}

		std::list<std::string> filenames;
		disk->GetFilenames( filenames, i );

		for( auto &filename : filenames )
		{
			fprintf( outFile, "%s%s\n", directory.c_str( ), filename.c_str( ));
		}
	}
}

void ShowDirectory( FILE *outFile, cRefPtr<cFileSystem> disk, bool verboseMode, bool showSha1 )
{
	bool bFoundTimestamp = false;

	// Get a list of all the files and directories
	struct sDirectory
	{
		int                       index;
		std::string               name;
		std::list<cRefPtr<cFile>> files;
	};

	std::list<sDirectory> directories;
	for( int i = -1; i < disk->DirectoryCount( ); i++ )
	{
		const char *dirName = disk->DirectoryName( i );

		sDirectory dir;

		dir.index = i;
		dir.name = dirName ? dirName : "";

		std::list<std::string> filenames;
		disk->GetFilenames( filenames, i );

		for( auto &name : filenames )
		{
			cRefPtr<cFile> file = disk->OpenFile( name.c_str( ), i );
			if( file != nullptr )
			{
				dir.files.push_back( file );
				bFoundTimestamp |= HasTimestamp( file->GetFDR( ));
			}
		}

		directories.push_back( dir );
	}

	std::string name = disk->GetName( );

	int sectorsUsed = 0;
	int auSize = disk->AllocationSize( );

	int flags = 0;
	flags |= bFoundTimestamp ? LISTING_FLAG_TIMESTAMPS : 0;
	flags |= verboseMode ? LISTING_FLAG_VERBOSE : 0;
	flags |= showSha1 ? LISTING_FLAG_SHA1 : 0;

	for( auto &dir : directories )
	{
		fprintf( outFile, "\n" );
		fprintf( outFile, "Directory of %s%s\n", name.c_str( ), dir.name.c_str( ));
		fprintf( outFile, "\n" );

		std::list<std::string> headers;
		disk->ListingHeader( flags, headers );

		for( auto &header : headers )
		{
			fprintf( outFile, " %s", header.c_str( ));
		}
		fprintf( outFile, "\n" );

		for( auto &header : headers )
		{
			int length = header.length( );
			fprintf( outFile, " %*.*s", length, length, "========================================" );
		}
		fprintf( outFile, "\n" );

		for( auto &file : dir.files )
		{
			const sFileDescriptorRecord *fdr = file->GetFDR( );

			int size = (( GetUINT16( &fdr->TotalSectors ) + auSize / 2 ) / auSize + 1 ) * auSize;
			if( size <= disk->TotalSectors( ))
			{
				sectorsUsed += size;
			}

			std::list<std::string> fields;
			disk->ListingData( file, dir.index, flags, fields );

			for( auto &field : fields )
			{
				fprintf( outFile, " %s", field.c_str( ));
			}

			fprintf( outFile, "\n" );
		}
	}

	int totalSectors   = disk->TotalSectors( );
	int totalAvailable = disk->FreeSectors( );

	fprintf( outFile, "\n" );
	fprintf( outFile, "  Available: %4d  Used: %4d\n", totalAvailable, sectorsUsed );
	fprintf( outFile, "      Total: %4d   Bad: %4d\n", totalSectors, totalSectors - sectorsUsed - totalAvailable );
	fprintf( outFile, "\n" );
}

int main( int argc, char *argv[] )
{
	FUNCTION_ENTRY( nullptr, "main", true );

	bool dumpFiles    = false;
	bool checkDisk    = false;
	bool bareFiles    = false;
	bool convertFiles = false;
	bool verboseMode  = false;
	bool showSha1     = false;
	bool showLayout   = false;

	std::list<std::string> addFiles;
	std::list<std::string> delFiles;
	std::list<std::string> extFiles;

	sCreateDescriptor diskLayout{ };

	std::string outputFilename;

	eDiskFormat outputFormat = FORMAT_UNKNOWN;
	eDiskFormat forcedFormat = FORMAT_UNKNOWN;

	sOption optList[ ] =
	{
		{ 'a', "add=*<filename>",      OPT_NONE,                      true,              &addFiles,       ParseFileNames, "Add <filename> to the disk image" },
		{ 'b', "bare",                 OPT_VALUE_SET | OPT_SIZE_BOOL, true,              &bareFiles,      nullptr,        "Restrict output to a list of files" },
		{  0,  "check",                OPT_VALUE_SET | OPT_SIZE_BOOL, true,              &checkDisk,      nullptr,        "Check the integrity of the disk structures" },
		{ 'c', "convert",              OPT_VALUE_SET | OPT_SIZE_BOOL, true,              &convertFiles,   nullptr,        "Convert extracted files to DOS files" },
		{  0,  "create=*<C:H:S>",      OPT_NONE,                      true,              &diskLayout,     ParseCreate,    "Create a blank disk image with (C)Tracks, (H)Sides, and (S)Sectors" },
		{  0,  "create=*<size>",       OPT_NONE,                      true,              &diskLayout,     ParseCreate,    "Create a blank disk image of given size (90K,160K,180K,320K,360K,400K,640K,720K,1.44M)" },
		{  0,  "create=*<type>",       OPT_NONE,                      true,              &diskLayout,     ParseCreate,    "Create a blank disk image of given type (SSSD,SSDD,DSSD,DSDD,CF7+,DSHD)" },
		{ 'd', "dump",                 OPT_VALUE_SET | OPT_SIZE_BOOL, true,              &dumpFiles,      nullptr,        "Extract all files to FIAD files" },
		{ 'e', "extract*=<filename>",  OPT_NONE,                      true,              &extFiles,       ParseFileNames, "Extract <filename> to v9t9 FIAD file" },
		{  0,  "filename=*<filename>", OPT_NONE,                      true,              &outputFilename, ParseFileName,  "Name of the file to convert to" },
		{  0,  "force=*<format>",      OPT_NONE,                      true,              &forcedFormat,   ParseFormat,    "Force the disk image to be treated as the specified format (PC99,V9T9,AnaDisk,CF7+,HFE)" },
		{ 'l', "layout",               OPT_VALUE_SET | OPT_SIZE_BOOL, true,              &showLayout,     nullptr,        "Display the disk sector layout" },
		{  0,  "output=*<format>",     OPT_NONE,                      true,              &outputFormat,   ParseFormat,    "Convert disk to the specified format (PC99,V9T9,AnaDisk,CF7+,HFE)" },
		{ 'r', "remove=*<filename>",   OPT_NONE,                      true,              &delFiles,       ParseFileNames, "Remove <filename> from the disk image" },
		{ 's', "sha1",                 OPT_VALUE_SET | OPT_SIZE_BOOL, true,              &showSha1,       nullptr,        "Display the SHA1 checksum for each file" },
		{ 'v', "verbose",              OPT_VALUE_SET | OPT_SIZE_BOOL, true,              &verboseMode,    nullptr,        "Display information about the disk image" }
	};

	/*
	   -f, --fix

	   FDR >0014->001B    >8F >1B >09 >02 >8F >27 >09 >02
	 */

	if( argc == 1 )
	{
		PrintHelp( SIZE( optList ), optList );
		return 0;
	}

	#if defined ( OS_LINUX )
		if( isatty( fileno( stdout )))
		{
			printf( "TI-99/4A Diskette Viewer\n" );
		}
	#endif

	cRefPtr<cFileSystem> fileSystem;

	int index = 1;
	while( index < argc )
	{
		index = ParseArgs( index, argc, argv, SIZE( optList ), optList );

		if( index < argc )
		{
			if( fileSystem != nullptr )
			{
				fprintf( stderr, "\n" );
				fprintf( stderr, "Only one disk image file can be specified\n" );
				return -1;
			}
			if( diskLayout.cylinders != 0 )
			{
				fileSystem = CreateFileSystem( diskLayout, argv[ index++ ] );
				if( fileSystem == nullptr )
				{
					fprintf( stderr, "\n" );
					fprintf( stderr, "Unable to create new disk image file \"%s\" - file already exists\n", argv[ index - 1 ] );
					return -1;
				}
				outputFilename = argv[ index - 1 ];
			}
			else
			{
				if( forcedFormat != FORMAT_UNKNOWN )
				{
					fileSystem = cDiskFileSystem::Open( argv[ index++ ], forcedFormat );
				}
				else
				{
					fileSystem = cFileSystem::Open( argv[ index++ ] );
				}
				if( fileSystem == nullptr )
				{
					if( LocateFile( "disks", argv[ index - 1 ] ).empty( ))
					{
						fprintf( stderr, "\n" );
						fprintf( stderr, "Unable to open disk image file \"%s\"\n", argv[ index - 1 ] );
						return -1;
					}
					fprintf( stderr, "\n" );
					fprintf( stderr, "File format not recognized for disk image file \"%s\"\n", argv[ index - 1 ] );
					return -1;
				}
			}
		}
	}

	if( fileSystem == nullptr )
	{
		fprintf( stderr, "\n" );
		fprintf( stderr, "No disk image file specified\n" );
		return -1;
	}

	cDiskFileSystem* diskFileSystem = dynamic_cast<cDiskFileSystem*> ( fileSystem.get( ));

	cRefPtr<cDiskMedia> media;

	if( diskFileSystem != nullptr )
	{
		media = diskFileSystem->GetMedia( );
	}

	if( media != nullptr )
	{
		if(( media->GetVolume( ) == 0 ) && ( media->MaxVolume( ) > 0 ))
		{
			std::string filename = fileSystem->GetPath( );
			for( int i = 1; i <= media->MaxVolume( ); i++ )
			{
				char volumeName[ 4096 ];
				sprintf( volumeName, "%s#%d", filename.c_str( ), i );
				cRefPtr<cFileSystem> vol = cFileSystem::Open( volumeName );
				if(( vol != nullptr ) && vol->IsValid( ))
				{
					if( verboseMode == true )
					{
						PrintDiskInfo( dynamic_cast<cDiskFileSystem*> ( vol.get( ))->GetMedia( ));
					}

					ShowDirectory( stdout, vol, verboseMode, showSha1 );

					if( checkDisk )
					{
						vol->CheckDisk( verboseMode );
					}
				}
			}
			return 0;
		}

		if( showLayout == true )
		{
			PrintDiskLayout( media );
		}
		if( verboseMode == true )
		{
			PrintDiskInfo( media );
		}
	}
	else
	{
		if( verboseMode == true )
		{
			printf( "\n" );
			printf( "     File: %s\n", fileSystem->GetPath( ).c_str( ));
			printf( "   Format: %s\n",
				dynamic_cast<cArchiveFileSystem*>( fileSystem.get( )) ? "ARK Archive" : "<native file>" );
		}
	}

	if( fileSystem->IsValid( ) == false )
	{
		std::string name = fileSystem->GetPath( );
		if( diskFileSystem != nullptr )
		{
			const char *format = GetDiskFormatString( media->GetFormat( ));
			fprintf( stderr, "\n" );
			fprintf( stderr, "File \"%s\" appears to be a %s disk image, but is not formatted for the TI-99/4A\n", name.c_str( ), format );
		}
		else
		{
			fprintf( stderr, "\n" );
			fprintf( stderr, "File \"%s\" does not contain a recognized disk format\n", name.c_str( ));
		}
		return -1;
	}

	if( dumpFiles == true )
	{
		// Ignore any files specified with '--extract=' and dump them all
		fileSystem->GetFilenames( extFiles );
	}

	// Extract existing files
	if( extFiles.empty( ) == false )
	{
		if( verboseMode == true )
		{
			fprintf( stdout, "\n" );
			fprintf( stdout, "Extracting files:\n" );
		}
		for( auto extFile : extFiles )
		{
			cRefPtr<cFile> file = fileSystem->OpenFile( extFile.c_str( ));
			if( file != nullptr )
			{
				if( verboseMode == true )
				{
					fprintf( stdout, "  %s\n", extFile.c_str( ));
				}
				DumpFile( file, convertFiles );
			}
			else
			{
				fprintf( stderr, "  Unable to locate file %s\n", extFile.c_str( ));
			}
		}
	}

	// Remove existing files
	if( delFiles.empty( ) == false )
	{
		if( verboseMode == true )
		{
			fprintf( stdout, "\n" );
			fprintf( stdout, "Removing files:\n" );
		}
		for( auto delFile : delFiles )
		{
			if( fileSystem->DeleteFile( delFile.c_str( )) == true )
			{
				if( verboseMode == true )
				{
					fprintf( stdout, "  %s\n", delFile.c_str( ));
				}
			}
			else
			{
				fprintf( stderr, "  Unable to delete file %s\n", delFile.c_str( ));
			}
		}
	}

	// Add new files here
	if( addFiles.empty( ) == false )
	{
		if( verboseMode == true )
		{
			fprintf( stdout, "\n" );
			fprintf( stdout, "Adding files:\n" );
		}
		for( auto addFile : addFiles )
		{
			cRefPtr<cFile> file = cFile::Open( addFile.c_str( ));
			if( file != nullptr )
			{
				if( verboseMode == true )
				{
					fprintf( stdout, "  %s\n", addFile.c_str( ));
				}
				if( fileSystem->AddFile( file ) == false )
				{
					fprintf( stderr, "  Unable to add file %s\n", addFile.c_str( ));
				}
			}
			else
			{
				fprintf( stderr, "  Unable to locate file %s\n", addFile.c_str( ));
			}
		}
	}

	if( bareFiles )
	{
		ListFiles( stdout, fileSystem );
	}
	else
	{
		ShowDirectory( stdout, fileSystem, verboseMode, showSha1 );

		if( checkDisk )
		{
			fileSystem->CheckDisk( verboseMode );
		}
	}

	if( media != nullptr )
	{
		// First, write out any changes made
		if( media->HasChanged( ) == true )
		{
			if( diskLayout.cylinders != 0 )
			{
				if( media->SaveFileAs( outputFilename.c_str( ), outputFormat ) == false )
				{
					fprintf( stderr, "Unable to write to file \"%s\"\n", outputFilename.c_str( ));
					return -1;
				}

				return 0;
			}

			if( media->SaveFile( ) == false )
			{
				fprintf( stderr, "Unable to write to file \"%s\"\n", media->GetName( ));
				return -1;
			}
		}

		// See if a conversion was requested
		if( outputFormat != FORMAT_UNKNOWN )
		{
			if( outputFilename.empty( ))
			{
				outputFilename = media->GetName( );
			}

			if(( outputFormat == media->GetFormat( )) && ( outputFilename == media->GetName( )))
			{
				fprintf( stderr, "Conversion skipped - output file matches source\n" );
				return 0;
			}

			// Make sure we don't wipe out other volumes on a CF7+ disk by changing formats
			if(( media->GetFormat( ) == FORMAT_CF7 ) && ( outputFormat != FORMAT_CF7 ) && ( outputFilename == media->GetName( )))
			{
				fprintf( stderr, "Changing a CF7+ disk's format is not supported\n" );
				return -1;
			}

			if( media->SaveFileAs( outputFilename.c_str( ), outputFormat ) == false )
			{
				fprintf( stderr, "Unable to write to file \"%s\"\n", outputFilename.c_str( ));
				return -1;
			}
		}
	}

	return 0;
}
