#pragma once
#include <winsock2.h>
#include <Windows.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>
#include <inaddr.h>
#include <WS2tcpip.h>
#include <mstcpip.h>
#include <ip2string.h>
#include <winternl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include "wintun.h"

SOCKET server_socket;
sockaddr_in recvAddr;
int sock_len = sizeof(sockaddr);

#pragma pack(1)
typedef struct in_port
{
    union {
        struct { UCHAR s_b1, s_b2; } S_un_b;
        USHORT S_port;
    } S_un;
}in_port_t;

// flag = 0  , heart data
// flag = 1  , init
// flag = 2  , data
// flag = 3  , 
// flag = 4  , no target and wait init
#pragma pack(1)
struct xibai_target
{
    in_addr addr;
    in_port_t port;
};

#pragma pack(1)
struct xibai_data
{
    xibai_target src_target;
    xibai_target dst_target;
    unsigned char flag;
    struct { USHORT s_b1;UCHAR s_b2; } S_num;
    unsigned int len;
    unsigned char data[1452];
} *recvBuff = (xibai_data*)malloc(sizeof(xibai_data)),
*sendBuff = (xibai_data*)malloc(sizeof(xibai_data));

#pragma pack(1)
struct xibai_ready
{
    sockaddr_in real_target;
    socklen_t realt_len;
    char flag;
};

#pragma pack(1)
struct packet_node
{
    char flag;
    BYTE* packet_data;
};

packet_node packet_list[0x10000] = { 0 };