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
    std::wstring eventID;         // ������������� �������
    std::wstring timeCreated;     // ����� �������� �������
    std::wstring level;           // ������� �������� �������
    std::wstring providerName;    // ��� ��������� �������
    std::wstring processID;       // ������������� ��������
    std::wstring threadID;        // ������������� ������
    std::wstring channel;         // ����� �������
    std::wstring task;            // ������ �������
    std::wstring keywords;        // �������� ����� �������
    std::wstring data;            // �������������� ������ �������
};

void GetEvents(const std::wstring& deviceInstanceID, std::vector<EventData>& events);