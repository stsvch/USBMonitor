#pragma once
#include <windows.h>
#include <setupapi.h>
#include <cstdio>
#include <tchar.h>
#include <chrono>
#include <cfgmgr32.h>
#include <shlwapi.h>
#include <set>
#include <vector>
#include <string>
#include <winevt.h>
#include <evntprov.h>
#include <evntrace.h>
#include <strsafe.h>
#include <iostream>
#include <stdlib.h>
#include <memory>

#pragma comment(lib, "setupapi.lib")

#define MAX_BUFFER_SIZE 512

bool GetDevicePath(const WCHAR* deviceID, WCHAR* devicePathBuffer);
void GetDeviceType(const WCHAR* deviceID, WCHAR* deviceTypeBuffer);
double MeasureReadSpeed(const WCHAR* volumePath);
double MeasureWriteSpeed(const WCHAR* volumePath);