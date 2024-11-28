#pragma once
#include "Windows.h"
#include "unordered_map"

enum en_LOG_LEVEL
{
	LEVEL_DEBUG,
	LEVEL_ERROR,
	LEVEL_SYSTEM,
};

class CLogManager {
private:
	CLogManager() {}
	~CLogManager()
	{
		for (auto& it : _logLockMap)
			DeleteCriticalSection(&it.second);
	}

public:
	static CLogManager* GetInstance();
	void SetDirectory(const WCHAR* szDir);
	void SetLogLevel(en_LOG_LEVEL level);
	void Log(const WCHAR* szType, int LogLevel, const WCHAR* szStringFormat, ...);
	void LogHex(const WCHAR* szType, int LogLevel, const WCHAR* szLog, BYTE* pByte, int iByteLen);

private:
	int _logLevel;
	unsigned int _logCount;
	WCHAR _dir[MAX_PATH]; // 로그 파일 경로
	std::unordered_map<const WCHAR*, CRITICAL_SECTION> _logLockMap;
	CRITICAL_SECTION _mapLock;
};

