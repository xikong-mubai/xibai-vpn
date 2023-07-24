#include "main.h"

int server(int count) {
    int flag = fork();
    switch (flag)
    {
    case -1:
        return -1;
        break;
    case 0:                 // 子进程
        return count;
        break;
    default:                // 父进程
        return 0;
        break;
    }
}

int main()
{
    int server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_fd == -1)
    {
        log("socket create error\n");
    }
    sockaddr_in RecvAddr;
    RecvAddr.sin_family = AF_INET;
    RecvAddr.sin_port = htons(60001);
    RecvAddr.sin_addr.s_addr = getIP();

    bind(server_fd, (struct sockaddr*)&RecvAddr, sizeof(RecvAddr));

    printf("%s 向你问好!\n", "xibai_server");
    return 0;
}