#include <iostream>
#include <thread>
#include "NetLib.h"
#include "Windows.h"
#include "Session.h"
#include "unordered_map"



int main()
{
	HANDLE thread[5];
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
	thread[2] = (HANDLE) _beginthreadex(nullptr, 0, &NetworkThread, &hcp, 0, nullptr);
	thread[3] = (HANDLE) _beginthreadex(nullptr, 0, &NetworkThread, &hcp, 0, nullptr);
	thread[4] = (HANDLE) _beginthreadex(nullptr, 0, &NetworkThread, &hcp, 0, nullptr);

	for (auto& pair : g_sessionMap)
	{
		if (pair.second->_IOCount != 0)
		{
			CancelIo((HANDLE)pair.second->_sock);
		}
	}


	while (true)
	{
		if (GetAsyncKeyState('Q') & 0x8000) {
			// PQCS 구조체 동적 할당

			// IOCP에 완료 패킷 전송
			for (int i = 0 ; i < 5; i++)
				PostQueuedCompletionStatus(hcp, 0, 0, 0);
			std::cout << "'q' 키 입력: PQCS 전송 완료" << std::endl;
			break;
		}
	}

	g_bShutdown = true;
	closesocket(listenSocket);

	WaitForMultipleObjects(5, thread, true, INFINITE);

	for (auto& pair : g_sessionMap)
	{
		delete pair.second;
	}

	// 윈속 종료
	WSACleanup();
	return 0;
}
