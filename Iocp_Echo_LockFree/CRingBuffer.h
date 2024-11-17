#pragma once
#include <memory.h>
#include <iostream>
#include <assert.h>

class CRingBuffer
{
public:
	CRingBuffer(int iBufferSize = 2000)
	{
		_capacity = iBufferSize + 1;
		_buffer = new char[_capacity];
	}
	~CRingBuffer(void)
	{
		delete[] _buffer;
	}


	void Resize(int size)
	{
		char* newBuffer = new char[size + 1];
		int currentSize = size;

		// Copy data from old buffer to new buffer
		if (_front < _rear)
			memcpy(newBuffer, _buffer + _front, GetUseSize());
		else
		{
			int firstPart = _capacity - _front;
			memcpy(newBuffer, _buffer + _front, firstPart);
			memcpy(newBuffer + firstPart, _buffer, _rear);
		}

		delete[] _buffer;
		_buffer = newBuffer;
		_capacity = size + 1;
		_front = 0;
		_rear = GetUseSize() % _capacity;
	}
	inline int GetBufferSize(void)
	{
		return _capacity - 1;
	};
	/////////////////////////////////////////////////////////////////////////
	// ���� ������� �뷮 ���.
	//
	// Parameters: ����.
	// Return: (int)������� �뷮.
	/////////////////////////////////////////////////////////////////////////
	inline int GetUseSize(void)
	{
		int front = _front;
		int rear = _rear;
		if (front <= rear)
			return rear - front;
		else
			return _capacity - front + rear;
	};

	///////////////////////////////////////////////////////////////////////
	// ���� ���ۿ� ���� �뷮 ���. 
	//
	// Parameters: ����.
	// Return: (int)�����뷮.
	/////////////////////////////////////////////////////////////////////////
	inline int GetFreeSize(void)
	{
		int front = _front;
		int rear = _rear;
		int useSize;
		if (front <= rear)
			useSize = rear - front;
		else
			useSize = _capacity - front + rear;
		return _capacity - 1 - useSize;
	};

	inline bool IsFull()
	{
		return GetFreeSize() == 0;
	}

	inline char* GetBufferPtr()
	{
		return _buffer;
	}

	/////////////////////////////////////////////////////////////////////////
	// WritePos �� ����Ÿ ����.
	//
	// Parameters: (char *)����Ÿ ������. (int)ũ��. 
	// Return: (int)���� ũ��.
	/////////////////////////////////////////////////////////////////////////
	int	Enqueue(const char* chpData, int iSize)
	{
#ifdef _DEBUG
		// ����� ��忡���� ������ �����÷ο� ����
		if (IsFull())
		{
			//_LOG(dfLOG_LEVEL_ERROR, L"RingBuffer Overflow");
			//DebugBreak();
			return 0; // ������ �߰��� ����
		}
#endif

		int freeSize = GetFreeSize();
		if (freeSize < iSize)
			iSize = freeSize; // ���� ������ �°� ũ�� ����

		int firstPart = _capacity - _rear;
		if (firstPart >= iSize)
		{
			memcpy(_buffer + _rear, chpData, iSize);
			_rear = (_rear + iSize) % _capacity;
		}
		else
		{
			memcpy(_buffer + _rear, chpData, firstPart);
			memcpy(_buffer, chpData + firstPart, iSize - firstPart);
			_rear = (_rear + iSize) % _capacity;
		}

		return iSize;
	}

	/////////////////////////////////////////////////////////////////////////
	// ReadPos ���� ����Ÿ ������. ReadPos �̵�.
	//
	// Parameters: (char *)����Ÿ ������. (int)ũ��.
	// Return: (int)������ ũ��.
	/////////////////////////////////////////////////////////////////////////
	int	Dequeue(char* chpDest, int iSize)
	{
		int size = GetUseSize();
		if (size < iSize)
			iSize = size; // ��û�� ũ�⺸�� ���� ������ ũ�Ⱑ ���� ���

		int firstPart = _capacity - _front;
		if (firstPart >= iSize)
		{
			memcpy(chpDest, _buffer + _front, iSize);
			_front = (_front + iSize) % _capacity;
		}
		else
		{
			memcpy(chpDest, _buffer + _front, firstPart);
			memcpy(chpDest + firstPart, _buffer, iSize - firstPart);
			_front = (_front + iSize) % _capacity;
		}


		return iSize;
	}

	/////////////////////////////////////////////////////////////////////////
	// ReadPos ���� ����Ÿ �о��. ReadPos ����.
	//
	// Parameters: (char *)����Ÿ ������. (int)ũ��.
	// Return: (int)������ ũ��.
	/////////////////////////////////////////////////////////////////////////
	int	Peek(char* chpDest, int iSize)
	{
		int size = GetUseSize();
		if (size < iSize)
			iSize = size; // ��û�� ũ�⺸�� ���� ������ ũ�Ⱑ ���� ���

		int firstPart = _capacity - _front;
		if (firstPart >= iSize)
			memcpy(chpDest, _buffer + _front, iSize);
		else
		{
			memcpy(chpDest, _buffer + _front, firstPart);
			memcpy(chpDest + firstPart, _buffer, iSize - firstPart);
		}
		return iSize;
	}
	/////////////////////////////////////////////////////////////////////////
	// ������ ��� ����Ÿ ����.
	//
	// Parameters: ����.
	// Return: ����.
	/////////////////////////////////////////////////////////////////////////
	void ClearBuffer(void)
	{
		_front = 0;
		_rear = 0;
	};

public:
	/////////////////////////////////////////////////////////////////////////
	// ���� �����ͷ� �ܺο��� �ѹ濡 �а�, �� �� �ִ� ����.
	// (������ ���� ����)
	//
	// ���� ť�� ������ ������ ���ܿ� �ִ� �����ʹ� �� -> ó������ ���ư���
	// 2���� �����͸� ��ų� ���� �� ����. �� �κп��� �������� ���� ���̸� �ǹ�
	//
	// Parameters: ����.
	// Return: (int)��밡�� �뷮.
	////////////////////////////////////////////////////////////////////////
	int	DirectEnqueueSize(void)
	{
		int front = _front;
		int rear = _rear;
		if (front > _rear)
			return front - rear - 1;
		// ������ ������ ���� �ʱ� ������ front0�϶��� ����ó���� �ʿ�
		if (front == 0)
			return _capacity - rear - 1;
		else
			return _capacity - rear;
	}

	int DirectDequeueSize(void)
	{
		int front = _front;
		int rear = _rear;
		if (front > rear)
			return _capacity - front;
		return rear - front;
	}

	/////////////////////////////////////////////////////////////////////////
	// ���ϴ� ���̸�ŭ �б���ġ ���� ���� / ���� ��ġ �̵�
	//
	// Parameters: ����.
	// Return: (int)�̵�ũ��
	/////////////////////////////////////////////////////////////////////////
	int	MoveRear(int iSize)
	{
		return _rear = (_rear + iSize) % _capacity;
	}

	int	MoveFront(int iSize)
	{
		return _front = (_front + iSize) % _capacity;
	}
	/////////////////////////////////////////////////////////////////////////
	// ������ Front ������ ����.
	//
	// Parameters: ����.
	// Return: (char *) ���� ������.
	/////////////////////////////////////////////////////////////////////////
	char* GetFrontBufferPtr(void)
	{
		return  _buffer + _front;
	}

	/////////////////////////////////////////////////////////////////////////
	// ������ RearPos ������ ����.
	//
	// Parameters: ����.
	// Return: (char *) ���� ������.
	/////////////////////////////////////////////////////////////////////////
	char* GetRearBufferPtr(void)
	{
		return _buffer + _rear;
	}

	/////////////////////////////////////////////////////////////////////////
	// 
	// WSABUF ������ send��
	// 
	/////////////////////////////////////////////////////////////////////////
	void SetRecvWsabufs(WSABUF* wsabufs)
	{

		int front = _front;
		int rear = _rear;
		int useSize;
		if (front <= rear)
			useSize = rear - front;
		else
			useSize = _capacity - front + rear;

		int freeSize = _capacity - 1 - useSize;

		wsabufs[0].buf = _buffer + rear;

		int directEnqueueSize;

		if (front > rear)
			directEnqueueSize = front - rear - 1;
		// ������ ������ ���� �ʱ� ������ front0�϶��� ����ó���� �ʿ�
		else if (_front == 0)
			directEnqueueSize = _capacity - rear - 1;
		else
			directEnqueueSize = _capacity - rear;

		wsabufs[0].len = directEnqueueSize;

		wsabufs[1].buf = _buffer;
		wsabufs[1].len = freeSize - wsabufs[0].len;
	}

	/////////////////////////////////////////////////////////////////////////
	// 
	// WSABUF ������ recv��
	// 
	/////////////////////////////////////////////////////////////////////////
	void SetSendWsabufs(WSABUF* wsabufs)
	{
		int front = _front;
		int rear = _rear;
		int useSize;
		if (front <= rear)
			useSize = rear - front;
		else
			useSize = _capacity - front + rear;

		wsabufs[0].buf = _buffer + front;

		int directDequeueSize;
		if (front > rear)
			directDequeueSize = _capacity - front;
		else
		{
			directDequeueSize = rear - front;
		}

		wsabufs[0].len = directDequeueSize;

		wsabufs[1].buf = _buffer;
		wsabufs[1].len = (useSize - wsabufs[0].len);
	}

	//private:
public:
	char* _buffer;
	int _rear = 0;
	int _front = 0;
	int _capacity;
};