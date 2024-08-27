#ifndef REGISTER_MAP_TRGFW_HH
#define REGISTER_MAP_TRGFW_HH

namespace LBUS {
	//-------------------------------------------------------------------
	// ADC Module
	//-------------------------------------------------------------------
	namespace ADC {
		enum LocalAddress
			{
				kAddrPtrOfs       = 0x20000000, //-- W/R, [10:0] Offset read pointer
				kAddrWindowMax    = 0x20100000, //-- W/R, [10:0] Search window max
				kAddrWindowMin    = 0x20200000, //-- W/R, [10:0] Search window min
				kAddrAdcRoReset   = 0x20300000, //-- W/R, [0:0]  AdcRo reset signal
				kAddrAdcRoIsReady = 0x20400000, //-- R,   [3:0]  AdcRo IsReady signals
			};
	};

	//-------------------------------------------------------------------
	// ADC Trigger
	//-------------------------------------------------------------------
	//namespace ATR {

	//};

	//-------------------------------------------------------------------
	// Trigger Manger
	//-------------------------------------------------------------------
	namespace TRM {
		enum LocalAddress
			{
				kAddrSelectTrigger = 0x40000000 //-- W/R, [11:0] select trigger line
			};

		enum command_sel_trig
			{
				kRegL1Ext  = 0x1,
				kRegL1J0   = 0x2,
				kRegL1RM   = 0x4,
				kRegL2Ext  = 0x8,
				kRegL2J0   = 0x10,
				kRegL2RM   = 0x20,
				kRegClrExt = 0x40,
				kRegClrJ0  = 0x80,
				kRegClrRM  = 0x100,
				kRegEnL2   = 0x200,
				kRegEnJ0   = 0x400,
				kRegEnRM   = 0x800
			};
	};

	//-------------------------------------------------------------------
	// DAQ Controller
	//-------------------------------------------------------------------
	namespace DCT {
		enum LocalAddress
			{
				kAddrDaqGate  = 0x50000000, //-- W/R, [0:0] Set DAQ Gate  reg
				kAddrResetEvb = 0x50100000  //-- W,   Assert EVB reset (self counter reset)
			};
	};

};

#endif;
