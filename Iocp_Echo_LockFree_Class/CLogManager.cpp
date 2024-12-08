#include "CLogManager.h"
#include <strsafe.h>
#include <string>
#include <windows.h>

CLogManager::CLogManager()
{
    // ���Ϸ� �о������ ���� ����
    // �ݺ������� ���� ����
    const WCHAR* arr[MAX_LOG_TAG_COUNT] = { L"Battle" , L"TestType" , L"Network" ,L"System" };
    SetDirectory(L"C:\\Logs\\");

    // ���� ��¥�� ���� �� ���� ��� ����
    SYSTEMTIME st;
    GetLocalTime(&st);

    for (int i = 0; i < 4; i++)
    {
        wcscpy_s(_logLockArray[_logTagCount]._tagName, arr[i]);
        _logLockArray[_logTagCount]._bufLen = MAX_LOG_BUFFER_LEN;
        _logLockArray[_logTagCount]._buffer = new WCHAR[_logLockArray[_logTagCount]._bufLen];
        InitializeCriticalSection(&_logLockArray[_logTagCount++]._lock);
    }
}

// ������ ������ ���� �ʱ� ������ �Ѱ������� ȣ���ؾ���.
void CLogManager::SetDirectory(const WCHAR* szDir)
{
    HRESULT hr = StringCchCopyW(_dir, MAX_PATH, szDir);
    if (FAILED(hr))
        return;

    // ��� ���� '\' �߰�
    size_t len = wcslen(_dir);
    if (_dir[len - 1] != L'\\' && len + 1 < MAX_PATH)
    {
        _dir[len] = L'\\';
        _dir[len + 1] = L'\0';
    }

    // ���丮�� ������ ����
    if (GetFileAttributesW(_dir) == INVALID_FILE_ATTRIBUTES)
    {
        CreateDirectoryW(_dir, NULL);
    }

    // ���� ��¥�� ���� �� ���� ��� ����
    SYSTEMTIME st;
    GetLocalTime(&st);

    WCHAR monthDir[MAX_PATH];
    HRESULT hr2 = StringCchPrintfW(monthDir, MAX_PATH, L"%s%04d_%02d", _dir, st.wYear, st.wMonth);
    if (FAILED(hr2))
        return;

    // �� ������ ������ ����
    if (GetFileAttributesW(monthDir) == INVALID_FILE_ATTRIBUTES)
    {
        CreateDirectoryW(monthDir, NULL);
    }
}


// �ǵ��� �ѹ��� �����ϵ��� ����
void CLogManager::SetLogLevel(en_LOG_LEVEL level)
{
    _logLevel = level;
}

void CLogManager::SetMonth(LogTagLock* _logTagLock)
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    _logTagLock->_month = st.wMonth;

    WCHAR logFilePath[MAX_PATH];
    StringCchPrintfW(logFilePath, MAX_PATH, L"%s\\%04d_%02d\\%04d%02d_%s.txt", _dir,
        st.wYear, st.wMonth, st.wYear, st.wMonth, _logTagLock->_tagName);

    HANDLE handle = CreateFileW(logFilePath, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle == INVALID_HANDLE_VALUE)
        return;

    DWORD fileSize = GetFileSize(handle, NULL);

    if (fileSize == 0)
    {
        DWORD bytesWritten;
        const WCHAR BOM = 0xFEFF;
        WriteFile(handle, &BOM, sizeof(WCHAR), &bytesWritten, NULL);
    }

    CloseHandle(handle);
}

void CLogManager::Log(const WCHAR* szType, int LogLevel, const WCHAR* szStringFormat, ...)
{
    if (_logLevel > LogLevel)
        return;

    LogTagLock* lockStruct = GetLockStruct(szType);

    // ���� �ð� ��������
    SYSTEMTIME st;
    GetLocalTime(&st);

    if (st.wMonth != lockStruct->_month)
        SetMonth(lockStruct);

    EnterCriticalSection(&lockStruct->_lock);
    // ���� �α� �޽��� ����
    WCHAR* logMessage;
    int bufSize;
    int headerLen;
    DWORD bytesToWrite;
    do
    {
        bufSize = lockStruct->_bufLen;
        logMessage = lockStruct->_buffer;
        switch (LogLevel)
        {
        case LEVEL_DEBUG:
            headerLen = _snwprintf_s(logMessage, bufSize, _TRUNCATE,
                L"[[%04d-%02d-%02d %02d:%02d:%02d] / %s / %09d] ", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, L"DBG", InterlockedIncrement(&_logCount));
            break;
        case LEVEL_ERROR:
            headerLen = _snwprintf_s(logMessage, bufSize, _TRUNCATE,
                L"[[%04d-%02d-%02d %02d:%02d:%02d] / %s / %09d] ", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, L"ERR", InterlockedIncrement(&_logCount));
            break;
        case LEVEL_SYSTEM:
            headerLen = _snwprintf_s(logMessage, bufSize, _TRUNCATE,
                L"[[%04d-%02d-%02d %02d:%02d:%02d] / %s / %09d] ", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, L"SYS", InterlockedIncrement(&_logCount));
            break;
        }
        if (headerLen == -1 || headerLen >= bufSize)
        {
            lockStruct->ResizeBuffer();
            continue;
        }

        va_list args;
        va_start(args, szStringFormat);
        int len = _vsnwprintf_s(logMessage + headerLen, bufSize - headerLen, bufSize - headerLen - 1, szStringFormat, args);

        va_end(args);

        bytesToWrite = headerLen + len;

        if (len == -1 || bytesToWrite + 2 >= bufSize)
            lockStruct->ResizeBuffer();
        else
            break;
    } while (true);


    logMessage[bytesToWrite] = L'\n';
    logMessage[bytesToWrite + 1] = L'\0';


    // ���丮�� ���� �̸� ���� (szType�� �����Ͽ� ���� �̸� ����)
    WCHAR logFilePath[MAX_PATH];
    HRESULT hr = StringCchPrintfW(logFilePath, MAX_PATH, L"%s\\%04d_%02d\\%04d%02d_%s.txt", _dir,
        st.wYear, st.wMonth, st.wYear, st.wMonth, szType);

    // ���� ����
    DWORD bytesWritten;
    HANDLE handle = CreateFileW(logFilePath, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    SetFilePointer(handle, 0, NULL, FILE_END);
    DWORD fileSize = GetFileSize(handle, NULL);

    WriteFile(handle, logMessage, (bytesToWrite + 1) * sizeof(WCHAR), &bytesWritten, NULL);
    CloseHandle(handle);

    LeaveCriticalSection(&lockStruct->_lock);
}

void CLogManager::LogHex(const WCHAR* szType, int LogLevel, const WCHAR* szLog, BYTE* pByte, int iByteLen)
{
    if (_logLevel > LogLevel)
        return;
    LogTagLock* lockStruct = GetLockStruct(szType);
    EnterCriticalSection(&lockStruct->_lock);

    // ���� �ð� ��������
    SYSTEMTIME st;
    GetLocalTime(&st);

    if (st.wMonth != lockStruct->_month)
        SetMonth(lockStruct);
    // ���� �α� �޽��� ����
    int headerLen;
    int bufSize;
    WCHAR* logMessage;
    DWORD bytesToWrite;
    int hexOffset = 0;
    do
    {
        bufSize = lockStruct->_bufLen;
        logMessage = lockStruct->_buffer;
        switch (LogLevel)
        {
        case LEVEL_DEBUG:
            headerLen = _snwprintf_s(logMessage, bufSize, bufSize - 1,
                L"[[%04d-%02d-%02d %02d:%02d:%02d] / %s / %09d] ", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, L"DBG", InterlockedIncrement(&_logCount));
            break;
        case LEVEL_ERROR:
            headerLen = _snwprintf_s(logMessage, bufSize, bufSize - 1,
                L"[[%04d-%02d-%02d %02d:%02d:%02d] / %s / %09d] ", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, L"ERR", InterlockedIncrement(&_logCount));
            break;
        case LEVEL_SYSTEM:
            headerLen = _snwprintf_s(logMessage, bufSize, bufSize - 1,
                L"[[%04d-%02d-%02d %02d:%02d:%02d] / %s / %09d] ", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, L"SYS", InterlockedIncrement(&_logCount));
            break;
        }
        if (headerLen == -1 || lockStruct->_bufLen < iByteLen * 16 + headerLen)
        {
            lockStruct->ResizeBuffer();
            continue;
        }

        int len = _snwprintf_s((logMessage + headerLen), bufSize - headerLen, bufSize - headerLen - 1, L"%s\n", szLog);
        if (len == -1 || lockStruct->_bufLen < iByteLen * 16 + headerLen + len)
        {
            lockStruct->ResizeBuffer();
            continue;
        }
        bytesToWrite = headerLen + len;
        break;
    } while (true);


    // 16���� ����Ʈ �迭 �α� �߰�
    WCHAR hexBuffer[2048];

    for (int i = 0; i < (iByteLen / 16) * 16; i += 16)
    {
        hexOffset += _snwprintf_s(logMessage + bytesToWrite + hexOffset, bufSize - bytesToWrite - hexOffset, bufSize - bytesToWrite - hexOffset - 1,
            L"%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
            pByte[i], pByte[i + 1], pByte[i + 2], pByte[i + 3], pByte[i + 4], pByte[i + 5], pByte[i + 6], pByte[i + 7],
            pByte[i + 8], pByte[i + 9], pByte[i + 10], pByte[i + 11], pByte[i + 12], pByte[i + 13], pByte[i + 14], pByte[i + 15]);
    }

    for (int i = iByteLen - iByteLen % 16; i < iByteLen; i++)
    {
        hexOffset += _snwprintf_s(logMessage + bytesToWrite + hexOffset, bufSize - bytesToWrite - hexOffset, bufSize - bytesToWrite - hexOffset - 1, L"%02X ", pByte[i]);
        // �� �ٿ� 16����Ʈ�� ���
        if (i == iByteLen - 1)
            hexOffset += _snwprintf_s(logMessage + bytesToWrite + hexOffset, bufSize - bytesToWrite - hexOffset, bufSize - bytesToWrite - hexOffset - 1, L"\n");
    }
    // ���丮�� ���� �̸� ���� (szType�� �����Ͽ� ���� �̸� ����)
    WCHAR logFilePath[MAX_PATH];
    HRESULT hr = StringCchPrintfW(logFilePath, MAX_PATH, L"%s\\%04d_%02d\\%04d%02d_%s.txt", _dir,
        st.wYear, st.wMonth, st.wYear, st.wMonth, szType);

    // ���� ����
    HANDLE handle = CreateFileW(logFilePath, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    SetFilePointer(handle, 0, NULL, FILE_END);
    DWORD fileSize = GetFileSize(handle, NULL);

    DWORD bytesWritten;
    WriteFile(handle, logMessage, (bytesToWrite + hexOffset) * sizeof(WCHAR), &bytesWritten, NULL);
    CloseHandle(handle);

    LeaveCriticalSection(&lockStruct->_lock);
}

bool LogTagLock::ResizeBuffer()
{
    WCHAR* newBuffer = new WCHAR[_bufLen * 2];
    wmemcpy(newBuffer, _buffer, _bufLen);
    delete[] _buffer;
    _buffer = newBuffer;
    int newbufLen = _bufLen * 2;
    if (newbufLen < _bufLen)
    {
        //_log(_tagName, LEVEL_SYSTEM, L"���� ũ�⸦ ������ �� �����ϴ�");
        return false;
    }
    _bufLen = newbufLen;
    _log(_tagName, LEVEL_SYSTEM, L"���� ũ�� ����, ���� ũ�� : %d", _bufLen);
    return true;
}
