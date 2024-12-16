#pragma once
#include "history_util.h"
#include <devguid.h>
#include <newdev.h>
#include <windows.h>
#include <commdlg.h>  // ��� OPENFILENAME - ����������� ������ ������ ������
#include <shlobj.h>   // ��� ������ ����� - ������������ ��� ������ ����� � ����������
#include <setupapi.h> // ��� ������ � ���������� - ������������� ������� ��� ��������� � ���������� ����������
#include <cfgmgr32.h>
#include <initguid.h>
#include <usbiodef.h>
#include <usbioctl.h> // ��� IOCTL ��������
#include <dbt.h>
#include <mmsystem.h>
#include <string>
#include <iostream>
#include <thread>
#include <chrono>

#pragma comment(lib, "SetupAPI.lib")
#pragma comment(lib, "newdev.lib")
#pragma comment(lib, "winmm.lib")


BOOL ChangeUSBDeviceState(const WCHAR* deviceID, BOOL connected);
DeviceInfo* ListConnectedUSBDevices(int* deviceCount);
void DisableSystemSound();
void EnableSystemSound(const wchar_t* soundFile);
void PlayCustomSound(const wchar_t* soundFile);
bool IsTargetDevice(const wchar_t* deviceID);