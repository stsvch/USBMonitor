#include "usb_utils.h"

BOOL ChangeUSBDeviceState(const WCHAR* deviceID, BOOL connected)
{
    // Получаем список всех установленных устройств
    HDEVINFO hDevInfo = SetupDiGetClassDevs(NULL, NULL, NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (hDevInfo == INVALID_HANDLE_VALUE) return FALSE; // Если не удалось получить список устройств, возвращаем FALSE

    SP_DEVINFO_DATA devInfoData;
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA); // Инициализируем структуру данных устройства

    DWORD index = 0;
    // Перебираем все устройства в списке
    while (SetupDiEnumDeviceInfo(hDevInfo, index++, &devInfoData))
    {
        WCHAR currentDeviceID[MAX_DEVICE_ID_LEN];
        // Получаем текущий DeviceID устройства
        if (SetupDiGetDeviceInstanceIdW(hDevInfo, &devInfoData, currentDeviceID, MAX_DEVICE_ID_LEN, NULL))
        {
            // Сравниваем текущий DeviceID с указанным deviceID
            if (_wcsicmp(currentDeviceID, deviceID) == 0)
            {
                // Устройство найдено, теперь будем изменять его состояние
                SP_PROPCHANGE_PARAMS propChangeParams;
                propChangeParams.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER); // Инициализация заголовка
                propChangeParams.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE; // Указываем, что изменяем свойства устройства
                propChangeParams.StateChange = connected ? DICS_DISABLE : DICS_ENABLE; // Выбираем действие: включить или отключить устройство
                propChangeParams.Scope = DICS_FLAG_GLOBAL; // Применяем изменения ко всем профилям устройства
                propChangeParams.HwProfile = 0; // Не указываем конкретный профиль устройства

                // Устанавливаем параметры для изменения состояния устройства
                if (!SetupDiSetClassInstallParams(hDevInfo, &devInfoData,
                    (SP_CLASSINSTALL_HEADER*)&propChangeParams,
                    sizeof(SP_PROPCHANGE_PARAMS)))
                {
                    SetupDiDestroyDeviceInfoList(hDevInfo); // Освобождаем ресурсы
                    return FALSE; // Ошибка при установке параметров
                }

                // Применяем изменения состояния устройства
                if (!SetupDiChangeState(hDevInfo, &devInfoData))
                {
                    SetupDiDestroyDeviceInfoList(hDevInfo); // Освобождаем ресурсы
                    return FALSE; // Ошибка при изменении состояния устройства
                }

                SetupDiDestroyDeviceInfoList(hDevInfo); // Освобождаем ресурсы
                return TRUE; // Устройство успешно изменило состояние
            }
        }
    }
    SetupDiDestroyDeviceInfoList(hDevInfo); // Освобождаем ресурсы
    return FALSE; // Устройство с таким ID не найдено
}

DeviceInfo* ListConnectedUSBDevices(int* deviceCount)
{
    *deviceCount = 0;  // Инициализируем количество устройств

    // Получаем список всех устройств, подключенных через USB
    HDEVINFO deviceInfoSet = SetupDiGetClassDevs(
        NULL,
        L"USB",  // Фильтруем устройства по классу "USB"
        NULL,
        DIGCF_PRESENT | DIGCF_ALLCLASSES  // Получаем только устройства, которые присутствуют в системе
    );

    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        return NULL;  // Если не удалось получить список устройств, возвращаем NULL
    }

    SP_DEVINFO_DATA deviceInfoData;
    deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);  // Инициализируем структуру данных для устройства

    int capacity = 10;  // Начальная вместимость массива устройств
    DeviceInfo* devices = new DeviceInfo[capacity];  // Создаем массив для хранения информации о устройствах

    // Перебираем все устройства в списке
    for (int i = 0; SetupDiEnumDeviceInfo(deviceInfoSet, i, &deviceInfoData); ++i)
    {
        // Инициализируем строки для хранения информации о текущем устройстве
        WCHAR deviceDesc[MAX_BUFFER_SIZE] = L"Unknown Device";
        WCHAR deviceClass[MAX_BUFFER_SIZE] = L"Unknown Class";
        WCHAR deviceDriver[MAX_BUFFER_SIZE] = L"Unknown Driver";
        WCHAR deviceClassGUID[MAX_BUFFER_SIZE] = L"Unknown GUID";

        // Получаем описание устройства
        if (SetupDiGetDeviceRegistryProperty(
            deviceInfoSet,
            &deviceInfoData,
            SPDRP_DEVICEDESC,  // Свойство: описание устройства
            NULL,
            (PBYTE)deviceDesc,
            sizeof(deviceDesc),
            NULL))
        {
        }

        // Получаем класс устройства (например, USB)
        if (SetupDiGetDeviceRegistryProperty(
            deviceInfoSet,
            &deviceInfoData,
            SPDRP_CLASS,  // Свойство: класс устройства
            NULL,
            (PBYTE)deviceClass,
            sizeof(deviceClass),
            NULL))
        {
        }

        // Получаем GUID класса устройства
        if (SetupDiGetDeviceRegistryProperty(
            deviceInfoSet,
            &deviceInfoData,
            SPDRP_CLASSGUID,  // Свойство: GUID класса устройства
            NULL,
            (PBYTE)deviceClassGUID,
            sizeof(deviceClassGUID),
            NULL))
        {
        }

        // Фильтруем системные устройства (например, концентраторы и контроллеры)
        if (wcsstr(deviceDesc, L"Hub") != nullptr ||
            wcsstr(deviceDesc, L"Controller") != nullptr)
        {
            continue;  // Пропускаем устройства, связанные с хабами и контроллерами
        }

        // Пропускаем устройства, которые не являются USB
        if (wcsstr(deviceClass, L"USB") == nullptr)
        {
            continue;  // Пропускаем устройства, не относящиеся к классу USB
        }

        // Проверяем свойства устройства, чтобы определить, является ли оно съемным
        DWORD capabilities = 0;
        if (SetupDiGetDeviceRegistryProperty(
            deviceInfoSet,
            &deviceInfoData,
            SPDRP_CAPABILITIES,  // Свойство: возможности устройства
            NULL,
            (PBYTE)&capabilities,
            sizeof(capabilities),
            NULL))
        {
            // Если устройство не является съемным, пропускаем его
            if ((capabilities & CM_DEVCAP_REMOVABLE) == 0) {
                continue;  // Пропускаем устройства, которые не являются съемными
            }
        }

        // Получаем уникальный идентификатор устройства (Device ID)
        WCHAR deviceID[MAX_BUFFER_SIZE] = L"Unknown Device ID";
        if (SetupDiGetDeviceInstanceId(
            deviceInfoSet,
            &deviceInfoData,
            deviceID,
            sizeof(deviceID),
            NULL))
        {
        }

        // Если количество устройств превышает текущую вместимость массива, увеличиваем его
        if (*deviceCount >= capacity) {
            capacity *= 2;  // Удваиваем вместимость массива
            DeviceInfo* newDevices = new DeviceInfo[capacity];  // Создаем новый массив с большей вместимостью
            memcpy(newDevices, devices, sizeof(DeviceInfo) * (*deviceCount));  // Копируем старые данные в новый массив
            delete[] devices;  // Освобождаем старый массив
            devices = newDevices;  // Перенаправляем указатель на новый массив
        }

        // Сохраняем информацию о текущем устройстве в массив
        DeviceInfo& device = devices[*deviceCount];
        wcscpy_s(device.Caption, deviceDesc);  // Копируем описание устройства
        wcscpy_s(device.DeviceID, deviceID);  // Копируем ID устройства

        ++(*deviceCount);  // Увеличиваем счетчик количества устройств
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);  // Освобождаем ресурсы, связанные с списком устройств
    return devices;  // Возвращаем массив с информацией о подключенных устройствах
}