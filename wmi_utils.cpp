#include "wmi_utils.h"

HRESULT InitWMI(IWbemLocator** pLoc, IWbemServices** pSvc)
{
    HRESULT hres = NULL;

    // Инициализация COM-библиотеки для многозадачности
    hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres))
    {
        return hres; // Возвращаем ошибку, если инициализация COM не удалась
    }

    // Установка уровня безопасности COM
    hres = CoInitializeSecurity(
        NULL, -1, NULL, NULL,
        RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL, EOAC_NONE, NULL);
    if (FAILED(hres))
    {
        // Если настройка безопасности не удалась, разъединяем COM и возвращаем ошибку
        CoUninitialize();
        return hres;
    }

    // Создание WbemLocator для взаимодействия с WMI
    hres = CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)pLoc);
    if (FAILED(hres))
    {
        // Если создание WbemLocator не удалось, разъединяем COM и возвращаем ошибку
        CoUninitialize();
        return hres;
    }

    // Подключение к WMI-серверу на ROOT\\CIMV2
    hres = (*pLoc)->ConnectServer(
        _bstr_t(L"ROOT\\CIMV2"), NULL, NULL, 0, NULL, 0, 0, pSvc);
    if (FAILED(hres))
    {
        // Если подключение не удалось, освобождаем ресурсы и разъединяем COM
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

    // Выполнение WMI-запроса 
    hres = pSvc->ExecQuery(
        bstr_t(L"WQL"),  
        bstr_t(query),  
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, // Флаги для выполнения запроса
        NULL, &pEnumerator); // Указатель на перечислитель результатов

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

    //получение объектов из перечислителя
    while (true)
    {
        // Получаем следующий объект из перечислителя
        hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
        if (FAILED(hres) || uReturn == 0)
        {
            break; // Выход из цикла, если не удалось получить объект или объекты закончились
        }

        SAFEARRAY* pNames = NULL;
        // Получаем имена всех свойств объекта
        hres = pclsObj->GetNames(NULL, WBEM_FLAG_ALWAYS, NULL, &pNames);
        if (FAILED(hres) || !pNames) {
            pclsObj->Release(); // Освобождаем объект, если не удалось получить имена
            continue; // Переходим к следующему объекту
        }

        LONG lBound, uBound;
        // Получаем границы массива имен свойств
        SafeArrayGetLBound(pNames, 1, &lBound);
        SafeArrayGetUBound(pNames, 1, &uBound);

        // Обрабатываем каждое свойство объекта
        for (LONG i = lBound; i <= uBound; ++i)
        {
            BSTR propertyName;
            // Получаем имя свойства
            SafeArrayGetElement(pNames, &i, &propertyName);

            VARIANT vtProp;
            VariantInit(&vtProp); // Инициализируем переменную для хранения значения свойства

            // Получаем значение свойства
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

                // Сохраняем имя свойства в результатах
                wcsncpy_s(results[*count].Key, propertyName, MAX_BUFFER_SIZE - 1);

                // Обрабатываем значения различных типов
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
                case VT_NULL: // NULL-значения
                    wcsncpy_s(results[*count].Value, L"(null)", MAX_BUFFER_SIZE - 1);
                    break;
                default: // Неизвестные типы данных
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
    // Формируем WQL-запрос для получения информации о конкретном устройстве по его DeviceID
    swprintf_s(query, 512, L"SELECT * FROM Win32_PnPEntity WHERE DeviceID = '%s'", deviceID);

    IEnumWbemClassObject* pEnumerator = NULL;

    // Выполняем запрос, чтобы получить данные о конкретном устройстве по DeviceID
    hres = pSvc->ExecQuery(bstr_t(L"WQL"), bstr_t(query),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, // Используем флаги для немедленного получения результатов
        NULL, &pEnumerator); // Получаем указатель на перечислитель объектов

    if (FAILED(hres)) return FALSE; // Если запрос не удался, возвращаем FALSE (устройство не подключено)

    IWbemClassObject* pClassObject = NULL;
    ULONG returnCount = 0;
    BOOL isConnected = FALSE;  // Переменная для хранения статуса подключения устройства

    // Обрабатываем полученные объекты, возвращаемые запросом
    while (true)
    {
        // Получаем следующий объект из перечислителя
        hres = pEnumerator->Next(WBEM_INFINITE, 1, &pClassObject, &returnCount);
        if (returnCount == 0) break;  // Если объектов больше нет, выходим из цикла

        VARIANT vtProp;

        // Получаем статус устройства (например, "OK", если оно работает корректно)
        hres = pClassObject->Get(L"Status", 0, &vtProp, 0, 0);
        if (FAILED(hres))
        {
            pClassObject->Release(); // Освобождаем объект, если не удалось получить свойство
            continue;  // Переходим к следующему объекту
        }

        // Проверяем, что статус устройства "OK", что означает нормальное подключение
        if (wcscmp(vtProp.bstrVal, L"OK") == 0)
        {
            isConnected = TRUE;  // Устройство подключено и работает корректно
        }

        VariantClear(&vtProp); // Очищаем переменную, занятую значением статуса

        // Получаем код ошибки конфигурации устройства
        hres = pClassObject->Get(L"ConfigManagerErrorCode", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hres))
        {
            // Если код ошибки не равен 0, это означает, что устройство имеет ошибку конфигурации
            if (vtProp.intVal != 0)
            {
                isConnected = FALSE; // Устройство не подключено корректно
            }
        }
        VariantClear(&vtProp); // Очищаем переменную, занятую кодом ошибки

        pClassObject->Release(); // Освобождаем объект
        break;  // Останавливаем цикл, так как нашли нужное устройство
    }

    pEnumerator->Release(); // Освобождаем перечислитель
    return isConnected;  // Возвращаем результат проверки подключения устройства
}