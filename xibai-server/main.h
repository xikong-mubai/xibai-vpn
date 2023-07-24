#pragma once

#include <cstdio>
#include <signal.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/udp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>

#include <sys/time.h>
#include <time.h>

// ��Ϣ��־
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

// ÿ���ͻ���������һ���µ��ӽ��̴�����Ӧ�����ӱ��Ϊ count
int server(int count);

// ��ȡ����ip
in_addr_t getIP()
{
    struct sockaddr_in* sin = NULL;
    struct ifaddrs* ifa = NULL, * ifList;

    if (getifaddrs(&ifList) < 0)
    {
        log("��ȡ������Ϣʧ��\n");
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
