// -* C++ --

// Author: Tomonori Takahashi

#ifndef HDDAQ__BIT_DUMP_H
#define HDDAQ__BIT_DUMP_H

namespace hddaq
{
	class BitDump
	{
	private:
		int m_count;

	public:
		BitDump();
		~BitDump();

		void operator() (unsigned int data);
		void operator() (unsigned short data);
	};
}

#endif;