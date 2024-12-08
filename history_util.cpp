#include "history_util.h"

BOOL CreateDirectoryIfNotExists(const WCHAR* directoryPath)
{
    if (PathFileExistsW(directoryPath)) 
    {
        return TRUE; 
    }

    if (CreateDirectoryW(directoryPath, NULL))
    {
        return TRUE; 
    }
    else
    {
        if (GetLastError() == ERROR_ALREADY_EXISTS)
        {
            return TRUE;
        }
        else
        {
            WCHAR errorMsg[256];
            swprintf_s(errorMsg, sizeof(errorMsg) / sizeof(WCHAR), L"Failed to create directory. Error: %lu", GetLastError());
            MessageBoxW(NULL, errorMsg, L"Error", MB_OK | MB_ICONERROR);
            return FALSE; 
        }
    }
}

void SaveDriverHistory(const WCHAR* deviceID, const WCHAR* infPath) 
{
    FILE* file = NULL;

    errno_t err = _wfopen_s(&file, L"driver_history.log", L"a"); 

    if (file == NULL) 
    {
        MessageBoxW(NULL, L"Failed to open history file for writing.", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    time_t currentTime = time(NULL);
    if (currentTime != -1) 
    {
        fwprintf(file, L"DeviceID: %s\nINF Path: %s\nDate: %s\n\n",
            deviceID, infPath, _wctime(&currentTime));
    }
    else 
    {
        fwprintf(file, L"DeviceID: %s\nINF Path: %s\nDate: Unknown\n\n",
            deviceID, infPath);
    }

    fclose(file);
}

BOOL LoadDriverHistory(const WCHAR* deviceID, WCHAR* infPath, size_t infPathSize) 
{
    FILE* file = NULL;
    errno_t err = _wfopen_s(&file, L"driver_history.log", L"r"); 

    if (err != 0 || file == NULL)
    {
        MessageBoxW(NULL, L"Failed to open history file for reading.", L"Error", MB_OK | MB_ICONERROR);
        return FALSE;
    }

    WCHAR line[512];
    WCHAR previousInfPath[512] = L"";
    WCHAR currentInfPath[512] = L""; 
    BOOL found = FALSE;

    while (fgetws(line, sizeof(line) / sizeof(WCHAR), file))
    {
        if (wcsstr(line, L"DeviceID:") && wcsstr(line, deviceID)) 
        {
            wcscpy_s(previousInfPath, currentInfPath);
            currentInfPath[0] = L'\0';
            found = FALSE; 
        }

        if (wcsstr(line, L"INF Path:")) 
        {
            swscanf_s(line, L"INF Path: %s\n", currentInfPath, (unsigned)sizeof(currentInfPath) / sizeof(WCHAR));
            found = TRUE;
        }
    }

    fclose(file);

    if (found && wcscmp(previousInfPath, currentInfPath) != 0 && previousInfPath[0] != L'\0')
    {
        wcscpy_s(infPath, infPathSize, previousInfPath);
        return TRUE;
    }

    return FALSE; 
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

void ExtractBaseDeviceID(const WCHAR* fullDeviceID, WCHAR* baseDeviceID, size_t bufferSize) {
    const WCHAR* firstSlash = wcschr(fullDeviceID, L'\\');
    if (firstSlash)
    {
        const WCHAR* secondSlash = wcschr(firstSlash + 1, L'\\');
        if (secondSlash) 
        {
            size_t length = secondSlash - fullDeviceID;
            if (length < bufferSize) 
            {
                wcsncpy_s(baseDeviceID, bufferSize, fullDeviceID, length);
            }
            else 
            {
                baseDeviceID[0] = L'\0';
            }
            return;
        }
    }

    wcsncpy_s(baseDeviceID, bufferSize, fullDeviceID, _TRUNCATE);
}

void DeleteDriverHistory(const WCHAR* deviceID) 
{
    FILE* file = NULL;
    FILE* tempFile = NULL;

    errno_t err = _wfopen_s(&file, L"./driver_history.log", L"r");
    _wfopen_s(&tempFile, L"./temp_driver_history.log", L"w");

    if (file == NULL || tempFile == NULL) 
    {
        MessageBoxW(NULL, L"Failed to process history file.", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    WCHAR line[512];
    BOOL skip = FALSE;

    while (fgetws(line, sizeof(line) / sizeof(WCHAR), file)) 
    {
        if (wcsstr(line, L"DeviceID:"))
        {
            skip = wcsstr(line, deviceID) != NULL;
        }

        if (!skip) 
        {
            fputws(line, tempFile);
        }
    }

    fclose(file);
    fclose(tempFile);

    _wremove(L"./driver_history.log");
    _wrename(L"./temp_driver_history.log", L"./driver_history.log");
}

void DeleteLastDriverHistory(const WCHAR* deviceID) 
{
    FILE* file = NULL;
    FILE* tempFile = NULL;

    errno_t err = _wfopen_s(&file, L"./driver_history.log", L"r");
    _wfopen_s(&tempFile, L"./temp_driver_history.log", L"w");

    if (file == NULL || tempFile == NULL)
    {
        MessageBoxW(NULL, L"Failed to process history file.", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    WCHAR line[512];
    WCHAR lastRecord[3][512] = { L"", L"", L"" }; 
    BOOL skip = FALSE;
    BOOL isDeviceIDMatch = FALSE;

    while (fgetws(line, sizeof(line) / sizeof(WCHAR), file)) 
    {
        if (wcsstr(line, L"DeviceID:"))
        {
            isDeviceIDMatch = wcsstr(line, deviceID) != NULL;

            if (isDeviceIDMatch) 
            {
                wcscpy_s(lastRecord[0], lastRecord[1]);
                wcscpy_s(lastRecord[1], lastRecord[2]);
                wcscpy_s(lastRecord[2], line);
            }
        }

        if (!isDeviceIDMatch || (isDeviceIDMatch && wcscmp(line, lastRecord[2]) != 0))
        {
            fputws(line, tempFile); 
        }
    }

    fclose(file);
    fclose(tempFile);

    _wremove(L"./driver_history.log");
    _wrename(L"./temp_driver_history.log", L"./driver_history.log");
}
