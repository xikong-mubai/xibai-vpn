#pragma once

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/udp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>

#include <sys/time.h>
#include <time.h>


//最多允许的客户端数量
#define NUM 100


// 消息日志
int log(char* message) {
    FILE* log_fp = fopen("xibai_log", "a+");
    struct timeval te;
    gettimeofday(&te, NULL);
    fputs(ctime((const time_t*)&te.tv_sec), log_fp);
    fputs("        ", log_fp);
    fputs(message, log_fp);
    fclose(log_fp);
    return 0;
}

// 每个客户端连接由一个新的子进程维持，对应的连接编号为 num
int heart(int server_fd, sockaddr_in target_addr, int num);

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
            if (ifa->ifa_name == "eth0" || ifa->ifa_name == "ens32")
            {
                sin = (struct sockaddr_in*)ifa->ifa_addr;
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


// flag = 0  , heart data
// flag = 1  , init
// flag = 2  , data
// flag = 3  , 
// flag = 4  , no target and wait init
struct xibai_target
{
    in_addr addr;
    ushort port;
};

struct xibai_data
{
    xibai_target src_target;
    xibai_target dst_target;
    char flag;
    int len;
    uint8_t data[65519];
};

struct xibai_ready
{
    sockaddr_in real_target;
    socklen_t realt_len;
    char flag;
};

int udp_len = 0;
struct xibai_ready target_list[NUM] = { 0 };
int fork_pid[NUM] = { 0 };
int currentNum = 1;      //当前ip数量