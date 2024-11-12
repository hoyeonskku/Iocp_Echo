#pragma once
#include <memory.h>
#include <iostream>
#include <assert.h>

class DebugLog
{
public:
	int _prevFront;
	int _prevRear;
	int _currentFront;
	int _currentRear;
	int _moveFrontValue;
	int _moveRearValue;
};




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
	// 현재 사용중인 용량 얻기.
	//
	// Parameters: 없음.
	// Return: (int)사용중인 용량.
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
	// 현재 버퍼에 남은 용량 얻기. 
	//
	// Parameters: 없음.
	// Return: (int)남은용량.
	/////////////////////////////////////////////////////////////////////////
	inline int GetFreeSize(void)
	{
		return _capacity - 1 - GetUseSize();
	};

	inline bool IsFull()
	{
		return GetFreeSize() == 0;
	}

	/////////////////////////////////////////////////////////////////////////
	// WritePos 에 데이타 넣음.
	//
	// Parameters: (char *)데이타 포인터. (int)크기. 
	// Return: (int)넣은 크기.
	/////////////////////////////////////////////////////////////////////////
	int	Enqueue(const char* chpData, int iSize)
	{
#ifdef _DEBUG
		// 디버그 모드에서만 링버퍼 오버플로우 검증
		if (IsFull())
		{
			//_LOG(dfLOG_LEVEL_ERROR, L"RingBuffer Overflow");
			//DebugBreak();
			return 0; // 데이터 추가를 방지
		}
#endif
		/*DebugLog debugLog;
		debugLog._prevFront = _front;
		debugLog._prevRear = _rear;*/

		int freeSize = GetFreeSize();
		if (freeSize < iSize)
			iSize = freeSize; // 남은 공간에 맞게 크기 조정

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

		/*debugLog._currentFront = _front;
		debugLog._currentRear = _rear;
		debugLog._moveRearValue = iSize;
		_debugLogList.push_back(debugLog);*/
		return iSize;
	}

	/////////////////////////////////////////////////////////////////////////
	// ReadPos 에서 데이타 가져옴. ReadPos 이동.
	//
	// Parameters: (char *)데이타 포인터. (int)크기.
	// Return: (int)가져온 크기.
	/////////////////////////////////////////////////////////////////////////
	int	Dequeue(char* chpDest, int iSize)
	{
		/*DebugLog debugLog;
		debugLog._prevFront = _front;
		debugLog._prevRear = _rear;*/
		int size = GetUseSize();
		if (size < iSize)
			iSize = size; // 요청한 크기보다 실제 데이터 크기가 작을 경우

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

		/*debugLog._currentFront = _front;
		debugLog._currentRear = _rear;
		debugLog._moveFrontValue = iSize;
		_debugLogList.push_back(debugLog);*/

		return iSize;
	}

	/////////////////////////////////////////////////////////////////////////
	// ReadPos 에서 데이타 읽어옴. ReadPos 고정.
	//
	// Parameters: (char *)데이타 포인터. (int)크기.
	// Return: (int)가져온 크기.
	/////////////////////////////////////////////////////////////////////////
	int	Peek(char* chpDest, int iSize)
	{
		int size = GetUseSize();
		if (size < iSize)
			iSize = size; // 요청한 크기보다 실제 데이터 크기가 작을 경우

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
	// 버퍼의 모든 데이타 삭제.
	//
	// Parameters: 없음.
	// Return: 없음.
	/////////////////////////////////////////////////////////////////////////
	void ClearBuffer(void)
	{
		_front = 0;
		_rear = 0;
	};

public:
	/////////////////////////////////////////////////////////////////////////
	// 버퍼 포인터로 외부에서 한방에 읽고, 쓸 수 있는 길이.
	// (끊기지 않은 길이)
	//
	// 원형 큐의 구조상 버퍼의 끝단에 있는 데이터는 끝 -> 처음으로 돌아가서
	// 2번에 데이터를 얻거나 넣을 수 있음. 이 부분에서 끊어지지 않은 길이를 의미
	//
	// Parameters: 없음.
	// Return: (int)사용가능 용량.
	////////////////////////////////////////////////////////////////////////
	int	DirectEnqueueSize(void)
	{
		if (_front > _rear)
			return _front - _rear - 1;
		// 사이즈 검증을 하지 않기 때문에 front0일때의 예외처리가 필요
		if (_front == 0)
			return _capacity - _rear - 1;
		else
			return _capacity - _rear;
	}

	int DirectDequeueSize(void)
	{
		if (_front > _rear)
			return _capacity - _front;
		return _rear - _front;
	}

	/////////////////////////////////////////////////////////////////////////
	// 원하는 길이만큼 읽기위치 에서 삭제 / 쓰기 위치 이동
	//
	// Parameters: 없음.
	// Return: (int)이동크기
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
	// 버퍼의 Front 포인터 얻음.
	//
	// Parameters: 없음.
	// Return: (char *) 버퍼 포인터.
	/////////////////////////////////////////////////////////////////////////
	char* GetFrontBufferPtr(void)
	{
		return  _buffer + _front;
	}

	/////////////////////////////////////////////////////////////////////////
	// 버퍼의 RearPos 포인터 얻음.
	//
	// Parameters: 없음.
	// Return: (char *) 버퍼 포인터.
	/////////////////////////////////////////////////////////////////////////
	char* GetRearBufferPtr(void)
	{
		return _buffer + _rear;
	}
public:
	//private:
	char* _buffer;
	int _rear = 0;
	int _front = 0;
	int _capacity;

	//std::list<DebugLog> _debugLogList;
};