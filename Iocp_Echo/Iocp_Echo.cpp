#include <iostream>
#include <thread>
#include "NetLib.h"
#include "Windows.h"


int main()
{
	HANDLE thread[3];
	// 입출력 완료 포트 생성
	int WSAStartUpRetval;


	WSADATA wsaData;

	int WSAStartUpError;
	WSAStartUpRetval = ::WSAStartup(MAKEWORD(2, 2), OUT & wsaData);
	if (WSAStartUpRetval != 0)
	{
		WSAStartUpError = WSAGetLastError();
		return 0;
	}

	HANDLE hcp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (hcp == NULL)
		return 1;


	int BindRetval;
	int ListenRetval;
	int SocketError;
	int IoctlsocketError;
	int BindError;
	int ListenError;

	listenSocket = ::socket(AF_INET, SOCK_STREAM, 0);
	if (listenSocket == INVALID_SOCKET)
	{
		SocketError = WSAGetLastError();
		g_bShutdown = false;
		return 0;
	}

	int send_buf_size = 0;
	setsockopt(listenSocket, SOL_SOCKET, SO_SNDBUF, (const char*)&send_buf_size, sizeof(int));

	SOCKADDR_IN myAddress;
	myAddress.sin_family = AF_INET;
	myAddress.sin_addr.s_addr = ::htonl(INADDR_ANY);
	myAddress.sin_port = ::htons(SERVERPORT);

	BindRetval = ::bind(listenSocket, (SOCKADDR*)(&myAddress), sizeof(myAddress));
	if (BindRetval == SOCKET_ERROR)
	{
		BindError = WSAGetLastError();
		g_bShutdown = false;
		return 0;
	}

	ListenRetval = ::listen(listenSocket, SOMAXCONN_HINT(65535));
	if (ListenRetval == SOCKET_ERROR)
	{
		ListenError = WSAGetLastError();
		g_bShutdown = false;
		return 0;
	}

	thread[0] = (HANDLE)_beginthreadex(nullptr, 0, &AcceptThread, &hcp, 0, nullptr);
	thread[1] = (HANDLE)_beginthreadex(nullptr, 0, &NetworkThread, &hcp, 0, nullptr);
	//thread[2] = (HANDLE) _beginthreadex(nullptr, 0, &NetworkThread, &hcp, 0, nullptr);
	while (true)
	{
	}

	// 윈속 종료
	WSACleanup();
	return 0;
}
