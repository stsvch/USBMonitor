#include "driver_utils.h"

void RemoveDriver(const WCHAR* deviceId)
{
    HDEVINFO hDevInfo;
    SP_DEVINFO_DATA DeviceInfoData;
    // Получаем список устройств
    hDevInfo = SetupDiGetClassDevs(NULL, deviceId, NULL, DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (hDevInfo == INVALID_HANDLE_VALUE)
    {
        MessageBoxW(NULL, L"Ошибка получения списка устройств", L"Ошибка", MB_OK | MB_ICONERROR);
        return;
    }
    DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    // Проверяем, существует ли устройство
    if (!SetupDiEnumDeviceInfo(hDevInfo, 0, &DeviceInfoData)) 
    {
        MessageBoxW(NULL, L"Устройство не найдено для удаления.", L"Ошибка", MB_OK | MB_ICONERROR);
        SetupDiDestroyDeviceInfoList(hDevInfo);
        return;
    }
    // Удаляем устройство
    if (!SetupDiRemoveDevice(hDevInfo, &DeviceInfoData)) 
    {
        DWORD dwError = GetLastError();
        WCHAR messageBuffer[256];
        StringCchPrintfW(messageBuffer, 256, L"Ошибка  отключения. Код ошибки: %lu", dwError);
        MessageBoxW(NULL, messageBuffer, L"Ошибка", MB_OK | MB_ICONERROR);
    }
    else 
    {
        DeleteDriverHistory(deviceId);
        /*
        HKEY hKey;
        WCHAR regPath[256];
        StringCchPrintfW(regPath, 256, L"SYSTEM\\CurrentControlSet\\Enum\\%s", deviceId);

        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, regPath, 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS)
        {
            if (RegDeleteTreeW(hKey, NULL) == ERROR_SUCCESS)
            {
                MessageBoxW(NULL, L"Данные драйвера в реестре успешно удалены.", L"Информация", MB_OK | MB_ICONINFORMATION);
            }
            else
            {
                MessageBoxW(NULL, L"Не удалось удалить данные драйвера в реестре.", L"Ошибка", MB_OK | MB_ICONERROR);
            }
            RegCloseKey(hKey);
        }
        else
        {
            MessageBoxW(NULL, L"Не удалось открыть ключ реестра для удаления данных.", L"Ошибка", MB_OK | MB_ICONERROR);
        }
        */
        MessageBoxW(NULL, L"Устройство успешно удалено.", L"Информация", MB_OK | MB_ICONINFORMATION);
    }
    // Очищаем список устройств
    SetupDiDestroyDeviceInfoList(hDevInfo);
}

bool InstallDriver(const WCHAR* deviceId, const WCHAR* infPath) 
{
    HDEVINFO hDevInfo;
    SP_DEVINFO_DATA DeviceInfoData;
    DWORD dwError;
    BOOL rebootRequired = FALSE;


    // Обновляем состояние устройств
    hDevInfo = SetupDiGetClassDevs(NULL, deviceId, NULL, DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (hDevInfo == INVALID_HANDLE_VALUE) {
        MessageBoxW(NULL, L"Ошибка получения списка устройств после удаления", L"Ошибка", MB_OK | MB_ICONERROR);
        return false;
    }

    DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    // Проверяем, существует ли устройство
    if (!SetupDiEnumDeviceInfo(hDevInfo, 0, &DeviceInfoData)) {
        dwError = GetLastError();
        WCHAR messageBuffer[256];
        StringCchPrintfW(messageBuffer, 256, L"Устройство не найдено после удаления. Код ошибки: %lu", dwError);
        MessageBoxW(NULL, messageBuffer, L"Ошибка", MB_OK | MB_ICONERROR);
        SetupDiDestroyDeviceInfoList(hDevInfo);
        return false;
    }

    if (!UpdateDriverForPlugAndPlayDevicesW(
        NULL,           
        deviceId,      
        infPath,      
        INSTALLFLAG_FORCE | INSTALLFLAG_NONINTERACTIVE,
        &rebootRequired 
    )) 
    {
        dwError = GetLastError();
        WCHAR messageBuffer[256];
        StringCchPrintfW(messageBuffer, 256, L"Ошибка установки драйвера. Код ошибки: %lu", dwError);
        MessageBoxW(NULL, messageBuffer, L"Ошибка", MB_OK | MB_ICONERROR);
    }
    else
    {
        MessageBoxW(NULL, L"Драйвер успешно установлен!", L"Успех", MB_OK | MB_ICONINFORMATION);
        SaveDriverHistory(deviceId, infPath);
        SetupDiDestroyDeviceInfoList(hDevInfo);
        return true;
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);
    return false;
}

WCHAR selectedFilePath[260] = { 0 };

bool RollbackDriver(const WCHAR* deviceIdentifier)
{
    HDEVINFO deviceInfoSet = SetupDiGetClassDevsW(NULL, NULL, NULL, DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (deviceInfoSet == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    SP_DEVINFO_DATA deviceInfoData;
    deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    BOOL found = FALSE;
    WCHAR infPath[MAX_PATH];

    for (DWORD i = 0; SetupDiEnumDeviceInfo(deviceInfoSet, i, &deviceInfoData); i++) 
    {
        WCHAR deviceID[256];
        if (SetupDiGetDeviceInstanceIdW(deviceInfoSet, &deviceInfoData, deviceID, sizeof(deviceID) / sizeof(WCHAR), NULL))
        {
            if (wcsstr(deviceID, deviceIdentifier) != NULL)
            {
                found = true;

                if (!LoadDriverHistory(deviceIdentifier, infPath, MAX_PATH)) 
                {
                    MessageBoxW(NULL, L"История не найдена.", L"Ошибка", MB_OK | MB_ICONERROR);
                    break;
                }

                if (InstallDriver(deviceIdentifier, infPath))
                {
                    DeleteLastDriverHistory(deviceIdentifier);
                    SetupDiDestroyDeviceInfoList(deviceInfoSet);
                    return true;
                }
            }
        }
    }
    if (!found)
    {
        MessageBoxW(NULL, L"Устройство не найдено", L"Ошибка", MB_OK | MB_ICONERROR);
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);
    return false;
}

INT_PTR CALLBACK DriverSelectionProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    static WCHAR** drivers;
    static int driverCount;

    switch (message)
    {
    case WM_INITDIALOG:
        drivers = (WCHAR**)lParam; // Получаем массив драйверов
        driverCount = 0;

        // Заполняем список драйверов
        while (drivers[driverCount] != NULL)
        {
            SendDlgItemMessage(hDlg, IDC_LISTBOX, LB_ADDSTRING, 0, (LPARAM)drivers[driverCount]);
            driverCount++;
        }
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) 
        {
            int selected = (int)SendDlgItemMessage(hDlg, IDC_LISTBOX, LB_GETCURSEL, 0, 0);
            if (selected != LB_ERR)
            {
                EndDialog(hDlg, selected); // Возвращаем индекс выбранного драйвера
            }
            else
            {
                EndDialog(hDlg, -1); // Ничего не выбрано
            }
        }
        else if (LOWORD(wParam) == IDCANCEL) 
        {
            EndDialog(hDlg, -1); // Отмена
        }
        else if (LOWORD(wParam) == IDC_BROWSE) 
        {
            // Открываем диалог выбора файла
            OPENFILENAME ofn;
            ZeroMemory(&ofn, sizeof(ofn));
            WCHAR szFile[260] = { 0 };

            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hDlg;
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile);
            ofn.lpstrFilter = L"INF Files\0*.INF\0All Files\0*.*\0";
            ofn.nFilterIndex = 1;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

            if (GetOpenFileName(&ofn)) {
                wcscpy_s(selectedFilePath, szFile); // Сохраняем путь
                EndDialog(hDlg, -2);
            }
        }
        break;

    case WM_CLOSE:
        EndDialog(hDlg, -1);
        break;
    }
    return (INT_PTR)FALSE;
}

LPWSTR ShowDriverSelectionDialog(HWND hwnd, LPCWSTR deviceId) 
{
    HDEVINFO deviceInfoSet;
    SP_DEVINFO_DATA deviceInfoData;
    SP_DRVINFO_DATA driverInfoData;
    WCHAR** drivers = NULL; // Динамический массив указателей
    int driverCount = 0;

    // Получение списка всех устройств в системе
    deviceInfoSet = SetupDiGetClassDevs(NULL, NULL, NULL, DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (deviceInfoSet == INVALID_HANDLE_VALUE)
    {
        MessageBox(hwnd, L"Failed to get the list of devices.", L"Error", MB_OK | MB_ICONERROR);
        return NULL;
    }

    deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    int deviceIndex = 0;

    // Перебор всех устройств
    while (SetupDiEnumDeviceInfo(deviceInfoSet, deviceIndex, &deviceInfoData)) 
    {
        deviceIndex++;
        // Получение ID текущего устройства
        WCHAR currentDeviceId[MAX_DEVICE_ID_LEN];
        if (SetupDiGetDeviceInstanceId(deviceInfoSet, &deviceInfoData, currentDeviceId, MAX_DEVICE_ID_LEN, NULL)) 
        {
            // Сравнение текущего ID устройства с переданным deviceId
            if (_wcsicmp(currentDeviceId, deviceId) == 0)
            {
                // Если ID совпадает, обрабатываем драйверы
                if (SetupDiBuildDriverInfoList(deviceInfoSet, &deviceInfoData, SPDIT_COMPATDRIVER))
                {
                    int driverIndex = 0;
                    driverInfoData.cbSize = sizeof(SP_DRVINFO_DATA);

                    // Перебор совместимых драйверов
                    while (SetupDiEnumDriverInfo(deviceInfoSet, &deviceInfoData, SPDIT_COMPATDRIVER, driverIndex, &driverInfoData))
                    {
                        driverIndex++;

                        // Получение детальной информации о драйвере
                        DWORD requiredSize = 0;
                        SetupDiGetDriverInfoDetail(deviceInfoSet, &deviceInfoData, &driverInfoData, NULL, 0, &requiredSize);

                        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) 
                        {
                            SP_DRVINFO_DETAIL_DATA* driverDetailData = (SP_DRVINFO_DETAIL_DATA*)malloc(requiredSize);
                            if (driverDetailData) {
                                driverDetailData->cbSize = sizeof(SP_DRVINFO_DETAIL_DATA);
                                if (SetupDiGetDriverInfoDetail(deviceInfoSet, &deviceInfoData, &driverInfoData, driverDetailData, requiredSize, NULL))
                                {
                                    // Сохранение имени файла INF
                                    WCHAR* driverInfo = (WCHAR*)malloc((lstrlenW(driverDetailData->InfFileName) + 1) * sizeof(WCHAR));
                                    if (driverInfo) 
                                    {
                                        wcscpy_s(driverInfo, lstrlenW(driverDetailData->InfFileName) + 1, driverDetailData->InfFileName);
                                        drivers = (WCHAR**)realloc(drivers, (driverCount + 1) * sizeof(WCHAR*));
                                        drivers[driverCount] = driverInfo;
                                        driverCount++;
                                    }
                                }
                                free(driverDetailData);
                            }
                        }
                    }
                }
            }
        }
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);

    // Проверка доступности драйверов
    if (driverCount == 0) 
    {
        MessageBox(hwnd, L"Нет совместимых драйверов", L"Информация", MB_OK | MB_ICONINFORMATION);
        return NULL;
    }

    // Добавление завершающего NULL в массив
    drivers = (WCHAR**)realloc(drivers, (driverCount + 1) * sizeof(WCHAR*));
    drivers[driverCount] = NULL;

    // Создание диалога выбора драйвера
    int selectedDriverIndex = DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DRIVER_SELECTION), hwnd, DriverSelectionProc, (LPARAM)drivers);

    LPWSTR selectedDriver = NULL;
    if (selectedDriverIndex == -2) 
    {
        // Путь из OpenFileDialog
        selectedDriver = (LPWSTR)malloc((lstrlenW(selectedFilePath) + 1) * sizeof(WCHAR));
        if (selectedDriver) 
        {
            wcscpy_s(selectedDriver, lstrlenW(selectedFilePath) + 1, selectedFilePath);
        }
    }
    else if (selectedDriverIndex >= 0 && selectedDriverIndex < driverCount)
    {
        // Путь из списка
        selectedDriver = (LPWSTR)malloc((lstrlenW(drivers[selectedDriverIndex]) + 1) * sizeof(WCHAR));
        if (selectedDriver) 
        {
            wcscpy_s(selectedDriver, lstrlenW(drivers[selectedDriverIndex]) + 1, drivers[selectedDriverIndex]);
        }
    }
    for (int i = 0; i < driverCount; i++)
    {
        free(drivers[i]);
    }
    free(drivers);

    return selectedDriver;
}
