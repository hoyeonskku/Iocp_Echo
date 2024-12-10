#include "CLanServer.h"
#include "SerializingBuffer.h"
#include "CLogManager.h"
#include "CProcessCpuUsage.h"
#include "CProcessorCpuUsage.h"
#include "CNetworkUsage.h"

bool CLanServer::Start(const wchar_t* IP, short port, int numOfWorkerThreads, bool nagle, int sessionMax)
{
	_port = port;
	_numOfWorkerThreads = numOfWorkerThreads;
	_nagle = nagle;
	_sessionMax = sessionMax;

	_startTime = _currentTime = timeGetTime();

	_sessionArray = new Session[sessionMax];

	for (int i = sessionMax - 1; i > -1; --i)
		_sessionIndexStack.Push(i);

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
	_monitorThread = (HANDLE)_beginthreadex(NULL, 0, MonitorThread, this, 0, nullptr);

	_networkThreads = new HANDLE[_numOfWorkerThreads];
	for (int i = 0; i < _numOfWorkerThreads; i++)
		_networkThreads[i] = (HANDLE)_beginthreadex(NULL, 0, NetworkThread, this, 0, nullptr);
	
	return true;
}

void CLanServer::Stop()
{
	while (true)
	{
		closesocket(_listenSocket);

		// ���� ���Ŵ� ���߿� �����غ�...
		for (int i = 0; i < _sessionMax; i++)
		{
			// �̹� ��Ŀ��Ʈ �Ǿ�����, cancleioex�� ȣ��Ǿ��ٴ� ����.
			if (_sessionArray[i]._sock == INVALID_SOCKET)
				continue;
			_sessionArray[i]._sock = INVALID_SOCKET;
			CancelIoEx((HANDLE)_sessionArray[i]._toBeDeletedSock, nullptr);
		}
		
		for (int i = 0; i < _numOfWorkerThreads; i++)
			PostQueuedCompletionStatus(_iocpHandle, 0, 0, 0);
		std::cout << "'q' Ű �Է�: PQCS ���� �Ϸ�" << std::endl;
		break;
	}

	// ������ ���� ���
	WaitForMultipleObjects(_numOfWorkerThreads, _networkThreads, true, INFINITE);

	// iocp �ڵ� ����
	CloseHandle(_iocpHandle);

	// ���� ����
	WSACleanup();

	return;
}

bool CLanServer::Disconnect(unsigned long long sessionID)
{
	USHORT index = static_cast<USHORT>((sessionID >> 48) & 0xFFFF);
	Session* pSession = &_sessionArray[index];
	if (pSession->_sock == INVALID_SOCKET)
		return true;

	pSession->_sock = INVALID_SOCKET;
	CancelIoEx((HANDLE)_sessionArray[index]._toBeDeletedSock, nullptr);
	return false;
}

bool CLanServer::SendPacket(unsigned long long sessionID, CPacket* pPacket)
{
	USHORT index = static_cast<USHORT>((sessionID >> 48) & 0xFFFF);
	Session* pSession = &_sessionArray[index];
	pPacket->AddRef();
	pSession->_sendBuf.Enqueue(&pPacket);
	InterlockedIncrement(&_sendMessageTPS);

	SendPost(pSession);
	return true;
}

unsigned int __stdcall CLanServer::AcceptThread(void* arg)
{
	CLanServer* server = (CLanServer*)arg;
	int IoctlsocketRetval;

	// ������ ��ſ� ����� ����
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
			{
				DebugBreak();
			}
			break;
		}

		Session* pSession = nullptr;

		int index;
		server->_sessionIndexStack.Pop(index);
		pSession = &server->_sessionArray[index];
		SessionLog log;
		log.eventType = 0;
		log.index = index;
		log.threadID = GetCurrentThreadId();
		pSession->_sessionLogQueue.enqueue(log);
		if (pSession->_invalidFlag == 1)
			DebugBreak();
		pSession->_invalidFlag = 1;
		pSession->Clear(client_sock, clientaddr, server->_sessionIDCount++, index);

		//for (int i = 0; i < 10000; i++)
		//{
		//	if (/*g_SessionArray[i]._invalidFlag == -1 &&*/ InterlockedExchange(&server->_sessionArray[i]._invalidFlag, 1) == -1)
		//	{
		//		pSession = &server->_sessionArray[i];
		//		pSession->Clear(client_sock, clientaddr, server->_sessionIDCount++, i);
		//		break;
		//	}
		//	if (i == 9999)
		//		DebugBreak();
		//}

		CreateIoCompletionPort((HANDLE)client_sock, server->_iocpHandle, (ULONG_PTR)pSession, 0);

		// accept ���� �ϳ��� io �ϰ����� ó���ؾ� ���ú갡 �� �ɸ� ������ ������ ���� �� ����.
		InterlockedIncrement(&pSession->_IOCount);

		InterlockedIncrement(&server->_sessionCount);
		server->OnAccept(pSession->_sessionID);
		server->RecvPost(pSession);

		// accept���� �����ߴٸ� ���⼭ ��������� ��.
		if (InterlockedDecrement(&pSession->_IOCount) == 0)
		{
			server->Release(pSession->_sessionID);
		}

		InterlockedIncrement(&server->_acceptCount);
		InterlockedIncrement(&server->_acceptTotal);
		InterlockedIncrement(&server->_acceptTPS);
	}
	return 0;
}

unsigned int __stdcall CLanServer::NetworkThread(void* arg)
{
	CLanServer* server = (CLanServer*)arg;
	int retval;

	while (1)
	{
		// �񵿱� ����� �Ϸ� ��ٸ���
		DWORD cbTransferred;
		Session* pSession = nullptr;
		WSAOVERLAPPED* ovl;

		retval = GetQueuedCompletionStatus(server->_iocpHandle, &cbTransferred,
			(PULONG_PTR)&pSession, &ovl, INFINITE);

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
			if (pSession->_sendCount == 0)
				DebugBreak();
			for (unsigned int i = 0; i < pSession->_sendCount; i++)
			{
				if (pSession->_sendBuf.GetUseSize() == 0)
					DebugBreak();
				CPacket* packet = nullptr;
				pSession->_sendBuf.Dequeue(&packet);
				packet->Release();
			}
			InterlockedExchange(&pSession->_sendCount, 0);
			//pSession->_sendCount = 0;

			// ���⼭ Ǯ���ִ� ������ ����� ���� ���� �� ���� �� Ǯ���ְ� �Ǹ� �� ���̿� ��ť�� �ع����� �����尡 ���� �� �־ �ƹ��� send�� ���� �ʰ� ��
			InterlockedExchange(&pSession->_sendFlag, 0);
			server->SendPost(pSession);
		}

		if (InterlockedDecrement(&pSession->_IOCount) == 0)
		{
			server->Release(pSession->_sessionID);
		}
	}
}

unsigned int __stdcall CLanServer::MonitorThread(void* arg)
{
	CLanServer* server = (CLanServer*)arg;
	CProcessCpuUsage cProcessCpuUsage;
	CProcessorCpuUsage cProcessorCpuUsage;
	CNetworkUsage cNetworkUsage;

	WCHAR text[1000];
	while (true)
	{
		server->_currentTime = timeGetTime();
		DWORD timeDiff = server->_currentTime - server->_startTime;
		cProcessorCpuUsage.UpdateProcessorCpuTime();
		cProcessCpuUsage.UpdateProcessCpuTime();
		cProcessCpuUsage.UpdateProcessMemory();
		cProcessCpuUsage.UpdateMemory();
		cNetworkUsage.UpdateNetworkUsage();

		SYSTEMTIME stTime;
		GetLocalTime(&stTime);
		swprintf_s(
			text, 1000,
			L"[%s %02d:%02d:%02d]\n\n"

			L"Connected Session : %d\n"
			L"Total Accept : %d\n"
			L"Total Disconnect : %d\n"

			L"Recv / 1sec: % d\n"
			L"Send / 1sec: % d\n"
			L"Accept / 1sec: % d\n"
			L"Disconnect / 1sec: % d\n\n"

			L"CPU Usage: % lf\n"
			L"Available Memory MBytes: % lld\n"
			L"Pool Nonpaged Bytes: % lld\n"

			L"Process CPU Usage: % lf\n"
			L"Process Private Memory Bytes: % lld\n"
			L"Process Pool Nonpaged Bytes: % lld\n"

			L"Ethernet Recv Bytes: % lf\n"
			L"Ethernet Send Bytes: % lf\n"

			L"============================================\n\n",

			L"" __DATE__, stTime.wHour, stTime.wMinute, stTime.wSecond,

			server->GetSessionCount(),
			server->GetAcceptTotal(),
			server->GetDisconnectTotal(),

			server->ResetRecvMessageTPS(),
			server->ResetSendMessageTPS(),
			server->ResetAcceptTPS(),
			server->ResetDisconnectTPS(),

			cProcessorCpuUsage.GetProcessorTotalTime(),
			cProcessCpuUsage.GetAvailableMemoryUsage(),
			cProcessCpuUsage.GetPoolNonpagedMemoryUsage(),

			cProcessCpuUsage.GetProcessTotalTime(),
			cProcessCpuUsage.GetProcessPrivateMemoryUsage(),
			cProcessCpuUsage.GetProcessPoolNonpagedMemoryUsage(),

			cNetworkUsage.GetRecvBytes(),
			cNetworkUsage.GetSendBytes()

		);

		::wprintf(L"%s", text);
		Sleep(1000);
	}
	return 0;
}

void CLanServer::ProcessRecvMessage(Session* pSession, int cbTransferred)
{
	pSession->_recvBuf.MoveRear(cbTransferred);
	while (pSession->_recvBuf.GetUseSize() >= sizeof(stHeader))
	{
		// �����ŭ ���� �� ������
		short num;
		int ret = pSession->_recvBuf.Peek((char*)&num, sizeof(short));
		short size = static_cast<short>(num);
		if (pSession->_recvBuf.GetUseSize() < size + sizeof(short))
			break;
		pSession->_recvBuf.MoveFront(sizeof(short));

		CPacket* packetData = new CPacket();
		packetData->AddRef();
		*packetData << size;

		char* writePos = packetData->GetBufferPtr() + sizeof(short);
		// ������� �������� ���� �۴ٸ�
		if (pSession->_recvBuf.DirectDequeueSize() < size)
		{
			int freeSize = pSession->_recvBuf.DirectDequeueSize();
			int remainLength = size - freeSize;
			memcpy(writePos, pSession->_recvBuf.GetFrontBufferPtr(), freeSize);
			pSession->_recvBuf.MoveFront(freeSize);

			memcpy(writePos + freeSize, pSession->_recvBuf.GetFrontBufferPtr(), remainLength);
			pSession->_recvBuf.MoveFront(remainLength);
		}
		// ����� ���� �� ������
		else
		{
			memcpy(writePos, pSession->_recvBuf.GetFrontBufferPtr(), size);
			pSession->_recvBuf.MoveFront(size);
		}
		packetData->MoveWritePos(size);
		OnRecv(pSession->_sessionID, packetData);
		InterlockedIncrement(&_recvMessageTPS);
		packetData->Release();
	}
	RecvPost(pSession);
}

bool CLanServer::RecvPost(Session* pSession)
{
	WSABUF wsabufs[2];
	unsigned int bufsNum = pSession->_recvBuf.SetRecvWsabufs(wsabufs);
	if (wsabufs[0].len + wsabufs[1].len == 0)
		DebugBreak();
	DWORD flags = 0;
	ZeroMemory(&pSession->_recvOvl, sizeof(pSession->_recvOvl));

	InterlockedIncrement(&pSession->_IOCount);
	if (pSession->_sock == INVALID_SOCKET)
		DebugBreak();
	DWORD wsaRecvRetval = WSARecv(pSession->_sock, wsabufs, bufsNum, NULL, &flags, &pSession->_recvOvl, NULL);
	if (wsaRecvRetval == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err != ERROR_IO_PENDING)
		{
			if (err == 10054) {}
			else if (err == 10053) {}
			else if (err == WSAENOTSOCK) {}
			else DebugBreak();
			InterlockedDecrement(&pSession->_IOCount);
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

	// send �̾� �Ǵ� ���� �ذ�
	// ������ send�� �Ϸ�Ǵ� �������� ù��° ������ üũ���, �ٸ� ���ú갡 send, recv����, �� send �Ϸ������� ���� �� �����÷��׸� Ǯ����
	// �� �� ù send �Ϸ������� �ι�° ������ üũ���� ����� ��, recv �Ϸ������� ��ť �ϸ� send�� �̾ƵǴ� ���� �߻� (���ú갡 ������ send�� ����)
	// �̸� �ٸ� �����尡 �÷��׸� �ٲ�ٸ� �纸���ִ� ������ �ڵ带 ¥��, ���� �������� �ߴµ� ���� �� ���ٸ�, �ٽ� ��ݺ� �ϴ� �������� �ذ�
	// ���� while ���� ���� ���ǿ��� ���ϵǾ��ٸ�, ��ť �� ���ú� �Ϸ��������� send�� �� ���̱� ������ ����
	// ���� �� ���Ŀ��� send �õ��Ϸ��� �����尡 �ִٸ� �� �����尡 �õ��� ���̱� ������ ����
	// �ݺ�
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

	WSABUF wsabufs[200];

	int bufsNum = pSession->_sendBuf.SetSendWsabufs(wsabufs);
	if (bufsNum == 0) DebugBreak();

	ZeroMemory(&pSession->_sendOvl, sizeof(pSession->_sendOvl));
	InterlockedIncrement(&pSession->_IOCount);
	if (pSession->_sock == INVALID_SOCKET)
		DebugBreak();

	// ��������, �ٷ� bufsNum�� �ٲ��൵ ������.
	int sendCount = InterlockedExchange(&pSession->_sendCount, 0);
	if (sendCount != 0) DebugBreak();
	InterlockedExchange(&pSession->_sendCount, bufsNum);

	DWORD wsaSendRetval = WSASend(pSession->_sock, wsabufs, bufsNum, NULL, 0, &pSession->_sendOvl, NULL);

	if (wsaSendRetval == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err != ERROR_IO_PENDING)
		{
			if (err == 10054) {}
			else if (err == 10053) {}
			else if (err == WSAENOTSOCK) {}
			else DebugBreak();
			InterlockedDecrement(&pSession->_IOCount);
			// ����� ��������, �����൵ ������
			pSession->_sendCount = 0;

			InterlockedExchange(&pSession->_sendFlag, 0);
			return false;
		}
	}
	return true;
}

bool CLanServer::Release(unsigned long long sessionID)
{
	USHORT index = static_cast<USHORT>((sessionID >> 48) & 0xFFFF);
	Session* pSession = &_sessionArray[index];
	int useSize = pSession->_sendBuf.GetUseSize();
	for (unsigned int i = 0; i < useSize / sizeof(void*); i++)
	{
		CPacket* packet = nullptr;
		pSession->_sendBuf.Dequeue(&packet);
		packet->Release();
	}
	if (pSession->_sendBuf.GetUseSize() != 0)
		DebugBreak();
	InterlockedDecrement(&_acceptCount);
	InterlockedIncrement(&_disconnectTotal);
	InterlockedIncrement(&_disconnectTPS);
	InterlockedDecrement(&_sessionCount);
	closesocket(_sessionArray[index]._toBeDeletedSock);
	_sessionArray[index]._invalidFlag = 0;

	SessionLog log;
	log.eventType = 14;
	log.index = index;
	log.threadID = GetCurrentThreadId();
	pSession->_sessionLogQueue.enqueue(log);

	_sessionIndexStack.Push(index);
	return true;
}

