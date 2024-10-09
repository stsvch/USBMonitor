#include <windows.h>
#include <commctrl.h>
#include <setupapi.h>
#include <devguid.h>
#include <regstr.h>
#include <stdio.h>
#include <initguid.h>
#include <usbiodef.h>
#include <string.h>
#include <BluetoothAPIs.h> 

#pragma comment(lib, "Bthprops.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "comctl32.lib")

#define IDC_TREEVIEW 1
#define IDC_INFOTEXT 2
#define MAX_BUFFER_SIZE 1024

typedef struct
{
    wchar_t name[MAX_BUFFER_SIZE];
    wchar_t hardwareID[MAX_BUFFER_SIZE];
    wchar_t manufacturerName[MAX_BUFFER_SIZE];
    wchar_t deviceType[MAX_BUFFER_SIZE];
    wchar_t driverName[MAX_BUFFER_SIZE];
    wchar_t driverVersion[MAX_BUFFER_SIZE];
    wchar_t deviceStatus[MAX_BUFFER_SIZE];
} USBDeviceInfo;

typedef struct BluetoothDeviceInfo {
    WCHAR name[MAX_BUFFER_SIZE];
    WCHAR manufacturerName[MAX_BUFFER_SIZE];
    WCHAR deviceType[MAX_BUFFER_SIZE];
    WCHAR driverName[MAX_BUFFER_SIZE];
    WCHAR driverVersion[MAX_BUFFER_SIZE];
    WCHAR deviceStatus[MAX_BUFFER_SIZE];
} BluetoothDeviceInfo;

USBDeviceInfo usbDevices[256]; 
BluetoothDeviceInfo bluetoothDevices[256];
int usbDeviceCount = 0;
int bluetoothDeviceCount = 0;

void AddUSBDeviceToList(USBDeviceInfo* deviceInfo) {
    if (usbDeviceCount < 256) {
        usbDevices[usbDeviceCount++] = *deviceInfo;
    }
}

void AddBluetoothDeviceToList(BluetoothDeviceInfo* deviceInfo) {
    if (bluetoothDeviceCount < 256) {
        bluetoothDevices[bluetoothDeviceCount++] = *deviceInfo;
    }
}

void ClearDeviceLists() {
    usbDeviceCount = 0;
    bluetoothDeviceCount = 0;
}

void DisplayDeviceInfo(HWND hInfoText, int deviceIndex, int isBluetooth) {
    if (isBluetooth) {
        if (deviceIndex < 0 || deviceIndex >= bluetoothDeviceCount) return;
        USBDeviceInfo* deviceInfo = &bluetoothDevices[deviceIndex];
        wchar_t infoBuffer[MAX_BUFFER_SIZE * 6];
        swprintf_s(infoBuffer, MAX_BUFFER_SIZE * 6,
            L"Имя: %s \n Идентификатор оборудования: %s \n Производитель: %s \n Тип устройства: %s \n Имя драйвера: %s \n Версия драйвера: %s \n Статус устройства: %s",
            deviceInfo->name, deviceInfo->hardwareID, deviceInfo->manufacturerName,
            deviceInfo->deviceType, deviceInfo->driverName, deviceInfo->driverVersion, deviceInfo->deviceStatus);
        SetWindowTextW(hInfoText, infoBuffer);
    }
    else {
        if (deviceIndex < 0 || deviceIndex >= usbDeviceCount) return;
        USBDeviceInfo* deviceInfo = &usbDevices[deviceIndex];
        wchar_t infoBuffer[MAX_BUFFER_SIZE * 6];
        swprintf_s(infoBuffer, MAX_BUFFER_SIZE * 6,
            L"Имя: %s \n Идентификатор оборудования: %s \n Производитель: %s \n Тип устройства: %s \n Имя драйвера: %s \n Версия драйвера: %s \n Статус устройства: %s",
            deviceInfo->name, deviceInfo->hardwareID, deviceInfo->manufacturerName,
            deviceInfo->deviceType, deviceInfo->driverName, deviceInfo->driverVersion, deviceInfo->deviceStatus);
        SetWindowTextW(hInfoText, infoBuffer);
    }
}



HTREEITEM AddTreeViewItem(HWND hTreeView, HTREEITEM hParent, const wchar_t* text, int deviceIndex) {
    TVINSERTSTRUCTW tvinsert;
    tvinsert.hParent = hParent;
    tvinsert.hInsertAfter = TVI_LAST;
    tvinsert.item.mask = TVIF_TEXT | TVIF_PARAM; 
    tvinsert.item.pszText = (LPWSTR)text;
    tvinsert.item.lParam = (LPARAM)deviceIndex; 

    return TreeView_InsertItem(hTreeView, &tvinsert);
}

void PopulateTreeViewWithUSBDevices(HWND hTreeView) {
    ClearDeviceLists();
    TreeView_DeleteAllItems(hTreeView); 

    HDEVINFO deviceInfoSet = SetupDiGetClassDevsW(&GUID_DEVINTERFACE_USB_DEVICE, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        MessageBoxW(NULL, L"Ошибка получения списка USB-устройств", L"Ошибка", MB_ICONERROR);
        return;
    }

    SP_DEVINFO_DATA deviceInfoData;
    deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    DWORD i = 0;

    HTREEITEM hUsbRootItem = AddTreeViewItem(hTreeView, NULL, L"USB устройства",-1);

    while (SetupDiEnumDeviceInfo(deviceInfoSet, i, &deviceInfoData)) {
        USBDeviceInfo deviceInfo = { 0 };
        DWORD DataT;

        if (SetupDiGetDeviceRegistryPropertyW(deviceInfoSet, &deviceInfoData, SPDRP_DEVICEDESC, &DataT,
            (PBYTE)deviceInfo.name, sizeof(deviceInfo.name), NULL)) {

            HTREEITEM hDeviceItem = AddTreeViewItem(hTreeView, hUsbRootItem, deviceInfo.name, usbDeviceCount);

            if (SetupDiGetDeviceRegistryPropertyW(deviceInfoSet, &deviceInfoData, SPDRP_HARDWAREID, &DataT,
                (PBYTE)deviceInfo.hardwareID, sizeof(deviceInfo.hardwareID), NULL)) {
                wcscpy_s(deviceInfo.hardwareID, MAX_BUFFER_SIZE, deviceInfo.hardwareID);
            }
            else {
                wcscpy_s(deviceInfo.hardwareID, MAX_BUFFER_SIZE, L"Не удалось получить идентификатор оборудования");
            }

            if (SetupDiGetDeviceRegistryPropertyW(deviceInfoSet, &deviceInfoData, SPDRP_MFG, &DataT,
                (PBYTE)deviceInfo.manufacturerName, sizeof(deviceInfo.manufacturerName), NULL)) {
                wcscpy_s(deviceInfo.manufacturerName, MAX_BUFFER_SIZE, deviceInfo.manufacturerName);
            }
            else {
                wcscpy_s(deviceInfo.manufacturerName, MAX_BUFFER_SIZE, L"Не удалось получить имя производителя");
            }

            if (SetupDiGetDeviceRegistryPropertyW(deviceInfoSet, &deviceInfoData, SPDRP_SERVICE, &DataT,
                (PBYTE)deviceInfo.deviceType, sizeof(deviceInfo.deviceType), NULL)) {
                wcscpy_s(deviceInfo.deviceType, MAX_BUFFER_SIZE, deviceInfo.deviceType);
            }
            else {
                wcscpy_s(deviceInfo.deviceType, MAX_BUFFER_SIZE, L"Не удалось получить тип устройства");
            }

            if (SetupDiGetDeviceRegistryPropertyW(deviceInfoSet, &deviceInfoData, SPDRP_SERVICE, &DataT,
                (PBYTE)deviceInfo.driverName, sizeof(deviceInfo.driverName), NULL)) {
                wcscpy_s(deviceInfo.driverName, MAX_BUFFER_SIZE, deviceInfo.driverName);
            }
            else {
                wcscpy_s(deviceInfo.driverName, MAX_BUFFER_SIZE, L"Не удалось получить имя драйвера");
            }

            if (SetupDiGetDeviceRegistryPropertyW(deviceInfoSet, &deviceInfoData, SPDRP_DRIVER, &DataT,
                (PBYTE)deviceInfo.driverVersion, sizeof(deviceInfo.driverVersion), NULL)) {
                wcscpy_s(deviceInfo.driverVersion, MAX_BUFFER_SIZE, deviceInfo.driverVersion);
            }
            else {
                wcscpy_s(deviceInfo.driverVersion, MAX_BUFFER_SIZE, L"Не удалось получить версию драйвера");
            }
            DWORD configFlags;
            if (SetupDiGetDeviceRegistryPropertyW(deviceInfoSet, &deviceInfoData, SPDRP_CONFIGFLAGS, &DataT,
                (PBYTE)&configFlags, sizeof(configFlags), NULL)) {
                if (configFlags & CONFIGFLAG_DISABLED) {
                    wcscpy_s(deviceInfo.deviceStatus, MAX_BUFFER_SIZE, L"Устройство отключено");
                }
                else {
                    wcscpy_s(deviceInfo.deviceStatus, MAX_BUFFER_SIZE, L"Устройство активно");
                }
            }
            else {
                wcscpy_s(deviceInfo.deviceStatus, MAX_BUFFER_SIZE, L"Не удалось получить состояние устройства");
            }

            AddUSBDeviceToList(&deviceInfo);
        }
        i++;
    }
    SetupDiDestroyDeviceInfoList(deviceInfoSet); 
}

void PopulateTreeViewWithBluetoothDevices(HWND hTreeView) {
    BLUETOOTH_DEVICE_SEARCH_PARAMS searchParams;
    ZeroMemory(&searchParams, sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS));
    searchParams.dwSize = sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS);
    searchParams.fReturnAuthenticated = TRUE;
    searchParams.fReturnRemembered = TRUE;
    searchParams.fReturnUnknown = TRUE;
    searchParams.fReturnConnected = TRUE;
    searchParams.hRadio = NULL; 

    BLUETOOTH_DEVICE_INFO deviceInfo;
    ZeroMemory(&deviceInfo, sizeof(BLUETOOTH_DEVICE_INFO));
    deviceInfo.dwSize = sizeof(BLUETOOTH_DEVICE_INFO);

    HBLUETOOTH_DEVICE_FIND hDeviceFind = BluetoothFindFirstDevice(&searchParams, &deviceInfo);

    if (hDeviceFind == NULL) {
        MessageBoxW(NULL, L"Bluetooth устройства не найдены", L"Информация", MB_OK);
        return;
    }

    HTREEITEM hBluetoothRootItem = AddTreeViewItem(hTreeView, NULL, L"Bluetooth устройства", -1);
    int bluetoothDeviceCount = 0;

    do {
        BluetoothDeviceInfo btDeviceInfo = { 0 };
        wcscpy_s(btDeviceInfo.name, MAX_BUFFER_SIZE, deviceInfo.szName);

        wcscpy_s(btDeviceInfo.manufacturerName, MAX_BUFFER_SIZE, L"Неизвестный производитель");
        wcscpy_s(btDeviceInfo.deviceType, MAX_BUFFER_SIZE, L"Bluetooth устройство");
        wcscpy_s(btDeviceInfo.driverName, MAX_BUFFER_SIZE, L"Неизвестный драйвер");
        wcscpy_s(btDeviceInfo.driverVersion, MAX_BUFFER_SIZE, L"Неизвестная версия");

        wcscpy_s(btDeviceInfo.deviceStatus, MAX_BUFFER_SIZE,
            (deviceInfo.fConnected) ? L"Устройство подключено" : L"Устройство не подключено");

        HTREEITEM hDeviceItem = AddTreeViewItem(hTreeView, hBluetoothRootItem, btDeviceInfo.name, bluetoothDeviceCount);

        bluetoothDeviceCount++;

        AddBluetoothDeviceToList(&btDeviceInfo);

    } while (BluetoothFindNextDevice(hDeviceFind, &deviceInfo));

    BluetoothFindDeviceClose(hDeviceFind);

    if (bluetoothDeviceCount == 0) {
        MessageBoxW(NULL, L"Bluetooth устройства не найдены", L"Информация", MB_OK);
    }
}    


LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static HWND hTreeView;
    static HWND hInfoText;
    static HTREEITEM hUsbRootItem; 
    static HTREEITEM hBluetoothRootItem; 

    switch (uMsg) {
    case WM_CREATE: {
        hTreeView = CreateWindowExW(0, WC_TREEVIEW, L"", WS_VISIBLE | WS_CHILD | WS_BORDER | TVS_HASLINES | TVS_LINESATROOT,
            10, 10, 300, 400, hwnd, (HMENU)IDC_TREEVIEW, NULL, NULL);
        hInfoText = CreateWindowExW(0, WC_EDIT, L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_MULTILINE | ES_READONLY,
            320, 10, 450, 400, hwnd, (HMENU)IDC_INFOTEXT, NULL, NULL);

        PopulateTreeViewWithUSBDevices(hTreeView);
        PopulateTreeViewWithBluetoothDevices(hTreeView); 
        break;
    }
    case WM_NOTIFY:
        if (((LPNMHDR)lParam)->hwndFrom == hTreeView) {
            LPNMTREEVIEW pnmTreeView = (LPNMTREEVIEW)lParam;
            if (pnmTreeView->hdr.code == NM_CLICK) {
                TVITEM item;
                item.mask = TVIF_PARAM | TVIF_HANDLE;
                item.hItem = TreeView_GetSelection(hTreeView);
                TreeView_GetItem(hTreeView, &item);
                int deviceIndex = (int)item.lParam;

                HTREEITEM selectedItem = TreeView_GetSelection(hTreeView);
                HTREEITEM hUsbRootItem = TreeView_GetChild(hTreeView, NULL);
                HTREEITEM hBluetoothRootItem = TreeView_GetNextSibling(hTreeView, hUsbRootItem);

                if (selectedItem != NULL) {
                    if (TreeView_GetParent(hTreeView, selectedItem) == hUsbRootItem) {
                        DisplayDeviceInfo(hInfoText, deviceIndex, 0);
                    }
                    else if (TreeView_GetParent(hTreeView, selectedItem) == hBluetoothRootItem) {
                        DisplayDeviceInfo(hInfoText, deviceIndex, 1);  
                    }
                }
            }
        }
        break;


    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
    MSG msg;
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"DeviceInfoApp";
    wc.hIcon = (HICON)LoadImage(NULL, L"m.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
    RegisterClass(&wc);

    HWND hwnd = CreateWindowExW(0, L"DeviceInfoApp", L"Device Information", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 450, NULL, NULL, hInstance, NULL);
    ShowWindow(hwnd, nShowCmd);

    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}






























