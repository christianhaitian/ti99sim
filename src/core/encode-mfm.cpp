#include <list>
#include <vector>
#include "disk-util.hpp"

std::vector<UINT8> EncodeDataMFM( const std::list<sDataFragment> &fragments, bool lsb )
{
	std::vector<UINT8> data;

	int bits = 0;
	int accum = 0;

	auto WriteBit = [ & ]( int bit )
	{
		accum >>= 1;
		accum |= bit ? 0x80 : 0x00;

		bits += 1;
		if( bits == 8 )
		{
			data.push_back( accum );
			bits = 0;
		}
	};

	auto WriteByte = [ & ]( int byte, int clock )
	{
		int mask = 0x80;
		for( size_t i = 0; i < 8; i++ )
		{
			int l = ( accum & 0x80 ) ? 0 : 1;
			int b = ( byte & mask ) ? 0 : 1;
			int c = ( clock != -1 ) ? ( clock & mask ) : ( l & b );
			WriteBit( c );
			WriteBit( byte & mask );
			mask >>= 1;
		}
	};

	size_t lastOffset = 0;

	for( auto &fragment : fragments )
	{
		int bit = 1;
		while( lastOffset != fragment.bitOffsetStart )
		{
			WriteBit( bit );
			bit = !bit;
			lastOffset++;
		}

		for( size_t i = 0; i < fragment.byteData.size( ); i++ )
		{
			int byte =  fragment.byteData[ i ];
			int clock = ( i == 0 ) ? fragment.clock : -1;

			WriteByte( byte, clock );
		}

		lastOffset = fragment.bitOffsetEnd;
	}

	return data;
}
