#pragma once
#include <wbemidl.h>
#include <comutil.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <bthsdpdef.h>
#include <sddl.h>
#include "global.h"

#pragma comment(lib, "SetupAPI.lib")
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "comsupp.lib")


HRESULT InitWMI(IWbemLocator** pLoc, IWbemServices** pSvc);
Map* FullQueryDevices(IWbemServices* pSvc, const wchar_t* query, int* count);
DeviceInfo* QueryDevices(IWbemServices* pSvc, const wchar_t* query, int* count);
BOOL IsUsbDeviceConnected(IWbemServices* pSvc, const WCHAR* deviceID);
void GetDiskPathFromDeviceID(IWbemServices* pSvc, const WCHAR* deviceID);