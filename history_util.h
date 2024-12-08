#pragma once   // Для Windows API
#include <shlwapi.h>    // Для PathFileExistsW
#include <stdio.h>      // Для работы с файлами
#include <time.h>       // Для работы с временем
#include <tchar.h>      // Для обработки строк
#include <errno.h>      // Для обработки ошибок файловой системы

#pragma comment(lib, "shlwapi.lib") // Линковка с библиотекой Shlwapi

// Проверяет и создаёт каталог, если он не существует
BOOL CreateDirectoryIfNotExists(const WCHAR* directoryPath);

// Сохраняет информацию о драйверах (DeviceID, путь к INF-файлу, версию и дату обновления) в текстовый файл
void SaveDriverHistory(const WCHAR* deviceID, const WCHAR* infPath);

// Загружает историю драйверов по DeviceID и возвращает путь к INF-файлу
BOOL LoadDriverHistory(const WCHAR* deviceID, WCHAR* infPath, size_t infPathSize);

void EscapeBackslashes(WCHAR* str);
void ExtractBaseDeviceID(const WCHAR* fullDeviceID, WCHAR* baseDeviceID, size_t bufferSize);

void DeleteLastDriverHistory(const WCHAR* deviceID);