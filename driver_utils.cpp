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

// Функция для отображения диалогового окна выбора драйвера и возврата пути к выбранному драйверу
LPWSTR ShowDriverSelectionDialog(HWND hwnd)
{
    HDEVINFO deviceInfoSet;
    SP_DEVINFO_DATA deviceInfoData;
    SP_DRVINFO_DATA driverInfoData;
    WCHAR drivers[1024][MAX_PATH]; // Массив для хранения описаний драйверов
    int driverCount = 0;

    // Получаем список всех устройств в системе
    deviceInfoSet = SetupDiGetClassDevs(NULL, NULL, NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        MessageBox(hwnd, L"Не удалось получить список устройств.", L"Ошибка", MB_OK | MB_ICONERROR);
        return NULL;
    }

    deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    int deviceIndex = 0;

    // Перебираем все устройства
    while (SetupDiEnumDeviceInfo(deviceInfoSet, deviceIndex, &deviceInfoData)) {
        deviceIndex++;

        // Создаем список доступных драйверов для текущего устройства
        if (SetupDiBuildDriverInfoList(deviceInfoSet, &deviceInfoData, SPDIT_COMPATDRIVER)) {
            int driverIndex = 0;
            driverInfoData.cbSize = sizeof(SP_DRVINFO_DATA);

            // Перебираем доступные драйверы
            while (SetupDiEnumDriverInfo(deviceInfoSet, &deviceInfoData, SPDIT_COMPATDRIVER, driverIndex, &driverInfoData)) {
                driverIndex++;

                // Сохраняем информацию о драйвере в список
                if (driverCount < 1024) {
                    wcscpy_s(drivers[driverCount], driverInfoData.Description);
                    driverCount++;
                }
            }
        }
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);

    // Создаем диалоговое окно с доступными драйверами
    if (driverCount == 0) {
        MessageBox(hwnd, L"Нет доступных драйверов для выбора.", L"Информация", MB_OK | MB_ICONINFORMATION);
        return NULL;
    }

    // Формируем строку с описанием всех доступных драйверов
    WCHAR driverList[4096] = L"";
    for (int i = 0; i < driverCount; ++i) {
        WCHAR line[128];
        swprintf_s(line, L"%d: %s\n", i + 1, drivers[i]);
        wcscat_s(driverList, line);
    }

    // Показываем список пользователю
    MessageBox(hwnd, driverList, L"Доступные драйверы", MB_OK | MB_ICONINFORMATION);

    // Запрашиваем у пользователя выбор драйвера
    WCHAR inputBuffer[10];
    int selectedDriverIndex = -1;
    while (selectedDriverIndex < 1 || selectedDriverIndex > driverCount) {
        // Попросим пользователя ввести номер драйвера
        int response = MessageBox(hwnd, L"Введите номер выбранного драйвера (например, 1):", L"Выбор драйвера", MB_OKCANCEL | MB_ICONQUESTION);
        if (response == IDCANCEL) {
            return NULL; // Если пользователь отменил выбор
        }

        // Получаем номер драйвера
        if (GetWindowText(hwnd, inputBuffer, sizeof(inputBuffer) / sizeof(WCHAR)) != 0) {
            selectedDriverIndex = _wtoi(inputBuffer);
        }

        // Проверяем, что выбранный индекс допустим
        if (selectedDriverIndex < 1 || selectedDriverIndex > driverCount) {
            MessageBox(hwnd, L"Некорректный номер драйвера. Пожалуйста, попробуйте снова.", L"Ошибка", MB_OK | MB_ICONERROR);
        }
    }

    // Если пользователь выбрал корректный драйвер, возвращаем его описание
    if (selectedDriverIndex >= 1 && selectedDriverIndex <= driverCount) {
        LPWSTR selectedDriver = (LPWSTR)LocalAlloc(LPTR, (lstrlenW(drivers[selectedDriverIndex - 1]) + 1) * sizeof(WCHAR));
        if (selectedDriver) {
            wcscpy_s(selectedDriver, lstrlenW(drivers[selectedDriverIndex - 1]) + 1, drivers[selectedDriverIndex - 1]);
        }
        return selectedDriver;
    }
    return NULL;
}
