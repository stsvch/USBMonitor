#include "statistic.h"

constexpr size_t BUFFER_SIZE = 1024 * 1024; // Размер буфера 1 МБ

double MeasureWriteSpeed(const WCHAR* volumePath) 
{
    const size_t fileSizeMB = 10; // Размер тестового файла в МБ
    const size_t fileSize = fileSizeMB * BUFFER_SIZE; // Размер файла в байтах
    WCHAR testFile[MAX_PATH];
    swprintf_s(testFile, MAX_PATH, L"%s\\test_speed.tmp", volumePath); // Исправление пути

    FILE* file = nullptr;
    if (_wfopen_s(&file, testFile, L"wb") != 0 || !file) return 0;

    char* buffer = new char[BUFFER_SIZE]; // Буфер размером 1 МБ
    memset(buffer, 'A', BUFFER_SIZE); // Заполнение буфера фиктивными данными

    LARGE_INTEGER frequency, start, end;
    if (!QueryPerformanceFrequency(&frequency)) {
        delete[] buffer;
        fclose(file);
        return 0;
    }

    QueryPerformanceCounter(&start); // Запуск таймера

    for (size_t written = 0; written < fileSize; written += BUFFER_SIZE) {
        size_t writeSize = (fileSize - written) > BUFFER_SIZE ? BUFFER_SIZE : (fileSize - written);
        if (fwrite(buffer, 1, writeSize, file) != writeSize) {
            delete[] buffer;
            fclose(file);
            _wremove(testFile);
            return 0;
        }
    }

    QueryPerformanceCounter(&end); // Остановка таймера
    fclose(file); // Закрытие файла
    delete[] buffer; // Освобождение памяти буфера
    _wremove(testFile); // Удаление временного файла

    double elapsed = static_cast<double>(end.QuadPart - start.QuadPart) / frequency.QuadPart;
    if (elapsed == 0) return 0;
    return fileSizeMB / elapsed;
}

double MeasureReadSpeed(const WCHAR* volumePath) {
    const size_t fileSizeMB = 10; // Размер тестового файла в МБ
    const size_t fileSize = fileSizeMB * BUFFER_SIZE; // Размер файла в байтах
    WCHAR testFile[MAX_PATH];

    // Создаем путь для тестового файла
    swprintf_s(testFile, MAX_PATH, L"%s\\test_speed.tmp", volumePath);

    // Создаем тестовый файл для чтения
    FILE* tempFile = nullptr;
    if (_wfopen_s(&tempFile, testFile, L"wb") != 0 || !tempFile) {
        return 0; // Ошибка создания тестового файла
    }

    // Записываем данные в тестовый файл
    char* buffer = new char[BUFFER_SIZE];
    memset(buffer, 'A', BUFFER_SIZE); // Заполняем буфер символами

    for (size_t written = 0; written < fileSize; written += BUFFER_SIZE) {
        size_t writeSize = (fileSize - written) > BUFFER_SIZE ? BUFFER_SIZE : (fileSize - written);
        if (fwrite(buffer, 1, writeSize, tempFile) != writeSize) {
            delete[] buffer;
            fclose(tempFile);
            _wremove(testFile);
            return 0; // Ошибка записи в файл
        }
    }
    fclose(tempFile);

    // Измеряем скорость чтения
    FILE* file = nullptr;
    if (_wfopen_s(&file, testFile, L"rb") != 0 || !file) {
        delete[] buffer;
        return 0; // Ошибка открытия тестового файла для чтения
    }

    LARGE_INTEGER frequency, start, end;
    if (!QueryPerformanceFrequency(&frequency)) {
        delete[] buffer;
        fclose(file);
        _wremove(testFile);
        return 0; // Ошибка получения частоты таймера
    }

    QueryPerformanceCounter(&start);

    for (size_t read = 0; read < fileSize; read += BUFFER_SIZE) {
        size_t readSize = (fileSize - read) > BUFFER_SIZE ? BUFFER_SIZE : (fileSize - read);
        if (fread(buffer, 1, readSize, file) != readSize) {
            delete[] buffer;
            fclose(file);
            _wremove(testFile);
            return 0; // Ошибка чтения из файла
        }
    }

    QueryPerformanceCounter(&end);
    fclose(file);
    delete[] buffer;

    // Удаляем тестовый файл
    _wremove(testFile);

    // Вычисляем время в секундах
    double elapsed = static_cast<double>(end.QuadPart - start.QuadPart) / frequency.QuadPart;
    if (elapsed == 0) return 0;

    // Возвращаем скорость чтения в МБ/с
    return fileSizeMB / elapsed;
}


// Получение типа устройства
// Эта функция определяет класс устройства (например, HID, USB Mass Storage) по DeviceID.
void GetDeviceType(const WCHAR* deviceID, WCHAR* deviceTypeBuffer) {
    HDEVINFO hDevInfo = SetupDiGetClassDevs(NULL, NULL, NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT); // Получение списка всех устройств
    if (hDevInfo == INVALID_HANDLE_VALUE) {
        wcscpy_s(deviceTypeBuffer, MAX_BUFFER_SIZE, L"Unknown"); // Возврат Unknown при ошибке
        return;
    }

    SP_DEVINFO_DATA devInfoData;
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    // Перебор всех устройств
    for (int i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData); i++) {
        WCHAR currentDeviceID[MAX_BUFFER_SIZE];
        if (SetupDiGetDeviceInstanceId(hDevInfo, &devInfoData, currentDeviceID, MAX_BUFFER_SIZE, NULL)) {
            if (wcscmp(currentDeviceID, deviceID) == 0) {
                WCHAR deviceClass[MAX_BUFFER_SIZE];
                if (SetupDiGetDeviceRegistryProperty(hDevInfo, &devInfoData, SPDRP_CLASS, NULL, (PBYTE)deviceClass, sizeof(deviceClass), NULL)) {
                    wcscpy_s(deviceTypeBuffer, MAX_BUFFER_SIZE, deviceClass); // Запись класса устройства
                }
                else {
                    wcscpy_s(deviceTypeBuffer, MAX_BUFFER_SIZE, L"Unknown"); // Устройство найдено, но класс неизвестен
                }
                SetupDiDestroyDeviceInfoList(hDevInfo); // Освобождение ресурсов
                return;
            }
        }
    }

    wcscpy_s(deviceTypeBuffer, MAX_BUFFER_SIZE, L"Unknown"); // Устройство не найдено
    SetupDiDestroyDeviceInfoList(hDevInfo); // Освобождение ресурсов
}

bool GetDevicePath(const WCHAR* deviceID, WCHAR* devicePathBuffer) {
    static std::set<std::wstring> usedPaths; // Хранение уже найденных путей

    // Получаем список всех логических дисков
    DWORD drives = GetLogicalDrives();
    if (drives == 0) {
        wcscpy_s(devicePathBuffer, MAX_PATH, L"Unknown");
        return false;
    }

    WCHAR driveLetter[4] = L"A:\\";
    WCHAR volumeName[MAX_PATH];
    WCHAR parentDeviceID[MAX_PATH];
    WCHAR currentDeviceInstanceID[MAX_PATH];

    // Перебираем все диски
    for (int i = 0; i < 26; ++i) {
        if ((drives & (1 << i)) == 0) {
            continue; // Диск не подключен
        }

        driveLetter[0] = L'A' + i; // Формируем имя диска, например, "C:\\"

        // Проверяем, что это съёмный диск
        if (GetDriveType(driveLetter) != DRIVE_REMOVABLE) {
            continue;
        }

        // Проверяем, использовался ли уже этот путь
        if (usedPaths.find(driveLetter) != usedPaths.end()) {
            continue; // Этот путь уже был найден для другого устройства
        }

        // Получаем имя тома
        if (!GetVolumeNameForVolumeMountPoint(driveLetter, volumeName, MAX_PATH)) {
            continue; // Не удалось получить имя тома
        }

        // Получаем список устройств
        HDEVINFO deviceInfoSet = SetupDiGetClassDevs(NULL, NULL, NULL, DIGCF_PRESENT | DIGCF_ALLCLASSES);
        if (deviceInfoSet == INVALID_HANDLE_VALUE) {
            continue;
        }

        SP_DEVINFO_DATA deviceInfoData = {};
        deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

        // Перебираем устройства
        for (DWORD j = 0; SetupDiEnumDeviceInfo(deviceInfoSet, j, &deviceInfoData); j++) {
            if (SetupDiGetDeviceInstanceId(deviceInfoSet, &deviceInfoData, currentDeviceInstanceID, MAX_PATH, NULL)) {
                // Проверяем связь с родителем
                DEVINST devInst = 0, parentDevInst = 0;
                CONFIGRET cr = CM_Locate_DevNode(&devInst, currentDeviceInstanceID, CM_LOCATE_DEVNODE_NORMAL);
                if (cr == CR_SUCCESS) {
                    cr = CM_Get_Parent(&parentDevInst, devInst, 0);
                    if (cr == CR_SUCCESS) {
                        CM_Get_Device_ID(parentDevInst, parentDeviceID, MAX_PATH, 0);
                        if (wcscmp(parentDeviceID, deviceID) == 0) {
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
// Функция для извлечения последних событий для устройства по его deviceId
void AppendToBuffer(std::wstringstream& buffer, const std::wstring& message) {
    buffer << message << std::endl;
}

void ParseAndStoreEventXML(const wchar_t* xml, std::wstringstream& buffer) {
    IXmlReader* pReader = nullptr;
    IStream* pStream = SHCreateMemStream(reinterpret_cast<const BYTE*>(xml), wcslen(xml) * sizeof(wchar_t));

    if (!pStream) {
        AppendToBuffer(buffer, L"Ошибка при создании потока из XML.");
        return;
    }

    HRESULT hr = CreateXmlReader(IID_PPV_ARGS(&pReader), nullptr);
    if (FAILED(hr)) {
        AppendToBuffer(buffer, L"Ошибка при создании XML-ридера.");
        pStream->Release();
        return;
    }

    hr = pReader->SetInput(pStream);
    if (FAILED(hr)) {
        AppendToBuffer(buffer, L"Ошибка при установке входных данных для XML-ридера.");
        pReader->Release();
        pStream->Release();
        return;
    }

    XmlNodeType nodeType;
    std::wstring elementName;
    std::wstring eventID, timeCreated, level, data;

    while (S_OK == pReader->Read(&nodeType)) {
        if (nodeType == XmlNodeType_Element) {
            const WCHAR* localName = nullptr;
            hr = pReader->GetLocalName(&localName, nullptr);
            if (FAILED(hr)) continue;

            elementName = localName;

            if (elementName == L"EventID" || elementName == L"Level" || elementName == L"Data") {
                if (pReader->Read(&nodeType) == S_OK && nodeType == XmlNodeType_Text) {
                    const WCHAR* value = nullptr;
                    hr = pReader->GetValue(&value, nullptr);
                    if (SUCCEEDED(hr)) {
                        if (elementName == L"EventID") {
                            eventID = value;
                        }
                        else if (elementName == L"Level") {
                            level = value;
                        }
                        else if (elementName == L"Data") {
                            data += value;
                            data += L"; ";
                        }
                    }
                }
            }
            else if (elementName == L"TimeCreated") {
                const WCHAR* attributeName = nullptr;
                const WCHAR* attributeValue = nullptr;
                while (pReader->MoveToNextAttribute() == S_OK) {
                    hr = pReader->GetLocalName(&attributeName, nullptr);
                    if (SUCCEEDED(hr) && wcscmp(attributeName, L"SystemTime") == 0) {
                        hr = pReader->GetValue(&attributeValue, nullptr);
                        if (SUCCEEDED(hr)) {
                            timeCreated = attributeValue;
                        }
                    }
                }
            }
        }
    }

    // Записываем читаемое событие в буфер
    buffer << L"Событие: " << std::endl;
    buffer << L"  EventID: " << eventID << std::endl;
    buffer << L"  TimeCreated: " << timeCreated << std::endl;
    buffer << L"  Level: " << level << std::endl;
    buffer << L"  Data: " << data << std::endl;

    pReader->Release();
    pStream->Release();
}


void FetchLast10Events(const std::wstring& log, const std::wstring& deviceInstanceID, std::wstringstream& buffer) {
    buffer << L"Поиск в журнале: " << log << std::endl;

    // Формируем запрос с фильтром по DeviceInstanceID
    std::wstring query = L"*[EventData/Data[@Name='DeviceInstanceId']='" + deviceInstanceID + L"']";

    EVT_HANDLE hQuery = EvtQuery(NULL, log.c_str(), query.c_str(), EvtQueryReverseDirection);
    if (hQuery == NULL) {
        buffer << L"Ошибка при создании запроса к журналу: " << log << L". Код ошибки: " << GetLastError() << std::endl;
        return;
    }

    EVT_HANDLE hEvents[10];
    DWORD dwReturned = 0;

    // Запрос последних 10 событий
    if (!EvtNext(hQuery, ARRAYSIZE(hEvents), hEvents, INFINITE, 0, &dwReturned)) {
        DWORD error = GetLastError();
        if (error != ERROR_NO_MORE_ITEMS) {
            buffer << L"Ошибка при получении событий из журнала: " << log << L". Код ошибки: " << error << std::endl;
        }
        EvtClose(hQuery);
        return;
    }

    // Обработка событий
    for (DWORD i = 0; i < dwReturned; ++i) {
        DWORD dwBufferSize = 0;
        DWORD dwPropertyCount = 0;

        // Получаем размер буфера для XML
        if (!EvtRender(NULL, hEvents[i], EvtRenderEventXml, 0, NULL, &dwBufferSize, &dwPropertyCount)) {
            if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
                buffer << L"Ошибка при рендеринге события." << std::endl;
                EvtClose(hEvents[i]);
                continue;
            }
        }

        // Выделяем память для буфера
        std::unique_ptr<wchar_t[]> pXmlBuffer(new wchar_t[dwBufferSize]);

        // Извлекаем XML описание события
        if (!EvtRender(NULL, hEvents[i], EvtRenderEventXml, dwBufferSize, pXmlBuffer.get(), &dwBufferSize, &dwPropertyCount)) {
            buffer << L"Ошибка при извлечении XML события." << std::endl;
            EvtClose(hEvents[i]);
            continue;
        }

        // Парсим и записываем событие в буфер
        ParseAndStoreEventXML(pXmlBuffer.get(), buffer);

        // Закрываем хэндл события
        EvtClose(hEvents[i]);
    }

    // Закрываем хэндл запроса
    EvtClose(hQuery);
}

void FetchEventsFromLogs(const std::wstring& deviceInstanceID, std::wstringstream& buffer) {
    const std::vector<std::wstring> logs = {
        L"System",
        L"Application",
        L"Security",
        L"Setup",
        L"Microsoft-Windows-UserPnp/Operational",
        L"Microsoft-Windows-Kernel-PnP/Configuration",
        L"Microsoft-Windows-PowerShell/Operational", // Журнал PowerShell
        L"Microsoft-Windows-TerminalServices-LocalSessionManager/Operational", // Журнал Remote Desktop
        L"Microsoft-Windows-DeviceSetupManager/Admin", // Журнал управления устройствами
        L"Microsoft-Windows-EventCollector/Operational", // Сборщик событий
        L"HP Analytics",
        L"Microsoft-Windows-Diagnostics-Performance/Operational", // Диагностика производительности
        L"Windows PowerShell",
        L"Служба управления ключами",
        L"События оборудования",
        L"Microsoft-Windows-Kernel-PnP/Driver Watchdog",
        L"Microsoft-Windows-Kernel-Power/Thermal-Operational",
        L"Microsoft-Windows-Kernel-Boot/Operational",
        L"Microsoft-Windows-Kernel-EventTracing/Admin",
        L"Microsoft-Windows-WPD-MTPClassDriver/Operational",
    L"Microsoft-Windows-UserPnp/DeviceInstall",
    L"Microsoft-Windows-UserPnp/ActionCenter",
    L"Microsoft-Windows-User-Loader/Operational",
    L"Microsoft-Windows-User Profile Service/Operational",
    L"Microsoft-Windows-User Device Registration/Admin",
    L"Microsoft-Windows-User Control Panel/Operational"
    };
    // Цикл по каждому журналу
    for (const auto& log : logs) {
        FetchLast10Events(log, deviceInstanceID, buffer);
    }
}

wchar_t* ConvertToWideCharBuffer(const std::wstringstream& stream) {
    std::wstring wstr = stream.str(); // Получаем строку
    size_t size = (wstr.length() + 1) * sizeof(wchar_t);
    wchar_t* buffer = new wchar_t[wstr.length() + 1];
    wcscpy_s(buffer, wstr.length() + 1, wstr.c_str());
    return buffer; // Возвращаем указатель на буфер
}

void GetEvent(wchar_t* buffer,size_t bSize, wchar_t* deviceID)
{
    // Установка кодировки консоли на UTF-8
    SetConsoleOutputCP(CP_UTF8);
    _setmode(_fileno(stdout), _O_U8TEXT);
    _setmode(_fileno(stderr), _O_U8TEXT);

    // Укажите DeviceInstanceID
    std::wstring deviceInstanceID = deviceID;

    // Буфер для хранения событий
    std::wstringstream eventBuffer;

    // Поиск событий по DeviceInstanceID в каждом журнале
    FetchEventsFromLogs(deviceInstanceID, eventBuffer);

    // Получаем строку из eventBuffer
    std::wstring eventString = eventBuffer.str();

    // Убедимся, что размер строки не превышает размер буфера
    if (eventString.size() >= bSize - 1) {
        throw std::overflow_error("Buffer size is too small to hold the event data.");
    }

    // Копируем содержимое eventString в переданный buffer
    wcscpy_s(buffer, bSize, eventString.c_str());

    // Выводим содержимое буфера
    std::wcout << buffer << std::endl;
}



