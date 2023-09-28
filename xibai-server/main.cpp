#include "main.h"

char init_before(sockaddr_in* target_addr) {
    char init_message[3] = "\x00\x00", recv_message[10] = "";
    int num = 0;
    for (size_t i = 1; i < NUM; i++)
    {
        if (target_list[i].flag == 0)
        {
            num = i;
            init_message[0] = (char)(num & 0xff);
            while (-1 == sendto(server_fd, init_message, 2, 0, (sockaddr*)target_addr, (socklen_t)sizeof(sockaddr_in))) {
                
                sprintf(message, "init message send failed: %d - %s\n", errno, strerror(errno));
                log(message);
                printf("%s", message);
            }
            target_list[num].flag = 2;
            return num;
        }
    }
    while (-1 == sendto(server_fd, "client's number is max!\n", 24, 0, (sockaddr*)target_addr, (socklen_t)sizeof(sockaddr_in))) {
        
        sprintf(message, "client's number is max, but send failed: %d - %s\n", errno, strerror(errno));
        log(message);
        printf("%s", message);
    }
    return num;
}

int heart(xibai_data* buff, sockaddr_in* target_addr) {
    unsigned int child_index = 0, flag = 0;
    child_index = buff->src_target.addr.s_addr >> 24;

    if (target_list[child_index].flag == 0)
    {
        memcpy(show_ip[0], inet_ntoa(target_addr->sin_addr), 16);
        printf("client_%d's reconnected.(%s:%d)\n", child_index,
            show_ip[0],
            htons(target_addr->sin_port)
        ); target_list[child_index].flag = 1;
        flag |= 1;
    }
    if (target_list[child_index].real_target.sin_addr.s_addr != target_addr->sin_addr.s_addr || target_list[child_index].real_target.sin_port != target_addr->sin_port)
    {
        memcpy(show_ip[0], inet_ntoa(target_list[child_index].real_target.sin_addr), 16);
        memcpy(show_ip[1], inet_ntoa(target_addr->sin_addr), 16);
        printf("client_%d's ip:port changed.(%s:%d -> %s:%d)\n", child_index,
            show_ip[0],
            htons(target_list[child_index].real_target.sin_port),
            show_ip[1],
            htons(target_addr->sin_port)
        );
        target_list[child_index].real_target = *target_addr;
        flag |= 2;
    }
    if (flag != 0)
    {
        printf("parent send update sig...\n");
        for (int i = 1; i < 2048; i++)
        {
            sprintf(path, "/proc/%d", fork_pid[i]);
            if (stat(path, &sts) == -1 && errno == ENOENT) {
                continue;
            }
            while (-1 == write(pipe_fd[child_index][1], target_list + child_index, sizeof(xibai_ready)))
            {
                sprintf(message, "pipe_%d write failed\n", child_index);
                log(message);
                printf("%s", message);
            }
            kill(fork_pid[i], child_index + 34);
        }
        printf("parent send update sig done.\n");
    }
    printf("%d %d %s %d %d\n", 
        server_fd, 
        *(unsigned long*)buff, 
        inet_ntoa(target_addr->sin_addr),
        htons(target_addr->sin_port), 
        sizeof(sockaddr_in)
    );
    if (-1 == sendto(server_fd, buff, 20, 0, (sockaddr*)target_addr, (socklen_t)sizeof(sockaddr_in))) {
        sprintf(message, "heart message send failed: %d - %s\n", errno, strerror(errno));
        log(message);
        printf("%s", message);
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
    if (server_fd != -1)
    {
        close(server_fd);
    }
    fclose(log_fp);
    exit(0);
}
void update(int sign) {
    printf("recv sig: %d\n", sign);
    int tmp_sign = sign - 34;
    while(-1 == read(pipe_fd[tmp_sign][0], target_list+tmp_sign, sizeof(xibai_ready))) {
        sprintf(message, "pipe_%d read failed: %d - %s\n", tmp_sign, errno, strerror(errno));
        log(message);
        printf("%s", message);
    }
}

int control(xibai_data* buff,sockaddr_in* target_addr,int len) {
    int udp_len = 0, src_ip_index = 0, dst_ip_index = 0, pid = -1;
    socklen_t ta_len = sizeof(sockaddr_in); xibai_ready target = { 0 };
    unsigned int num = 0;
    switch (buff->flag)
    {
    case 0:         // heart
        heart(buff, target_addr);
        break;
    case 1:         // init
        if (buff->src_target.addr.s_addr == 0)
        {
            num = init_before(target_addr);
            if (num)
            {
                printf("client_%d connecting...\n",num);
            }
            else
            {
                printf("client's num is max\n");
                return -1;
            }
        }
        else
        {
            num = buff->src_target.addr.s_addr >> 24;
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
                        num
                    );
                }
                else
                {
                    sprintf(message, "client_%d's message error\n", num);
                    printf("%s", message);
                    log(message);
                    return -2;
                }
            }
            else
            {
                sprintf(message, "client_%d connected\n", num);
                printf("%s", message);
                log(message);
                return -3;
            }
        }
        break;
    case 2:         // data
        do
        {
            pid = fork();
        } while (pid == -1);
        if (pid)
        {
            fork_pid[currentNum] = pid;
            currentNum++;
            currentNum %= 2048;
        }
        else {
            udp_len = buff->len;
            src_ip_index = (buff->src_target.addr.s_addr >> 24);
            dst_ip_index = (buff->dst_target.addr.s_addr >> 24);
            if (udp_len > 1500) {
                char* message = (char*)malloc(1024);
                sprintf(message, "client udp length is exception ( %d ) !!! maybe is error!!!", udp_len);
                printf("%s\n", message);
                log(message);
                return -4;
            }
            else if (dst_ip_index >= NUM && dst_ip_index != 255)
            {
                printf("unkown server, dst_ip: %s\n", inet_ntoa(buff->dst_target.addr));
                return -5;
            }
            else if (dst_ip_index == 255)
            {
                for (int i = 1; i < NUM; i++)
                {
                    target = target_list[i];
                    if (target.flag == 1)//&& target_addr.sin_addr.s_addr != target.real_target.sin_addr.s_addr)
                    {
                        if (i != src_ip_index)
                        {
                            while (-1 == sendto(server_fd, buff, len, 0, (sockaddr*)&(target.real_target), ta_len))
                            {
                                sprintf(message, "send error(dst:%s): %d - %s\n", inet_ntoa(buff->dst_target.addr), errno, strerror(errno));
                                log(message);
                                printf("%s", message);
                            }
                            memcpy(show_ip[0], inet_ntoa(buff->src_target.addr), 16);
                            memcpy(show_ip[1], inet_ntoa(target_addr->sin_addr), 16);
                            memcpy(show_ip[2], inet_ntoa(buff->dst_target.addr), 16);
                            memcpy(show_ip[3], inet_ntoa(target.real_target.sin_addr), 16);
                            printf("send %d bytes success: %s(%s:%d) -> %s(%s:%d)\n",
                                len,
                                show_ip[0],
                                show_ip[1],
                                ntohs(target_addr->sin_port),
                                show_ip[2],
                                show_ip[3],
                                ntohs(target.real_target.sin_port)
                            );
                        }
                    }
                }
            }
            else {
                target = target_list[dst_ip_index];
                if (target.flag == 1)
                {
                    while (-1 == sendto(server_fd, buff, len, 0, (sockaddr*)&(target.real_target), ta_len))
                    {
                        sprintf(message, "send error(dst:%s): %d - %s\n", inet_ntoa(buff->dst_target.addr), errno, strerror(errno));
                        log(message);
                        printf("%s", message);
                    }
                    memcpy(show_ip[0], inet_ntoa(buff->src_target.addr), 16);
                    memcpy(show_ip[1], inet_ntoa(target_addr->sin_addr), 16);
                    memcpy(show_ip[2], inet_ntoa(buff->dst_target.addr), 16);
                    memcpy(show_ip[3], inet_ntoa(target.real_target.sin_addr), 16);
                    printf("send %d bytes success: %s(%s:%d) -> %s(%s:%d)\n",
                        len,
                        show_ip[0],
                        show_ip[1],
                        ntohs(target_addr->sin_port),
                        show_ip[2],
                        show_ip[3],
                        ntohs(target.real_target.sin_port)
                    );
                }
                else
                {
                    printf("dst_ip: 192.168.222.%d not found\n", dst_ip_index);
                    return -6;
                }
            }
            exit(0);
        }
        break;
    case 3:         //exit
    default:
        printf("shouldn't show\n");
        break;
    }
    return 0;
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
        signal(34 + i, update);
    }
    for (size_t i = 0; i < 10; i++)
    {
        pipe(pipe_fd[i]);
    }
    fork_pid[0] = getpid();
    server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_fd == -1)
    {
        log("socket create error\n");
        exit(0);
    }
    printf("socket already created\n");

    // 绑定主服务监听
    sockaddr_in RecvAddr = { 0 };
    RecvAddr.sin_family = AF_INET;
    RecvAddr.sin_addr.s_addr = getIP();
    RecvAddr.sin_port = htons(50001);
    if (-1 == bind(server_fd, (sockaddr *)&RecvAddr, sizeof(sockaddr_in)))
    {
        sprintf(message,"main socket bind error: %d - %s\n", errno, strerror(errno));
        log(message);
        printf("%s", message);
        close(server_fd);
        exit(0);
    }
    printf("main socket already binded\n");

    xibai_data* buff;
    do
    {
        printf("buffer creating...\n");
        buff = (xibai_data*)malloc(sizeof(xibai_data));
    } while (!buff);
    printf("buffer already created\n");
    
    int len = 0, pid = -1, result = 0;
    sockaddr_in target_addr = { 0 };
    socklen_t ta_len = sizeof(sockaddr_in);
    while (1) {
        //bzero((char*)buff, 1500);
        len = recvfrom(server_fd, buff, 1472, 0, (sockaddr*)&target_addr, (socklen_t*)&ta_len);
        if (len == -1) {
            sprintf(message, "recvfrom error: %d - %s\n", errno, strerror(errno));
            log(message);
            printf("%s", message);
        }
        else if (len < 1473 && len > 0) {
            printf("[%s]  ",get_stime());
            printf("recv %s:%d success: %d\n", inet_ntoa(target_addr.sin_addr),ntohs(target_addr.sin_port), len);
            result = control(buff, &target_addr, len);
            if (result != 0) {
                printf("WarningCode: %d\n", result);
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