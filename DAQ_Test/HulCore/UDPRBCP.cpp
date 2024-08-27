//*********************************************************
// orginal code : hul-common-lib/HulCore/UDPRBCP.cc
//  URL : https://github.com/spadi-alliance/hul-common-lib
// editted for gamma-I by T.Mizoguchi 
//*********************************************************

#include"UDPRBCP.h"
#include"rbcp.h"
#include"BitDump.h"

#include<cstdlib>
#include<cstdio>
#include<cstring>

#include<winsock.h>
#include<sys/types.h>

#define Windows

namespace RBCP
{


    void
        UDPRBCP::SetWD(uint32_t address, const uint32_t length,
            const uint8_t* send_data)
    {
        send_header_.command = kRbcpCmdWr_;
        send_header_.length = length;
        send_header_.address = htonl(address);

        memcpy(wd_buffer_, send_data, length);
    }

    void
        UDPRBCP::SetRD(const uint32_t address, const uint32_t length)
    {
        send_header_.command = kRbcpCmdRd_;
        send_header_.length = length;
        send_header_.address = htonl(address);
    }

    int32_t
        UDPRBCP::DoRBCP()
    {
        struct sockaddr_in sitcp_addr;
        int32_t sock;

        struct timeval timeout;
        fd_set set_select;

        int32_t snd_data_len;
        int32_t cmd_pck_len;

        uint8_t snd_buf[kSizeUdpBuf_];
        int32_t i = 0, j = 0;
        int32_t rcvd_bytes;
        uint8_t rcvd_buf[kSizeUdpBuf_];
        int32_t num_re_trans = 0;

        /* Create a Socket */
        if (mode_ == DebugMode::kInteractive) { printf("Create socket...\n"); }

        #ifdef Windows
            WSADATA wsaData;
            if (WSAStartup(MAKEWORD(1, 1), &wsaData) != 0) {
                perror("WSAStartup Error !\n");
                return -2;
            }
        #endif

        sock = socket(AF_INET, SOCK_DGRAM, 0);

        sitcp_addr.sin_family = AF_INET;
        sitcp_addr.sin_port = htons(port_);
        sitcp_addr.sin_addr.s_addr = inet_addr(ip_addr_.c_str());

        snd_data_len = (int32_t)send_header_.length;

        if (mode_ == DebugMode::kDebug) { printf(" Length = %d", snd_data_len); }

        /* Copy header data */
        memcpy(snd_buf, &send_header_, sizeof(struct RbcpHeader));

        if (send_header_.command == kRbcpCmdWr_) {
            memcpy(snd_buf + sizeof(struct RbcpHeader), wd_buffer_, snd_data_len);
            cmd_pck_len = snd_data_len + sizeof(struct RbcpHeader);
        }
        else {
            cmd_pck_len = sizeof(struct RbcpHeader);
        }


        if (mode_ == DebugMode::kDebug) {
            for (i = 0; i < cmd_pck_len; i++) {
                if (j == 0) {
                    printf("\t[%.3x]:%.2x ", i, (uint8_t)snd_buf[i]);
                    j++;
                }
                else if (j == 3) {
                    printf("%.2x\n", (uint8_t)snd_buf[i]);
                    j = 0;
                }
                else {
                    printf("%.2x ", (uint8_t)snd_buf[i]);
                    j++;
                }
            }
            if (j != 3) printf("\n");
        }

        /* send a packet*/
        sendto(sock, (const char*)snd_buf, cmd_pck_len, 0, (struct sockaddr*)&sitcp_addr, sizeof(sitcp_addr));

        if (mode_ == DebugMode::kDebug) { printf("The packet have been sent!\n"); }

        /* Receive packets*/
        if (mode_ == DebugMode::kDebug) { printf("Wait to receive the ACK packet...\n"); }

        while (num_re_trans < 3) {

            FD_ZERO(&set_select);
            FD_SET(sock, &set_select);

            timeout.tv_sec = 1;
            timeout.tv_usec = 0;

            if (select(sock + 1, &set_select, NULL, NULL, &timeout) == 0) {
                /* time out */
                printf("***** Timeout ! *****\n");
                send_header_.id++;
                memcpy(snd_buf, &send_header_, sizeof(struct RbcpHeader));
                sendto(sock, (const char*)snd_buf, cmd_pck_len, 0, (struct sockaddr*)&sitcp_addr, sizeof(sitcp_addr));
                num_re_trans++;
                FD_ZERO(&set_select);
                FD_SET(sock, &set_select);
            }
            else {
                /* receive packet */
                if (FD_ISSET(sock, &set_select)) {
                    rcvd_bytes = recvfrom(sock, (char*)rcvd_buf, kSizeUdpBuf_, 0, NULL, NULL);

                    if ((uint32_t)rcvd_bytes < sizeof(struct RbcpHeader)) {
                        fprintf(stderr, "#E UDPRBCP::DoRBCP ACK packet is too short\n");
                        #ifdef Windows
                            closesocket(sock);
                            WSACleanup();
                        #else
                            cloce(sock);
                        #endif
                        return -1;
                    }

                    if ((0x0f & rcvd_buf[1]) != 0x8) {
                        fprintf(stderr, "#E UDPRBCP::DoRBCP Detected bus error\n");
                        #ifdef Windows
                            closesocket(sock);
                            WSACleanup();
                        #else
                            cloce(sock);
                        #endif
                        return -1;
                    }

                    rcvd_buf[rcvd_bytes] = 0;

                    if (send_header_.command == kRbcpCmdRd_) {
                        memcpy(rd_buffer_, rcvd_buf + sizeof(struct RbcpHeader), rcvd_bytes - sizeof(struct RbcpHeader));
                        length_rd_ = rcvd_bytes - sizeof(struct RbcpHeader);
                    }

                    if (mode_ == DebugMode::kDebug) {
                        printf("***** A pacekt is received ! *****.\n");
                        printf("Received data:\n");

                        j = 0;

                        for (i = 0; i < rcvd_bytes; i++) {
                            if (j == 0) {
                                printf("\t[%.3x]:%.2x ", i, (uint8_t)rcvd_buf[i]);
                                j++;
                            }
                            else if (j == 3) {
                                printf("%.2x\n", (uint8_t)rcvd_buf[i]);
                                j = 0;
                            }
                            else {
                                printf("%.2x ", (uint8_t)rcvd_buf[i]);
                                j++;
                            }
                            if (i == 7) printf("\n Data:\n");
                        }

                        if (j != 3) puts(" ");
                    }
                    else if (mode_ == DebugMode::kInteractive) {
                        if (send_header_.command == kRbcpCmdRd_) {
                            j = 0;
                            puts(" ");

                            for (i = 8; i < rcvd_bytes; i++) {
                                if (j == 0) {
                                    printf(" [0x%.8x] %.2x ", ntohl(send_header_.address) + i - 8, (uint8_t)rcvd_buf[i]);
                                    j++;
                                }
                                else if (j == 7) {
                                    printf("%.2x\n", (uint8_t)rcvd_buf[i]);
                                    j = 0;
                                }
                                else if (j == 4) {
                                    printf("- %.2x ", (uint8_t)rcvd_buf[i]);
                                    j++;
                                }
                                else {
                                    printf("%.2x ", (uint8_t)rcvd_buf[i]);
                                    j++;
                                }

                                hddaq::BitDump dump;
                                dump((uint32_t)rcvd_buf[i]);
                            }

                            if (j != 15) puts(" ");
                        }
                        else {
                            printf(" 0x%x: OK\n", ntohl(send_header_.address));
                        }
                    }
                    num_re_trans = 4;
                    #ifdef Windows
                        closesocket(sock);
                        WSACleanup();
                    #else
                        close(sock);
                    #endif
                    return(rcvd_bytes);
                }
            }
        }
        #ifdef Windows
            closesocket(sock);
            WSACleanup();
        #else
            close(sock);
        #endif

        return -3;
    }
};
