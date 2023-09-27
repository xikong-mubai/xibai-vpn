#include "main.h"

char init_before(sockaddr_in* target_addr, int num) {
    char message[3] = "\x00\x00", recv_message[10] = "";
    for (size_t i = 1; i < NUM; i++)
    {
        if (target_list[i].flag == 0)
        {
            num = i;
            message[0] = (char)(num & 0xff);
            while (-1 == sendto(server_fd, message, 2, 0, (sockaddr*)target_addr, (socklen_t)sizeof(sockaddr_in))) {
                log("init message send failed\n");
                printf("init message send failed\n");
            }
            target_list[num].flag = 2;
            return 1;
        }
    }
    while (-1 == sendto(server_fd, "client's number is max!\n", 24, 0, (sockaddr*)target_addr, (socklen_t)sizeof(sockaddr_in))) {
        log("client's number is max, but send failed\n");
        printf("client's number is max, but send failed\n");
    }
    return 0;
}

int heart() {
    int flag = fork();
    int len = 0, child_index = 0;
    char recv_message[1500] = "";
    sockaddr_in tmp_target_addr = { 0 };
    switch (flag)
    {
    case -1: {
        return -1;
    }
    case 0:                 // 子进程
    {
        int parent = getppid();
        while (true)
        {
            len = sizeof(sockaddr_in);
            len = recvfrom(server_fd, recv_message, 20, 0, (sockaddr*)&tmp_target_addr, (socklen_t*)&len);
            if (len == -1)
            {
                log("heart message recv failed\n");
                printf("heart message recv failed\n");
                continue;
            }
            child_index = recv_message[3];
            if (target_list[child_index].real_target.sin_addr.s_addr != tmp_target_addr.sin_addr.s_addr || target_list[child_index].real_target.sin_port != tmp_target_addr.sin_port)
            {
                printf("client_%d's ip:port changed.(%s:%d -> %s:%d)\n", child_index,
                    inet_ntoa(target_list[child_index].real_target.sin_addr),
                    htons(target_list[child_index].real_target.sin_port),
                    inet_ntoa(tmp_target_addr.sin_addr),
                    htons(tmp_target_addr.sin_port)
                );
                target_list[child_index].real_target = tmp_target_addr;
                while (-1 == write(pipe_fd[child_index][1], &tmp_target_addr, sizeof(sockaddr_in))) {

                }
                kill(parent, child_index + 23332);
            }
            while (-1 == sendto(server_fd, recv_message, 20, 0, (sockaddr*)&tmp_target_addr, (socklen_t)sizeof(sockaddr_in))) {
                log("heart message recv failed\n");
                printf("heart message recv failed\n");
            }
        }
        return 0;
    }
    default: {
        return flag;
    }

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
    for (int i = 1; i < 10; i++)
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

int control(xibai_data* buff,sockaddr_in* target_addr) {
    int udp_len = 0, len = 0, src_ip_index = 0, dst_ip_index = 0, pid = -1;
    socklen_t ta_len = sizeof(sockaddr_in);
    switch (buff->flag)
    {
    case 0:         // heart
        printf("why have heart packet?\n");
        break;
    case 1:         // init
        if (buff->src_target.addr.s_addr == 0)
        {
            if (init_before(target_addr, currentNum))
            {
                printf("client_%d connecting...\n", currentNum);
            }
            else
            {
                printf("client's num is max\n");
            }
        }
        else
        {
            unsigned int num = buff->src_target.addr.s_addr >> 24;
            if (num < 1 || num > 9)
            {
                printf("???\n");
                return -1;
            }
            if (target_list[num].flag == 2)
            {
                if (!memcmp(buff->data, "success", 7))
                {
                    sprintf(message,"client_%d connected\n", num);
                    printf("%s", message);
                    log(message);

                    target_list[num].real_target = *target_addr;
                    target_list[num].flag = 1;
                    printf("client add. real ip:port -> %s:%d, xibai ip: 192.168.222.%d\n",
                        inet_ntoa(target_addr->sin_addr), 
                        ntohs(target_addr->sin_port), 
                        currentNum
                    );
                    return 1;
                }
                else
                {
                    sprintf(message, "client_%d connected\n", num);
                    printf("%s", message);
                    log(message);
                }
            }
            else
            {
                sprintf(message, "client_%d connected\n", num);
                printf("%s", message);
                log(message);
            }
        }
        break;
    case 2:         // data
        udp_len = buff->len;
        src_ip_index = (buff->src_target.addr.s_addr >> 24);
        dst_ip_index = (buff->dst_target.addr.s_addr >> 24);
        if (udp_len > 1500) {
            char* message = (char*)malloc(1024);
            sprintf(message, "client udp length is exception ( %d ) !!! maybe is error!!!", udp_len);
            printf("%s\n", message);
            log(message);
        }
        else {
            if (dst_ip_index >= NUM && dst_ip_index != 255)
            {
                printf("unkown server, dst_ip: %s\n", inet_ntoa(buff->dst_target.addr));
            }
            else if (dst_ip_index == 255)
            {
                for (int i = 1; i < NUM; i++)
                {
                    target = target_list[i];
                    if (target.flag)//&& target_addr.sin_addr.s_addr != target.real_target.sin_addr.s_addr)
                    {
                        if (i != src_ip_index)
                        {
                            while (-1 == sendto(server_fd[0], buff, len, 0, (sockaddr*)&(target.real_target), target.realt_len))
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
                }
            }
            else {
                target = target_list[dst_ip_index];
                if (target.flag)
                {
                    while (-1 == sendto(server_fd[0], buff, len, 0, (sockaddr*)&(target.real_target), target.realt_len))
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
                else
                {
                    printf("dst_ip: 192.168.222.%d not found\n", dst_ip_index);
                }
            }
        }
        break;
    case 3:         //exit
    default:
        printf("shouldn't show\n");
        break;
    }
}

int main()
{
    signal(SIGINT, stop);
    signal(SIGQUIT, stop);
    signal(SIGKILL, stop);
    //struct sigaction sig = { 0 };
    //sig.sa_flags = SA_SIGINFO;
    //sig.sa_sigaction = update;
    for (int i = 1; i < 10; i++)
    {
        signal(23332 + i, update);
    }
    for (size_t i = 0; i < 10; i++)
    {
        pipe(pipe_fd[i]);
    }
    server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_fd == -1)
    {
        log("socket create error\n");
        exit(0);
    }
    printf("socket already created\n");

    // 绑定主服务监听
    RecvAddr = (sockaddr_in*)malloc(sizeof(sockaddr_in));
    RecvAddr->sin_family = AF_INET;
    RecvAddr->sin_addr.s_addr = getIP();
    RecvAddr->sin_port = htons(50001);
    if (-1 == bind(server_fd, (sockaddr *)RecvAddr, sizeof(sockaddr_in)))
    {
        char tmp[1024] = { 0 };
        sprintf(tmp,"main socket bind error: %d - %s\n", errno, strerror(errno));
        log(tmp);
        printf("%s", tmp);
        close(server_fd);
        exit(0);
    }
    printf("main socket already binded\n");

    xibai_data* buff;
    do
    {
        buff = (xibai_data*)malloc(sizeof(xibai_data));
    } while (!buff);
    printf("buffer already created\n");
    
    int udp_len = 0, len = 0, src_ip_index = 0, dst_ip_index = 0, pid = -1;
    sockaddr_in target_addr = { 0 }; xibai_ready target = { 0 };
    socklen_t ta_len = sizeof(sockaddr_in);
    while (1) {
        //bzero((char*)buff, 1500);
        len = recvfrom(server_fd, buff, 1472, 0, (sockaddr*)&target_addr, (socklen_t*)&ta_len);
        if (len == -1) {
            printf("recvfrom error\n");
            log("recvfrom error\n");
        }
        else if (len < 1473 && len > 0) {
            printf("[%s]  ",get_stime());
            printf("recv %s:%d success: %d\n", inet_ntoa(target_addr.sin_addr),ntohs(target_addr.sin_port), len);
            do
            {
                pid = fork();
            } while (pid == -1);
            if (!pid)
            {
                //switch (buff->flag)
                //{
                //case 0:         //        ; no need
                //    printf("why have heart packet?\n");
                //    break;
                //case 1:         //  heart  and   init
                //    if (buff->src_target.addr.s_addr == 0)
                //    {
                //        target_list[currentNum].flag = init(server_fd, &target_addr, currentNum);
                //        if (!target_list[currentNum].flag)
                //        {
                //            log("client connect failed\n");
                //            printf("client connect failed\n");
                //            continue;
                //        }
                //        else
                //        {
                //            target_list[currentNum].real_target = target_addr;
                //            fork_pid[currentNum] = heart();
                //            printf("client add. real ip:port -> %s:%d, xibai ip: 192.168.222.%d\n", inet_ntoa(target_addr.sin_addr), ntohs(target_addr.sin_port), currentNum);
                //            ++currentNum;
                //        }
                //    }
                //    else
                //    {
                //        printf("???\n");
                //    }
                //    break;
                //case 2:         // data
                //    udp_len = buff->len;
                //    src_ip_index = (buff->src_target.addr.s_addr >> 24);
                //    dst_ip_index = (buff->dst_target.addr.s_addr >> 24);
                //    if (udp_len > 1500) {
                //        char* message = (char*)malloc(1024);
                //        sprintf(message, "client udp length is exception ( %d ) !!! maybe is error!!!", udp_len);
                //        printf("%s\n", message);
                //        log(message);
                //    }
                //    else {
                //        if (dst_ip_index >= NUM && dst_ip_index != 255)
                //        {
                //            printf("unkown server, dst_ip: %s\n", inet_ntoa(buff->dst_target.addr));
                //        }
                //        else if (dst_ip_index == 255)
                //        {
                //            for (int i = 1; i < NUM; i++)
                //            {
                //                target = target_list[i];
                //                if (target.flag)//&& target_addr.sin_addr.s_addr != target.real_target.sin_addr.s_addr)
                //                {
                //                    if (i != src_ip_index)
                //                    {
                //                        while (-1 == sendto(server_fd[0], buff, len, 0, (sockaddr*)&(target.real_target), target.realt_len))
                //                        {
                //                            printf("send error: %s\n", inet_ntoa(buff->dst_target.addr));
                //                            log("send error\n");
                //                        }
                //                        printf("send %d bytes success: %s(%s:%d) -> %s(%s:%d)\n",
                //                            len,
                //                            inet_ntoa(buff->src_target.addr),
                //                            inet_ntoa(target_addr.sin_addr),
                //                            ntohs(target_addr.sin_port),
                //                            inet_ntoa(buff->dst_target.addr),
                //                            inet_ntoa(target.real_target.sin_addr),
                //                            ntohs(target.real_target.sin_port)
                //                        );
                //                    }
                //                }
                //            }
                //        }
                //        else {
                //            target = target_list[dst_ip_index];
                //            if (target.flag)
                //            {
                //                while (-1 == sendto(server_fd[0], buff, len, 0, (sockaddr*)&(target.real_target), target.realt_len))
                //                {
                //                    printf("send error: %s\n", inet_ntoa(buff->dst_target.addr));
                //                    log("send error\n");
                //                }
                //                printf("send %d bytes success: %s(%s:%d) -> %s(%s:%d)\n",
                //                    len,
                //                    inet_ntoa(buff->src_target.addr),
                //                    inet_ntoa(target_addr.sin_addr),
                //                    ntohs(target_addr.sin_port),
                //                    inet_ntoa(buff->dst_target.addr),
                //                    inet_ntoa(target.real_target.sin_addr),
                //                    ntohs(target.real_target.sin_port)
                //                );
                //            }
                //            else
                //            {
                //                printf("dst_ip: 192.168.222.%d not found\n", dst_ip_index);
                //            }
                //        }
                //    }
                //    break;
                //case 3:         //exit
                //default:
                //    printf("shouldn't show\n");
                //    break;
                //}
            }
            
        }
        else
        {
            char* message = (char*)malloc(1024);
            sprintf(message, "vpn udp length is exception ( %d ) !!! maybe is error!!!", len);
            printf("%s\n", message);
            log(message);
        }

    }

    close(server_fd);
    return 0;
}