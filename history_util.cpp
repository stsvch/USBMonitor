#include "history_util.h"

// ��������� � ������ �������, ���� �� �� ����������
BOOL CreateDirectoryIfNotExists(const WCHAR* directoryPath)
{
    if (PathFileExistsW(directoryPath)) {
        return TRUE; // ������� ��� ����������
    }

    if (CreateDirectoryW(directoryPath, NULL)) {
        return TRUE; // ������� ������� ������
    }
    else {
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            return TRUE; // ������� ��� ������ ������ ���������
        }
        else {
            WCHAR errorMsg[256];
            swprintf_s(errorMsg, sizeof(errorMsg) / sizeof(WCHAR), L"Failed to create directory. Error: %lu", GetLastError());
            MessageBoxW(NULL, errorMsg, L"Error", MB_OK | MB_ICONERROR);
            return FALSE; // ������ �������� ��������
        }
    }
}

// ��������� ���������� � ��������� (DeviceID, ���� � INF-�����, ������ � ���� ����������) � ��������� ����
void SaveDriverHistory(const WCHAR* deviceID, const WCHAR* infPath, const WCHAR* driverVersion)
{
    FILE* file = NULL;
    errno_t err = _wfopen_s(&file, L"driver_history.log", L"a"); // ���������� �������� �����

    if (err == 0 && file != NULL) {
        time_t currentTime = time(NULL); // �������� ������� �����
        if (currentTime != -1) {
            fwprintf(file, L"DeviceID: %s\nINF Path: %s\nVersion: %s\nDate: %s\n\n",
                deviceID, infPath, driverVersion, _wctime(&currentTime));
        }
        else {
            fwprintf(file, L"DeviceID: %s\nINF Path: %s\nVersion: %s\nDate: Unknown\n\n",
                deviceID, infPath, driverVersion);
        }


        fclose(file);
    }
    else {
        MessageBoxW(NULL, L"Failed to open history file for writing.", L"Error", MB_OK | MB_ICONERROR);
    }
}

// ��������� ������� ��������� �� DeviceID � ���������� ���� � INF-�����
BOOL LoadDriverHistory(const WCHAR* deviceID, WCHAR* infPath, size_t infPathSize)
{
    FILE* file = NULL;
    errno_t err = _wfopen_s(&file, L"driver_history.log", L"r"); // ���������� �������� �����

    if (err != 0 || file == NULL) {
        MessageBoxW(NULL, L"Failed to open history file for reading.", L"Error", MB_OK | MB_ICONERROR);
        return FALSE;
    }

    WCHAR line[512];
    BOOL found = FALSE;

    while (fgetws(line, sizeof(line) / sizeof(WCHAR), file)) {
        if (wcsstr(line, deviceID)) {
            // ���� ������ � INF Path
            while (fgetws(line, sizeof(line) / sizeof(WCHAR), file)) {
                if (wcsstr(line, L"INF Path:")) {
                    swscanf_s(line, L"INF Path: %s\n", infPath, (unsigned)infPathSize);
                    found = TRUE;
                    break;
                }
            }
            break;
        }
    }

    fclose(file);
    return found;
}

/*
BackupDriverINF(L"C:\\Windows\\INF\\oem12.inf", L"C:\\Backup");

*/

// ������ ����� INF-����� �������� � ��������� ���������� ���������� �����������
void BackupDriverINF(const WCHAR* infPath, const WCHAR* backupDirectory)
{
    // ��������, ��� ������� ����������
    if (!CreateDirectoryIfNotExists(backupDirectory)) {
        return; // �� ������� ������� �������, �������
    }

    // ������ ������ ���� � ��������� �����
    WCHAR backupFilePath[MAX_PATH];
    const WCHAR* fileName = PathFindFileNameW(infPath);
    swprintf_s(backupFilePath, MAX_PATH, L"%s\\%s", backupDirectory, fileName);

    // �������� ����
    if (CopyFileW(infPath, backupFilePath, FALSE)) {
        MessageBoxW(NULL, L"Driver INF backed up successfully.", L"Success", MB_OK | MB_ICONINFORMATION);
    }
    else {
        WCHAR errorMsg[256];
        swprintf_s(errorMsg, sizeof(errorMsg) / sizeof(WCHAR), L"Failed to back up INF file. Error: %lu", GetLastError());
        MessageBoxW(NULL, errorMsg, L"Error", MB_OK | MB_ICONERROR);
    }
}

void EscapeBackslashes(WCHAR* str)
{
    WCHAR* p = str;
    while (*p)
    {
        if (*p == L'\\')
        {
            memmove(p + 1, p, (wcslen(p) + 1) * sizeof(WCHAR));
            *p = L'\\';
            p++;
        }
        p++;
    }
}