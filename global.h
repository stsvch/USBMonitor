#pragma once

#define MAX_BUFFER_SIZE 256
#define CONFIGFLAG_DISABLED 0x00000001
#define CONFIGFLAG_FAILEDINSTALL 0x00000002
#define BACKUPPATH  L"C:\\Backup";

struct DeviceInfo
{
    WCHAR DeviceID[MAX_BUFFER_SIZE];
    WCHAR Description[MAX_BUFFER_SIZE];
    WCHAR Status[MAX_BUFFER_SIZE];
    WCHAR Caption[MAX_BUFFER_SIZE];
    BOOL IsConnected;
    WCHAR Path[MAX_BUFFER_SIZE];
    WCHAR Type[MAX_BUFFER_SIZE];
};

struct Map
{
    WCHAR Key[MAX_BUFFER_SIZE];
    WCHAR Value[MAX_BUFFER_SIZE];
};

