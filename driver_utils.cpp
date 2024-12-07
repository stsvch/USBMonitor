#include "driver_utils.h"

// Установка драйвера по InfName
void InstallDriver(const WCHAR* infPath, const WCHAR* deviceID, const WCHAR* driverVersion)
{
    BOOL rebootRequired = FALSE;

    // Установка драйвера
    BOOL result = DiInstallDriverW(NULL, infPath, DIIRFLAG_FORCE_INF, &rebootRequired);
    if (result) {
        // Успешная установка
        MessageBoxW(NULL, L"Driver installed successfully.", L"Success", MB_OK | MB_ICONINFORMATION);

        // Сохранение истории драйвера
        SaveDriverHistory(deviceID, infPath, driverVersion);

        if (rebootRequired) {
            MessageBoxW(NULL, L"Reboot is required to complete installation.", L"Info", MB_OK | MB_ICONINFORMATION);
        }
    }
    else {
        // Обработка ошибок
        WCHAR errorMsg[256];
        swprintf_s(errorMsg, sizeof(errorMsg) / sizeof(WCHAR), L"Failed to install driver. Error: %lu", GetLastError());
        MessageBoxW(NULL, errorMsg, L"Error", MB_OK | MB_ICONERROR);
    }
}

//удаляет привязку драйвера к устройству, но не удаляет сам драйвер
// из системы.
void RemoveDriver(const WCHAR* deviceIdentifier)
{
    HDEVINFO deviceInfoSet = SetupDiGetClassDevsW(NULL, NULL, NULL, DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        MessageBoxW(NULL, L"Failed to get device info set.", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    SP_DEVINFO_DATA deviceInfoData;
    deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    BOOL found = FALSE;

    for (DWORD i = 0; SetupDiEnumDeviceInfo(deviceInfoSet, i, &deviceInfoData); i++) {
        WCHAR deviceID[256];
        if (SetupDiGetDeviceInstanceIdW(deviceInfoSet, &deviceInfoData, deviceID, sizeof(deviceID) / sizeof(WCHAR), NULL)) {
            if (wcsstr(deviceID, deviceIdentifier) != NULL) {
                found = TRUE;

                // Удаление текущего драйвера
                BOOL removeResult = SetupDiCallClassInstaller(DIF_REMOVE, deviceInfoSet, &deviceInfoData);
                if (removeResult) {
                    MessageBoxW(NULL, L"Driver removed successfully.", L"Success", MB_OK | MB_ICONINFORMATION);
                }
                else {
                    WCHAR errorMsg[256];
                    swprintf_s(errorMsg, sizeof(errorMsg) / sizeof(WCHAR), L"Failed to remove driver. Error: %lu", GetLastError());
                    MessageBoxW(NULL, errorMsg, L"Error", MB_OK | MB_ICONERROR);
                }
                break;
            }
        }
    }

    if (!found) {
        MessageBoxW(NULL, L"Device not found.", L"Error", MB_OK | MB_ICONERROR);
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);
}

// Функция для отката драйвера
void RollbackDriver(const WCHAR* deviceIdentifier)
{
    HDEVINFO deviceInfoSet = SetupDiGetClassDevsW(NULL, NULL, NULL, DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        MessageBoxW(NULL, L"Failed to get device info set.", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    SP_DEVINFO_DATA deviceInfoData;
    deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    BOOL found = FALSE;

    WCHAR infPath[MAX_PATH];

    for (DWORD i = 0; SetupDiEnumDeviceInfo(deviceInfoSet, i, &deviceInfoData); i++) {
        WCHAR deviceID[256];
        if (SetupDiGetDeviceInstanceIdW(deviceInfoSet, &deviceInfoData, deviceID, sizeof(deviceID) / sizeof(WCHAR), NULL)) {
            if (wcsstr(deviceID, deviceIdentifier) != NULL) {
                found = TRUE;

                // Загрузка истории драйвера для устройства
                if (!LoadDriverHistory(deviceIdentifier, infPath, MAX_PATH)) {
                    MessageBoxW(NULL, L"No history found for rollback.", L"Error", MB_OK | MB_ICONERROR);
                    break;
                }

                // Замена драйвера на предыдущую версию
                BOOL updateResult = UpdateDriverForPlugAndPlayDevicesW(
                    NULL,                // HWND (окно)
                    deviceID,            // Идентификатор устройства
                    infPath,             // Путь к INF-файлу
                    INSTALLFLAG_FORCE,   // Принудительная установка
                    NULL                 // Установка без перезагрузки
                );

                if (updateResult) {
                    MessageBoxW(NULL, L"Driver rollback successful.", L"Success", MB_OK | MB_ICONINFORMATION);
                }
                else {
                    WCHAR errorMsg[256];
                    swprintf_s(errorMsg, sizeof(errorMsg) / sizeof(WCHAR), L"Failed to rollback driver. Error: %lu", GetLastError());
                    MessageBoxW(NULL, errorMsg, L"Error", MB_OK | MB_ICONERROR);
                }
                break;
            }
        }
    }

    if (!found) {
        MessageBoxW(NULL, L"Device not found.", L"Error", MB_OK | MB_ICONERROR);
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);
}

// Функция, вызываемая при нажатии на кнопку для выбора папки и установки драйвера
void OnBrowseAndInstallDriver(HWND hwnd) 
{
    BROWSEINFO bi = { 0 };
    bi.lpszTitle = L"Выберите папку с драйверами"; // Заголовок диалогового окна выбора папки
    LPITEMIDLIST pidl = SHBrowseForFolder(&bi); // Открываем диалог выбора папки

    if (pidl != 0) {
        // Получаем путь к выбранной папке
        WCHAR path[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, path)) {
            MessageBox(hwnd, path, L"Выбранная папка", MB_OK);

            // Здесь выполняется установка драйвера
            // Пытаемся установить драйверы из указанной папки
            BOOL result = SetupCopyOEMInfW(
                path,           // Путь к папке с INF файлами драйвера
                NULL,           // Каталог, куда копировать (NULL - системный каталог, обычно C:\Windows\inf)
                SPOST_PATH,     // Тип источника (путь к папке с файлами)
                0,              // Дополнительные флаги (0 - стандартное копирование)
                NULL,           // Буфер для имени файла (не нужен, поэтому NULL)
                0,              // Размер буфера (0, так как буфер не используется)
                NULL,           // Фактический размер буфера (NULL, так как не используется)
                NULL            // Путь к скопированному файлу (NULL, если не требуется)
            );

            if (result) {
                // Сообщение об успешной установке драйвера
                MessageBox(hwnd, L"Драйвер успешно установлен!", L"Успех", MB_OK | MB_ICONINFORMATION);
            }
            else {
                // Обработка ошибки установки драйвера
                DWORD error = GetLastError();
                WCHAR errorMsg[256];
                swprintf_s(errorMsg, L"Ошибка установки драйвера: %ld", error);
                MessageBox(hwnd, errorMsg, L"Ошибка", MB_OK | MB_ICONERROR);
            }
        }
    }
    else {
        // Сообщение об ошибке, если папка не была выбрана
        MessageBox(hwnd, L"Папка не была выбрана.", L"Ошибка", MB_OK | MB_ICONERROR);
    }
}

INT_PTR CALLBACK DriverSelectionProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    static WCHAR** drivers;
    static int driverCount;

    switch (message)
    {
    case WM_INITDIALOG:
        drivers = (WCHAR**)lParam; // Get the pointer to the driver array
        driverCount = 0;

        // Populate the listbox with drivers
        while (drivers[driverCount] != NULL) {
            SendDlgItemMessage(hDlg, IDC_LISTBOX, LB_ADDSTRING, 0, (LPARAM)drivers[driverCount]);
            driverCount++;
        }
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) {
            // Get the selected driver's index
            int selected = (int)SendDlgItemMessage(hDlg, IDC_LISTBOX, LB_GETCURSEL, 0, 0);
            if (selected != LB_ERR) {
                EndDialog(hDlg, selected);
                return (INT_PTR)TRUE;
            }
        }
        else if (LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, -1);
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

LPWSTR ShowDriverSelectionDialog(HWND hwnd, LPCWSTR deviceId)
{
    HDEVINFO deviceInfoSet;
    SP_DEVINFO_DATA deviceInfoData;
    SP_DRVINFO_DATA driverInfoData;
    WCHAR** drivers = NULL; // Dynamic array of pointers
    int driverCount = 0;

    // Get the list of all devices in the system
    deviceInfoSet = SetupDiGetClassDevs(NULL, NULL, NULL, DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        MessageBox(hwnd, L"Failed to get the list of devices.", L"Error", MB_OK | MB_ICONERROR);
        return NULL;
    }

    deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    int deviceIndex = 0;

    // Enumerate all devices
    while (SetupDiEnumDeviceInfo(deviceInfoSet, deviceIndex, &deviceInfoData)) {
        deviceIndex++;

        // Get the device ID of the current device
        WCHAR currentDeviceId[MAX_DEVICE_ID_LEN];
        if (SetupDiGetDeviceInstanceId(deviceInfoSet, &deviceInfoData, currentDeviceId, MAX_DEVICE_ID_LEN, NULL)) {
            // Compare the current device ID with the passed deviceId
            if (_wcsicmp(currentDeviceId, deviceId) == 0) {
                // If deviceId matches, process drivers
                if (SetupDiBuildDriverInfoList(deviceInfoSet, &deviceInfoData, SPDIT_COMPATDRIVER)) {
                    int driverIndex = 0;
                    driverInfoData.cbSize = sizeof(SP_DRVINFO_DATA);

                    // Enumerate available drivers
                    while (SetupDiEnumDriverInfo(deviceInfoSet, &deviceInfoData, SPDIT_COMPATDRIVER, driverIndex, &driverInfoData)) {
                        driverIndex++;

                        // Retrieve detailed driver info
                        DWORD requiredSize = 0;
                        SetupDiGetDriverInfoDetail(deviceInfoSet, &deviceInfoData, &driverInfoData, NULL, 0, &requiredSize);

                        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                            SP_DRVINFO_DETAIL_DATA* driverDetailData = (SP_DRVINFO_DETAIL_DATA*)malloc(requiredSize);
                            if (driverDetailData) {
                                driverDetailData->cbSize = sizeof(SP_DRVINFO_DETAIL_DATA);
                                if (SetupDiGetDriverInfoDetail(deviceInfoSet, &deviceInfoData, &driverInfoData, driverDetailData, requiredSize, NULL)) {
                                    // Save the INF filename only
                                    WCHAR* driverInfo = (WCHAR*)malloc((lstrlenW(driverDetailData->InfFileName) + 1) * sizeof(WCHAR));
                                    if (driverInfo) {
                                        wcscpy_s(driverInfo, lstrlenW(driverDetailData->InfFileName) + 1, driverDetailData->InfFileName);
                                        drivers = (WCHAR**)realloc(drivers, (driverCount + 1) * sizeof(WCHAR*));
                                        drivers[driverCount] = driverInfo;
                                        driverCount++;
                                    }
                                }
                                else {
                                    DWORD error = GetLastError();
                                    WCHAR errorMessage[256];
                                    wsprintf(errorMessage, L"SetupDiGetDriverInfoDetail failed even with allocated buffer. Error code: %lu", error);
                                    MessageBox(hwnd, errorMessage, L"Error", MB_OK | MB_ICONERROR);
                                }
                                free(driverDetailData);
                            }
                            else {
                                MessageBox(hwnd, L"Failed to allocate memory for driver detail.", L"Error", MB_OK | MB_ICONERROR);
                            }
                        }
                    }
                }
                else {
                    MessageBox(hwnd, L"Failed to build driver info list.", L"Error", MB_OK | MB_ICONERROR);
                }
            }
        }
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);

    // Check if any drivers are available
    if (driverCount == 0) 
    {
        MessageBox(hwnd, L"No available drivers for selection.", L"И", MB_OK | MB_ICONINFORMATION);
        return NULL;
    }

    // Add terminating NULL to the array
    drivers = (WCHAR**)realloc(drivers, (driverCount + 1) * sizeof(WCHAR*));
    drivers[driverCount] = NULL;

    // Create the driver selection dialog
    int selectedDriverIndex = -1;
    selectedDriverIndex = DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DRIVER_SELECTION), hwnd, DriverSelectionProc, (LPARAM)drivers);

    LPWSTR selectedDriver = NULL;
    if (selectedDriverIndex >= 0 && selectedDriverIndex < driverCount) 
    {
        selectedDriver = (LPWSTR)malloc((lstrlenW(drivers[selectedDriverIndex]) + 1) * sizeof(WCHAR));
        if (selectedDriver) 
        {
            wcscpy_s(selectedDriver, lstrlenW(drivers[selectedDriverIndex]) + 1, drivers[selectedDriverIndex]);
        }
    }

    // Free memory
    for (int i = 0; i < driverCount; i++) 
    {
        free(drivers[i]);
    }
    free(drivers);

    return selectedDriver;
}
