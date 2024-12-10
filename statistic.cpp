#include "statistic.h"

constexpr size_t BUFFER_SIZE = 1024 * 1024; // ������ ������ 1 ��

double MeasureWriteSpeed(const WCHAR* volumePath) 
{
    const size_t fileSizeMB = 10; // ������ ��������� ����� � ��
    const size_t fileSize = fileSizeMB * BUFFER_SIZE; // ������ ����� � ������
    WCHAR testFile[MAX_PATH];
    swprintf_s(testFile, MAX_PATH, L"%s\\test_speed.tmp", volumePath); // ����������� ����

    FILE* file = nullptr;
    if (_wfopen_s(&file, testFile, L"wb") != 0 || !file) return 0;

    char* buffer = new char[BUFFER_SIZE]; // ����� �������� 1 ��
    memset(buffer, 'A', BUFFER_SIZE); // ���������� ������ ���������� �������

    LARGE_INTEGER frequency, start, end;
    if (!QueryPerformanceFrequency(&frequency)) {
        delete[] buffer;
        fclose(file);
        return 0;
    }

    QueryPerformanceCounter(&start); // ������ �������

    for (size_t written = 0; written < fileSize; written += BUFFER_SIZE) {
        size_t writeSize = (fileSize - written) > BUFFER_SIZE ? BUFFER_SIZE : (fileSize - written);
        if (fwrite(buffer, 1, writeSize, file) != writeSize) {
            delete[] buffer;
            fclose(file);
            _wremove(testFile);
            return 0;
        }
    }

    QueryPerformanceCounter(&end); // ��������� �������
    fclose(file); // �������� �����
    delete[] buffer; // ������������ ������ ������
    _wremove(testFile); // �������� ���������� �����

    double elapsed = static_cast<double>(end.QuadPart - start.QuadPart) / frequency.QuadPart;
    if (elapsed == 0) return 0;
    return fileSizeMB / elapsed;
}

double MeasureReadSpeed(const WCHAR* volumePath) {
    const size_t fileSizeMB = 10; // ������ ��������� ����� � ��
    const size_t fileSize = fileSizeMB * BUFFER_SIZE; // ������ ����� � ������
    WCHAR testFile[MAX_PATH];

    // ������� ���� ��� ��������� �����
    swprintf_s(testFile, MAX_PATH, L"%s\\test_speed.tmp", volumePath);

    // ������� �������� ���� ��� ������
    FILE* tempFile = nullptr;
    if (_wfopen_s(&tempFile, testFile, L"wb") != 0 || !tempFile) {
        return 0; // ������ �������� ��������� �����
    }

    // ���������� ������ � �������� ����
    char* buffer = new char[BUFFER_SIZE];
    memset(buffer, 'A', BUFFER_SIZE); // ��������� ����� ���������

    for (size_t written = 0; written < fileSize; written += BUFFER_SIZE) {
        size_t writeSize = (fileSize - written) > BUFFER_SIZE ? BUFFER_SIZE : (fileSize - written);
        if (fwrite(buffer, 1, writeSize, tempFile) != writeSize) {
            delete[] buffer;
            fclose(tempFile);
            _wremove(testFile);
            return 0; // ������ ������ � ����
        }
    }
    fclose(tempFile);

    // �������� �������� ������
    FILE* file = nullptr;
    if (_wfopen_s(&file, testFile, L"rb") != 0 || !file) {
        delete[] buffer;
        return 0; // ������ �������� ��������� ����� ��� ������
    }

    LARGE_INTEGER frequency, start, end;
    if (!QueryPerformanceFrequency(&frequency)) {
        delete[] buffer;
        fclose(file);
        _wremove(testFile);
        return 0; // ������ ��������� ������� �������
    }

    QueryPerformanceCounter(&start);

    for (size_t read = 0; read < fileSize; read += BUFFER_SIZE) {
        size_t readSize = (fileSize - read) > BUFFER_SIZE ? BUFFER_SIZE : (fileSize - read);
        if (fread(buffer, 1, readSize, file) != readSize) {
            delete[] buffer;
            fclose(file);
            _wremove(testFile);
            return 0; // ������ ������ �� �����
        }
    }

    QueryPerformanceCounter(&end);
    fclose(file);
    delete[] buffer;

    // ������� �������� ����
    _wremove(testFile);

    // ��������� ����� � ��������
    double elapsed = static_cast<double>(end.QuadPart - start.QuadPart) / frequency.QuadPart;
    if (elapsed == 0) return 0;

    // ���������� �������� ������ � ��/�
    return fileSizeMB / elapsed;
}


// ��������� ���� ����������
// ��� ������� ���������� ����� ���������� (��������, HID, USB Mass Storage) �� DeviceID.
void GetDeviceType(const WCHAR* deviceID, WCHAR* deviceTypeBuffer) {
    HDEVINFO hDevInfo = SetupDiGetClassDevs(NULL, NULL, NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT); // ��������� ������ ���� ���������
    if (hDevInfo == INVALID_HANDLE_VALUE) {
        wcscpy_s(deviceTypeBuffer, MAX_BUFFER_SIZE, L"Unknown"); // ������� Unknown ��� ������
        return;
    }

    SP_DEVINFO_DATA devInfoData;
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    // ������� ���� ���������
    for (int i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData); i++) {
        WCHAR currentDeviceID[MAX_BUFFER_SIZE];
        if (SetupDiGetDeviceInstanceId(hDevInfo, &devInfoData, currentDeviceID, MAX_BUFFER_SIZE, NULL)) {
            if (wcscmp(currentDeviceID, deviceID) == 0) {
                WCHAR deviceClass[MAX_BUFFER_SIZE];
                if (SetupDiGetDeviceRegistryProperty(hDevInfo, &devInfoData, SPDRP_CLASS, NULL, (PBYTE)deviceClass, sizeof(deviceClass), NULL)) {
                    wcscpy_s(deviceTypeBuffer, MAX_BUFFER_SIZE, deviceClass); // ������ ������ ����������
                }
                else {
                    wcscpy_s(deviceTypeBuffer, MAX_BUFFER_SIZE, L"Unknown"); // ���������� �������, �� ����� ����������
                }
                SetupDiDestroyDeviceInfoList(hDevInfo); // ������������ ��������
                return;
            }
        }
    }

    wcscpy_s(deviceTypeBuffer, MAX_BUFFER_SIZE, L"Unknown"); // ���������� �� �������
    SetupDiDestroyDeviceInfoList(hDevInfo); // ������������ ��������
}

bool GetDevicePath(const WCHAR* deviceID, WCHAR* devicePathBuffer) {
    static std::set<std::wstring> usedPaths; // �������� ��� ��������� �����

    // �������� ������ ���� ���������� ������
    DWORD drives = GetLogicalDrives();
    if (drives == 0) {
        wcscpy_s(devicePathBuffer, MAX_PATH, L"Unknown");
        return false;
    }

    WCHAR driveLetter[4] = L"A:\\";
    WCHAR volumeName[MAX_PATH];
    WCHAR parentDeviceID[MAX_PATH];
    WCHAR currentDeviceInstanceID[MAX_PATH];

    // ���������� ��� �����
    for (int i = 0; i < 26; ++i) {
        if ((drives & (1 << i)) == 0) {
            continue; // ���� �� ���������
        }

        driveLetter[0] = L'A' + i; // ��������� ��� �����, ��������, "C:\\"

        // ���������, ��� ��� ������� ����
        if (GetDriveType(driveLetter) != DRIVE_REMOVABLE) {
            continue;
        }

        // ���������, ������������� �� ��� ���� ����
        if (usedPaths.find(driveLetter) != usedPaths.end()) {
            continue; // ���� ���� ��� ��� ������ ��� ������� ����������
        }

        // �������� ��� ����
        if (!GetVolumeNameForVolumeMountPoint(driveLetter, volumeName, MAX_PATH)) {
            continue; // �� ������� �������� ��� ����
        }

        // �������� ������ ���������
        HDEVINFO deviceInfoSet = SetupDiGetClassDevs(NULL, NULL, NULL, DIGCF_PRESENT | DIGCF_ALLCLASSES);
        if (deviceInfoSet == INVALID_HANDLE_VALUE) {
            continue;
        }

        SP_DEVINFO_DATA deviceInfoData = {};
        deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

        // ���������� ����������
        for (DWORD j = 0; SetupDiEnumDeviceInfo(deviceInfoSet, j, &deviceInfoData); j++) {
            if (SetupDiGetDeviceInstanceId(deviceInfoSet, &deviceInfoData, currentDeviceInstanceID, MAX_PATH, NULL)) {
                // ��������� ����� � ���������
                DEVINST devInst = 0, parentDevInst = 0;
                CONFIGRET cr = CM_Locate_DevNode(&devInst, currentDeviceInstanceID, CM_LOCATE_DEVNODE_NORMAL);
                if (cr == CR_SUCCESS) {
                    cr = CM_Get_Parent(&parentDevInst, devInst, 0);
                    if (cr == CR_SUCCESS) {
                        CM_Get_Device_ID(parentDevInst, parentDeviceID, MAX_PATH, 0);
                        if (wcscmp(parentDeviceID, deviceID) == 0) {
                            // ���������� �������, ��������� ����� �����
                            wcscpy_s(devicePathBuffer, MAX_PATH, driveLetter);
                            usedPaths.insert(driveLetter); // ��������� ���� � ������
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
// ������� ��� ���������� ��������� ������� ��� ���������� �� ��� deviceId
void AppendToBuffer(std::wstringstream& buffer, const std::wstring& message) {
    buffer << message << std::endl;
}

void ParseAndStoreEventXML(const wchar_t* xml, std::wstringstream& buffer) {
    IXmlReader* pReader = nullptr;
    IStream* pStream = SHCreateMemStream(reinterpret_cast<const BYTE*>(xml), wcslen(xml) * sizeof(wchar_t));

    if (!pStream) {
        AppendToBuffer(buffer, L"������ ��� �������� ������ �� XML.");
        return;
    }

    HRESULT hr = CreateXmlReader(IID_PPV_ARGS(&pReader), nullptr);
    if (FAILED(hr)) {
        AppendToBuffer(buffer, L"������ ��� �������� XML-������.");
        pStream->Release();
        return;
    }

    hr = pReader->SetInput(pStream);
    if (FAILED(hr)) {
        AppendToBuffer(buffer, L"������ ��� ��������� ������� ������ ��� XML-������.");
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

    // ���������� �������� ������� � �����
    buffer << L"�������: " << std::endl;
    buffer << L"  EventID: " << eventID << std::endl;
    buffer << L"  TimeCreated: " << timeCreated << std::endl;
    buffer << L"  Level: " << level << std::endl;
    buffer << L"  Data: " << data << std::endl;

    pReader->Release();
    pStream->Release();
}


void FetchLast10Events(const std::wstring& log, const std::wstring& deviceInstanceID, std::wstringstream& buffer) {
    buffer << L"����� � �������: " << log << std::endl;

    // ��������� ������ � �������� �� DeviceInstanceID
    std::wstring query = L"*[EventData/Data[@Name='DeviceInstanceId']='" + deviceInstanceID + L"']";

    EVT_HANDLE hQuery = EvtQuery(NULL, log.c_str(), query.c_str(), EvtQueryReverseDirection);
    if (hQuery == NULL) {
        buffer << L"������ ��� �������� ������� � �������: " << log << L". ��� ������: " << GetLastError() << std::endl;
        return;
    }

    EVT_HANDLE hEvents[10];
    DWORD dwReturned = 0;

    // ������ ��������� 10 �������
    if (!EvtNext(hQuery, ARRAYSIZE(hEvents), hEvents, INFINITE, 0, &dwReturned)) {
        DWORD error = GetLastError();
        if (error != ERROR_NO_MORE_ITEMS) {
            buffer << L"������ ��� ��������� ������� �� �������: " << log << L". ��� ������: " << error << std::endl;
        }
        EvtClose(hQuery);
        return;
    }

    // ��������� �������
    for (DWORD i = 0; i < dwReturned; ++i) {
        DWORD dwBufferSize = 0;
        DWORD dwPropertyCount = 0;

        // �������� ������ ������ ��� XML
        if (!EvtRender(NULL, hEvents[i], EvtRenderEventXml, 0, NULL, &dwBufferSize, &dwPropertyCount)) {
            if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
                buffer << L"������ ��� ���������� �������." << std::endl;
                EvtClose(hEvents[i]);
                continue;
            }
        }

        // �������� ������ ��� ������
        std::unique_ptr<wchar_t[]> pXmlBuffer(new wchar_t[dwBufferSize]);

        // ��������� XML �������� �������
        if (!EvtRender(NULL, hEvents[i], EvtRenderEventXml, dwBufferSize, pXmlBuffer.get(), &dwBufferSize, &dwPropertyCount)) {
            buffer << L"������ ��� ���������� XML �������." << std::endl;
            EvtClose(hEvents[i]);
            continue;
        }

        // ������ � ���������� ������� � �����
        ParseAndStoreEventXML(pXmlBuffer.get(), buffer);

        // ��������� ����� �������
        EvtClose(hEvents[i]);
    }

    // ��������� ����� �������
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
        L"Microsoft-Windows-PowerShell/Operational", // ������ PowerShell
        L"Microsoft-Windows-TerminalServices-LocalSessionManager/Operational", // ������ Remote Desktop
        L"Microsoft-Windows-DeviceSetupManager/Admin", // ������ ���������� ������������
        L"Microsoft-Windows-EventCollector/Operational", // ������� �������
        L"HP Analytics",
        L"Microsoft-Windows-Diagnostics-Performance/Operational", // ����������� ������������������
        L"Windows PowerShell",
        L"������ ���������� �������",
        L"������� ������������",
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
    // ���� �� ������� �������
    for (const auto& log : logs) {
        FetchLast10Events(log, deviceInstanceID, buffer);
    }
}

wchar_t* ConvertToWideCharBuffer(const std::wstringstream& stream) {
    std::wstring wstr = stream.str(); // �������� ������
    size_t size = (wstr.length() + 1) * sizeof(wchar_t);
    wchar_t* buffer = new wchar_t[wstr.length() + 1];
    wcscpy_s(buffer, wstr.length() + 1, wstr.c_str());
    return buffer; // ���������� ��������� �� �����
}

void GetEvent(wchar_t* buffer,size_t bSize, wchar_t* deviceID)
{
    // ��������� ��������� ������� �� UTF-8
    SetConsoleOutputCP(CP_UTF8);
    _setmode(_fileno(stdout), _O_U8TEXT);
    _setmode(_fileno(stderr), _O_U8TEXT);

    // ������� DeviceInstanceID
    std::wstring deviceInstanceID = deviceID;

    // ����� ��� �������� �������
    std::wstringstream eventBuffer;

    // ����� ������� �� DeviceInstanceID � ������ �������
    FetchEventsFromLogs(deviceInstanceID, eventBuffer);

    // �������� ������ �� eventBuffer
    std::wstring eventString = eventBuffer.str();

    // ��������, ��� ������ ������ �� ��������� ������ ������
    if (eventString.size() >= bSize - 1) {
        throw std::overflow_error("Buffer size is too small to hold the event data.");
    }

    // �������� ���������� eventString � ���������� buffer
    wcscpy_s(buffer, bSize, eventString.c_str());

    // ������� ���������� ������
    std::wcout << buffer << std::endl;
}



