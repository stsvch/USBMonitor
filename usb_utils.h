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
#include <windows.h>
#include <setupapi.h>
#include <initguid.h>
#include <usbiodef.h>

#pragma comment(lib, "SetupAPI.lib")
#pragma comment(lib, "newdev.lib")

BOOL ChangeUSBDeviceState(const WCHAR* deviceID, BOOL connected);
DeviceInfo* ListConnectedUSBDevices(int* deviceCount);
double GetUSBDevicePowerConsumption(const WCHAR* deviceID);