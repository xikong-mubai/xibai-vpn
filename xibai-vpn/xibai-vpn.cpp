// xibai-vpn.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <Ws2tcpip.h>
#include <stdio.h>
#include "wintun.h"

// Link with ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_IP (char*)"127.0.0.1"
#define MAX_PATH 260

// 获得本机的IP地址
char* GetLocalIP()
{
	// 获得本机主机名
	char hostname[MAX_PATH] = { 0 };
	gethostname(hostname, MAX_PATH);
	struct hostent FAR* lpHostEnt = gethostbyname(hostname);
	if (lpHostEnt == NULL)
	{
		return DEFAULT_IP;
	}

	// 取得IP地址列表中的第一个为返回的IP(因为一台主机可能会绑定多个IP)
	LPSTR lpAddr = lpHostEnt->h_addr_list[0];
	printf("%d\n", lpHostEnt->h_length);
	// 将IP地址转化成字符串形式
	struct in_addr inAddr;
	memmove(&inAddr, lpAddr, 4);

	return inet_ntoa(inAddr);
}

int tttmain() {

	int iResult;
	WSADATA wsaData;
	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != NO_ERROR) {
		wprintf(L"WSAStartup failed with error: %d\n", iResult);
		return 1;
	}


	SOCKET s;// = INVALID_SOCKET;
	s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == INVALID_SOCKET) {
		wprintf(L"socket failed with error: %ld\n", WSAGetLastError());
		WSACleanup();
		return 1;
	}

	const char* buf = "Hello World!\n";

	hostent *hostinfo = gethostbyname("HostName");
	unsigned long *serverip = (unsigned long *)(hostinfo->h_addr_list[0]);

	sockaddr_in RecvAddr;
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(60001);
	//RecvAddr.sin_addr.s_addr = inet_addr("255.255.255.254");
	RecvAddr.sin_addr.s_addr = *serverip;
	char* tmp_ip = GetLocalIP();
	printf("%s\n", tmp_ip);
	//RecvAddr.sin_addr.s_addr = inet_addr(tmp_ip);

	//sockaddr *to = (sockaddr*)malloc(sizeof(sockaddr));
	//char broadcast = 'a';
	bool broadcast = 1;
	if (setsockopt(s, SOL_SOCKET, SO_BROADCAST, (char*)&broadcast, sizeof(bool)) < 0)
	{
		perror("broadcast options");
		closesocket(s);
		return 1;
	}

	sockaddr_in target_addr = { 0 };
	int len = sizeof(target_addr);
	char* buffer = (char*)malloc(1024);
	iResult = sendto(s, buf, 14, 0, (sockaddr*)&RecvAddr, sizeof(RecvAddr));
	if (iResult == SOCKET_ERROR) {
		wprintf(L"sendto failed with error: %d\n", WSAGetLastError());
		closesocket(s);
		WSACleanup();
		return 1;
	}
	recvfrom(s, buffer, 1024, 0, (sockaddr*)&target_addr, &len);
	printf("%s => %s\n", inet_ntoa(target_addr.sin_addr), buffer);


	return 0;
}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门使用技巧: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件
