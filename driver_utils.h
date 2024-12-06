#pragma once
#include "history_util.h"
#include "global.h"
#include <devguid.h>
#include <newdev.h>
#include <windows.h>
#include <commdlg.h>  // Для OPENFILENAME - стандартный диалог выбора файлов
#include <shlobj.h>   // Для выбора папки - используется для выбора папки с драйверами
#include <setupapi.h> // Для работы с драйверами - предоставляет функции для установки и управления драйверами
#include <cfgmgr32.h>

#pragma comment(lib, "SetupAPI.lib")
#pragma comment(lib, "newdev.lib")

void RollbackDriver(const WCHAR* deviceIdentifier);
void RemoveDriver(const WCHAR* deviceIdentifier);
void InstallDriver(const WCHAR* infPath, const WCHAR* deviceID, const WCHAR* driverVersion);
LPWSTR ShowDriverSelectionDialog(HWND hwnd);
// Функция, вызываемая при нажатии на кнопку для выбора папки и установки драйвера
void OnBrowseAndInstallDriver(HWND hwnd);