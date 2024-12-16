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
                if (*count == capacity)
                {
                    capacity *= 2; 
                    Map* newResults = new Map[capacity];
                    for (size_t j = 0; j < *count; j++) 
                    {
                        newResults[j] = results[j];
                    }
                    delete[] results; 
                    results = newResults; 
                }

                // ��������� ��� �������� � �����������
                wcsncpy_s(results[*count].Key, propertyName, MAX_BUFFER_SIZE - 1);

                // ������������ �������� ��������� �����
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
                case VT_NULL: // NULL-��������
                    wcsncpy_s(results[*count].Value, L"(null)", MAX_BUFFER_SIZE - 1);
                    break;
                default: // ����������� ���� ������
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