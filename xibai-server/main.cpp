#include "main.h"

int init(int server_fd[], sockaddr_in target_addr, int num) {
    char message[3] = "\x00\x00", recv_message[10] = "";
    message[0] = '\x00' + num;
    int len = 0;
    if (num >= NUM)
    {
        while (-1 == sendto(server_fd[0], "client's number is max!\n", 24, NULL, (sockaddr*)&target_addr, (socklen_t)sizeof(target_addr))) {
            log("client's number is max, but send failed\n");
            printf("client's number is max, but send failed\n");
        }
        return 0;
    }
    while (-1 == sendto(server_fd[0], message, 2, NULL, (sockaddr*)&target_addr, (socklen_t)sizeof(target_addr))) {
        log("init message send failed\n");
        printf("init message send failed\n");
    }
    printf("init\n");
    len = sizeof(target_addr);
    len = recvfrom(server_fd[1], recv_message, 8, NULL, (sockaddr*)&target_addr, (socklen_t*)&len);
    if (len == -1)
    {
        log("init message recv failed\n");
        printf("init message recv failed\n");
        return 0;
    }
    if (!memcmp(recv_message, "success", 7))
    {
        //heart(server_fd[2], target_addr, num);
        return 1;
    }
    return 0;
}


int heart(int server_fd, sockaddr_in target_addr, char num) {
    int flag = fork(), len = 0, child_index = 0;
    char recv_message[1500] = "";
    sockaddr_in tmp_target_addr = { 0 };
    switch (flag)
    {
    case -1:
        return -1;
    case 0:                 // 子进程
        int parent = getppid();
        while (true)
        {
            len = sizeof(target_addr);
            len = recvfrom(server_fd, recv_message, 20, NULL, (sockaddr*)&tmp_target_addr, (socklen_t*)&len);
            if (len == -1)
            {
                log("heart message recv failed\n");
                printf("heart message recv failed\n");
                continue;
            }
            child_index = recv_message[3];
            printf("12123  %d\n", child_index);//////////////////////////////////////////
            if (target_list[child_index].real_target.sin_addr.s_addr != tmp_target_addr.sin_addr.s_addr || target_list[child_index].real_target.sin_port != tmp_target_addr.sin_port)
            {
                target_list[child_index].real_target = tmp_target_addr;
                while (-1 == write(pipe_fd[child_index][1], &tmp_target_addr, sizeof(target_addr))) {

                }
                kill(parent, child_index + 23332);
            }
            while (-1 == sendto(server_fd, recv_message, 20, NULL, (sockaddr*)&tmp_target_addr, (socklen_t)sizeof(target_addr))){
                log("heart message recv failed\n");
                printf("heart message recv failed\n");
            }
        }
        return 0;
    default:                // 父进程
        return flag;
    }
}

void stop(int sign) {
    //printf("%d\n", sign);
    //for (size_t i = 1; i < NUM; i++)
    //{
    //    if (target_list[i].flag == 1) {
    //        kill(fork_pid[i],SIGQUIT);
    //    }
    //}
    fclose(log_fp);
    exit(0);
}
void update(int sign) {
    sockaddr_in *tmp = (sockaddr_in*)malloc(sizeof(sockaddr_in));
    char message[100] = "";
    int tmp_sign = sign - 23332;
    while(-1 == read(pipe_fd[tmp_sign][0], tmp, sizeof(sockaddr_in))) {
        sprintf(message, "pipe_%d read failed\n", tmp_sign);
        log(message);
        printf("%s", message);
    }
    target_list[tmp_sign].real_target = *tmp;
    for (size_t i = 1; i < 10; i++)
    {
        if (i != tmp_sign) {
            while (-1 == write(pipe_fd[i][1], &tmp, sizeof(sockaddr_in)))
            {
                sprintf(message, "pipe_%d write failed\n", tmp_sign);
                log(message);
                printf("%s", message);
            }
            kill(fork_pid[i], sign);
        }
    }
}

int main()
{
    signal(SIGINT, stop);
    signal(SIGQUIT, stop);
    signal(SIGKILL, stop);
    for (int i = 1; i < 10; i++)
    {
        signal(23332+i, update);
    }
    for (size_t i = 0; i < 10; i++)
    {
        pipe(pipe_fd[i]);
    }
    int server_fd[3] = { socket(AF_INET, SOCK_DGRAM, 0),socket(AF_INET, SOCK_DGRAM, 0),socket(AF_INET, SOCK_DGRAM, 0) };
    if (server_fd[0] == -1 || server_fd[1] == -1 || server_fd[2] == -1)
    {
        log("socket create error\n");
        exit(0);
    }
    printf("socket already created\n");

    // 绑定主服务监听
    sockaddr_in RecvAddr = { 0 };
    RecvAddr.sin_family = AF_INET;
    RecvAddr.sin_port = htons(50001);
    RecvAddr.sin_addr.s_addr = getIP();
    if (-1 == bind(server_fd[0], (struct sockaddr*)&RecvAddr, sizeof(RecvAddr)))
    {
        char tmp[1024] = { 0 };
        sprintf(tmp,"main socket bind error: %d - %s\n", errno, strerror(errno));
        log(tmp);
        printf("%s", tmp);
        close(server_fd[0]);
        close(server_fd[1]);
        close(server_fd[2]);
        exit(0);
    }
    printf("main socket already binded\n");

    // 绑定初始化服务监听
    RecvAddr.sin_port = htons(50002);
    if (-1 == bind(server_fd[1], (struct sockaddr*)&RecvAddr, sizeof(RecvAddr)))
    {
        char tmp[1024] = { 0 };
        sprintf(tmp, "init socket bind error: %d - %s\n", errno, strerror(errno));
        log(tmp);
        printf("%s", tmp);
        close(server_fd[0]);
        close(server_fd[1]);
        close(server_fd[2]);
        exit(0);
    }
    printf("init socket already binded\n");

    // 绑定心跳重连服务监听
    RecvAddr.sin_port = htons(50003);
    if (-1 == bind(server_fd[2], (struct sockaddr*)&RecvAddr, sizeof(RecvAddr)))
    {
        char tmp[1024] = { 0 };
        sprintf(tmp, "heart socket bind error: %d - %s\n", errno, strerror(errno));
        log(tmp);
        printf("%s", tmp);
        close(server_fd[0]);
        close(server_fd[1]);
        close(server_fd[2]);
        exit(0);
    }
    printf("heart socket already binded\n");

    xibai_data *buff = (xibai_data*)malloc(2000);
    int len = 0, src_ip_index = 0, dst_ip_index = 0;

    sockaddr_in target_addr = { 0 }; xibai_ready target = { 0 };
    socklen_t ta_len = sizeof(sockaddr_in);
    while (1) {
        //bzero((char*)buff, 1500);
        len = recvfrom(server_fd[0], buff, 1500, NULL, (sockaddr*)&target_addr, (socklen_t*)&ta_len);
        if (len == -1) {
            printf("recvfrom error\n");
            log("recvfrom error\n");
        }
        else if (len < 1473 && len > 0) {
            printf("[%s]  ",get_stime());
            printf("recv %s:%d success: %d\n", inet_ntoa(target_addr.sin_addr),ntohs(target_addr.sin_port), len);
            switch (buff->flag)
            {
            case 0:         //        ; no need
                printf("why have heart packet?\n");
                break;
            case 1:         //  heart  and   init
                if (buff->src_target.addr.s_addr == 0)
                {
                    target_list[currentNum].flag = init(server_fd, target_addr, currentNum);
                    if (!target_list[currentNum].flag)
                    {
                        log("client connect failed\n");
                        printf("client connect failed\n");
                        continue;
                    }
                    else 
                    {
                        target_list[currentNum].real_target = target_addr;
                        target_list[currentNum].realt_len = ta_len;
                        fork_pid[currentNum] = heart(server_fd[2],target_addr,currentNum);
                        printf("client add. real ip:port -> %s:%d, xibai ip: 192.168.222.%d\n", inet_ntoa(target_addr.sin_addr), ntohs(target_addr.sin_port), currentNum);
                        ++currentNum;
                    }
                }
                else
                {
                    printf("???\n");
                }
                break;
            case 2:         // data
                udp_len = buff->len;
                src_ip_index = (buff->src_target.addr.s_addr >> 24);
                dst_ip_index = (buff->dst_target.addr.s_addr >> 24);
                if (udp_len > len) {
                    char* message = (char*)malloc(1024);
                    sprintf(message, "client udp length is exception ( %d ) !!! maybe is error!!!", len);
                    printf("%s\n", message);
                    log(message);
                }
                else {
                    if (dst_ip_index == 255)
                    {
                        for (size_t i = 1; i < NUM; i++)
                        {
                            target = target_list[i];
                            if (target.flag)//&& target_addr.sin_addr.s_addr != target.real_target.sin_addr.s_addr)
                            {
                                if (i != src_ip_index)
                                {
                                    while (-1 == sendto(server_fd[0], buff, len, NULL, (sockaddr*)&(target.real_target), target.realt_len))
                                    {
                                        printf("send error: %s\n", inet_ntoa(buff->dst_target.addr));
                                        log("send error\n");
                                    }
                                    printf("send %d bytes success: %s(%s:%d) -> %s(%s:%d)\n", len, inet_ntoa(buff->src_target.addr), inet_ntoa(target_addr.sin_addr), ntohs(target_addr.sin_port), inet_ntoa(buff->dst_target.addr), inet_ntoa(target.real_target.sin_addr), ntohs(target.real_target.sin_port));
                                }
                            }
                        }
                    }
                    else if (dst_ip_index >= NUM)
                    {
                        printf("unkown server, dst_ip: %s\n", inet_ntoa(buff->dst_target.addr));
                    }
                    else
                    {
                        target = target_list[dst_ip_index];
                        while (-1 == sendto(server_fd[0], buff, len, NULL, (sockaddr*)&(target.real_target), target.realt_len))
                        {
                            printf("send error: %s\n", inet_ntoa(buff->dst_target.addr));
                            log("send error\n");
                        }
                        printf("send %d bytes success: %s(%s:%d) -> %s(%s:%d)\n", 
                            len, 
                            inet_ntoa(buff->src_target.addr), 
                            inet_ntoa(target_addr.sin_addr), 
                            ntohs(target_addr.sin_port), 
                            inet_ntoa(buff->dst_target.addr), 
                            inet_ntoa(target.real_target.sin_addr), 
                            ntohs(target.real_target.sin_port)
                        );
                    }
                }
                break;
            case 3:         //exit
            default:
                printf("shouldn't show\n");
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

    }

    close(server_fd[0]);
    close(server_fd[1]);
    close(server_fd[2]);
    return 0;
}