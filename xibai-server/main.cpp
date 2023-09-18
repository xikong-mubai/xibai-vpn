#include "main.h"

int heart(int server_fd,sockaddr_in target_addr,char num) {
    int flag = fork();
    char message[3] = "0\x00";
    message[0] = '0' + num;
    switch (flag)
    {
    case -1:
        return -1;
        break;
    case 0:                 // 子进程
        if (num >= NUM)
        {
            sendto(server_fd, "client's number is max!\n", 24, NULL, (sockaddr*)&target_addr, (socklen_t)sizeof(target_addr));
            return 0;
        }
        while (true)
        {
            sendto(server_fd, message, 2, NULL, (sockaddr*)&target_addr, (socklen_t)sizeof(target_addr));
            printf("heart\n");
            sleep(1);
        }
        break;
    default:                // 父进程
        return flag;
    }
}

void stop(int sign) {
    exit(0);
}

int main()
{
    signal(SIGINT, stop);
    int server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_fd == -1)
    {
        log("socket create error\n");
        exit(0);
    }
    printf("socket already created\n");

    sockaddr_in RecvAddr = { 0 };
    RecvAddr.sin_family = AF_INET;
    RecvAddr.sin_port = htons(50001);
    RecvAddr.sin_addr.s_addr = getIP();

    // 绑定
    if (-1 == bind(server_fd, (struct sockaddr*)&RecvAddr, sizeof(RecvAddr)))
    {
        log("socket bind error\n");
        close(server_fd);
        exit(0);
    }
    printf("socket already binded\n");

    xibai_data *buff = (xibai_data*)malloc(1500);
    int len = 0;

    sockaddr_in target_addr = { 0 };
    socklen_t ta_len = sizeof(sockaddr_in);
    while (1) {
        len = recvfrom(server_fd, buff, 1500, NULL, (sockaddr*)&target_addr, (socklen_t*)&ta_len);
        if (len == -1) {
            printf("recvfrom error\n");
            log("recvfrom error\n");
        }
        else if (len < 1472 && len > 0) {
            printf("recv success: %d\n", len);
            switch (buff->flag)
            {
            case 0:         //  heart
                break;
            case 1:         //  init
                fork_pid[currentNum] = heart(server_fd, target_addr, currentNum);
                if (fork_pid[currentNum])
                {
                    target_list[currentNum].real_target = target_addr;
                    target_list[currentNum].realt_len = ta_len;
                    target_list[currentNum].flag = 1;
                    printf("client add. real ip: %s, xibai ip: 192.168.222.%d\n", inet_ntoa(target_addr.sin_addr),currentNum);
                    ++currentNum;
                }
                else {
                    printf("fork failed %d", currentNum);
                }
                break;
            case 2:         // data
                udp_len = ((xibai_data*)buff)->len;
                if (udp_len > len) {
                    char* message = (char*)malloc(1024);
                    sprintf(message, "client udp length is exception ( %d ) !!! maybe is error!!!", len);
                    printf("%s\n", message);
                    log(message);
                }
                else {
                    for (size_t i = 0; i < NUM; i++)
                    {
                        xibai_ready target = target_list[i];
                        if (target.flag)
                        {
                            if ((i & ntohl(buff->dst_target.addr.s_addr)) == i)
                            {
                                len = sendto(server_fd, buff, len, NULL, (sockaddr*)&(target.real_target), target.realt_len);
                                if (len == -1)
                                {
                                    printf("send error: %s\n", inet_ntoa(buff->dst_target.addr));
                                    log("send error\n");
                                }
                                printf("send %d bytes success: %s -> %s\n", len, inet_ntoa(buff->dst_target.addr), inet_ntoa(target.real_target.sin_addr));
                            }
                        }
                    }
                }
                break;
            case 3:         //exit
                break;
            default:
                break;
            }
            //printf("%s => %s\n", inet_ntoa(target_addr.sin_addr), buff);
            /*
            buff[0] = '1';
            sendto(server_fd, buff, 13, NULL, (sockaddr*)&target_addr, (socklen_t)ta_len);
            */
        }
        else
        {
            char* message = (char*)malloc(1024);
            sprintf(message, "vpn udp length is exception ( %d ) !!! maybe is error!!!", len);
            printf("%s\n", message);
            log(message);
        }
        bzero(buff, 0x10000);
    }

    close(server_fd);
    return 0;
}