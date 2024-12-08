#pragma once
#include "history_util.h"
#include "resource.h"
#include "global.h"
#include <devguid.h>
#include <newdev.h>
#include <windows.h>
#include <commdlg.h>  // Для OPENFILENAME - стандартный диалог выбора файлов
#include <shlobj.h>   // Для выбора папки - используется для выбора папки с драйверами
#include <setupapi.h> // Для работы с драйверами - предоставляет функции для установки и управления драйверами
#include <strsafe.h>
#include <cfgmgr32.h>
#include <thread> 

#pragma comment(lib, "SetupAPI.lib")
#pragma comment(lib, "newdev.lib")

void RollbackDriver(const WCHAR* deviceIdentifier);
void RemoveDriver(const WCHAR* deviceId);
void InstallDriver(const WCHAR* deviceId, const WCHAR* infPath);
LPWSTR ShowDriverSelectionDialog(HWND hwnd, LPCWSTR deviceId);
void DeleteDriverHistory(const WCHAR* deviceID);