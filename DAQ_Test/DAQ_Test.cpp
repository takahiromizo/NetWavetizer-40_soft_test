#include <iostream>
#include <iomanip>
#include <ios>
#include <string>
#include <fstream>
#include <sstream>
#include <stdint.h>
#include <csignal>
#include <chrono>
#include <thread>

#include "RegisterMap.h"
#include "FPGAModule.h"
#include "UDPRBCP.h"
#include "DaqFuncs.h"


//-- ROOT(CERN) library
#include <TROOT.h>
#include <TPad.h>
#include <TStyle.h>
#include <TCanvas.h>
#include <TApplication.h>
#include <TH1D.h>
#include <TH2D.h>
#include <TFile.h>
#include <TString.h>
#include <TStopwatch.h>
//---------------------

//-- define
#define Windows

#ifdef Windows
	//#define __USE_W32_SOCKETS

	#include <stdio.h>
	#include <winsock2.h>
#else
	#include <stdlib.h>
	#include <stdio.h>
	#include <string.h>

	#include <arpa/inet.h>
	#include <unistd.h>

	#include <sys/types.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
#endif;

using std::this_thread::sleep_for;

using namespace LBUS;
using namespace HUL::DAQ;

//-- class : ANALYSIS ---------------------------------------------------------------
namespace GAMMAI::DAQ {

	class ANALYSIS {
	public:
		ANALYSIS() = delete;
		ANALYSIS(const ANALYSIS&) = delete;

		//-- prepare histgrams for pedestal
		//-- Constructor
		ANALYSIS(int kRunNo, int kMaxCh) {

			//-- output root file
			TString fout  = "ROOT_file\\r";
			fout         += kRunNo;
			fout         += ".root";
			OutputFile = new TFile(fout, "RECREATE");

			//-- prepare histgram
			for (int ch = 0; ch < kMaxCh; ch++) {
				hPedestal[ch] = new TH1D(Form("Pedestal_ch0x%.2x", ch), Form("Pedestal_ch0x%.2x", ch), 4092, 0., 4092.);
				hWaveform[ch] = new TH1D(Form("Waveform_ch0x%.2x", ch), Form("Waveform_ch0x%.2x", ch), ADCDepth, 0, ADCDepth);
			}

			canvas->Divide(7, 5);
		};

		virtual ~ANALYSIS() = default;
		ANALYSIS& operator=(ANALYSIS&) = delete;

		//-- declare variables, histgrams, functions -----
		//-- explicit variables
		static const Int_t    ADCDepth            = 16;
		static const Int_t    kBoardCh            = 32;
		uint32_t              kCoarseCounterShift = 12;
		uint32_t              kChannelShift       = 23;
		uint32_t              kMagicWordShift     = 28;
		//-- variables
		TFile*   OutputFile;
		Int_t    kRunNo;
		Double_t ADCValue[ADCDepth * kBoardCh];
		UInt_t   Channel;
		UInt_t   CoarseCount;
		//-- histgram/canvas
		TH1D* hPedestal[32];
		TH1D* hWaveform[32];
		TCanvas* canvas = new TCanvas("canvas", "Firmware Test");

		//-- functions
		void DoANALYSIS(unsigned int kMaxCh, int32_t n_word, int32_t kNumHeader, int32_t num_event, uint32_t* buf) {

			//-- extract ADC data from buf
			for (int32_t i = 0; i < n_word; i++) {
				if (((buf[i + kNumHeader] & 0xf0000000) >> kMagicWordShift) == 0xa) {
					Channel = (buf[i + kNumHeader] & 0x0f800000) >> kChannelShift;
					CoarseCount = (buf[i + kNumHeader] & 0x007ff000) >> kCoarseCounterShift;
					ADCValue[ADCDepth * Channel + CoarseCount] = buf[i + kNumHeader] & 0x00000fff;

					//-- for Debug 
					//std::cout << "***  Event#  : " << num_event << " ***" << std::endl;
					//printf(" Channel     : %d\n", Channel);
					//printf(" CoarseCount : %d\n", CoarseCount);
					//printf(" ADCValue    : %d\n", ADCValue[ADCDepth * Channel + CoarseCount]);
				}
			}

			FillSignal(kMaxCh);

		};

		void FillSignal(Int_t MaxCh) {
			for (Int_t depth = 0; depth < ADCDepth; depth++) {
				for (Int_t ch = 0; ch < MaxCh; ch++) { 
					hPedestal[ch]->Fill(ADCValue[ADCDepth * ch + depth]);
					hWaveform[ch]->Fill(depth, ADCValue[ADCDepth * ch + depth]);
				}
			}
		};

		void Output() {
			OutputFile->Write();
			OutputFile->Close();
		};
		//------------------------------------------------

	};
};
//----------------------------------------------------------------------------------




//-- Ctrl C
namespace CTRL::Func {
	int stop = 0;

	void UserStop_FromCtrlC(int signal)
	{
		std::cout << std::endl;
		std::cout << "#D: Stop request" << std::endl;
		stop = 1;
	}

	int GetStatus()
	{
		return stop;
	}
};

enum ArgIndex { kBin, kIp, kNumEvent };

//----- main -----------------------------------------------------------------------
int main(int argc, char* argv[]) {

	if (1 == argc) {
		std::cout << "Usage\n";
		std::cout << "run_daq [IP address] [Num of events]" << std::endl;
		std::cout << " - Number of events  : Max event number. DAQ will stop at this event number or by Ctrl-C." << std::endl;

		return 0;
	}

	//-- body ---------------------------------------------------------
	std::string   board_ip  = argv[kIp];
	int32_t       num_event = atoi(argv[kNumEvent]);
	TRG           trg;
	int           sock;
	int           kRunNo;
	int           user_stop = 0;
	unsigned int  kMaxChannel = 32;

	trg.kNumHead          = 3;
	trg.kNumBodyMax       = 8192 + 65536; //-- to read ADC data (32 ch * 2048 max windowsize)
	trg.kNumData          = trg.kNumHead + trg.kNumBodyMax;
	trg.kNumByte          = 4;
	uint32_t* buf         = new uint32_t[trg.kNumData];

	//-- import run number
	std::ifstream fin("runnumber.txt");
	fin >> kRunNo;
	fin.close();

	std::string filename  = "dat_file\\test_run";
	filename             += std::to_string(kRunNo);
	filename             += ".dat";
	std::ofstream ofs(filename.c_str(), std::ios::binary);
	if (!ofs.is_open()) {
		std::cerr << "#E: Data file cannot be created.\n" << std::endl;
	}

	RBCP::UDPRBCP udp_rbcp(board_ip, RBCP::gUdpPort, RBCP::DebugMode::kNoDisp);
	HUL::FPGAModule fpga_module(udp_rbcp);

	//-- Release AdcRo reset
	if (1 == fpga_module.ReadModule(ADC::kAddrAdcRoReset)) {
		fpga_module.WriteModule(ADC::kAddrAdcRoReset, 0);
	}

	std::cout << "********** Firmware Development **********" << std::endl;
	std::cout << "********** For Firmware Test    **********" << std::endl;

	gROOT->Reset();
	gStyle->SetOptStat(0);
	TApplication app("Firmware_Test", &argc, argv);

	//-- AdcRo initialize status
	std::cout << "#D: AdcRo IsReady status: " << std::hex << fpga_module.ReadModule(ADC::kAddrAdcRoIsReady) << std::dec << std::endl;

	//-- Prepare for analysis
	GAMMAI::DAQ::ANALYSIS analysis(kRunNo, kMaxChannel);
	std::cout << "GAMMAI::DAQ::ANALYSIS is Ready." << std::endl;

	(void)signal(SIGINT, CTRL::Func::UserStop_FromCtrlC);

	//-- Reset event counter
	fpga_module.WriteModule(DCT::kAddrResetEvb, 0);
    //-- Start DAQ
	fpga_module.WriteModule(DCT::kAddrDaqGate, 1);
	std::cout << "#D: Open the DAQ gate and starts data aquisition." << std::endl;

	//-- TCP socket
	#ifdef Windows
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(1, 1), &wsaData) != 0) {
			perror("WSAStartup Error !\n");
			return -2;
		}
	#endif
	if (-1 == (sock = ConnectSocket(board_ip))) {
		#ifdef Windows
			closesocket(sock);
			WSACleanup();
		#else
			close(sock);
		#endif
		std::cout << "#E: Socket connection was failed." << std::endl;
		return sock;
	}
	std::cout << "#D: Socket connected." << std::endl;

	sleep_for(std::chrono::milliseconds(500)); //-- unit : msec

	int32_t   n = 0;
	int32_t   ret_receive;
	uint32_t  n_word;
	//-- DAQ cycle
	for ( ; n < num_event; ++n) {

		while (kRecvTimeOut == (ret_receive = DoEventCycle(sock, buf, trg)) && !(user_stop = CTRL::Func::GetStatus())) continue;
		n_word = ret_receive;

		if (user_stop == 1) break;
        
		if (ret_receive == kRecvZero) {
			std::cout << "#E: Recv() returns 0" << std::endl;
			break;
		}

		if (ret_receive == kRecvError) {
			std::cout << "#E: Recv() returns -1" << std::endl;
			break;
		}

		// -- for debug--------------------------------------------------------------------
			uint32_t MagicWord = buf[0];
		uint32_t n_word_data = buf[1] & 0x3ffff;
		printf("Magic word : %08x | Word length[words] : %d\n", MagicWord, n_word_data);
		//---------------------------------------------------------------------------------

		ofs.write((const char*)buf, n_word * trg.kNumByte);
		analysis.DoANALYSIS(kMaxChannel, n_word - trg.kNumHead, trg.kNumHead, n, buf);

	} //-- for(n)
	

	//getchar();
	std::cout << "#D: " << n << " events were accumulated." << std::endl;
	std::cout << "#D: Close the DAQ gate." << std::endl;

	fpga_module.WriteModule(DCT::kAddrDaqGate, 0);
	analysis.Output();

	std::cout << "#D: Read Remaining data in FPGA. Wait time out." << std::endl;
	sleep_for(std::chrono::milliseconds(1000)); //-- unit : msec
	while (kRecvTimeOut != DoEventCycle(sock, buf, trg));
	std::cout << "#D: Time out is detected. End of DAQ. " << std::endl;

	#ifdef Windows
		closesocket(sock);
		WSACleanup();
	#else
		close(sock);
	#endif
	ofs.close();
	delete[] buf;
	

	return 0;
} //-- main
