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
	
	linger linger;
	linger.l_linger = 0;
	linger.l_onoff = (USHORT) 1;
	setsockopt(listenSocket, SOL_SOCKET, SO_LINGER, (const char*)&linger, sizeof(linger));


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



	while (true)
	{
		if (GetAsyncKeyState('Q') & 0x8000) 
		{

			g_bShutdown = true;

			// 리슨소켓 제거하여 새로운 연결 없애기
			closesocket(listenSocket);

			// 세션 제거는 나중에 생각해봄...
			for (auto& pair : g_sessionMap)
			{
				if (pair.second->_IOCount != 0)
				{
					//CancelIoEx((HANDLE)pair.second->_sock, nullptr);
					//Release(pair.second->_sessionID);
				}
			}
			LeaveCriticalSection(&g_sessionMapCs);


			for (int i = 0 ; i < 5; i++)
				PostQueuedCompletionStatus(hcp, 0, 0, 0);
			std::cout << "'q' 키 입력: PQCS 전송 완료" << std::endl;
			break;
		}
	}

	// 스레드 종료 대기
	WaitForMultipleObjects(5, thread, true, INFINITE);

	// iocp 핸들 제거
	CloseHandle(hcp);

	// 윈속 종료
	WSACleanup();
	return 0;
}
