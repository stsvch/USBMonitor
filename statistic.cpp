#include "statistic.h"

const size_t BUFFER_SIZE = 1024 * 1024; 

double MeasureWriteSpeed(const WCHAR* volumePath)
{
    const size_t fileSizeMB = 10;
    const size_t fileSize = fileSizeMB * BUFFER_SIZE;
    WCHAR testFile[MAX_PATH];
    swprintf_s(testFile, MAX_PATH, L"%s\\test_speed.tmp", volumePath);

    ULARGE_INTEGER freeBytesAvailable, totalBytes, totalFreeBytes;
    if (!GetDiskFreeSpaceEx(volumePath, &freeBytesAvailable, &totalBytes, &totalFreeBytes))
    {
        return 0; 
    }
    if (freeBytesAvailable.QuadPart < fileSize)
    {
        return 0; 
    }

    FILE* file = nullptr;
    if (_wfopen_s(&file, testFile, L"wb") != 0 || !file) return 0;

    char* buffer = new char[BUFFER_SIZE]; 
    memset(buffer, 'A', BUFFER_SIZE); 

    LARGE_INTEGER frequency, start, end;
    if (!QueryPerformanceFrequency(&frequency)) 
    {
        delete[] buffer;
        fclose(file);
        return 0;
    }

    QueryPerformanceCounter(&start); 

    for (size_t written = 0; written < fileSize; written += BUFFER_SIZE) 
    {
        size_t writeSize = (fileSize - written) > BUFFER_SIZE ? BUFFER_SIZE : (fileSize - written);
        if (fwrite(buffer, 1, writeSize, file) != writeSize) 
        {
            delete[] buffer;
            fclose(file);
            _wremove(testFile);
            return 0;
        }
    }

    QueryPerformanceCounter(&end); 
    fclose(file);
    delete[] buffer; 
    _wremove(testFile); 

    double elapsed = static_cast<double>(end.QuadPart - start.QuadPart) / frequency.QuadPart;
    if (elapsed == 0) return 0;
    return fileSizeMB / elapsed;
}

double MeasureReadSpeed(const WCHAR* volumePath)
{
    const size_t fileSizeMB = 10; 
    const size_t fileSize = fileSizeMB * BUFFER_SIZE;
    WCHAR testFile[MAX_PATH];

    swprintf_s(testFile, MAX_PATH, L"%s\\test_speed.tmp", volumePath);

    ULARGE_INTEGER freeBytesAvailable, totalBytes, totalFreeBytes;
    if (!GetDiskFreeSpaceEx(volumePath, &freeBytesAvailable, &totalBytes, &totalFreeBytes)) 
    {
        return 0; 
    }
    if (freeBytesAvailable.QuadPart < fileSize)
    {
        return 0; 
    }

    FILE* tempFile = nullptr;
    if (_wfopen_s(&tempFile, testFile, L"wb") != 0 || !tempFile)
    {
        return 0; 
    }

    char* buffer = new char[BUFFER_SIZE];
    memset(buffer, 'A', BUFFER_SIZE); 

    for (size_t written = 0; written < fileSize; written += BUFFER_SIZE)
    {
        size_t writeSize = (fileSize - written) > BUFFER_SIZE ? BUFFER_SIZE : (fileSize - written);
        if (fwrite(buffer, 1, writeSize, tempFile) != writeSize) 
        {
            delete[] buffer;
            fclose(tempFile);
            _wremove(testFile);
            return 0; 
        }
    }
    fclose(tempFile);

    FILE* file = nullptr;
    if (_wfopen_s(&file, testFile, L"rb") != 0 || !file)
    {
        delete[] buffer;
        return 0; 
    }

    LARGE_INTEGER frequency, start, end;
    if (!QueryPerformanceFrequency(&frequency)) 
    {
        delete[] buffer;
        fclose(file);
        _wremove(testFile);
        return 0; 
    }

    QueryPerformanceCounter(&start);

    for (size_t read = 0; read < fileSize; read += BUFFER_SIZE)
    {
        size_t readSize = (fileSize - read) > BUFFER_SIZE ? BUFFER_SIZE : (fileSize - read);
        if (fread(buffer, 1, readSize, file) != readSize) 
        {
            delete[] buffer;
            fclose(file);
            _wremove(testFile);
            return 0; 
        }
    }
    QueryPerformanceCounter(&end);
    fclose(file);
    delete[] buffer;

    _wremove(testFile);

    double elapsed = static_cast<double>(end.QuadPart - start.QuadPart) / frequency.QuadPart;
    if (elapsed == 0) return 0;
    return fileSizeMB / elapsed;
}

void GetDeviceType(const WCHAR* deviceID, WCHAR* deviceTypeBuffer)
{
    HDEVINFO hDevInfo = SetupDiGetClassDevs(NULL, NULL, NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (hDevInfo == INVALID_HANDLE_VALUE)
    {
        wcscpy_s(deviceTypeBuffer, MAX_BUFFER_SIZE, L"Unknown"); 
        return;
    }

    SP_DEVINFO_DATA devInfoData;
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    // Перебор всех устройств
    for (int i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData); i++) 
    {
        WCHAR currentDeviceID[MAX_BUFFER_SIZE];
        if (SetupDiGetDeviceInstanceId(hDevInfo, &devInfoData, currentDeviceID, MAX_BUFFER_SIZE, NULL))
        {
            if (wcscmp(currentDeviceID, deviceID) == 0)
            {
                WCHAR deviceClass[MAX_BUFFER_SIZE];
                if (SetupDiGetDeviceRegistryProperty(hDevInfo, &devInfoData, SPDRP_CLASS, NULL, (PBYTE)deviceClass, sizeof(deviceClass), NULL)) 
                {
                    wcscpy_s(deviceTypeBuffer, MAX_BUFFER_SIZE, deviceClass); 
                }
                else 
                {
                    wcscpy_s(deviceTypeBuffer, MAX_BUFFER_SIZE, L"Unknown"); 
                }
                SetupDiDestroyDeviceInfoList(hDevInfo);
                return;
            }
        }
    }

    wcscpy_s(deviceTypeBuffer, MAX_BUFFER_SIZE, L"Unknown"); 
    SetupDiDestroyDeviceInfoList(hDevInfo); 
}

bool GetDevicePath(const WCHAR* deviceID, WCHAR* devicePathBuffer) 
{
    static std::set<std::wstring> usedPaths; 

    DWORD drives = GetLogicalDrives();
    if (drives == 0) 
    {
        wcscpy_s(devicePathBuffer, MAX_PATH, L"Unknown");
        return false;
    }

    WCHAR driveLetter[4] = L"A:\\";
    WCHAR volumeName[MAX_PATH];
    WCHAR parentDeviceID[MAX_PATH];
    WCHAR currentDeviceInstanceID[MAX_PATH];

    for (int i = 0; i < 26; ++i) 
    {
        if ((drives & (1 << i)) == 0) 
        {
            continue; // Диск не подключен
        }

        driveLetter[0] = L'A' + i; 

        if (GetDriveType(driveLetter) != DRIVE_REMOVABLE)
        {
            continue;
        }

        if (usedPaths.find(driveLetter) != usedPaths.end()) 
        {
            continue; 
        }

        // Получаем имя тома
        if (!GetVolumeNameForVolumeMountPoint(driveLetter, volumeName, MAX_PATH)) 
        {
            continue; 
        }

        HDEVINFO deviceInfoSet = SetupDiGetClassDevs(NULL, NULL, NULL, DIGCF_PRESENT | DIGCF_ALLCLASSES);
        if (deviceInfoSet == INVALID_HANDLE_VALUE)
        {
            continue;
        }

        SP_DEVINFO_DATA deviceInfoData = {};
        deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

        // Перебираем устройства
        for (DWORD j = 0; SetupDiEnumDeviceInfo(deviceInfoSet, j, &deviceInfoData); j++) 
        {
            if (SetupDiGetDeviceInstanceId(deviceInfoSet, &deviceInfoData, currentDeviceInstanceID, MAX_PATH, NULL))
            {
                // Проверяем связь с родителем
                DEVINST devInst = 0, parentDevInst = 0;
                //узел устройства (devInst) в иерархии устройств системы
                CONFIGRET cr = CM_Locate_DevNode(&devInst, currentDeviceInstanceID, CM_LOCATE_DEVNODE_NORMAL);
                if (cr == CR_SUCCESS)
                {
                    cr = CM_Get_Parent(&parentDevInst, devInst, 0);
                    if (cr == CR_SUCCESS)
                    {
                        CM_Get_Device_ID(parentDevInst, parentDeviceID, MAX_PATH, 0);
                        if (wcscmp(parentDeviceID, deviceID) == 0) 
                        {
                            // Устройство связано, сохраняем букву диска
                            wcscpy_s(devicePathBuffer, MAX_PATH, driveLetter);
                            usedPaths.insert(driveLetter); // Добавляем путь в список
                            SetupDiDestroyDeviceInfoList(deviceInfoSet);
                            return true;
                        }
                    }
                }
            }
        }

        SetupDiDestroyDeviceInfoList(deviceInfoSet);
    }

    wcscpy_s(devicePathBuffer, MAX_PATH, L"Unknown");
    return false;
}


