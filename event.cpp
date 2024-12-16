#include "event.h"

void ParseAndStoreEventXML(const wchar_t* xml, std::vector<EventData>& events)
{
    IXmlReader* pReader = nullptr;
    IStream* pStream = SHCreateMemStream(reinterpret_cast<const BYTE*>(xml), wcslen(xml) * sizeof(wchar_t));//создание потока 

    if (!pStream) return;

    HRESULT hr = CreateXmlReader(IID_PPV_ARGS(&pReader), nullptr);//создание объекта IXmlReader для последовательного чтения XML
    if (FAILED(hr)) 
    {
        pStream->Release();
        return;
    }
    //Устанавка потока pStream в качестве источника данных
    hr = pReader->SetInput(pStream);
    if (FAILED(hr))
    {
        pReader->Release();
        pStream->Release();
        return;
    }

    XmlNodeType nodeType;
    std::wstring elementName, eventID, timeCreated, level, providerName, processID, threadID, channel, task, keywords, data;

    while (S_OK == pReader->Read(&nodeType))
    {
        if (nodeType == XmlNodeType_Element)
        {
            const WCHAR* localName = nullptr;
            hr = pReader->GetLocalName(&localName, nullptr);
            if (FAILED(hr)) continue;

            elementName = localName;

            if (elementName == L"EventID" || elementName == L"Level" || elementName == L"Data") 
            {
                if (pReader->Read(&nodeType) == S_OK && nodeType == XmlNodeType_Text) 
                {
                    const WCHAR* value = nullptr;
                    hr = pReader->GetValue(&value, nullptr);
                    if (SUCCEEDED(hr)) 
                    {
                        if (elementName == L"EventID") 
                        {
                            eventID = value;
                        }
                        else if (elementName == L"Level") 
                        {
                            level = value;
                        }
                        else if (elementName == L"Data") 
                        {
                            data += value;
                            data += L"; ";
                        }
                    }
                }
            }
            else if (elementName == L"TimeCreated") 
            {
                const WCHAR* attributeName = nullptr;
                const WCHAR* attributeValue = nullptr;
                while (pReader->MoveToNextAttribute() == S_OK) 
                {
                    hr = pReader->GetLocalName(&attributeName, nullptr);
                    if (SUCCEEDED(hr) && wcscmp(attributeName, L"SystemTime") == 0)
                    {
                        hr = pReader->GetValue(&attributeValue, nullptr);
                        if (SUCCEEDED(hr))
                        {
                            timeCreated = attributeValue;
                        }
                    }
                }
            }
            else if (elementName == L"Provider") 
            {
                const WCHAR* attributeName = nullptr;
                const WCHAR* attributeValue = nullptr;
                while (pReader->MoveToNextAttribute() == S_OK) 
                {
                    hr = pReader->GetLocalName(&attributeName, nullptr);
                    if (SUCCEEDED(hr) && wcscmp(attributeName, L"Name") == 0) 
                    {
                        hr = pReader->GetValue(&attributeValue, nullptr);
                        if (SUCCEEDED(hr))
                        {
                            providerName = attributeValue;
                        }
                    }
                }
            }
            else if (elementName == L"Execution") 
            {
                const WCHAR* attributeName = nullptr;
                const WCHAR* attributeValue = nullptr;
                while (pReader->MoveToNextAttribute() == S_OK) 
                {
                    hr = pReader->GetLocalName(&attributeName, nullptr);
                    if (SUCCEEDED(hr)) {
                        hr = pReader->GetValue(&attributeValue, nullptr);
                        if (SUCCEEDED(hr)) {
                            if (wcscmp(attributeName, L"ProcessID") == 0) 
                            {
                                processID = attributeValue;
                            }
                            else if (wcscmp(attributeName, L"ThreadID") == 0)
                            {
                                threadID = attributeValue;
                            }
                        }
                    }
                }
            }
            else if (elementName == L"Channel") {
                if (pReader->Read(&nodeType) == S_OK && nodeType == XmlNodeType_Text) 
                {
                    const WCHAR* value = nullptr;
                    hr = pReader->GetValue(&value, nullptr);
                    if (SUCCEEDED(hr)) {
                        channel = value;
                    }
                }
            }
            else if (elementName == L"Task")
            {
                if (pReader->Read(&nodeType) == S_OK && nodeType == XmlNodeType_Text)
                {
                    const WCHAR* value = nullptr;
                    hr = pReader->GetValue(&value, nullptr);
                    if (SUCCEEDED(hr)) 
                    {
                        task = value;
                    }
                }
            }
            else if (elementName == L"Keywords") 
            {
                if (pReader->Read(&nodeType) == S_OK && nodeType == XmlNodeType_Text) 
                {
                    const WCHAR* value = nullptr;
                    hr = pReader->GetValue(&value, nullptr);
                    if (SUCCEEDED(hr)) 
                    {
                        keywords = value;
                    }
                }
            }
        }
    }

    if (!eventID.empty()) 
    {
        events.push_back({ eventID, timeCreated, level, providerName, processID, threadID, channel, task, keywords, data });
    }

    pReader->Release();
    pStream->Release();
}

void ReplaceAmpersand(WCHAR* buffer) 
{
    WCHAR* pos = buffer;
    while ((pos = wcsstr(pos, L"&amp;")) != NULL) 
    {
        memmove(pos + 1, pos + 5, (wcslen(pos + 5) + 1) * sizeof(WCHAR));
        pos[0] = L'&';
    }
}

void ParseAndFilterEvent(EVT_HANDLE hEvent, const std::wstring& deviceInstanceID, std::vector<EventData>& events) 
{
    DWORD bufferSize = 0;
    DWORD bufferUsed = 0;
    DWORD propertyCount = 0;

    if (!EvtRender(NULL, hEvent, EvtRenderEventXml, 0, NULL, &bufferSize, &propertyCount) &&
        GetLastError() == ERROR_INSUFFICIENT_BUFFER)
    {

        WCHAR* buffer = (WCHAR*)malloc(bufferSize * sizeof(WCHAR));
        if (EvtRender(NULL, hEvent, EvtRenderEventXml, bufferSize, buffer, &bufferUsed, &propertyCount)) 
        {
            ReplaceAmpersand(buffer);

            if (wcsstr(buffer, (L"<DeviceInstanceID>" + deviceInstanceID + L"</DeviceInstanceID>").c_str()) != NULL)
            {
                ParseAndStoreEventXML(buffer, events);
            }
        }
        free(buffer);
    }
}

void FetchSystemEvents(const std::wstring& deviceInstanceID, std::vector<EventData>& events) 
{
    std::wstring query = L"<QueryList>\
                            <Query Id='0' Path='System'>\
                            <Select Path='System'>\
                              *\
                            </Select>\
                            </Query>\
                            </QueryList>";

    EVT_HANDLE hQuery = EvtQuery(NULL, NULL, query.c_str(), EvtQueryChannelPath | EvtQueryForwardDirection);
    if (!hQuery) {
        return;
    }

    EVT_HANDLE hEvent = NULL;
    DWORD returned = 0;

    while (EvtNext(hQuery, 1, &hEvent, INFINITE, 0, &returned))
    {
        ParseAndFilterEvent(hEvent, deviceInstanceID, events);
        EvtClose(hEvent);
    }

    EvtClose(hQuery);
}

void FetchEventsForDevice(const std::wstring& log, const std::wstring& deviceInstanceID, std::vector<EventData>& events)
{
    // Формирование запроса для фильтрации событий по идентификатору устройства
    std::wstring query = L"*[EventData/Data[@Name='DeviceInstanceId']='" + deviceInstanceID + L"']";

    // Выполнение запроса к журналу событий с указанным фильтром
    EVT_HANDLE hQuery = EvtQuery(NULL, log.c_str(), query.c_str(), EvtQueryReverseDirection);
    if (hQuery == NULL)
    {
        // Если запрос не удаётся создать, функция завершает выполнение
        return;
    }

    EVT_HANDLE hEvents[10]; // Массив для хранения дескрипторов событий
    DWORD dwReturned = 0;   // Количество возвращённых событий

    while (EvtNext(hQuery, ARRAYSIZE(hEvents), hEvents, INFINITE, 0, &dwReturned))
    {
        // Обрабатываем каждое возвращённое событие
        for (DWORD i = 0; i < dwReturned; i++)
        {
            DWORD dwBufferSize = 0;       // Размер буфера для XML
            DWORD dwPropertyCount = 0;   // Количество свойств события

            // Определение размера буфера
            if (!EvtRender(NULL, hEvents[i], EvtRenderEventXml, 0, NULL, &dwBufferSize, &dwPropertyCount))
            {
                if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
                {
                    EvtClose(hEvents[i]); // Закрываем дескриптор события
                    continue;
                }
            }

            std::unique_ptr<wchar_t[]> pXmlBuffer(new wchar_t[dwBufferSize]);

            // Рендерим XML-содержимое события
            if (!EvtRender(NULL, hEvents[i], EvtRenderEventXml, dwBufferSize, pXmlBuffer.get(), &dwBufferSize, &dwPropertyCount))
            {
                // Если рендеринг не удался, закрываем дескриптор события и продолжаем
                EvtClose(hEvents[i]);
                continue;
            }

            // Парсим XML-содержимое и добавляем данные в вектор событий
            ParseAndStoreEventXML(pXmlBuffer.get(), events);
            EvtClose(hEvents[i]);
        }
    }
    // Закрываем дескриптор запроса событий
    EvtClose(hQuery);
}


void GetEvents(const std::wstring& deviceInstanceID, std::vector<EventData>& events)
{
    FetchSystemEvents(deviceInstanceID, events);
    FetchEventsForDevice(L"Microsoft-Windows-Kernel-PnP/Configuration", deviceInstanceID, events);
}