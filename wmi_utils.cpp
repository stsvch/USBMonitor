#include "wmi_utils.h"

HRESULT InitWMI(IWbemLocator** pLoc, IWbemServices** pSvc)
{
    HRESULT hres = NULL;

    // ������������� COM-���������� ��� ���������������
    hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres))
    {
        return hres; // ���������� ������, ���� ������������� COM �� �������
    }

    // ��������� ������ ������������ COM
    hres = CoInitializeSecurity(
        NULL, -1, NULL, NULL,
        RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL, EOAC_NONE, NULL);
    if (FAILED(hres))
    {
        // ���� ��������� ������������ �� �������, ����������� COM � ���������� ������
        CoUninitialize();
        return hres;
    }

    // �������� WbemLocator ��� �������������� � WMI
    hres = CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)pLoc);
    if (FAILED(hres))
    {
        // ���� �������� WbemLocator �� �������, ����������� COM � ���������� ������
        CoUninitialize();
        return hres;
    }

    // ����������� � WMI-������� �� ROOT\\CIMV2
    hres = (*pLoc)->ConnectServer(
        _bstr_t(L"ROOT\\CIMV2"), NULL, NULL, 0, NULL, 0, 0, pSvc);
    if (FAILED(hres))
    {
        // ���� ����������� �� �������, ����������� ������� � ����������� COM
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

    // ���������� WMI-������� 
    hres = pSvc->ExecQuery(
        bstr_t(L"WQL"),  
        bstr_t(query),  
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, // ����� ��� ���������� �������
        NULL, &pEnumerator); // ��������� �� ������������� �����������

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

    //��������� �������� �� �������������
    while (true)
    {
        // �������� ��������� ������ �� �������������
        hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
        if (FAILED(hres) || uReturn == 0)
        {
            break; // ����� �� �����, ���� �� ������� �������� ������ ��� ������� �����������
        }

        SAFEARRAY* pNames = NULL;
        // �������� ����� ���� ������� �������
        hres = pclsObj->GetNames(NULL, WBEM_FLAG_ALWAYS, NULL, &pNames);
        if (FAILED(hres) || !pNames) {
            pclsObj->Release(); // ����������� ������, ���� �� ������� �������� �����
            continue; // ��������� � ���������� �������
        }

        LONG lBound, uBound;
        // �������� ������� ������� ���� �������
        SafeArrayGetLBound(pNames, 1, &lBound);
        SafeArrayGetUBound(pNames, 1, &uBound);

        // ������������ ������ �������� �������
        for (LONG i = lBound; i <= uBound; ++i)
        {
            BSTR propertyName;
            // �������� ��� ��������
            SafeArrayGetElement(pNames, &i, &propertyName);

            VARIANT vtProp;
            VariantInit(&vtProp); // �������������� ���������� ��� �������� �������� ��������

            // �������� �������� ��������
            hres = pclsObj->Get(propertyName, 0, &vtProp, 0, 0);
            if (SUCCEEDED(hres))
            {
                // ���� ��������� ������ ������� �������, ����������� ���
                if (*count == capacity)
                {
                    capacity *= 2; // ����������� ������� ������� � ��� ����
                    Map* newResults = new Map[capacity];
                    // �������� ������ ���������� � ����� ������
                    for (size_t j = 0; j < *count; ++j) {
                        newResults[j] = results[j];
                    }
                    delete[] results; // ����������� ������ ������
                    results = newResults; // ��������� ��������� �� ����� ������
                }

                // ��������� ��� �������� � �����������
                wcsncpy_s(results[*count].Key, propertyName, MAX_BUFFER_SIZE - 1);

                // ������������ �������� ��������� �����
                switch (vtProp.vt)
                {
                case VT_BSTR: // ������
                    wcsncpy_s(results[*count].Value, vtProp.bstrVal ? vtProp.bstrVal : L"(null)", MAX_BUFFER_SIZE - 1);
                    break;
                case VT_I4: // ����� ����� (int)
                    swprintf_s(results[*count].Value, MAX_BUFFER_SIZE, L"%d", vtProp.lVal);
                    break;
                case VT_UI4: // ����������� ����� ����� (unsigned int)
                    swprintf_s(results[*count].Value, MAX_BUFFER_SIZE, L"%u", vtProp.ulVal);
                    break;
                case VT_BOOL: // ���������� �������� (True/False)
                    wcsncpy_s(results[*count].Value, vtProp.boolVal ? L"True" : L"False", MAX_BUFFER_SIZE - 1);
                    break;
                case VT_DATE: // ����
                    SYSTEMTIME st;
                    // ����������� ���� � ������ �������
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
                case VT_NULL: // NULL-��������
                    wcsncpy_s(results[*count].Value, L"(null)", MAX_BUFFER_SIZE - 1);
                    break;
                default: // ����������� ���� ������
                    wcsncpy_s(results[*count].Value, L"(unknown type)", MAX_BUFFER_SIZE - 1);
                    break;
                }

                // ����������� ������� �����������
                ++(*count);
            }

            // ������� ������
            VariantClear(&vtProp);
            SysFreeString(propertyName);
        }

        // ����������� ������ ���� �������
        SafeArrayDestroy(pNames);
        pclsObj->Release(); // ����������� ������ WMI
    }

    // ����������� �������������
    pEnumerator->Release();
    return results; // ���������� ������ �����������
}


DeviceInfo* QueryDevices(IWbemServices* pSvc, const wchar_t* query, int* count)
{
    HRESULT hres;

    IEnumWbemClassObject* pEnumerator = NULL;

    // ���������� WMI-������� � �������������� WQL (WMI Query Language)
    hres = pSvc->ExecQuery(
        bstr_t(L"WQL"),      // ���������� WQL ��� �������
        bstr_t(query),       // ������, ���������� � �������
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, // ����� ��� ���������� �������
        NULL, &pEnumerator); // ��������� �� ������������� �����������

    if (FAILED(hres))
    {
        *count = 0;
        return NULL; // ���������� NULL, ���� ������ �� ������
    }

    IWbemClassObject* pclsObj = NULL;
    ULONG uReturn = 0;

    size_t capacity = 20; // ��������� ������ ������� ��� �������� �����������
    *count = 0;  // ������������� �������� �����������
    DeviceInfo* devices = new DeviceInfo[capacity]; // ��������� ������ ��� ������� �����������

    // �������� ���� ��������� ��������, ������������ ��������
    while (true)
    {
        // �������� ��������� ������ �� �������������
        hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
        if (FAILED(hres)) break;  // ���������, ���� ��������� ������
        if (uReturn == 0) break;  // ���������, ���� ��� �������� ��� ���������

        // ���� ������� ������� ���������, ����������� ��
        if (*count == capacity)
        {
            capacity *= 2; // ��������� ������ �������
            DeviceInfo* newDevices = new DeviceInfo[capacity];  // �������� ����� ������
            // �������� ������ ���������� � ����� ������
            for (size_t i = 0; i < *count; ++i)
            {
                newDevices[i] = devices[i];
            }
            delete[] devices;  // ����������� ������ ������
            devices = newDevices; // ��������� ��������� �� ����� ������
        }

        VARIANT vtProp;
        VariantInit(&vtProp); // ������������� ���������� ��� �������� �������� ��������

        DeviceInfo& device = devices[*count];  // ������ �� ������� ������ ����������

        // �������� �������� �������� "DeviceID"
        hres = pclsObj->Get(L"DeviceID", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hres) && vtProp.vt == VT_BSTR)
        {
            wcsncpy_s(device.DeviceID, vtProp.bstrVal, MAX_BUFFER_SIZE); // �������� �������� � ���������
            device.DeviceID[MAX_BUFFER_SIZE - 1] = L'\0'; // ������������ ���������� ���������� ������
        }
        else
        {
            device.DeviceID[0] = L'\0'; // ���� �������� �� �������, ��������� ������ ������
        }
        VariantClear(&vtProp); // ������� ������, ������� ����������

        // �������� �������� �������� "Description"
        hres = pclsObj->Get(L"Description", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hres) && vtProp.vt == VT_BSTR)
        {
            wcsncpy_s(device.Description, vtProp.bstrVal, MAX_BUFFER_SIZE); // �������� ��������
            device.Description[MAX_BUFFER_SIZE - 1] = L'\0'; // ������������ ���������� ���������� ������
        }
        else
        {
            device.Description[0] = L'\0'; // ���� �������� �� �������, ��������� ������ ������
        }
        VariantClear(&vtProp); // ������� ������, ������� ����������

        // �������� �������� �������� "Status"
        hres = pclsObj->Get(L"Status", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hres) && vtProp.vt == VT_BSTR)
        {
            wcsncpy_s(device.Status, vtProp.bstrVal, MAX_BUFFER_SIZE); // �������� ������
            device.Status[MAX_BUFFER_SIZE - 1] = L'\0'; // ������������ ���������� ���������� ������
        }
        else
        {
            device.Status[0] = L'\0'; // ���� �������� �� �������, ��������� ������ ������
        }
        VariantClear(&vtProp); // ������� ������, ������� ����������

        // �������� �������� �������� "Caption"
        hres = pclsObj->Get(L"Caption", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hres) && vtProp.vt == VT_BSTR)
        {
            wcsncpy_s(device.Caption, vtProp.bstrVal, MAX_BUFFER_SIZE); // �������� ������� ����������
            device.Caption[MAX_BUFFER_SIZE - 1] = L'\0'; // ������������ ���������� ���������� ������
        }
        else
        {
            device.Caption[0] = L'\0'; // ���� �������� �� �������, ��������� ������ ������
        }
        VariantClear(&vtProp); // ������� ������, ������� ����������

        ++(*count); // ����������� ������� �����������

        pclsObj->Release(); // ����������� ������
    }

    pEnumerator->Release(); // ����������� �������������
    return devices; // ���������� ������ ���������
}


BOOL IsUsbDeviceConnected(IWbemServices* pSvc, const WCHAR* deviceID)
{
    HRESULT hres;

    WCHAR query[512];
    // ��������� WQL-������ ��� ��������� ���������� � ���������� ���������� �� ��� DeviceID
    swprintf_s(query, 512, L"SELECT * FROM Win32_PnPEntity WHERE DeviceID = '%s'", deviceID);

    IEnumWbemClassObject* pEnumerator = NULL;

    // ��������� ������, ����� �������� ������ � ���������� ���������� �� DeviceID
    hres = pSvc->ExecQuery(bstr_t(L"WQL"), bstr_t(query),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, // ���������� ����� ��� ������������ ��������� �����������
        NULL, &pEnumerator); // �������� ��������� �� ������������� ��������

    if (FAILED(hres)) return FALSE; // ���� ������ �� ������, ���������� FALSE (���������� �� ����������)

    IWbemClassObject* pClassObject = NULL;
    ULONG returnCount = 0;
    BOOL isConnected = FALSE;  // ���������� ��� �������� ������� ����������� ����������

    // ������������ ���������� �������, ������������ ��������
    while (true)
    {
        // �������� ��������� ������ �� �������������
        hres = pEnumerator->Next(WBEM_INFINITE, 1, &pClassObject, &returnCount);
        if (returnCount == 0) break;  // ���� �������� ������ ���, ������� �� �����

        VARIANT vtProp;

        // �������� ������ ���������� (��������, "OK", ���� ��� �������� ���������)
        hres = pClassObject->Get(L"Status", 0, &vtProp, 0, 0);
        if (FAILED(hres))
        {
            pClassObject->Release(); // ����������� ������, ���� �� ������� �������� ��������
            continue;  // ��������� � ���������� �������
        }

        // ���������, ��� ������ ���������� "OK", ��� �������� ���������� �����������
        if (wcscmp(vtProp.bstrVal, L"OK") == 0)
        {
            isConnected = TRUE;  // ���������� ���������� � �������� ���������
        }

        VariantClear(&vtProp); // ������� ����������, ������� ��������� �������

        // �������� ��� ������ ������������ ����������
        hres = pClassObject->Get(L"ConfigManagerErrorCode", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hres))
        {
            // ���� ��� ������ �� ����� 0, ��� ��������, ��� ���������� ����� ������ ������������
            if (vtProp.intVal != 0)
            {
                isConnected = FALSE; // ���������� �� ���������� ���������
            }
        }
        VariantClear(&vtProp); // ������� ����������, ������� ����� ������

        pClassObject->Release(); // ����������� ������
        break;  // ������������� ����, ��� ��� ����� ������ ����������
    }

    pEnumerator->Release(); // ����������� �������������
    return isConnected;  // ���������� ��������� �������� ����������� ����������
}

void GetDiskPathFromDeviceID(IWbemServices* pSsvc, const WCHAR* deviceID) {

    // �������� ������ ���� USB ���������
    GUID guid = GUID_DEVINTERFACE_DISK;  // ���� ����� ���������� ������ GUID, ���� ����� �������� � ������� ������ ���������
    HDEVINFO deviceInfoSet = SetupDiGetClassDevs(&guid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
      //  std::wcerr << L"������ ��������� ������ ���������.\n";
        return;
    }

    SP_DEVINFO_DATA deviceInfoData;
    deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    // ���������� ����������
    for (DWORD i = 0; SetupDiEnumDeviceInfo(deviceInfoSet, i, &deviceInfoData); ++i) {
        WCHAR szDeviceID[MAX_PATH];

        // �������� ������������� ����������
        if (SetupDiGetDeviceInstanceId(deviceInfoSet, &deviceInfoData, szDeviceID, sizeof(szDeviceID) / sizeof(szDeviceID[0]), NULL)) {
            // ���������� ������������� ���������� � ����������
            if (_wcsicmp(szDeviceID, deviceID) == 0) {
               // std::wcout << L"���������� �������: " << szDeviceID << std::endl;

                // ������ ������� ���������� � ���� ����������, ��������, ���� ����� ������� ����������
                SP_DEVICE_INTERFACE_DATA deviceInterfaceData;
                deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

                if (SetupDiEnumDeviceInterfaces(deviceInfoSet, &deviceInfoData, &guid, 0, &deviceInterfaceData)) {
                    DWORD requiredSize = 0;
                    SetupDiGetDeviceInterfaceDetail(deviceInfoSet, &deviceInterfaceData, NULL, 0, &requiredSize, NULL);
                    PSP_DEVICE_INTERFACE_DETAIL_DATA pDetail = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(requiredSize);

                    pDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

                    if (SetupDiGetDeviceInterfaceDetail(deviceInfoSet, &deviceInterfaceData, pDetail, requiredSize, &requiredSize, NULL)) {
                     //   std::wcout << L"���� ����������: " << pDetail->DevicePath << std::endl;
                    }

                    free(pDetail);
                }
                break;
            }
        }
    }
    // ����������� �������
    SetupDiDestroyDeviceInfoList(deviceInfoSet);
 }

void InstallDriverWithWMI(IWbemServices* pSvc, const WCHAR* deviceId, const WCHAR* infPath) {
    HRESULT hres;
    WCHAR query[512];
    swprintf_s(query, sizeof(query) / sizeof(WCHAR), L"SELECT * FROM Win32_PnPEntity WHERE DeviceID='%s'", deviceId);

    // ���������� WQL-�������
    IEnumWbemClassObject* pEnumerator = NULL;
    BSTR wqlQuery = SysAllocString(L"WQL");
    BSTR queryStr = SysAllocString(query);

    hres = pSvc->ExecQuery(
        wqlQuery,
        queryStr,
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &pEnumerator);

    SysFreeString(wqlQuery);
    SysFreeString(queryStr);

    if (FAILED(hres)) {
        MessageBoxW(NULL, L"������ ���������� WMI-�������. ���������� �� �������.", L"������", MB_OK | MB_ICONERROR);
        return;
    }

    IWbemClassObject* pclsObj = NULL;
    ULONG uReturn = 0;

    // ���������, ������� �� ����������
    bool deviceFound = false;
    while (pEnumerator) {
        hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
        if (uReturn == 0) {
            break;
        }
        deviceFound = true;
        VARIANT vtProp;

        // ��������� DeviceID
        hres = pclsObj->Get(L"DeviceID", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hres)) {
            MessageBoxW(NULL, L"���������� �������. �������� ��������...", L"����������", MB_OK | MB_ICONINFORMATION);
        }

        VariantClear(&vtProp);
        pclsObj->Release();
    }

    if (!deviceFound) {
        MessageBoxW(NULL, L"���������� � ��������� DeviceID �� �������.", L"������", MB_OK | MB_ICONERROR);
        pEnumerator->Release();
        return;
    }

    // �������� ������� ������� ����� WMI
    IWbemClassObject* pClass = NULL;
    IWbemClassObject* pInParamsDefinition = NULL;
    IWbemClassObject* pOutParams = NULL;
    IWbemClassObject* pInParams = NULL;

    BSTR className = SysAllocString(L"Win32_PnPSignedDriver");
    hres = pSvc->GetObject(className, 0, NULL, &pClass, NULL);
    SysFreeString(className);

    if (FAILED(hres)) {
        MessageBoxW(NULL, L"�� ������� �������� WMI-����� Win32_PnPSignedDriver.", L"������", MB_OK | MB_ICONERROR);
        pEnumerator->Release();
        return;
    }

    BSTR methodName = SysAllocString(L"AddDriver");
    hres = pClass->GetMethod(methodName, 0, &pInParamsDefinition, NULL);
    SysFreeString(methodName);

    if (FAILED(hres)) {
        MessageBoxW(NULL, L"�� ������� �������� ����� AddDriver.", L"������", MB_OK | MB_ICONERROR);
        pClass->Release();
        pEnumerator->Release();
        return;
    }

    // ������ ��������� ��� ������ AddDriver
    hres = pInParamsDefinition->SpawnInstance(0, &pInParams);
    if (FAILED(hres)) {
        MessageBoxW(NULL, L"�� ������� ������� ��������� ��� ������ AddDriver.", L"������", MB_OK | MB_ICONERROR);
        pInParamsDefinition->Release();
        pClass->Release();
        pEnumerator->Release();
        return;
    }

    // ��������� ���������
    VARIANT varINF;
    varINF.vt = VT_BSTR;
    varINF.bstrVal = SysAllocString(infPath);
    hres = pInParams->Put(L"INFPath", 0, &varINF, 0);
    VariantClear(&varINF);

    if (FAILED(hres)) {
        MessageBoxW(NULL, L"������ ���������� ���������� ������ AddDriver.", L"������", MB_OK | MB_ICONERROR);
        pInParams->Release();
        pInParamsDefinition->Release();
        pClass->Release();
        pEnumerator->Release();
        return;
    }

    // ��������� ����� AddDriver
    hres = pSvc->ExecMethod(className, methodName, 0, NULL, pInParams, &pOutParams, NULL);

    if (FAILED(hres)) {
        MessageBoxW(NULL, L"�� ������� ��������� ����� AddDriver.", L"������", MB_OK | MB_ICONERROR);
    }
    else {
        MessageBoxW(NULL, L"������� ������� ����������!", L"����������", MB_OK | MB_ICONINFORMATION);
    }

    // ������� ��������
    if (pOutParams) pOutParams->Release();
    if (pInParams) pInParams->Release();
    if (pInParamsDefinition) pInParamsDefinition->Release();
    pClass->Release();
    pEnumerator->Release();
}