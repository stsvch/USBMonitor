#pragma once
#include <windows.h>
#include <winevt.h>
#include <xmllite.h>
#include <shlwapi.h>
#include <tchar.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <string>
#include <sstream>
#include <vector>
#include <memory>
#include <iostream>
#pragma comment(lib, "wevtapi.lib")
#pragma comment(lib, "xmllite.lib")
#pragma comment(lib, "shlwapi.lib")

struct EventData 
{
    std::wstring eventID;         // Идентификатор события
    std::wstring timeCreated;     // Время создания события
    std::wstring level;           // Уровень важности события
    std::wstring providerName;    // Имя источника события
    std::wstring processID;       // Идентификатор процесса
    std::wstring threadID;        // Идентификатор потока
    std::wstring channel;         // Канал события
    std::wstring task;            // Задача события
    std::wstring keywords;        // Ключевые слова события
    std::wstring data;            // Дополнительные данные события
};

void GetEvents(const std::wstring& deviceInstanceID, std::vector<EventData>& events);