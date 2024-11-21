#include "CLanServer.h"
#include "SerializingBuffer.h"

bool CLanServer::Start(const wchar_t* IP, short port, int numOfWorkerThreads, bool nagle, int sessionMax)
{

	_port = port;
	_numOfWorkerThreads = numOfWorkerThreads;
	_nagle = nagle;
	_sessionMax = sessionMax;

	_sessionArray = new Session[sessionMax];
	int WSAStartUpRetval;
	timeBeginPeriod(1);
	WSADATA wsaData;
	int WSAStartUpError;
	WSAStartUpRetval = ::WSAStartup(MAKEWORD(2, 2), OUT & wsaData);
	if (WSAStartUpRetval != 0)
	{
		WSAStartUpError = WSAGetLastError();
		return false;
	}

	_iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (_iocpHandle == NULL)
		return false;

	int BindRetval;
	int ListenRetval;
	int SocketError;
	int BindError;
	int ListenError;

	_listenSocket = ::socket(AF_INET, SOCK_STREAM, 0);
	if (_listenSocket == INVALID_SOCKET)
	{
		SocketError = WSAGetLastError();
		_bShutdown = false;
		return false;
	}

	int send_buf_size = 0;
	setsockopt(_listenSocket, SOL_SOCKET, SO_SNDBUF, (const char*)&send_buf_size, sizeof(int));

	if (!nagle)
	{
		linger linger;
		linger.l_linger = 0;
		linger.l_onoff = (USHORT)1;
		setsockopt(_listenSocket, SOL_SOCKET, SO_LINGER, (const char*)&linger, sizeof(linger));
	}


	SOCKADDR_IN myAddress;
	myAddress.sin_family = AF_INET;

	if (IP)
		InetPton(AF_INET, IP, &myAddress.sin_addr);
	else
		myAddress.sin_addr.s_addr = ::htonl(INADDR_ANY);

	myAddress.sin_port = ::htons(port);

	BindRetval = ::bind(_listenSocket, (SOCKADDR*)(&myAddress), sizeof(myAddress));
	if (BindRetval == SOCKET_ERROR)
	{
		BindError = WSAGetLastError();
		_bShutdown = false;
		return false;
	}

	ListenRetval = ::listen(_listenSocket, SOMAXCONN_HINT(65535));
	if (ListenRetval == SOCKET_ERROR)
	{
		ListenError = WSAGetLastError();
		_bShutdown = false;
		return false;
	}

	_acceptThread = (HANDLE)_beginthreadex(NULL, 0, AcceptThread, this, 0, nullptr);

	_networkThreads = new HANDLE[_numOfWorkerThreads];
	for (int i = 0; i < _numOfWorkerThreads; i++)
		_networkThreads[i] = (HANDLE)_beginthreadex(NULL, 0, NetworkThread, this, 0, nullptr);
	
	return true;
}

void CLanServer::Stop()
{
	while (true)
	{
		//if (GetAsyncKeyState('Q') & 0x8000)
		//{

		//	g_bShutdown = true;

		//	// 리슨소켓 제거하여 새로운 연결 없애기
		//	closesocket(listenSocket);

		//	//// 세션 제거는 나중에 생각해봄...
		//	//for (auto& pair : )
		//	//{
		//	//	if (pair.second->_IOCount != 0)
		//	//	{
		//	//		//CancelIoEx((HANDLE)pair.second->_sock, nullptr);
		//	//		//Release(pair.second->_sessionID);
		//	//	}
		//	//}

		//	for (int i = 0; i < 5; i++)
		//		PostQueuedCompletionStatus(hcp, 0, 0, 0);
		//	std::cout << "'q' 키 입력: PQCS 전송 완료" << std::endl;
		//	break;
		//}
	}

	// 스레드 종료 대기
	WaitForMultipleObjects(_numOfWorkerThreads, _networkThreads, true, INFINITE);

	// iocp 핸들 제거
	CloseHandle(_iocpHandle);

	// 윈속 종료
	WSACleanup();

	return;
}

bool CLanServer::Disconnect(unsigned long long sessionID)
{
	return false;
}

bool  CLanServer::SendPacket(unsigned long long sessionID, CPacket* pPacket)
{
	USHORT index = static_cast<USHORT>((sessionID >> 48) & 0xFFFF);
	short size;
	*pPacket >> size;
	UINT64 data;
	*pPacket >> data;

	_sessionArray[index]._sendBuf.Enqueue((const char*)&size, sizeof(short));
	_sessionArray[index]._sendBuf.Enqueue((const char*)&data, size);

	if (_sessionArray[index]._sendBuf.GetBufferSize() > 0)
		SendPost(&_sessionArray[index]);

	return true;
}

unsigned int __stdcall CLanServer::AcceptThread(void* arg)
{
	CLanServer* server = (CLanServer*)arg;
	int IoctlsocketRetval;

	// 데이터 통신에 사용할 변수
	SOCKET client_sock;
	SOCKADDR_IN clientaddr;
	int addrlen;
	DWORD recvbytes, flags;
	int retval;

	while (1)
	{
		// accept()
		addrlen = sizeof(clientaddr);
		client_sock = accept(server->_listenSocket, (SOCKADDR*)&clientaddr, &addrlen);
		if (client_sock == INVALID_SOCKET)
		{
			int ret = WSAGetLastError();
			if (ret != 10038 && ret != 10004)
				DebugBreak();
			break;
		}

		Session* pSession = nullptr;

		for (int i = 0; i < 10000; i++)
		{
			if (/*g_SessionArray[i]._invalidFlag == -1 &&*/ InterlockedExchange(&server->_sessionArray[i]._invalidFlag, 1) == -1)
			{
				pSession = &server->_sessionArray[i];
				pSession->Clear(client_sock, clientaddr, server->_sessionIDCount++, i);
				break;
			}
			if (i == 9999)
				DebugBreak();
		}
		server->_acceptCount++;
		server->_acceptTotal++;
		CreateIoCompletionPort((HANDLE)client_sock, server->_iocpHandle, (ULONG_PTR)pSession, 0);

		if (pSession->_sock == INVALID_SOCKET)
			DebugBreak();

		// accept 또한 하나의 io 일감으로 처리해야 리시브가 안 걸린 순간에 삭제를 막을 수 있음.
		InterlockedIncrement(&pSession->_IOCount);

		unsigned int ioCount = server->OnAccept(pSession->_sessionID);
		if (ioCount == 0)
		{
			if (pSession->_invalidFlag == -1)
				DebugBreak();
			server->Release(pSession->_sessionID);
			return 0;
		}
		ioCount = server->RecvPost(pSession);
		if (ioCount == 0)
		{
			if (pSession->_invalidFlag == -1)
				DebugBreak();
			server->Release(pSession->_sessionID);
			return 0;
		}
		InterlockedDecrement(&pSession->_IOCount);
		return 0;
	}
}

unsigned int __stdcall CLanServer::NetworkThread(void* arg)
{
	CLanServer* server = (CLanServer*)arg;
	int retval;

	while (1)
	{
		// 비동기 입출력 완료 기다리기
		DWORD cbTransferred;
		SOCKET* client_sock;
		Session* pSession;
		WSAOVERLAPPED* ovl;
		WSABUF wsabuf;

		retval = GetQueuedCompletionStatus(server->_iocpHandle, &cbTransferred,
			(PULONG_PTR)&pSession, &ovl, INFINITE);

		int a = GetLastError();

		if (pSession->_sock == INVALID_SOCKET)
			DebugBreak();

		if (cbTransferred == 0 && pSession == 0 && ovl == 0)
			return 0;

		if (cbTransferred == 0)
		{

		}

		else if (&pSession->_recvOvl == ovl)
		{
			server->ProcessRecvMessage(pSession, cbTransferred);
		}
		else if (&pSession->_sendOvl == ovl)
		{
			pSession->_sendBuf.MoveFront(cbTransferred);
			InterlockedExchange(&pSession->_sendFlag, 0);

			if (pSession->_sendBuf.GetBufferSize() > 0)
				server->SendPost(pSession);
		}

		if (InterlockedDecrement(&pSession->_IOCount) == 0)
		{
			server->Release(pSession->_sessionID);
		}
	}
	return 0;
}

void CLanServer::ProcessRecvMessage(Session* pSession, int cbTransferred)
{
	pSession->_recvBuf.MoveRear(cbTransferred);
	while (pSession->_recvBuf.GetUseSize() >= sizeof(stHeader))
	{
		// 헤더만큼 읽을 수 있으면
		short num;
		int ret = pSession->_recvBuf.Peek((char*)&num, sizeof(short));
		short size = static_cast<short>(num);
		if (pSession->_recvBuf.GetUseSize() < size + sizeof(short))
			break;
		pSession->_recvBuf.MoveFront(sizeof(short));

		CPacket packetData;
		packetData << size;

		char* writePos = packetData.GetBufferPtr() + sizeof(short);
		// 사이즈보다 경계까지의 값이 작다면
		if (pSession->_recvBuf.DirectDequeueSize() < size)
		{
			int freeSize = pSession->_recvBuf.DirectDequeueSize();
			int remainLength = size - freeSize;
			memcpy(writePos, pSession->_recvBuf.GetFrontBufferPtr(), freeSize);
			pSession->_recvBuf.MoveFront(freeSize);

			memcpy(writePos + freeSize, pSession->_recvBuf.GetFrontBufferPtr(), remainLength);
			pSession->_recvBuf.MoveFront(remainLength);
		}
		// 충분히 읽을 수 있으면
		else
		{
			memcpy(writePos, pSession->_recvBuf.GetFrontBufferPtr(), size);
			pSession->_recvBuf.MoveFront(size);
		}
		packetData.MoveWritePos(size);
		OnRecv(pSession->_sessionID, &packetData);
	}

	RecvPost(pSession);
}

bool CLanServer::RecvPost(Session* pSession)
{
	WSABUF wsabufs[2];
	pSession->_recvBuf.SetRecvWsabufs(wsabufs);

	DWORD flags = 0;
	ZeroMemory(&pSession->_recvOvl, sizeof(pSession->_recvOvl));

	InterlockedIncrement(&pSession->_IOCount);
	//pSession->_queue.enqueue({ pSession->_sock, pSession->_IOCount, EventType::RECV, __LINE__ });
	if (pSession->_sock == INVALID_SOCKET)
		DebugBreak();
	DWORD wsaRecvRetval = WSARecv(pSession->_sock, wsabufs, 2, NULL, &flags, &pSession->_recvOvl, NULL);	
	// For monitor
	_recvMessageCount++;
	_recvMessageTPS += wsabufs[0].len + wsabufs[1].len;
	if (wsaRecvRetval == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err != ERROR_IO_PENDING)
		{
			if (err == 10054) {}
			else if (err == 10053) {}
			//else DebugBreak();
			InterlockedDecrement(&pSession->_IOCount);
			//pSession->_queue.enqueue({ pSession->_sock,pSession->_IOCount, EventType::RECVFAIL, __LINE__ });
			return false;
		}
	}
	return true;
}

bool CLanServer::SendPost(Session* pSession)
{
	if (pSession->_sendBuf.GetUseSize() == 0)
		return false;
	

	if (InterlockedExchange(&pSession->_sendFlag, 1) == 1)
		return false;
	

	// send 미아 되는 버그 해결
	// 과정은 send가 완료되는 시점에서 첫번째 사이즈 체크통과, 다른 리시브가 send, recv진행, 그 send 완료통지가 왔을 때 샌드플래그를 풀어줌
	// 그 후 첫 send 완료통지가 두번째 사이즈 체크까지 통과한 후, recv 완료통지가 인큐 하면 send가 미아되는 현상 발생 (리시브가 없으면 send가 없음)
	// 이를 다른 스레드가 플래그를 바꿨다면 양보해주는 식으로 코드를 짜되, 내가 보내려고 했는데 보낼 게 없다면, 다시 재반복 하는 형식으로 해결
	// 만약 while 루프 안의 세션에서 리턴되었다면, 인큐 할 리시브 완료통지에서 send를 할 것이기 때문에 리턴
	// 만약 그 이후에도 send 시도하려는 스레드가 있다면 그 스레드가 시도할 것이기 때문에 리턴
	// 반복
	if (pSession->_sendBuf.GetUseSize() == 0)
	{
		InterlockedExchange(&pSession->_sendFlag, 0);
		while (true)
		{
			if (pSession->_sendBuf.GetUseSize() == 0)
				return false;
			
			if (InterlockedExchange(&pSession->_sendFlag, 1) == 1)
				return false;
			
			if (pSession->_sendBuf.GetUseSize() == 0)
			{
				InterlockedExchange(&pSession->_sendFlag, 0);
				continue;
			}
			break;
		}
	}

	WSABUF wsabufs[2];
	pSession->_sendBuf.SetSendWsabufs(wsabufs);

	ZeroMemory(&pSession->_sendOvl, sizeof(pSession->_sendOvl));
	InterlockedIncrement(&pSession->_IOCount);
	//pSession->_queue.enqueue({ pSession->_sock,pSession->_IOCount, EventType::SEND, __LINE__ });
	if (pSession->_sock == INVALID_SOCKET)
		DebugBreak();
	DWORD wsaSendRetval = WSASend(pSession->_sock, wsabufs, 2, NULL, 0, &pSession->_sendOvl, NULL);
	// For monitor
	_sendMessageCount++;
	_sendMessageTPS += wsabufs[0].len + wsabufs[1].len;
	if (wsaSendRetval == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err != ERROR_IO_PENDING)
		{
			if (err == 10054) {}
			else if (err == 10053) {}
			else DebugBreak();
			InterlockedExchange(&pSession->_sendFlag, 0);
			InterlockedDecrement(&pSession->_IOCount);
			//pSession->_queue.enqueue({ pSession->_sock, pSession->_IOCount, EventType::SENDFAIL, __LINE__ });
			return;
		}
	}
}

bool CLanServer::Release(unsigned long long sessionID)
{
	USHORT index = static_cast<USHORT>((sessionID >> 48) & 0xFFFF);
	Session* session = &_sessionArray[index];
	_acceptCount--;
	closesocket(_sessionArray[index]._sock);
	_sessionArray[index]._sock = INVALID_SOCKET;
	InterlockedExchange(&_sessionArray[index]._invalidFlag, -1);
	return true;
}
