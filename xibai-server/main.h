#pragma once

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h> 

#include <sys/socket.h>
#include <netinet/udp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>

#include <sys/time.h>
#include <time.h>


//最多允许的客户端数量
#define NUM 10

struct timeval te;
struct tm* pTempTm;
FILE* log_fp = fopen("xibai_log", "a+");

char* get_stime(void)
{
    static char timestr[200] = { 0 };
    gettimeofday(&te, NULL);
    pTempTm = localtime(&te.tv_sec);
    if (NULL != pTempTm)
    {
        snprintf(timestr, 199, "%04d-%02d-%02d %02d:%02d:%02d.%03ld",
            pTempTm->tm_year + 1900,
            pTempTm->tm_mon + 1,
            pTempTm->tm_mday,
            pTempTm->tm_hour,
            pTempTm->tm_min,
            pTempTm->tm_sec,
            te.tv_usec / 1000);
    }
    return timestr;
}

// 消息日志
void log(char* message) {
    gettimeofday(&te, NULL);
    fputs(ctime((const time_t*)&te.tv_sec), log_fp);
    fputs("        ", log_fp);
    fputs(message, log_fp);
}

// 每个客户端连接由一个新的子进程维持，对应的连接编号为 num
int heart(int server_fd, sockaddr_in target_addr, char num);
void stop(int sign);

// 获取网卡ip
in_addr_t getIP()
{
    struct sockaddr_in* sin = NULL;
    struct ifaddrs* ifa = NULL, * ifList;

    if (getifaddrs(&ifList) < 0)
    {
        log("获取网卡信息失败\n");
        return inet_addr("0.0.0.0");
    }

    for (ifa = ifList; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr->sa_family == AF_INET)
        {
            if (!strcmp(ifa->ifa_name, "eth0") || !strcmp(ifa->ifa_name, "ens32"))
            {
                sin = (struct sockaddr_in*)ifa->ifa_addr;
                printf("ipAddress: %s, interfaceName: %s\n", inet_ntoa(sin->sin_addr), ifa->ifa_name);
                return sin->sin_addr.s_addr;
            }

            //printf(">>> interfaceName: %s\n", ifa->ifa_name);

            //sin = (struct sockaddr_in*)ifa->ifa_addr;
            //printf(">>> ipAddress: %s\n", inet_ntoa(sin->sin_addr));

            //sin = (struct sockaddr_in*)ifa->ifa_dstaddr;
            //printf(">>> broadcast: %s\n", inet_ntoa(sin->sin_addr));

            //sin = (struct sockaddr_in*)ifa->ifa_netmask;
            //printf(">>> subnetMask: %s\n", inet_ntoa(sin->sin_addr));
        }
    }

    freeifaddrs(ifList);
    return 0;
}


// flag = 0  , heart
// flag = 1  , init
// flag = 2  , data
// flag = 3  , exit
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
    struct { unsigned short s_b1;uint8_t s_b2; } S_num;
    int len;
    uint8_t data[1452];
};

struct xibai_ready
{
    sockaddr_in real_target;
    char flag;
};

struct xibai_ready target_list[NUM] = { 0 };

int server_fd = -1;
char message[1024] = "";
char show_ip[4][16];