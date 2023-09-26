/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2018-2021 WireGuard LLC. All Rights Reserved.
 */

#include "wintest.h"

#pragma comment (lib, "Ws2_32.lib")
#pragma comment(lib, "ntdll.lib")
#pragma comment(lib, "Iphlpapi.lib")


static WINTUN_CREATE_ADAPTER_FUNC* WintunCreateAdapter;
static WINTUN_CLOSE_ADAPTER_FUNC* WintunCloseAdapter;
static WINTUN_OPEN_ADAPTER_FUNC* WintunOpenAdapter;
static WINTUN_GET_ADAPTER_LUID_FUNC* WintunGetAdapterLUID;
static WINTUN_GET_RUNNING_DRIVER_VERSION_FUNC* WintunGetRunningDriverVersion;
static WINTUN_DELETE_DRIVER_FUNC* WintunDeleteDriver;
static WINTUN_SET_LOGGER_FUNC* WintunSetLogger;
static WINTUN_START_SESSION_FUNC* WintunStartSession;
static WINTUN_END_SESSION_FUNC* WintunEndSession;
static WINTUN_GET_READ_WAIT_EVENT_FUNC* WintunGetReadWaitEvent;
static WINTUN_RECEIVE_PACKET_FUNC* WintunReceivePacket;
static WINTUN_RELEASE_RECEIVE_PACKET_FUNC* WintunReleaseReceivePacket;
static WINTUN_ALLOCATE_SEND_PACKET_FUNC* WintunAllocateSendPacket;
static WINTUN_SEND_PACKET_FUNC* WintunSendPacket;
// 加载 wintun.dll 中的函数
static HMODULE
InitializeWintun(void)
{
    HMODULE Wintun =
        LoadLibraryExW(L"wintun.dll", NULL, LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!Wintun)
        return NULL;
#define X(Name) ((*(FARPROC *)&Name = GetProcAddress(Wintun, #Name)) == NULL)
    if (X(WintunCreateAdapter) || X(WintunCloseAdapter) || X(WintunOpenAdapter) || X(WintunGetAdapterLUID) ||
        X(WintunGetRunningDriverVersion) || X(WintunDeleteDriver) || X(WintunSetLogger) || X(WintunStartSession) ||
        X(WintunEndSession) || X(WintunGetReadWaitEvent) || X(WintunReceivePacket) || X(WintunReleaseReceivePacket) ||
        X(WintunAllocateSendPacket) || X(WintunSendPacket))
#undef X
    {
        DWORD LastError = GetLastError();
        FreeLibrary(Wintun);
        SetLastError(LastError);
        return NULL;
    }
    return Wintun;
}

static void CALLBACK
ConsoleLogger(_In_ WINTUN_LOGGER_LEVEL Level, _In_ DWORD64 Timestamp, _In_z_ const WCHAR* LogLine)
{
    SYSTEMTIME SystemTime;
    FileTimeToSystemTime((FILETIME*)&Timestamp, &SystemTime);
    WCHAR LevelMarker;
    switch (Level)
    {
    case WINTUN_LOG_INFO:
        LevelMarker = L'+';
        break;
    case WINTUN_LOG_WARN:
        LevelMarker = L'-';
        break;
    case WINTUN_LOG_ERR:
        LevelMarker = L'!';
        break;
    default:
        return;
    }
    fwprintf(
        stderr,
        L"%04u-%02u-%02u %02u:%02u:%02u.%04u [%c] %ls\n",
        SystemTime.wYear,
        SystemTime.wMonth,
        SystemTime.wDay,
        SystemTime.wHour,
        SystemTime.wMinute,
        SystemTime.wSecond,
        SystemTime.wMilliseconds,
        LevelMarker,
        LogLine);
}

static DWORD64 Now(VOID)
{
    LARGE_INTEGER Timestamp;
    NtQuerySystemTime(&Timestamp);
    return Timestamp.QuadPart;
}

// back
LPTSTR ConvertErrorCodeToString(DWORD ErrorCode)
{
    LPTSTR LocalAddress = NULL;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        NULL, ErrorCode, 0, (LPTSTR)&LocalAddress, 1024, NULL);
    return (LPTSTR)LocalAddress;
}

static DWORD
LogError(_In_z_ const WCHAR* Prefix, _In_ DWORD Error)
{
    LPTSTR SystemMessage = NULL,  FormattedMessage = NULL;
    if (!FormatMessageW(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
        NULL,
        HRESULT_FROM_SETUPAPI(Error),
        0,//MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&SystemMessage,
        1024,
        NULL)) {
        ConsoleLogger(WINTUN_LOG_ERR, Now(), ConvertErrorCodeToString(Error));
    }
    DWORD_PTR pArgs[] = { (DWORD_PTR)Prefix, (DWORD_PTR)Error, (DWORD_PTR)SystemMessage };
    if (!FormatMessageW(
        FORMAT_MESSAGE_FROM_STRING | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_ARGUMENT_ARRAY
        | FORMAT_MESSAGE_MAX_WIDTH_MASK,
        &SystemMessage ? L"%1: %3(Code 0x%2!08X!)" : L"%1: Code 0x%2!08X!",
        0,
        0,
        (LPTSTR)&FormattedMessage,
        1024,
        (va_list*)pArgs)) {
        ConsoleLogger(WINTUN_LOG_ERR, Now(), ConvertErrorCodeToString(Error));
    }
    if (FormattedMessage)
        ConsoleLogger(WINTUN_LOG_ERR, Now(), FormattedMessage);
    LocalFree(FormattedMessage);
    LocalFree(SystemMessage);
    return Error;
}

static DWORD
LogLastError(_In_z_ const WCHAR* Prefix)
{
    DWORD LastError = GetLastError();
    LogError(Prefix, LastError);
    SetLastError(LastError);
    return LastError;
}

static void
Log(_In_ WINTUN_LOGGER_LEVEL Level, _In_z_ const WCHAR* Format, ...)
{
    WCHAR LogLine[0x200];
    va_list args;
    va_start(args, Format);
    _vsnwprintf_s(LogLine, _countof(LogLine), _TRUNCATE, Format, args);
    va_end(args);
    ConsoleLogger(Level, Now(), LogLine);
}

static HANDLE QuitEvent;
static volatile BOOL HaveQuit;

static BOOL WINAPI
CtrlHandler(_In_ DWORD CtrlType)
{
    switch (CtrlType)
    {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        Log(WINTUN_LOG_INFO, L"Cleaning up and shutting down...");
        HaveQuit = TRUE;
        SetEvent(QuitEvent);
        return TRUE;
    }
    return FALSE;
}

static void
PrintPacket(_In_ const BYTE* Packet, _In_ DWORD PacketSize)
{
    if (PacketSize < 20)
    {
        Log(WINTUN_LOG_INFO, L"Received packet without room for an IP header");
        return;
    }
    BYTE IpVersion = Packet[0] >> 4, Proto;
    WCHAR Src[46], Dst[46];
    if (IpVersion == 4)
    {
        RtlIpv4AddressToStringW((struct in_addr*)&Packet[12], Src);
        RtlIpv4AddressToStringW((struct in_addr*)&Packet[16], Dst);
        Proto = Packet[9];
        Packet += 20, PacketSize -= 20;
    }
    else if (IpVersion == 6 && PacketSize < 40)
    {
        Log(WINTUN_LOG_INFO, L"Received packet without room for an IP header");
        return;
    }
    else if (IpVersion == 6)
    {
        RtlIpv6AddressToStringW((struct in6_addr*)&Packet[8], Src);
        RtlIpv6AddressToStringW((struct in6_addr*)&Packet[24], Dst);
        Proto = Packet[6];
        Packet += 40, PacketSize -= 40;
    }
    else
    {
        Log(WINTUN_LOG_INFO, L"Received packet that was not IP");
        return;
    }
    if (Proto == 1 && PacketSize >= 8 && Packet[0] == 0)
        Log(WINTUN_LOG_INFO, L"Received IPv%d ICMP echo reply from %s to %s", IpVersion, Src, Dst);
    else
        Log(WINTUN_LOG_INFO, L"Received IPv%d proto 0x%x packet from %s to %s", IpVersion, Proto, Src, Dst);
}

static USHORT
IPChecksum(_In_reads_bytes_(Len) BYTE* Buffer, _In_ DWORD Len)
{
    ULONG Sum = 0;
    for (; Len > 1; Len -= 2, Buffer += 2)
        Sum += *(USHORT*)Buffer;
    if (Len)
        Sum += *Buffer;
    Sum = (Sum >> 16) + (Sum & 0xffff);
    Sum += (Sum >> 16);
    return (USHORT)(~Sum);
}

static void
MakeICMP(_Out_writes_bytes_all_(28) BYTE Packet[28])
{
    memset(Packet, 0, 28);
    Packet[0] = 0x45;
    *(USHORT*)&Packet[2] = htons(28);
    Packet[8] = 255;
    Packet[9] = 1;
    *(ULONG*)&Packet[12] = htonl((10 << 24) | (6 << 16) | (7 << 8) | (8 << 0)); /* 10.6.7.8 */
    *(ULONG*)&Packet[16] = htonl((10 << 24) | (6 << 16) | (7 << 8) | (7 << 0)); /* 10.6.7.7 */
    *(USHORT*)&Packet[10] = IPChecksum(Packet, 20);
    Packet[20] = 8;
    *(USHORT*)&Packet[22] = IPChecksum(&Packet[20], 8);
    Log(WINTUN_LOG_INFO, L"Sending IPv4 ICMP echo request to 10.6.7.8 from 10.6.7.7");
}


static int
CheckPacket(_In_ const BYTE* Packet, _In_ DWORD PacketSize)
{
    if(Packet[9] != 17)
    {
        return 0;
    }
    else {
        return 1;
    }
}

static DWORD WINAPI
ReceivePackets(_Inout_ DWORD_PTR SessionPtr)
{
    WINTUN_SESSION_HANDLE Session = (WINTUN_SESSION_HANDLE)SessionPtr;
    HANDLE WaitHandles[] = { WintunGetReadWaitEvent(Session), QuitEvent };
    xibai_data* data = sendBuff;//(xibai_data*)malloc(sizeof(xibai_data));
    if (!data)
    {
        Log(WINTUN_LOG_ERR, L"recv_packet_buffer_ptr init error!");
        DWORD LastError = GetLastError();
        LogError(L"ReceivePackets Packet check failed", LastError);
        return LastError;
    }
    data->flag = 2; data->S_num = { 0 };
    int len = 0;
    while (!HaveQuit)
    {
        DWORD PacketSize;
        BYTE* Packet = WintunReceivePacket(Session, &PacketSize);

        if (Packet)
        {
            if (CheckPacket(Packet, PacketSize)) {
                data->src_target.port.S_un.S_port = *(short*)(Packet + 20);
                data->dst_target.addr.S_un.S_addr = *(u_long*)(Packet + 16);
                data->dst_target.port.S_un.S_port = *(short*)(Packet + 22);
                data->len = PacketSize; data->S_num.s_b2 = 0;
                if (PacketSize > 1500)
                {
                    Log(WINTUN_LOG_WARN, L"recv_packet_size so big!!!");
                    //memcpy(data->data, Packet, 65515);
                    //len = sendto(server_socket, (char*)data, 65536, NULL, (sockaddr*)&recvAddr, sizeof(recvAddr));
                    //if (len != -1) {
                    //    PrintPacket(Packet, PacketSize);
                    //    memcpy(data->data, Packet + 65515, PacketSize - 65515);
                    //    data->S_un.S_un_b.s_b2 += 1;
                    //    len = sendto(server_socket, (char*)data, PacketSize - 65515 + 21, NULL, (sockaddr*)&recvAddr, sizeof(recvAddr));
                    //    if (len != -1) {
                    //        data->S_un.S_un_b.s_b1 += 1;
                    //        PrintPacket(Packet, PacketSize);
                    //    }
                    //    else {
                    //        Log(WINTUN_LOG_ERR, L"send data failed");
                    //        DWORD LastError = GetLastError();
                    //        LogError(L"Packet check failed", LastError);
                    //    }
                    //}
                    //else
                    //{
                    //    Log(WINTUN_LOG_ERR, L"send data failed");
                    //    DWORD LastError = GetLastError();
                    //    LogError(L"Packet check failed", LastError);
                    //}
                }
                else if (PacketSize>1452)
                {
                    memcpy(data->data, Packet, 1452);
                    len = sendto(server_socket, (char*)data, 1472, NULL, (sockaddr*)recvAddr, sizeof(recvAddr));
                    if (len != -1) {
                        PrintPacket(Packet, PacketSize);
                        data->S_num.s_b2 += 1;
                        memcpy(data->data, Packet+1452, PacketSize-1452);
                        len = sendto(server_socket, (char*)data, PacketSize - 1452 + 20, NULL, (sockaddr*)recvAddr, sizeof(recvAddr));
                        if (len != -1) {
                            PrintPacket(Packet, PacketSize);
                            data->S_num.s_b1 += 1;
                        }
                        else
                        {
                            Log(WINTUN_LOG_ERR, L"send data failed");
                            DWORD LastError = GetLastError();
                            LogError(L"Packet check failed", LastError);
                        }
                    }
                    else
                    {
                        Log(WINTUN_LOG_ERR, L"send data failed");
                        DWORD LastError = GetLastError();
                        LogError(L"Packet check failed", LastError);
                    }
                }
                else {
                    memcpy(data->data, Packet, PacketSize);
                    len = sendto(server_socket, (char*)data, PacketSize + 20, NULL, (sockaddr*)recvAddr, sizeof(recvAddr));
                    if (len != -1) {
                        PrintPacket(Packet, PacketSize);
                        data->S_num.s_b1 += 1;
                    }
                    else
                    {
                        Log(WINTUN_LOG_ERR, L"send data failed");
                        DWORD LastError = GetLastError();
                        LogError(L"Packet check failed", LastError);
                    }
                }
            }
            else {
                Log(WINTUN_LOG_INFO,L"Packet is not udp.");
            }
            WintunReleaseReceivePacket(Session, Packet);
        }
        else
        {
            DWORD LastError = GetLastError();
            switch (LastError)
            {
            case ERROR_NO_MORE_ITEMS:
                if (WaitForMultipleObjects(_countof(WaitHandles), WaitHandles, FALSE, INFINITE) == WAIT_OBJECT_0)
                    continue;
                return ERROR_SUCCESS;
            default:
                LogError(L"recvPacket create failed", LastError);
                return LastError;
            }
        }
    }
    return ERROR_SUCCESS;
}

static DWORD WINAPI
SendPackets(_Inout_ DWORD_PTR SessionPtr)
{
    WINTUN_SESSION_HANDLE Session = (WINTUN_SESSION_HANDLE)SessionPtr;
    xibai_data* data = (xibai_data*)malloc(sizeof(xibai_data));
    if (!data)
    {
        Log(WINTUN_LOG_ERR, L"send_packet_buffer_ptr init error!");
        DWORD LastError = GetLastError();
        LogError(L"SendPackets Packet check failed", LastError);
        return LastError;
    }
    int len = 0;
    while (!HaveQuit)
    {
        len = recvfrom(server_socket, (char*)data, 1472, NULL, (sockaddr*)recvAddr, &sock_len);
        if (len == -1) {
            Log(WINTUN_LOG_ERR, L"recv data failed");
            continue;
        }
        if (len > 1472)
        {
            Log(WINTUN_LOG_WARN, L"真的有超过 1472 的包！！！");
            //BYTE* Packet = WintunAllocateSendPacket(Session, 0x10000);
            //if (Packet)
            //{
            //    //MakeICMP(Packet);
            //    memset(Packet, 0, 0x10000);
            //    memcpy(Packet, data->data, 65515);
            //    //len = recvfrom(server_socket, (char*)data, 0x10000, NULL, (sockaddr*)&recvAddr, &sock_len);
            //    //if (len == -1) {
            //    //    Log(WINTUN_LOG_ERR, L"recv data failed");
            //    //    continue;
            //    //}
            //    WintunSendPacket(Session, Packet);
            //}
            //else if (GetLastError() != ERROR_BUFFER_OVERFLOW)
            //    return LogLastError(L"Packet write failed");

            //switch (WaitForSingleObject(QuitEvent, 1000 /* 1 second */))
            //{
            //case WAIT_ABANDONED:
            //case WAIT_OBJECT_0:
            //    return ERROR_SUCCESS;
            //}
        }
        else {
            if (len == 2 && *(((char*)data)+1) == '\x00' && *(char*)data >= '0' && *(char*)data <= '9')
            {
                Log(WINTUN_LOG_INFO, L"this is a init");
                continue;
            }
            if (len == 20 && data->flag == 0)
            {
                Log(WINTUN_LOG_INFO, L"this is a heart");
                continue;
            }
            BYTE* Packet = WintunAllocateSendPacket(Session, data->len);
            if (Packet)
            {
                memset(Packet, 0, data->len);
                if (data->len > 1452)
                {
                    //MakeICMP(Packet);
                    packet_list[data->S_num.s_b1].flag += 1;
                    packet_list[data->S_num.s_b1].packet_data = Packet;
                    if (data->S_num.s_b2 == 0)
                    {
                        memcpy(Packet, data->data, 1452);
                    }
                    else
                    {
                        memcpy(Packet + 1452, data->data, data->len - 1452);
                    }
                    if (packet_list[data->S_num.s_b1].flag == 2)
                    {
                        PrintPacket(Packet, data->len);
                        WintunSendPacket(Session, Packet);
                        packet_list[data->S_num.s_b1].flag = 0;
                        packet_list[data->S_num.s_b1].packet_data = NULL;
                    }
                    //len = recvfrom(server_socket, (char*)data, 0x10000, NULL, (sockaddr*)&recvAddr, &sock_len);
                    //if (len == -1) {
                    //    Log(WINTUN_LOG_ERR, L"recv data failed");
                    //    continue;
                    //}
                }
                else {
                    //MakeICMP(Packet);
                    memcpy(Packet, data->data, data->len);
                    PrintPacket(Packet, data->len);
                    //len = recvfrom(server_socket, (char*)data, 0x10000, NULL, (sockaddr*)&recvAddr, &sock_len);
                    //if (len == -1) {
                    //    Log(WINTUN_LOG_ERR, L"recv data failed");
                    //    continue;
                    //}
                    WintunSendPacket(Session, Packet);
                }

            }
            else if (GetLastError() != ERROR_BUFFER_OVERFLOW)
                return LogLastError(L"sendPacket create failed");
            else {
                switch (WaitForSingleObject(QuitEvent, 1000 /* 1 second */))
                {
                case WAIT_ABANDONED:
                case WAIT_OBJECT_0:
                    return ERROR_SUCCESS;
                }
            }
        }

    }
    return ERROR_SUCCESS;
}

static DWORD WINAPI
Heart(_Inout_ xibai_data* sendBuff) {
    char* buff = (char*)malloc(20);

    while (!buff)
    {
        Log(WINTUN_LOG_WARN, L"HeartBuff create failed");
        buff = (char*)malloc(sizeof(xibai_data));
    }
    memcpy(buff, sendBuff, 4);
    memcpy(buff+4, "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 16);
    int len = 0;
    while (!HaveQuit)
    {
        len = sendto(server_socket, (char*)buff, 20, NULL, (sockaddr*)recvAddr + 2, sizeof(recvAddr));
        if (len == -1) {
            Log(WINTUN_LOG_ERR, L"send heart data failed");
            continue;
        }
        switch (WaitForSingleObject(QuitEvent, 2000 /* 1 second */))
        {
        case WAIT_ABANDONED:
        case WAIT_OBJECT_0:
            return ERROR_SUCCESS;
        }
    }

    return ERROR_SUCCESS;
}

void xibai_exit(int level,SOCKET server_socket, WINTUN_ADAPTER_HANDLE Adapter, HMODULE Wintun) {
    switch (level)
    {
    default:
        break;
    case 4:
        closesocket(server_socket);
    case 3:
        WSACleanup();
    case 2:
        WintunCloseAdapter(Adapter);
    case 1:
        SetConsoleCtrlHandler(CtrlHandler, FALSE);
        CloseHandle(QuitEvent);
    case 0:
        FreeLibrary(Wintun);
    }
}

int __cdecl main(int argc, char** argv)
{
    // 设置输出字符环境
    setlocale(LC_ALL, "zh_CN.UTF-8");
    if (argc != 3)
    {
        printf("usage: %s <hostname> <servicename>\n", argv[0]);
        printf("getaddrinfo provides protocol-independent translation\n");
        printf("   from an ANSI host name to an IP address\n");
        printf("   example usage:\n");
        printf("       %s www.contoso.com 0\n", argv[0]);
        return 1;
    }

    // 加载 wintun.dll api
    HMODULE Wintun = InitializeWintun();
    if (!Wintun)
        return LogError(L"Failed to initialize Wintun", GetLastError());
    WintunSetLogger(ConsoleLogger);
    Log(WINTUN_LOG_INFO, L"Wintun library loaded");

    // 创建事件处理
    DWORD LastError;
    HaveQuit = FALSE;
    QuitEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!QuitEvent)
    {
        LastError = LogError(L"Failed to create event", GetLastError());
        xibai_exit(0, NULL, NULL, Wintun);
        //FreeLibrary(Wintun);
        return LastError;
    }
    if (!SetConsoleCtrlHandler(CtrlHandler, TRUE))
    {
        LastError = LogError(L"Failed to set console handler", GetLastError());
        xibai_exit(1, NULL, NULL, Wintun);
        //SetConsoleCtrlHandler(CtrlHandler, FALSE);
        //CloseHandle(QuitEvent);
//    cleanupWintun:
        //FreeLibrary(Wintun);
        return LastError;
    }

    // 初始化虚拟网卡
    GUID ExampleGuid = { 0xdeadbabe, 0xcafe, 0xbeef, { 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef } };
    WINTUN_ADAPTER_HANDLE Adapter = WintunCreateAdapter(L"xibai-vpn", L"野蛮人6专用联机辅助", &ExampleGuid);
    if (!Adapter)
    {
        LastError = GetLastError();
        LogError(L"Failed to create adapter", LastError);
        xibai_exit(1, NULL, NULL, Wintun);
        //SetConsoleCtrlHandler(CtrlHandler, FALSE);
        //CloseHandle(QuitEvent);
//    cleanupWintun:
        //FreeLibrary(Wintun);
        return LastError;
    }

    Log(WINTUN_LOG_INFO, L"欢迎使用野蛮人6专用联机辅助");
    DWORD Version = WintunGetRunningDriverVersion();
    Log(WINTUN_LOG_INFO, L"Wintun v%u.%u loaded", (Version >> 16) & 0xff, (Version >> 0) & 0xff);

    int iResult;
    WSADATA wsaData;
    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != NO_ERROR) {
        xibai_exit(2, NULL, Adapter, Wintun);
        //WintunCloseAdapter(Adapter);
        //    cleanupQuit:
        //SetConsoleCtrlHandler(CtrlHandler, FALSE);
        //CloseHandle(QuitEvent);
        //    cleanupWintun:
        //FreeLibrary(Wintun);
        return LogError(L"WSAStartup failed with error: %d\n", iResult);
    }

    // 解析服务器域名，argv[1] hostname、argv[2] port（optional）
    Log(WINTUN_LOG_INFO, L"try resolve hostname...");
    struct addrinfo hints;
    DWORD dwRetval;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    addrinfo *server_addrinfo = NULL;
    dwRetval = getaddrinfo(argv[1], argv[2], &hints, &server_addrinfo);
    if (dwRetval)
    {
        xibai_exit(3, NULL, Adapter, Wintun);
        //WSACleanup();
        //WintunCloseAdapter(Adapter);
        //    cleanupQuit:
        //SetConsoleCtrlHandler(CtrlHandler, FALSE);
        //CloseHandle(QuitEvent);
        //    cleanupWintun:
        //FreeLibrary(Wintun);
        return LogError(L"getaddrinfo failed with error: %d\n", dwRetval);
    }
    sockaddr_in* sockaddr_ipv4 = (sockaddr_in*)(server_addrinfo->ai_addr);
    char* tmp_ip = inet_ntoa(sockaddr_ipv4->sin_addr); wchar_t* server_ip;
    int convertResult = MultiByteToWideChar(CP_UTF8, 0, tmp_ip, (int)strlen(tmp_ip), NULL, 0);
    if (convertResult <= 0)
    {
        xibai_exit(3, NULL, Adapter, Wintun);
        //WSACleanup();
        //WintunCloseAdapter(Adapter);
        //    cleanupQuit:
        //SetConsoleCtrlHandler(CtrlHandler, FALSE);
        //CloseHandle(QuitEvent);
        //    cleanupWintun:
        //FreeLibrary(Wintun);
        return LogError(L"Exception occurred: Failure to convert its message text using MultiByteToWideChar\n", GetLastError());
    }
    else
    {
        server_ip = (wchar_t*)malloc((convertResult + 1) * 2);
        if (!server_ip)
        {
            xibai_exit(3, NULL, Adapter, Wintun);
            return LogError(L"server_ip's buff create failed\n", GetLastError());
        }
        memset(server_ip, 0, (convertResult + 1) * 2);
        convertResult = MultiByteToWideChar(CP_UTF8, 0, tmp_ip, (int)strlen(tmp_ip), server_ip, convertResult);
        if (convertResult <= 0)
        {
            xibai_exit(3, NULL, Adapter, Wintun);
            //WSACleanup();
            //WintunCloseAdapter(Adapter);
            //    cleanupQuit:
            //SetConsoleCtrlHandler(CtrlHandler, FALSE);
            //CloseHandle(QuitEvent);
            //    cleanupWintun:
            //FreeLibrary(Wintun);
            return LogError(L"Exception occurred: Failure to convert its message text using MultiByteToWideChar\n", GetLastError());
        }
    }
    Log(WINTUN_LOG_INFO, L"resolve server ip: %s", server_ip);

    // 初始化客户端服务端连接，获取局域网ip
    Log(WINTUN_LOG_INFO, L"Initializing connection......");
    server_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (!server_socket){
        xibai_exit(3, NULL, Adapter, Wintun);
        //WSACleanup();
        //WintunCloseAdapter(Adapter);
        //    cleanupQuit:
        //SetConsoleCtrlHandler(CtrlHandler, FALSE);
        //CloseHandle(QuitEvent);
        //    cleanupWintun:
        //FreeLibrary(Wintun);
        return LogError(L"socket failed with error: %d\n", WSAGetLastError());
    }
    bool broadcast = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_BROADCAST, (char*)&broadcast, sizeof(bool)) < 0)
    {
        xibai_exit(4, server_socket, Adapter, Wintun);
        //WSACleanup();
        //closesocket(server_socket);
        //WintunCloseAdapter(Adapter);
        //    cleanupQuit:
        //SetConsoleCtrlHandler(CtrlHandler, FALSE);
        //CloseHandle(QuitEvent);
        //    cleanupWintun:
        //FreeLibrary(Wintun);
        return LogError(L"broadcast options failed\n", WSAGetLastError());
    }
    int timeout = 5000;
    if (setsockopt(server_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) < 0)
    {
        xibai_exit(4, server_socket, Adapter, Wintun);
        //WSACleanup();
        //closesocket(server_socket);
        //WintunCloseAdapter(Adapter);
        //    cleanupQuit:
        //SetConsoleCtrlHandler(CtrlHandler, FALSE);
        //CloseHandle(QuitEvent);
        //    cleanupWintun:
        //FreeLibrary(Wintun);
        return LogError(L"timeout options failed\n", WSAGetLastError());
    }
    Log(WINTUN_LOG_INFO, L"create conncetion's buffer");
    if (!recvBuff || !sendBuff)
    {
        xibai_exit(4, server_socket, Adapter, Wintun);
        return LogError(L"conncetion's buffer create failed with error: %d\n", WSAGetLastError());
    }
    Log(WINTUN_LOG_INFO, L"Initializing connection done");

    recvAddr[0].sin_family = AF_INET;
    recvAddr[0].sin_port = htons(50001);
    //RecvAddr.sin_addr.s_addr = inet_addr("255.255.255.254");
    recvAddr[0].sin_addr.s_addr = sockaddr_ipv4->sin_addr.S_un.S_addr;
    recvAddr[1].sin_family = AF_INET;
    recvAddr[1].sin_port = htons(50002);
    //RecvAddr.sin_addr.s_addr = inet_addr("255.255.255.254");
    recvAddr[1].sin_addr.s_addr = sockaddr_ipv4->sin_addr.S_un.S_addr;
    recvAddr[2].sin_family = AF_INET;
    recvAddr[2].sin_port = htons(50003);
    //RecvAddr.sin_addr.s_addr = inet_addr("255.255.255.254");
    recvAddr[2].sin_addr.s_addr = sockaddr_ipv4->sin_addr.S_un.S_addr;
    memcpy(sendBuff, "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00", 20);
    Log(WINTUN_LOG_INFO, L"try connect server...");
    while (-1 == sendto(server_socket, (char*)sendBuff, 20, NULL, (sockaddr*)recvAddr, sizeof(recvAddr))){
        //xibai_exit(4, server_socket, Adapter, Wintun);
        //WSACleanup();
        //closesocket(server_socket);
        //WintunCloseAdapter(Adapter);
        //    cleanupQuit:
        //SetConsoleCtrlHandler(CtrlHandler, FALSE);
        //CloseHandle(QuitEvent);
        //    cleanupWintun:
        //FreeLibrary(Wintun);
        LogError(L"sendto failed with error: %d\n", WSAGetLastError());
    }
    int count = 10;
    while (count)
    {
        int len = recvfrom(server_socket, (char*)recvBuff, 1500, NULL, (sockaddr*)recvAddr + 1, &sock_len);
        if (len != -1) {
            break;
        }
        //xibai_exit(4, server_socket, Adapter, Wintun);
        //WSACleanup();
        //closesocket(server_socket);
        //WintunCloseAdapter(Adapter);
        //    cleanupQuit:
        //SetConsoleCtrlHandler(CtrlHandler, FALSE);
        //CloseHandle(QuitEvent);
        //    cleanupWintun:
        //FreeLibrary(Wintun);
        LogError(L"recvfrom error: %d\n", WSAGetLastError()); count--;
    }
    if (!count)
    {
        xibai_exit(4, server_socket, Adapter, Wintun);
    }
    if (*(char*)recvBuff < 1 || *(char*)recvBuff > 9)  // || recvAddr.sin_port != htons(50001)
    {
        xibai_exit(4, server_socket, Adapter, Wintun);
        //WSACleanup();
        //closesocket(server_socket);
        //WintunCloseAdapter(Adapter);
        //    cleanupQuit:
        //SetConsoleCtrlHandler(CtrlHandler, FALSE);
        //CloseHandle(QuitEvent);
        //    cleanupWintun:
        //FreeLibrary(Wintun);
        Log(WINTUN_LOG_ERR, L"client num exception, please check connection number\n");
        return 0;
    }
    Log(WINTUN_LOG_INFO, L"connect server success");
    //tmp_ip[0] = sockaddr_ipv4->sin_addr.S_un.S_addr;

    Log(WINTUN_LOG_INFO, L"配置野蛮人6专用辅助……");
    MIB_UNICASTIPADDRESS_ROW AddressRow;
    InitializeUnicastIpAddressEntry(&AddressRow);
    WintunGetAdapterLUID(Adapter, &AddressRow.InterfaceLuid);
    AddressRow.Address.Ipv4.sin_family = AF_INET;
    AddressRow.Address.Ipv4.sin_addr.S_un.S_addr = htonl((192 << 24) | (168 << 16) | (222 << 8) | (*(char*)recvBuff) << 0); /* 192.168.222.client */
    sendBuff->src_target.addr.S_un.S_addr = AddressRow.Address.Ipv4.sin_addr.S_un.S_addr;
    //AddressRow.Address.Ipv4.sin_addr.S_un.S_addr = sockaddr_ipv4->sin_addr.S_un.S_addr; /* 10.6.7.7 */
    AddressRow.OnLinkPrefixLength = 24; /* This is a /24 network */
    AddressRow.DadState = IpDadStatePreferred;
    LastError = CreateUnicastIpAddressEntry(&AddressRow);
    if (LastError != ERROR_SUCCESS && LastError != ERROR_OBJECT_ALREADY_EXISTS)
    {
        xibai_exit(4, server_socket, Adapter, Wintun);
        //WSACleanup();
        //closesocket(server_socket);
        //WintunCloseAdapter(Adapter);
//    cleanupQuit:
        //SetConsoleCtrlHandler(CtrlHandler, FALSE);
        //CloseHandle(QuitEvent);
//    cleanupWintun:
        //FreeLibrary(Wintun);

        LogError(L"Failed to set IP address", LastError);
        return LastError;
    }
    while(-1 == sendto(server_socket, "success", 8, NULL, (sockaddr*)&recvAddr+1, sizeof(recvAddr))) {
        //xibai_exit(4, server_socket, Adapter, Wintun);
        //WSACleanup();
        //closesocket(server_socket);
        //WintunCloseAdapter(Adapter);
        //    cleanupQuit:
        //SetConsoleCtrlHandler(CtrlHandler, FALSE);
        //CloseHandle(QuitEvent);
        //    cleanupWintun:
        //FreeLibrary(Wintun);
        LogError(L"sendto failed with error: %d\n", WSAGetLastError());
    }
    Log(WINTUN_LOG_INFO, L"配置成功");

    // 准备接管网卡数据
    WINTUN_SESSION_HANDLE Session = WintunStartSession(Adapter, 0x400000);
    if (!Session)
    {
        xibai_exit(4, server_socket, Adapter, Wintun);
        //WSACleanup();
        //closesocket(server_socket);
        //WintunCloseAdapter(Adapter);
//    cleanupQuit:
        //SetConsoleCtrlHandler(CtrlHandler, FALSE);
        //CloseHandle(QuitEvent);
//    cleanupWintun:
        //FreeLibrary(Wintun);

        LastError = LogLastError(L"Failed to create adapter");
        return LastError;
    }

    Log(WINTUN_LOG_INFO, L"Launching threads and mangling packets...");

    HANDLE Workers[3] = { CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ReceivePackets, (LPVOID)Session, 0, NULL),
                         CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)SendPackets, (LPVOID)Session, 0, NULL),
                         CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Heart,(LPVOID)sendBuff,0,NULL) };
    if (!Workers[0] || !Workers[1])
    {
        LastError = LogError(L"Failed to create threads", GetLastError());
        goto cleanupWorkers;
    }
    WaitForMultipleObjectsEx(_countof(Workers), Workers, TRUE, INFINITE, TRUE);
    LastError = ERROR_SUCCESS;

cleanupWorkers:
    HaveQuit = TRUE;
    SetEvent(QuitEvent);
    for (size_t i = 0; i < _countof(Workers); ++i)
    {
        if (Workers[i])
        {
            WaitForSingleObject(Workers[i], INFINITE);
            CloseHandle(Workers[i]);
        }
    }
    WintunEndSession(Session);
    xibai_exit(4, server_socket, Adapter, Wintun);
    //WSACleanup();
    //closesocket(server_socket);
cleanupAdapter:
    //WintunCloseAdapter(Adapter);
cleanupQuit:
    //SetConsoleCtrlHandler(CtrlHandler, FALSE);
    //CloseHandle(QuitEvent);
cleanupWintun:
    //FreeLibrary(Wintun);
    return LastError;
}
