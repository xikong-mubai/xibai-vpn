#include "main.h"

int heart(int server_fd,sockaddr_in target_addr) {
    int flag = fork();
    switch (flag)
    {
    case -1:
        return -1;
        break;
    case 0:                 // 子进程
        while (true)
        {
            sendto(server_fd, "test\n", 5, NULL, (sockaddr*)&target_addr, (socklen_t)sizeof(target_addr));
            sleep(1);
        }
        break;
    default:                // 父进程
        return flag;
    }
}

int main()
{
    int server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_fd == -1)
    {
        log("socket create error\n");
        exit(0);
    }
    printf("socket already created\n");

    sockaddr_in RecvAddr;
    RecvAddr.sin_family = AF_INET;
    RecvAddr.sin_port = htons(60001);
    RecvAddr.sin_addr.s_addr = getIP();

    // 绑定
    if (-1 == bind(server_fd, (struct sockaddr*)&RecvAddr, sizeof(RecvAddr)))
    {
        log("socket bind error\n");
        close(server_fd);
        exit(0);
    }
    printf("socket already binded\n");

    char* buff = (char*)malloc(0x10000);
    int len = 0;

    sockaddr_in target_addr = { 0 };
    socklen_t ta_len = sizeof(sockaddr_in);
    while (1) {
        len = recvfrom(server_fd, buff, 0x10000, NULL, (sockaddr*)&target_addr, (socklen_t*)&ta_len);
        if (len == -1) {
            log("recvfrom error\n");
            
        }
        else if (len < 0x10000) {
            if (target_list[buff[3]])
            {

            }
            //printf("%s => %s\n", inet_ntoa(target_addr.sin_addr), buff);
            /*
            buff[0] = '1';
            sendto(server_fd, buff, 13, NULL, (sockaddr*)&target_addr, (socklen_t)ta_len);
            */
            
            break;
        }
        else
        {
            printf("udp length is max!!! maybe is error!!!\n");
            log("udp length is max!!! maybe is error!!!\n");
        }
    }

    close(server_fd);
    return 0;
}