#pragma once
#include "Windows.h"
#include "unordered_map"

#define MAX_LOG_TAG_COUNT 20
#define MAX_LOG_TAG_NAME_COUNT 20
#define MAX_LOG_BUFFER_LEN 1000

enum en_LOG_LEVEL
{
	LEVEL_DEBUG,
	LEVEL_ERROR,
	LEVEL_SYSTEM,
};

struct LogTagLock
{
	LogTagLock() {};
	// 생성자
	LogTagLock(const WCHAR* tagName)
	{
		size_t len = wcslen(tagName);
		if (len >= MAX_LOG_TAG_NAME_COUNT)
		{
			wcsncpy_s(_tagName, tagName, MAX_LOG_TAG_NAME_COUNT - 1);
			_tagName[MAX_LOG_TAG_NAME_COUNT - 1] = L'\0';
		}
		else
		{
			wcscpy_s(_tagName, tagName);
		}
	}

	// 소멸자
	~LogTagLock()
	{
		DeleteCriticalSection(&_lock);
	}

	bool ResizeBuffer();

	WCHAR _tagName[256];       // 태그 이름 저장 (최대 256자)
	CRITICAL_SECTION _lock;   // 태그별 락
	int _month = 0;
	WCHAR* _buffer;
	int _bufLen;
};

class CLogManager {
private:
	CLogManager();
	~CLogManager()
	{
		for (int i = 0; i < _logTagCount; ++i)
		{
			DeleteCriticalSection(&_logLockArray[i]._lock);
			delete[] _logLockArray[i]._buffer;
		}
	}

public:
	static CLogManager* GetInstance()
	{
		return &instance;
	};
	void SetDirectory(const WCHAR* szDir);
	void SetLogLevel(en_LOG_LEVEL level);
	void SetMonth(LogTagLock* _logTagLock);
	void Log(const WCHAR* szType, int LogLevel, const WCHAR* szStringFormat, ...);
	void LogHex(const WCHAR* szType, int LogLevel, const WCHAR* szLog, BYTE* pByte, int iByteLen);

	LogTagLock* GetLockStruct(const WCHAR* tagName)
	{
		for (int i = 0; i < MAX_LOG_TAG_COUNT; i++)
			if (wcscmp(_logLockArray[i]._tagName, tagName) == 0)
				return &_logLockArray[i];

		return nullptr;
	}

public:
	inline static int _logLevel;
private:
	static CLogManager instance;
	unsigned int _logCount = 0;
	WCHAR _dir[MAX_PATH]; // 로그 파일 경로
	LogTagLock _logLockArray[MAX_LOG_TAG_COUNT];
	unsigned int _logTagCount = 0;
};


#define _log( szType, LogLevel, szStringFormat, ... )\
if (LogLevel >= CLogManager::_logLevel)				\
    CLogManager::GetInstance()->Log(szType, LogLevel, szStringFormat, __VA_ARGS__)

#define _logHex(szType, LogLevel, szLog, pByte, iByteLen) \
if (LogLevel >= CLogManager::_logLevel)                  \
    CLogManager::GetInstance()->LogHex(szType, LogLevel, szLog, pByte, iByteLen)