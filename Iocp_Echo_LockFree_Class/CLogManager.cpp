#include "CLogManager.h"
#include <strsafe.h>
#include <string>
#include <windows.h>

CLogManager* CLogManager::GetInstance()
{
    static CLogManager instance; // 싱글톤 인스턴스 생성
    return &instance;
}


void CLogManager::SetDirectory(const WCHAR* szDir) {
    // 디렉토리가 유효한지 검사
    DWORD attributes = GetFileAttributesW(szDir);
    if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY))
        return;

    // 디렉토리 경로 복사
    HRESULT hr = StringCchCopyW(_dir, MAX_PATH, szDir);
    if (FAILED(hr))
        return;

    // 경로 끝에 '\' 추가
    size_t len = wcslen(_dir);
    if (_dir[len - 1] != L'\\')
    {
        if (len + 1 < MAX_PATH)
        {
            _dir[len] = L'\\';
            _dir[len + 1] = L'\0';
        }
        else
            return;
    }
}

void CLogManager::SetLogLevel(en_LOG_LEVEL level)
{
    _logLevel = level;
}

void CLogManager::Log(const WCHAR* szType, int LogLevel, const WCHAR* szStringFormat, ...) {

    if (_logLevel > LogLevel)
        return;
    const auto& lock = _logLockMap.find(szType);
    if (lock == _logLockMap.end())
        InitializeCriticalSection(&_logLockMap[szType]);
    EnterCriticalSection(&_logLockMap[szType]);
    va_list args;
    va_start(args, szStringFormat);

    // 로그 메시지 버퍼
    WCHAR buffer[1024];
    HRESULT hr = StringCchVPrintfW(buffer, sizeof(buffer) / sizeof(WCHAR), szStringFormat, args);

    va_end(args);

    // 현재 시간 가져오기
    SYSTEMTIME st;
    GetLocalTime(&st);
    WCHAR timestamp[64];
    swprintf_s(timestamp, L"[%04d-%02d-%02d %02d:%02d:%02d]",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    // 로그 레벨에 해당하는 문자열 변환
    WCHAR logLevelStr[32];
    switch (LogLevel) {
    case LEVEL_DEBUG:
        wcscpy_s(logLevelStr, L"DEBUG");
        break;
    case LEVEL_ERROR:
        wcscpy_s(logLevelStr, L"ERROR");
        break;
    case LEVEL_SYSTEM:
        wcscpy_s(logLevelStr, L"SYSTEM");
        break;
    }

    // 로그 순번 생성 (예: 000000001, 000000004 등)
    WCHAR logCounterStr[16];
    swprintf_s(logCounterStr, L"%09d", InterlockedIncrement(&_logCount));

    // 최종 로그 메시지 생성
    WCHAR logMessage[2048];
    hr = StringCchPrintfW(logMessage, sizeof(logMessage) / sizeof(WCHAR),
        L"[%s] [%s / %s / %s] %s", szType, timestamp, logLevelStr, logCounterStr, buffer);

    // 디렉토리와 파일 이름 생성 (szType을 포함하여 파일 이름 지정)
    WCHAR logFilePath[MAX_PATH];
    hr = StringCchPrintfW(logFilePath, MAX_PATH, L"%s%04d%02d_%s.txt", _dir,
        st.wYear, st.wMonth, szType);

    // 파일 쓰기
    FILE* pFile = nullptr;
    if (_wfopen_s(&pFile, logFilePath, L"a, ccs=UTF-16LE") == 0 && pFile)
    {
        fwprintf(pFile, L"%s\n", logMessage);
        fclose(pFile);
    }
    LeaveCriticalSection(&_logLockMap[szType]);
}

void CLogManager::LogHex(const WCHAR* szType, int LogLevel, const WCHAR* szLog, BYTE* pByte, int iByteLen)
{
    if (_logLevel > LogLevel)
        return;
    const auto& lock = _logLockMap.find(szType);
    if (lock == _logLockMap.end())
        InitializeCriticalSection(&_logLockMap[szType]);
    EnterCriticalSection(&_logLockMap[szType]);

    // 로그 메시지 버퍼
    WCHAR buffer[1024];
    HRESULT hr = StringCchCopyW(buffer, sizeof(buffer) / sizeof(WCHAR), szLog);

    // 현재 시간 가져오기
    SYSTEMTIME st;
    GetLocalTime(&st);
    WCHAR timestamp[64];
    swprintf_s(timestamp, L"[%04d-%02d-%02d %02d:%02d:%02d]",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    WCHAR logLevelStr[32];
    switch (LogLevel) {
    case LEVEL_DEBUG:
        wcscpy_s(logLevelStr, L"DEBUG");
        break;
    case LEVEL_ERROR:
        wcscpy_s(logLevelStr, L"ERROR");
        break;
    case LEVEL_SYSTEM:
        wcscpy_s(logLevelStr, L"SYSTEM");
        break;
    }

    // 로그 순번 생성 (예: 000000001, 000000004 등)
    WCHAR logCounterStr[16];
    swprintf_s(logCounterStr, L"%09d", InterlockedIncrement(&_logCount));

    // 최종 로그 메시지 생성
    WCHAR logMessage[2048];
    hr = StringCchPrintfW(logMessage, sizeof(logMessage) / sizeof(WCHAR),
        L"[%s] [%s / %s / %s] %s", szType, timestamp, logLevelStr, logCounterStr, buffer);

    // 디렉토리와 파일 이름 생성 (szType을 포함하여 파일 이름 지정)
    WCHAR logFilePath[MAX_PATH];
    hr = StringCchPrintfW(logFilePath, MAX_PATH, L"%s%04d%02d_%s.txt", _dir, st.wYear, st.wMonth, szType);
    // 16진수 바이트 배열 로그 추가
    WCHAR hexBuffer[2048];
    int hexOffset = 0;

    for (int i = 0; i < iByteLen; i++) {
        hexOffset += swprintf_s(hexBuffer + hexOffset, sizeof(hexBuffer) / sizeof(WCHAR) - hexOffset, L"%02X ", pByte[i]);
        // 한 줄에 16바이트씩 출력
        if ((i + 1) % 16 == 0)
            hexOffset += swprintf_s(hexBuffer + hexOffset, sizeof(hexBuffer) / sizeof(WCHAR) - hexOffset, L"\n");
    }

    // 파일 쓰기
    FILE* pFile = nullptr;
    if (_wfopen_s(&pFile, logFilePath, L"a, ccs=UTF-16LE") == 0 && pFile)
    {
        fwprintf(pFile, L"%s\n", logMessage);
        fwprintf(pFile, L"\n%s", hexBuffer);
        fclose(pFile);
    }
    LeaveCriticalSection(&_logLockMap[szType]);
}