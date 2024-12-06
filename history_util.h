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
void SaveDriverHistory(const WCHAR* deviceID, const WCHAR* infPath, const WCHAR* driverVersion);

// ��������� ������� ��������� �� DeviceID � ���������� ���� � INF-�����
BOOL LoadDriverHistory(const WCHAR* deviceID, WCHAR* infPath, size_t infPathSize);

// ������ ����� INF-����� �������� � ��������� ���������� ���������� �����������
void BackupDriverINF(const WCHAR* infPath, const WCHAR* backupDirectory);

void EscapeBackslashes(WCHAR* str);