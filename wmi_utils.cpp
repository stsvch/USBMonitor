#include "wmi_utils.h"

HRESULT InitWMI(IWbemLocator** pLoc, IWbemServices** pSvc) 
{
    HRESULT hres = NULL;

    hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres)) 
    {
        return hres; 
    }

    hres = CoInitializeSecurity(
        NULL, -1, NULL, NULL,
        RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL, EOAC_NONE, NULL);
    if (FAILED(hres)) 
    {
        CoUninitialize(); 
        return hres;
    }

    hres = CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)pLoc);
    if (FAILED(hres)) 
    {
        CoUninitialize();
        return hres;
    }

    hres = (*pLoc)->ConnectServer(
        _bstr_t(L"ROOT\\CIMV2"), NULL, NULL, 0, NULL, 0, 0, pSvc);
    if (FAILED(hres)) 
    {
        (*pLoc)->Release(); 
        CoUninitialize(); 
        return hres;
    }
    return S_OK; 
}

Map* FullQueryDevices(IWbemServices* pSvc, const wchar_t* query, int* count) 
{
    HRESULT hres;

    IEnumWbemClassObject* pEnumerator = NULL;
    hres = pSvc->ExecQuery(
        bstr_t(L"WQL"),
        bstr_t(query),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL, &pEnumerator);

    if (FAILED(hres)) 
    {
        *count = 0;
        return nullptr; 
    }

    size_t capacity = 10; 
    *count = 0;
    Map* results = new Map[capacity];

    IWbemClassObject* pclsObj = NULL;
    ULONG uReturn = 0;

    while (true) 
    {
        hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
        if (FAILED(hres) || uReturn == 0) 
        {
            break; 
        }
        SAFEARRAY* pNames = NULL;
        hres = pclsObj->GetNames(NULL, WBEM_FLAG_ALWAYS, NULL, &pNames);
        if (FAILED(hres) || !pNames) {
            pclsObj->Release();
            continue;
        }

        LONG lBound, uBound;
        SafeArrayGetLBound(pNames, 1, &lBound);
        SafeArrayGetUBound(pNames, 1, &uBound);

        for (LONG i = lBound; i <= uBound; ++i) 
        {
            BSTR propertyName;
            SafeArrayGetElement(pNames, &i, &propertyName);

            VARIANT vtProp;
            VariantInit(&vtProp);

            hres = pclsObj->Get(propertyName, 0, &vtProp, 0, 0);
            if (SUCCEEDED(hres)) 
            {
                if (*count == capacity) 
                {
                    capacity *= 2; 
                    Map* newResults = new Map[capacity];
                    for (size_t j = 0; j < *count; ++j) {
                        newResults[j] = results[j];
                    }
                    delete[] results;
                    results = newResults;
                }

                wcsncpy_s(results[*count].Key, propertyName, MAX_BUFFER_SIZE - 1);

                switch (vtProp.vt)
                {
                case VT_BSTR:
                    wcsncpy_s(results[*count].Value, vtProp.bstrVal ? vtProp.bstrVal : L"(null)", MAX_BUFFER_SIZE - 1);
                    break;
                case VT_I4:
                    swprintf_s(results[*count].Value, MAX_BUFFER_SIZE, L"%d", vtProp.lVal);
                    break;
                case VT_UI4:
                    swprintf_s(results[*count].Value, MAX_BUFFER_SIZE, L"%u", vtProp.ulVal);
                    break;
                case VT_BOOL:
                    wcsncpy_s(results[*count].Value, vtProp.boolVal ? L"True" : L"False", MAX_BUFFER_SIZE - 1);
                    break;
                case VT_DATE:
                    SYSTEMTIME st;
                    if (VariantTimeToSystemTime(vtProp.date, &st)) 
                    {
                        swprintf_s(results[*count].Value, MAX_BUFFER_SIZE, L"%02d/%02d/%04d %02d:%02d:%02d",
                            st.wMonth, st.wDay, st.wYear, st.wHour, st.wMinute, st.wSecond);
                    }
                    else 
                    {
                        wcsncpy_s(results[*count].Value, L"(invalid date)", MAX_BUFFER_SIZE - 1);
                    }
                    break;
                case VT_NULL:
                    wcsncpy_s(results[*count].Value, L"(null)", MAX_BUFFER_SIZE - 1);
                    break;
                default:
                    wcsncpy_s(results[*count].Value, L"(unknown type)", MAX_BUFFER_SIZE - 1);
                    break;
                }
                ++(*count); 
            }

            VariantClear(&vtProp);
            SysFreeString(propertyName); 
        }

        SafeArrayDestroy(pNames);
        pclsObj->Release();
    }

    pEnumerator->Release();
    return results; 
}

DeviceInfo* QueryDevices(IWbemServices* pSvc, const wchar_t* query, int* count) 
{
    HRESULT hres;

    IEnumWbemClassObject* pEnumerator = NULL;
    hres = pSvc->ExecQuery(
        bstr_t(L"WQL"),
        bstr_t(query),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL, &pEnumerator);

    if (FAILED(hres))
    {
        *count = 0;
        return NULL; 
    }

    IWbemClassObject* pclsObj = NULL;
    ULONG uReturn = 0;

    size_t capacity = 20; 
    *count = 0;
    DeviceInfo* devices = new DeviceInfo[capacity]; 

    while (true) 
    {
        hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
        if (FAILED(hres)) break;
        if (uReturn == 0) break;

        if (*count == capacity) 
        {
            capacity *= 2; 
            DeviceInfo* newDevices = new DeviceInfo[capacity];
            for (size_t i = 0; i < *count; ++i) 
            {
                newDevices[i] = devices[i];
            }
            delete[] devices;
            devices = newDevices;
        }

        VARIANT vtProp;
        VariantInit(&vtProp);

        DeviceInfo& device = devices[*count];

        hres = pclsObj->Get(L"DeviceID", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hres) && vtProp.vt == VT_BSTR)
        {
            wcsncpy_s(device.DeviceID, vtProp.bstrVal, MAX_BUFFER_SIZE); 
            device.DeviceID[MAX_BUFFER_SIZE - 1] = L'\0';
        }
        else
        {
            device.DeviceID[0] = L'\0'; 
        }
        VariantClear(&vtProp);

        hres = pclsObj->Get(L"Description", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hres) && vtProp.vt == VT_BSTR) 
        {
            wcsncpy_s(device.Description, vtProp.bstrVal, MAX_BUFFER_SIZE);
            device.Description[MAX_BUFFER_SIZE - 1] = L'\0';
        }
        else 
        {
            device.Description[0] = L'\0';
        }
        VariantClear(&vtProp);

        hres = pclsObj->Get(L"Status", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hres) && vtProp.vt == VT_BSTR)
        {
            wcsncpy_s(device.Status, vtProp.bstrVal, MAX_BUFFER_SIZE);
            device.Status[MAX_BUFFER_SIZE - 1] = L'\0';
        }
        else 
        {
            device.Status[0] = L'\0';
        }
        VariantClear(&vtProp);

        hres = pclsObj->Get(L"Caption", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hres) && vtProp.vt == VT_BSTR) {
            wcsncpy_s(device.Caption, vtProp.bstrVal, MAX_BUFFER_SIZE); 
            device.Caption[MAX_BUFFER_SIZE - 1] = L'\0'; 
        }
        else {
            device.Caption[0] = L'\0'; 
        }
        VariantClear(&vtProp);

        ++(*count); 

        pclsObj->Release();
    }

    pEnumerator->Release();
    return devices; 
}

BOOL IsUsbDeviceConnected(IWbemServices* pSvc, const WCHAR* deviceID)
{
    HRESULT hres;

    WCHAR query[512];  
    swprintf_s(query, 512, L"SELECT * FROM Win32_PnPEntity WHERE DeviceID = '%s'", deviceID);

    IEnumWbemClassObject* pEnumerator = NULL;
    hres = pSvc->ExecQuery(bstr_t(L"WQL"), bstr_t(query),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL, &pEnumerator);

    if (FAILED(hres)) return FALSE; 

    IWbemClassObject* pClassObject = NULL;
    ULONG returnCount = 0;
    BOOL isConnected = FALSE;

    while (true)
    {
        hres = pEnumerator->Next(WBEM_INFINITE, 1, &pClassObject, &returnCount);
        if (returnCount == 0) break;  

        VARIANT vtProp;

        hres = pClassObject->Get(L"Status", 0, &vtProp, 0, 0);
        if (FAILED(hres)) 
        {
            pClassObject->Release();
            continue;  
        }

        if (wcscmp(vtProp.bstrVal, L"OK") == 0)
        {
            isConnected = TRUE;
        }

        VariantClear(&vtProp);

        hres = pClassObject->Get(L"ConfigManagerErrorCode", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hres)) 
        {
            if (vtProp.intVal != 0) 
            {  
                isConnected = FALSE;
            }
        }
        VariantClear(&vtProp);

        pClassObject->Release();
        break;  
    }
    pEnumerator->Release();
    return isConnected; 
}

BOOL IsBluetoothDeviceConnected(const WCHAR* deviceName) 
{
    BLUETOOTH_DEVICE_SEARCH_PARAMS searchParams;
    BLUETOOTH_DEVICE_INFO deviceInfo;
    HANDLE hFind = NULL;

    ZeroMemory(&searchParams, sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS));
    searchParams.dwSize = sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS);
    searchParams.fReturnAuthenticated = TRUE;  
    searchParams.fReturnConnected = TRUE;    
    searchParams.fReturnRemembered = TRUE;  

    ZeroMemory(&deviceInfo, sizeof(BLUETOOTH_DEVICE_INFO));
    deviceInfo.dwSize = sizeof(BLUETOOTH_DEVICE_INFO);

    hFind = BluetoothFindFirstDevice(&searchParams, &deviceInfo);
    if (hFind == NULL) return FALSE;
    do 
    {
        if (_wcsicmp(deviceInfo.szName, deviceName) == 0) 
        {
            if (deviceInfo.fConnected) 
            {
                BluetoothFindDeviceClose(hFind);
                return TRUE;
            }
            else 
            {
                BluetoothFindDeviceClose(hFind);
                return FALSE;
            }
        }
    } while (BluetoothFindNextDevice(hFind, &deviceInfo));

    BluetoothFindDeviceClose(hFind);
    return FALSE;
}

BOOL ChangeDeviceState(const WCHAR* deviceID, BOOL connected)
{
    HDEVINFO hDevInfo = SetupDiGetClassDevs(NULL, NULL, NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (hDevInfo == INVALID_HANDLE_VALUE) return FALSE;

    SP_DEVINFO_DATA devInfoData;
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    DWORD index = 0;
    while (SetupDiEnumDeviceInfo(hDevInfo, index++, &devInfoData))
    {
        WCHAR currentDeviceID[MAX_DEVICE_ID_LEN];
        if (SetupDiGetDeviceInstanceIdW(hDevInfo, &devInfoData, currentDeviceID, MAX_DEVICE_ID_LEN, NULL))
        {
            if (_wcsicmp(currentDeviceID, deviceID) == 0)
            {
                SP_PROPCHANGE_PARAMS propChangeParams;
                propChangeParams.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
                propChangeParams.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
                propChangeParams.StateChange = connected ? DICS_DISABLE: DICS_ENABLE;
                propChangeParams.Scope = DICS_FLAG_GLOBAL;
                propChangeParams.HwProfile = 0;

                if (!SetupDiSetClassInstallParams(hDevInfo, &devInfoData,
                    (SP_CLASSINSTALL_HEADER*)&propChangeParams,
                    sizeof(SP_PROPCHANGE_PARAMS)))
                {
                    SetupDiDestroyDeviceInfoList(hDevInfo);
                    return FALSE;
                }

                if (!SetupDiChangeState(hDevInfo, &devInfoData))
                {
                    SetupDiDestroyDeviceInfoList(hDevInfo);
                    return FALSE;
                }
                SetupDiDestroyDeviceInfoList(hDevInfo);
                return TRUE;
            }
        }
    }
    SetupDiDestroyDeviceInfoList(hDevInfo);
    return FALSE;
}

DeviceInfo* ListConnectedUSBDevices(int* deviceCount) 
{
    *deviceCount = 0; 

    HDEVINFO deviceInfoSet = SetupDiGetClassDevs(
        NULL,
        L"USB",
        NULL,
        DIGCF_PRESENT | DIGCF_ALLCLASSES
    );

    if (deviceInfoSet == INVALID_HANDLE_VALUE) return NULL;

    SP_DEVINFO_DATA deviceInfoData;
    deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    int capacity = 10;
    DeviceInfo* devices = new DeviceInfo[capacity];

    for (int i = 0; SetupDiEnumDeviceInfo(deviceInfoSet, i, &deviceInfoData); ++i) 
    {
        if (*deviceCount >= capacity) {
            capacity *= 2; 
            DeviceInfo* newDevices = new DeviceInfo[capacity];
            memcpy(newDevices, devices, sizeof(DeviceInfo) * (*deviceCount)); 
            delete[] devices; 
            devices = newDevices; 
        }

        DeviceInfo& device = devices[*deviceCount];

        if (SetupDiGetDeviceRegistryProperty(
            deviceInfoSet,
            &deviceInfoData,
            SPDRP_DEVICEDESC,
            NULL,
            (PBYTE)device.Caption,
            sizeof(device.Caption),
            NULL))
        {
        }
        else 
        {
            wcscpy_s(device.Caption, L"Unknown Device");
        }

        if (SetupDiGetDeviceInstanceId(
            deviceInfoSet,
            &deviceInfoData,
            device.DeviceID,
            sizeof(device.DeviceID),
            NULL))
        {
        }
        else 
        {
            wcscpy_s(device.DeviceID, L"Unknown Device ID");
        }
        ++(*deviceCount); 
    }
    SetupDiDestroyDeviceInfoList(deviceInfoSet);
    return devices; 
}

