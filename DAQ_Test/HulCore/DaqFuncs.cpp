//*********************************************************
// orginal code : hul-common-lib/HulCore/DaqFuncs.cc
//  URL : https://github.com/spadi-alliance/hul-common-lib
// editted for gamma-I by T.Mizoguchi 
//*********************************************************

#include<fstream>
#include<cstdlib>
#include<iostream>
#include<iomanip>
#include<cstdio>
#include<csignal>
#include<list>
#include<bitset>

#include"UDPRBCP.h"
#include"FPGAModule.h"
#include"DaqFuncs.h"

#include<string.h>
#include<errno.h>
#include<chrono>
#include<thread>

#define Windows
#ifdef Windows
    #include <winsock.h>
    #include <winerror.h>
#else
    #include <errno.h>
#endif

#define errno WSAGetLastError() // for Windows

using std::this_thread::sleep_for;

namespace HUL::DAQ {

    // EventCycle -------------------------------------------------------------
    int32_t
        DoEventCycle(int sock, uint32_t* buffer, TRG trg)
    {
        // data read ---------------------------------------------------------
        static const uint32_t kSizeHeader = trg.kNumHead * trg.kNumByte;

        uint32_t received_bytes = 0;
        int recv_status = Receive(sock, (uint8_t*)buffer, kSizeHeader, received_bytes);
        if (recv_status <= 0) return recv_status;

        uint32_t n_word_data    = buffer[1] & 0x3ffff;
        uint32_t size_data   = n_word_data * trg.kNumByte;
       
        if (n_word_data == 0) return trg.kNumHead;

        recv_status = Receive(sock, (uint8_t*)(buffer + trg.kNumHead), size_data, received_bytes);
        if (recv_status <= 0) return recv_status;

        return trg.kNumHead + n_word_data;
    }

    // ConnectSocket ----------------------------------------------------------
    int
        ConnectSocket(std::string ip)
    {
        struct sockaddr_in SiTCP_ADDR;
        unsigned int port = 24;

        int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        SiTCP_ADDR.sin_family      = AF_INET;
        SiTCP_ADDR.sin_port        = htons((unsigned short int) port);
        SiTCP_ADDR.sin_addr.s_addr = inet_addr(ip.c_str());
        
        //-- timeout
        #ifdef Windows
            unsigned int to = 3000;
        #else
            struct timeval to;
            to.tv_sec = 3;
            to.tv_usec = 0;
        #endif
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&to, sizeof(to));

        //-- Nagle algorithm
        //-- In windows, this function is FALSE in default.
        #ifdef Windows
        #else
            int flag = 1;
            setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));
        #endif

        if (0 > connect(sock, (struct sockaddr*)&SiTCP_ADDR, sizeof(SiTCP_ADDR))) {
            std::cerr << "#E: TCP connection error" << std::endl;
            #ifdef Windows
                closesocket(sock);
                WSACleanup();
            #else
                close(sock);
            #endif
            return -1;
        }

        return sock;
    }

    // Socket read ----------------------------------------------------------------
    int32_t
        Receive(int sock, uint8_t* data_buf, uint32_t length, uint32_t& num_received_bytes)
    {
        uint32_t revd_size = 0;
        int      tmp_ret   = 0;
        int      ret_val   = HUL::DAQ::kRecvNormal;

        while (revd_size < length) {
            tmp_ret = recv(sock, (char*)data_buf + revd_size, length - revd_size, 0);
            if (tmp_ret == 0) {
                num_received_bytes = 0;
                ret_val = HUL::DAQ::kRecvZero;
                return ret_val;
            }

            if (tmp_ret < 0) {
                int errbuf = errno;
                perror("#D: TCP receive");
                if (errbuf == WSAETIMEDOUT) { // for Windows(blocking mode, timeout)
                    // socket read time out
                    std::cout << " recv() time out, received byte size: " << revd_size << std::endl;
                    num_received_bytes = static_cast<uint32_t>(revd_size);
                    ret_val = HUL::DAQ::kRecvTimeOut;
                }
                //else if (errbuf == ERROR_IO_PENDING) {
                //    sleep_for(std::chrono::milliseconds(1)); //-- unit : msec
                //    ret_val = 0;
               // }
                else {
                    // something wrong
                    std::cerr << "#E: TCP error  : " << errbuf << std::endl;

                    num_received_bytes = 0;
                    ret_val = HUL::DAQ::kRecvError;
                }

                return ret_val;
            }
            
            revd_size += tmp_ret;
        }

        num_received_bytes = static_cast<uint32_t>(revd_size);
        return ret_val;
    }

};
