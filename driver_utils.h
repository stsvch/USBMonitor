#pragma once
#include "history_util.h"
#include "global.h"
#include <devguid.h>
#include <newdev.h>
#include <windows.h>
#include <commdlg.h>  // ��� OPENFILENAME - ����������� ������ ������ ������
#include <shlobj.h>   // ��� ������ ����� - ������������ ��� ������ ����� � ����������
#include <setupapi.h> // ��� ������ � ���������� - ������������� ������� ��� ��������� � ���������� ����������
#include <cfgmgr32.h>

#pragma comment(lib, "SetupAPI.lib")
#pragma comment(lib, "newdev.lib")

void RollbackDriver(const WCHAR* deviceIdentifier);
void RemoveDriver(const WCHAR* deviceIdentifier);
void InstallDriver(const WCHAR* infPath, const WCHAR* deviceID, const WCHAR* driverVersion);
LPWSTR ShowDriverSelectionDialog(HWND hwnd);
// �������, ���������� ��� ������� �� ������ ��� ������ ����� � ��������� ��������
void OnBrowseAndInstallDriver(HWND hwnd);