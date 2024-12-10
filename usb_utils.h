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
#include <windows.h>
#include <setupapi.h>
#include <initguid.h>
#include <usbiodef.h>

#pragma comment(lib, "SetupAPI.lib")
#pragma comment(lib, "newdev.lib")

BOOL ChangeUSBDeviceState(const WCHAR* deviceID, BOOL connected);
DeviceInfo* ListConnectedUSBDevices(int* deviceCount);
double GetUSBDevicePowerConsumption(const WCHAR* deviceID);