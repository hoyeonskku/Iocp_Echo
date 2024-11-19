#pragma once
#define _WINSOCKAPI_
#include <windows.h>
#include <stdexcept>

#ifndef  __PACKET__
#define  __PACKET__

class CPacket
{
public:

	enum en_PACKET
	{
		eBUFFER_DEFAULT = 1400		// 패킷의 기본 버퍼 사이즈.
	};

	// 생성자, 파괴자.
	CPacket() : _capacity(eBUFFER_DEFAULT) {}

	~CPacket() {	}

	// 패킷 청소.
	void Clear(void)
	{
		_front = 0;
		_rear = 0;
	}

	int	GetBufferSize(void) { return _capacity; }
	int GetDataSize(void) { return _rear; }
	int GetFreeSize(void) { return _capacity - _rear; };
	char* GetBufferPtr(void) { return _buffer; }
	int		MoveReadPos(int iSize)
	{
		if (iSize <= 0)
			throw std::runtime_error("Serialization buffer has been requested to move readPos minus value!");

		_front += iSize;

		if (_front > _rear)
			throw std::runtime_error("Serialization buffer has been requested to read beyond its size!");

		return iSize;
	};
	int		MoveWritePos(int iSize)
	{
		if (iSize <= 0)
			throw std::runtime_error("Serialization buffer has been requested to move readPos minus value!");

		_rear += iSize;

		if (_rear >= _capacity)
			throw std::runtime_error("Serialization buffer has been requested to write beyond its capacity!");
		return iSize;
	};

	// 연산자 오버로딩
	CPacket& operator = (CPacket& clSrcPacket)
	{
		CPacket* cpyPacket = new CPacket;
		cpyPacket->_capacity = clSrcPacket._capacity;
		cpyPacket->_rear = clSrcPacket._rear;
		cpyPacket->_front = clSrcPacket._front;
		cpyPacket->_capacity = clSrcPacket._capacity;

		memcpy(cpyPacket->_buffer, clSrcPacket._buffer, _rear);
	}

	CPacket& operator << (UCHAR byValue)
	{
		if (_capacity < _rear + sizeof(UCHAR))
			throw std::runtime_error("Serialization buffer has been requested to write beyond its capacity!");

		*reinterpret_cast<UCHAR*>(_buffer + _rear) = byValue;
		_rear += sizeof(UCHAR);

		return *this;
	}

	CPacket& operator << (char chValue)
	{
		if (_rear < _front + sizeof(char) || _capacity < _rear + sizeof(char))
			throw std::runtime_error("Serialization buffer has been requested to write beyond its capacity!");

		*reinterpret_cast<char*>(_buffer + _rear) = chValue;
		_rear += sizeof(char);

		return *this;
	}

	CPacket& operator << (short shValue)
	{
		if (_capacity < _rear + sizeof(short))
			throw std::runtime_error("Serialization buffer has been requested to write beyond its capacity!");

		*reinterpret_cast<short*>(_buffer + _rear) = shValue;
		_rear += sizeof(short);

		return *this;
	}
	CPacket& operator << (USHORT wValue)
	{
		if (_capacity < _rear + sizeof(USHORT))
			throw std::runtime_error("Serialization buffer has been requested to write beyond its capacity!");

		*reinterpret_cast<USHORT*>(_buffer + _rear) = wValue;
		_rear += sizeof(USHORT);

		return *this;
	}

	CPacket& operator << (int iValue)
	{
		if (_capacity < _rear + sizeof(int))
			throw std::runtime_error("Serialization buffer has been requested to write beyond its capacity!");

		*reinterpret_cast<int*>(_buffer + _rear) = iValue;
		_rear += sizeof(int);

		return *this;
	}

	CPacket& operator << (UINT32 iValue)
	{
		if (_capacity < _rear + sizeof(UINT32))
			throw std::runtime_error("Serialization buffer has been requested to write beyond its capacity!");

		*reinterpret_cast<UINT32*>(_buffer + _rear) = iValue;
		_rear += sizeof(UINT32);

		return *this;
	}

	CPacket& operator << (long lValue)
	{
		if (_capacity < _rear + sizeof(long))
			throw std::runtime_error("Serialization buffer has been requested to write beyond its capacity!");

		*reinterpret_cast<long*>(_buffer + _rear) = lValue;
		_rear += sizeof(long);

		return *this;
	}

	CPacket& operator << (unsigned long lValue)
	{
		if (_capacity < _rear + sizeof(unsigned long))
			throw std::runtime_error("Serialization buffer has been requested to write beyond its capacity!");

		*reinterpret_cast<unsigned long*>(_buffer + _rear) = lValue;
		_rear += sizeof(unsigned long);

		return *this;
	}

	CPacket& operator << (float fValue)
	{
		if (_capacity < _rear + sizeof(float))
			throw std::runtime_error("Serialization buffer has been requested to write beyond its capacity!");

		*reinterpret_cast<float*>(_buffer + _rear) = fValue;
		_rear += sizeof(float);

		return *this;
	}

	CPacket& operator << (__int64 iValue)
	{
		if (_capacity < _rear + sizeof(__int64))
			throw std::runtime_error("Serialization buffer has been requested to write beyond its capacity!");

		*reinterpret_cast<__int64*>(_buffer + _rear) = iValue;
		_rear += sizeof(__int64);

		return *this;
	}

	CPacket& operator << (unsigned __int64 iValue)
	{
		if (_capacity < _rear + sizeof(unsigned __int64))
			throw std::runtime_error("Serialization buffer has been requested to write beyond its capacity!");

		*reinterpret_cast<unsigned __int64*>(_buffer + _rear) = iValue;
		_rear += sizeof(unsigned __int64);

		return *this;
	}

	CPacket& operator << (double dValue)
	{
		if (_capacity < _rear + sizeof(double))
			throw std::runtime_error("Serialization buffer has been requested to write beyond its capacity!");

		*reinterpret_cast<double*>(_buffer + _rear) = dValue;
		_rear += sizeof(double);

		return *this;
	}

	CPacket& operator >> (BYTE& byValue)
	{
		if (_rear < _front + sizeof(BYTE))
			throw std::runtime_error("Serialization buffer has been requested to read beyond its size!");

		byValue = *reinterpret_cast<BYTE*>(_buffer + _front);
		_front = _front + sizeof(BYTE);
		return *this;
	}
	CPacket& operator >> (char& chValue)
	{
		if (_rear < _front + sizeof(char))
			throw std::runtime_error("Serialization buffer has been requested to read beyond its size!");

		chValue = *reinterpret_cast<char*>(_buffer + _front);
		_front = _front + sizeof(char);
		return *this;
	}

	CPacket& operator >> (short& shValue)
	{
		if (_rear < _front + sizeof(short))
			throw std::runtime_error("Serialization buffer has been requested to read beyond its size!");

		shValue = *reinterpret_cast<short*>(_buffer + _front);
		_front = _front + sizeof(short);
		return *this;
	}
	CPacket& operator >> (WORD& wValue)
	{
		if (_rear < _front + sizeof(WORD))
			throw std::runtime_error("Serialization buffer has been requested to read beyond its size!");

		wValue = *reinterpret_cast<WORD*>(_buffer + _front);
		_front = _front + sizeof(WORD);
		return *this;
	}

	CPacket& operator >> (UINT32& iValue)
	{
		if (_rear < _front + sizeof(UINT32))
			throw std::runtime_error("Serialization buffer has been requested to read beyond its size!");

		iValue = *reinterpret_cast<UINT32*>(_buffer + _front);
		_front = _front + sizeof(UINT32);
		return *this;
	}

	CPacket& operator >> (int& iValue)
	{
		if (_rear < _front + sizeof(int))
			throw std::runtime_error("Serialization buffer has been requested to read beyond its size!");
		iValue = *reinterpret_cast<int*>(_buffer + _front);
		_front = _front + sizeof(int);
		return *this;
	}

	CPacket& operator >> (DWORD& dwValue)
	{
		if (_rear < _front + sizeof(DWORD))
			throw std::runtime_error("Serialization buffer has been requested to read beyond its size!");
		dwValue = *reinterpret_cast<DWORD*>(_buffer + _front);
		_front = _front + sizeof(DWORD);
		return *this;
	}

	CPacket& operator >> (float& fValue)
	{
		if (_rear < _front + sizeof(float))
			throw std::runtime_error("Serialization buffer has been requested to read beyond its size!");
		fValue = *reinterpret_cast<float*>(_buffer + _front);
		_front = _front + sizeof(float);
		return *this;
	}

	CPacket& operator >> (__int64& iValue)
	{
		if (_rear < _front + sizeof(__int64))
			throw std::runtime_error("Serialization buffer has been requested to read beyond its size!");
		iValue = *reinterpret_cast<__int64*>(_buffer + _front);
		_front = _front + sizeof(__int64);
		return *this;
	}

	CPacket& operator >> (unsigned __int64& iValue)
	{
		if (_rear < _front + sizeof(unsigned __int64))
			throw std::runtime_error("Serialization buffer has been requested to read beyond its size!");
		iValue = *reinterpret_cast<unsigned __int64*>(_buffer + _front);
		_front = _front + sizeof(unsigned __int64);
		return *this;
	}

	CPacket& operator >> (double& dValue)
	{
		if (_rear < _front + sizeof(double))
			throw std::runtime_error("Serialization buffer has been requested to read beyond its size!");
		dValue = *reinterpret_cast<double*>(_buffer + _front);
		_front = _front + sizeof(double);
		return *this;
	}

	int		GetData(char* chpDest, int iSize)
	{
		if (_rear < _front + iSize)
			throw std::runtime_error("Serialization buffer has been requested to write beyond its capacity!");

		memcpy(chpDest, _buffer + _front, iSize);
		_front = _front + iSize;
		return iSize;
	}

	int		PutData(char* chpSrc, int iSrcSize)
	{
		if (_capacity < _rear + iSrcSize)
			throw std::runtime_error("Serialization buffer has been requested to read beyond its size!");

		memcpy(_buffer + _rear, chpSrc, iSrcSize);
		_rear = _rear + iSrcSize;
		return iSrcSize;
	}

protected:
	char _buffer[eBUFFER_DEFAULT];
	int _rear = 0;
	int _front = 0;
	int _capacity;
};

#endif