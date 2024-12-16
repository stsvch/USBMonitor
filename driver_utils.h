#pragma once
#include "history_util.h"
#include "resource.h"
#include <devguid.h>
#include <newdev.h>
#include <windows.h>
#include <commdlg.h>  
#include <shlobj.h>   
#include <setupapi.h>
#include <strsafe.h>
#include <cfgmgr32.h>
#include <thread> 

#pragma comment(lib, "SetupAPI.lib")
#pragma comment(lib, "newdev.lib")

bool RollbackDriver(const WCHAR* deviceIdentifier);
void RemoveDriver(const WCHAR* deviceId);
bool InstallDriver(const WCHAR* deviceId, const WCHAR* infPath);
LPWSTR ShowDriverSelectionDialog(HWND hwnd, LPCWSTR deviceId);
void DeleteDriverHistory(const WCHAR* deviceID);