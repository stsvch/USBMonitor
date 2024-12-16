#pragma once   
#include <shlwapi.h> 
#include "global.h"
#include <stdio.h>  
#include <time.h>    
#include <tchar.h>
#include <errno.h>   

#pragma comment(lib, "shlwapi.lib") 

#define MAX_BUFFER_SIZE 256
#define DCONFIG_FILE L"C:\\Users\\yana0\\Desktop\\5 сем\\курсач\\USBMonitor\\driver_history.log"
#define TDCONFIG_FILE L"C:\\Users\\yana0\\Desktop\\5 сем\\курсач\\USBMonitor\\temp_driver_history.log"

BOOL CreateDirectoryIfNotExists(const WCHAR* directoryPath);
void SaveDriverHistory(const WCHAR* deviceID, const WCHAR* infPath);
BOOL LoadDriverHistory(const WCHAR* deviceID, WCHAR* infPath, size_t infPathSize);
void EscapeBackslashes(WCHAR* str);
void ExtractBaseDeviceID(const WCHAR* fullDeviceID, WCHAR* baseDeviceID, size_t bufferSize);
void DeleteLastDriverHistory(const WCHAR* deviceID);
