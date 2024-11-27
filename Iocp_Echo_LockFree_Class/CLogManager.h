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
    ~CLogManager() {  }

public:
	CLogManager* GetInstance();
	void SetDirectory(WCHAR* szDir);
	void SetLogLevel(en_LOG_LEVEL level);
	void Log(WCHAR* szType, en_LOG_LEVEL LogLevel, WCHAR* szStringFormat, ...);
	void LogHex(WCHAR* szType, en_LOG_LEVEL LogLevel, WCHAR* szLog, BYTE* pByte, int iByteLen);

private:
	en_LOG_LEVEL _logLevel;
	unsigned int _logCount;
	WCHAR _dir[MAX_PATH]; // 로그 파일 경로
	std::unordered_map<WCHAR*, CRITICAL_SECTION> _logLockMap;
};

