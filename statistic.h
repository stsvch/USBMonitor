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
#include <io.h>
#include <fcntl.h>
#include <xmllite.h>
#include <sstream>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "wevtapi.lib")
#pragma comment(lib, "xmllite.lib")
#pragma comment(lib, "cfgmgr32.lib")
#pragma comment(lib, "setupapi.lib")


#define MAX_BUFFER_SIZE 512

bool GetDevicePath(const WCHAR* deviceID, WCHAR* devicePathBuffer);
void GetDeviceType(const WCHAR* deviceID, WCHAR* deviceTypeBuffer);
double MeasureReadSpeed(const WCHAR* volumePath);
double MeasureWriteSpeed(const WCHAR* volumePath);
void GetEvent(wchar_t* buffer, size_t bSize, wchar_t* deviceID);