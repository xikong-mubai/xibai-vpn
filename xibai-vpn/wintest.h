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


#pragma pack(1)
typedef struct in_port_t
{
    union {
        struct { UCHAR s_b1, s_b2, s_b3, s_b4; } S_un_b;
        struct { USHORT s_w1, s_w2; } S_un_w;
        ULONG S_addr;
    } S_un;
};

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
    char flag;
    int len;
    unsigned char data[65519];
};

#pragma pack(1)
struct xibai_ready
{
    sockaddr_in real_target;
    socklen_t realt_len;
    char flag;
};