#pragma once   // ��� Windows API
#include <shlwapi.h>    // ��� PathFileExistsW
#include <stdio.h>      // ��� ������ � �������
#include <time.h>       // ��� ������ � ��������
#include <tchar.h>      // ��� ��������� �����
#include <errno.h>      // ��� ��������� ������ �������� �������

#pragma comment(lib, "shlwapi.lib") // �������� � ����������� Shlwapi

// ��������� � ������ �������, ���� �� �� ����������
BOOL CreateDirectoryIfNotExists(const WCHAR* directoryPath);

// ��������� ���������� � ��������� (DeviceID, ���� � INF-�����, ������ � ���� ����������) � ��������� ����
void SaveDriverHistory(const WCHAR* deviceID, const WCHAR* infPath);

// ��������� ������� ��������� �� DeviceID � ���������� ���� � INF-�����
BOOL LoadDriverHistory(const WCHAR* deviceID, WCHAR* infPath, size_t infPathSize);

void EscapeBackslashes(WCHAR* str);
void ExtractBaseDeviceID(const WCHAR* fullDeviceID, WCHAR* baseDeviceID, size_t bufferSize);

void DeleteLastDriverHistory(const WCHAR* deviceID);