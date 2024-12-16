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
            return FALSE; 
        }
    }
}

void SaveDriverHistory(const WCHAR* deviceID, const WCHAR* infPath) 
{
    FILE* file = NULL;

    errno_t err = _wfopen_s(&file, DCONFIG_FILE, L"a");

    if (file == NULL) return;
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
    errno_t err = _wfopen_s(&file, DCONFIG_FILE, L"r");

    if (err != 0 || file == NULL) return FALSE;

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

void ExtractBaseDeviceID(const WCHAR* fullDeviceID, WCHAR* baseDeviceID, size_t bufferSize) 
{
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

    _wfopen_s(&file, DCONFIG_FILE, L"r");
    _wfopen_s(&tempFile, TDCONFIG_FILE, L"w");

    if (file == NULL || tempFile == NULL)
    {
        return;
    }

    WCHAR line[512];
    BOOL isPartOfTargetRecord = FALSE;
    int linesToSkip = 0; 

    // --- Читаем файл построчно ---
    while (fgetws(line, sizeof(line) / sizeof(WCHAR), file))
    {
        if (wcsstr(line, L"DeviceID:"))
        {
            // Если это строка с DeviceID, проверяем соответствие
            if (wcsstr(line, deviceID) != NULL)
            {
                isPartOfTargetRecord = TRUE;
                linesToSkip = 2; // Установить пропуск двух следующих строк
                continue; // Пропускаем текущую строку с DeviceID
            }
            else
            {
                isPartOfTargetRecord = FALSE;
            }
        }

        if (isPartOfTargetRecord && linesToSkip > 0)
        {
            // Пропускаем дополнительные строки, связанные с DeviceID
            linesToSkip--;
            continue;
        }
        fputws(line, tempFile);
    }

    fclose(file);
    fclose(tempFile);

    // Заменяем исходный файл новым
    _wremove(DCONFIG_FILE);
    _wrename(TDCONFIG_FILE, DCONFIG_FILE);
}

void DeleteLastDriverHistory(const WCHAR* deviceID)
{
    FILE* file = NULL;
    FILE* tempFile = NULL;

    _wfopen_s(&file, DCONFIG_FILE, L"r");
    _wfopen_s(&tempFile, TDCONFIG_FILE, L"w");

    if (file == NULL || tempFile == NULL)
    {
        return;
    }

    WCHAR line[512];
    long lastRecordStart = -1; // Начало последней записи
    long currentPos = 0;  

    while (fgetws(line, sizeof(line) / sizeof(WCHAR), file))
    {
        if (wcsstr(line, L"DeviceID:") && wcsstr(line, deviceID))
        {
            lastRecordStart = currentPos; // Запоминаем начало записи
        }
        currentPos++;
    }

    rewind(file); 

    currentPos = 0;
    while (fgetws(line, sizeof(line) / sizeof(WCHAR), file))
    {
        // Пропускаем строки, относящиеся к последней записи
        if (currentPos == lastRecordStart ||
            currentPos == lastRecordStart + 1 ||
            currentPos == lastRecordStart + 2)
        {
            currentPos++;
            continue;
        }

        // Записываем все остальные строки
        fputws(line, tempFile);
        currentPos++;
    }

    fclose(file);
    fclose(tempFile);

    // Заменяем старый файл новым
    _wremove(DCONFIG_FILE);
    _wrename(TDCONFIG_FILE, DCONFIG_FILE);
}

