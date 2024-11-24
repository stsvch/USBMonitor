#pragma once
#include <windows.h>
#include <wbemidl.h>
#include <wchar.h>
#include <comutil.h>
#include <devguid.h>
#include <stdio.h>
#include <comdef.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <bluetoothapis.h>
#pragma comment(lib, "SetupAPI.lib")
#pragma comment(lib, "Bthprops.lib")
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "comsupp.lib")

#define MAX_BUFFER_SIZE 256
#define CONFIGFLAG_DISABLED 0x00000001
#define CONFIGFLAG_FAILEDINSTALL 0x00000002

struct DeviceInfo 
{
    WCHAR DeviceID[MAX_BUFFER_SIZE];
    WCHAR Description[MAX_BUFFER_SIZE];
    WCHAR Status[MAX_BUFFER_SIZE];
    WCHAR Caption[MAX_BUFFER_SIZE];
    BOOL IsConnected;
};

struct Map
{
    WCHAR Key[MAX_BUFFER_SIZE];
    WCHAR Value[MAX_BUFFER_SIZE];
};

HRESULT InitWMI(IWbemLocator** pLoc, IWbemServices** pSvc);
Map* FullQueryDevices(IWbemServices* pSvc, const wchar_t* query, int* count);
DeviceInfo* QueryDevices(IWbemServices* pSvc, const wchar_t* query, int* count);
BOOL IsBluetoothDeviceConnected(const WCHAR* deviceName);
BOOL ChangeDeviceState(const WCHAR* deviceID, BOOL connected);
DeviceInfo* ListConnectedUSBDevices(int* deviceCount);
BOOL IsUsbDeviceConnected(IWbemServices* pSvc, const WCHAR* deviceID);