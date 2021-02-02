#include <list>
#include <vector>
#include "disk-util.hpp"

std::vector<UINT8> EncodeDataFM( const std::list<sDataFragment> &fragments, bool lsb )
{
	std::vector<UINT8> data;

	int bits = 0;
	int accum = 0;

	auto WriteBit = [ & ]( int bit )
	{
		accum >>= 2;
		accum |= bit ? 0x80 : 0x00;

		bits += 2;
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
			WriteBit( clock & mask );
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

		int clock = fragment.clock;
		for( auto byte : fragment.byteData )
		{
			WriteByte( byte, clock );
			clock = 0xFF;
		}

		lastOffset = fragment.bitOffsetEnd;
	}

	return data;
}
