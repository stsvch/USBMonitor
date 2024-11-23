#include "wmi_utils.h"
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
#include <wchar.h>

#pragma comment(lib, "Bthprops.lib")
#pragma comment(lib, "version.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "comctl32.lib")

#define IDC_TREEVIEW 1
#define IDC_INFOTEXT 2

DeviceInfo usbDeviceProfile[256];
DeviceInfo bluetoothDeviceProfile[256];
int usbDeviceProfileCount = 0;
int bluetoothDeviceProfileCount = 0;
IWbemLocator* pLoc = NULL;
IWbemServices* pSvc = NULL;

void ClearUSBList()
{
    usbDeviceProfileCount = 0;
}

void ClearBluetoothList()
{
    bluetoothDeviceProfileCount = 0;
}

void EscapeBackslashes(WCHAR* str) 
{
    WCHAR* p = str;
    while (*p)
    {
        if (*p == L'\\') 
        {
            memmove(p + 1, p, (wcslen(p) + 1) * sizeof(WCHAR));
            *p = L'\\'; 
            p++; 
        }
        p++;
    }
}

void DisplayFullInfo(HWND hInfoText, const DeviceInfo* deviceInfo, const WCHAR* entityQuery, const WCHAR* driverQuery) 
{
    if (!deviceInfo || !hInfoText) return;

    WCHAR infoBuffer[MAX_BUFFER_SIZE * 10]; 
    wcscpy_s(infoBuffer, MAX_BUFFER_SIZE * 10, L"");

    int resultCount = 0;
    Map* deviceDetails = FullQueryDevices(pSvc, entityQuery, &resultCount);
    swprintf_s(infoBuffer, MAX_BUFFER_SIZE, L"%s: %s\r\n", L"Connected", deviceInfo->IsConnected ? L"True" : L"False");

    if (resultCount > 0 && deviceDetails) 
    {
        for (int i = 0; i < resultCount; ++i) 
        {
            WCHAR additionalInfo[MAX_BUFFER_SIZE];
            swprintf_s(additionalInfo, MAX_BUFFER_SIZE, L"%s: %s\r\n", deviceDetails[i].Key, deviceDetails[i].Value);

            if (wcslen(infoBuffer) + wcslen(additionalInfo) < MAX_BUFFER_SIZE * 10) 
            {
                wcscat_s(infoBuffer, MAX_BUFFER_SIZE * 10, additionalInfo);
            }
            else 
            {
                break;
            }
        }
        delete[] deviceDetails;
    }

    resultCount = 0;
    Map* driverDetails = FullQueryDevices(pSvc, driverQuery, &resultCount);

    if (resultCount > 0 && driverDetails) 
    {
        wcscat_s(infoBuffer, MAX_BUFFER_SIZE * 10, L"\r\nИнформация о драйверах:\r\n");

        for (int i = 0; i < resultCount; ++i) 
        {
            WCHAR driverInfo[MAX_BUFFER_SIZE];
            swprintf_s(driverInfo, MAX_BUFFER_SIZE, L"%s: %s\r\n", driverDetails[i].Key, driverDetails[i].Value);

            if (wcslen(infoBuffer) + wcslen(driverInfo) < MAX_BUFFER_SIZE * 10) 
            {
                wcscat_s(infoBuffer, MAX_BUFFER_SIZE * 10, driverInfo);
            }
            else 
            {
                break;
            }
        }
        delete[] driverDetails;
    }
    SetWindowTextW(hInfoText, infoBuffer);
}


void DisplayDeviceInfo(HWND hInfoText, int deviceIndex, int isBluetooth) 
{
    if (isBluetooth) 
    {
        if (deviceIndex < 0 || deviceIndex >= bluetoothDeviceProfileCount) return;

        DeviceInfo* deviceInfo = &bluetoothDeviceProfile[deviceIndex];
        WCHAR escapedID[MAX_BUFFER_SIZE];
        wcscpy_s(escapedID, MAX_BUFFER_SIZE, deviceInfo->DeviceID);
        EscapeBackslashes(escapedID);

        WCHAR entityQuery[MAX_BUFFER_SIZE];
        WCHAR driverQuery[MAX_BUFFER_SIZE];
        swprintf_s(entityQuery, MAX_BUFFER_SIZE, L"SELECT * FROM Win32_PnPEntity WHERE DeviceID = '%s'", escapedID);
        swprintf_s(driverQuery, MAX_BUFFER_SIZE, L"SELECT * FROM Win32_PnPSignedDriver WHERE DeviceID = '%s'", escapedID);

        DisplayFullInfo(hInfoText, deviceInfo, entityQuery, driverQuery);
    }
    else 
    {
        if (deviceIndex < 0 || deviceIndex >= usbDeviceProfileCount) return;

        DeviceInfo* deviceInfo = &usbDeviceProfile[deviceIndex];
        WCHAR escapedID[MAX_BUFFER_SIZE];
        wcscpy_s(escapedID, MAX_BUFFER_SIZE, deviceInfo->DeviceID);
        EscapeBackslashes(escapedID);

        WCHAR entityQuery[MAX_BUFFER_SIZE];
        WCHAR driverQuery[MAX_BUFFER_SIZE];
        swprintf_s(entityQuery, MAX_BUFFER_SIZE, L"SELECT * FROM Win32_PnPEntity WHERE DeviceID = '%s'", escapedID);
        swprintf_s(driverQuery, MAX_BUFFER_SIZE, L"SELECT * FROM Win32_PnPSignedDriver WHERE DeviceID = '%s'", escapedID);

        DisplayFullInfo(hInfoText, deviceInfo, entityQuery, driverQuery);
    }
}

HTREEITEM AddTreeViewItem(HWND hTreeView, HTREEITEM hParent, const WCHAR* text, int deviceIndex) {
    TVINSERTSTRUCTW tvinsert;
    tvinsert.hParent = hParent;
    tvinsert.hInsertAfter = TVI_LAST;
    tvinsert.item.mask = TVIF_TEXT | TVIF_PARAM;
    tvinsert.item.pszText = (LPWSTR)text;
    tvinsert.item.lParam = (LPARAM)deviceIndex;

    return TreeView_InsertItem(hTreeView, &tvinsert);
}

void GetUSBDevices()
{
    ClearUSBList();
    DeviceInfo* tempDevices = QueryDevices(pSvc, L"SELECT * FROM Win32_USBHub", &usbDeviceProfileCount);
    if (tempDevices != nullptr)
    {
        for (size_t i = 0; i < usbDeviceProfileCount && i < 256; ++i) 
        {
            usbDeviceProfile[i] = tempDevices[i];
            WCHAR escapedID[MAX_BUFFER_SIZE];
            wcscpy_s(escapedID, MAX_BUFFER_SIZE, usbDeviceProfile[i].DeviceID);
            EscapeBackslashes(escapedID);
            usbDeviceProfile[i].IsConnected = IsDeviceConnected(pSvc, escapedID);
        }
        delete[] tempDevices; 
    }
}

void GetBluetoothDevices()
{
    ClearBluetoothList();
    DeviceInfo* tempDevices = QueryDevices(
        pSvc,
        L"SELECT * FROM Win32_PnPEntity WHERE Description LIKE '%Bluetooth%' AND DeviceID LIKE '%DEV_%'",
        &bluetoothDeviceProfileCount
    );

    if (tempDevices != nullptr) 
    {
        for (size_t i = 0; i < bluetoothDeviceProfileCount && i < 256; ++i)
        {
            bluetoothDeviceProfile[i] = tempDevices[i];
            WCHAR escapedID[MAX_BUFFER_SIZE];
            wcscpy_s(escapedID, MAX_BUFFER_SIZE, bluetoothDeviceProfile[i].DeviceID);
            EscapeBackslashes(escapedID);
            bluetoothDeviceProfile[i].IsConnected = IsDeviceConnected(pSvc, bluetoothDeviceProfile[i].Caption);
        }
        delete[] tempDevices; 
    }
}

void PopulateTreeViewWithDevices(HWND hTreeView)
{
    TreeView_DeleteAllItems(hTreeView);
    GetUSBDevices();
    HTREEITEM hUsbRootItem = AddTreeViewItem(hTreeView, NULL, L"USB устройства", -1);
    for (int i = 0; i < usbDeviceProfileCount; i++)
    {
        HTREEITEM hDeviceItem = AddTreeViewItem(hTreeView, hUsbRootItem, usbDeviceProfile[i].Caption, i);
    }
    GetBluetoothDevices();
    HTREEITEM hBluetoothRootItem = AddTreeViewItem(hTreeView, NULL, L"Bluetooth устройства", -1);
    for (int i = 0; i < bluetoothDeviceProfileCount; i++)
    {
        HTREEITEM hDeviceItem = AddTreeViewItem(hTreeView, hBluetoothRootItem, bluetoothDeviceProfile[i].Caption, i);
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static HWND hTreeView;
    static HWND hInfoText;
    static HTREEITEM hUsbRootItem;
    static HTREEITEM hBluetoothRootItem;
    static HBRUSH hBrushWhite;

    switch (uMsg) 
    {
    case WM_ERASEBKGND:
    {
        HDC hdc = (HDC)wParam;
        RECT rect;
        GetClientRect(hwnd, &rect);
        FillRect(hdc, &rect, hBrushWhite);
    }
    break;
    case WM_CREATE:
    {
        hBrushWhite = CreateSolidBrush(RGB(255, 255, 255));
        hTreeView = CreateWindowExW(0, WC_TREEVIEW, L"", WS_VISIBLE | WS_CHILD | WS_BORDER | TVS_HASLINES | TVS_LINESATROOT,
            10, 10, 300, 400, hwnd, (HMENU)IDC_TREEVIEW, NULL, NULL);
        hInfoText = CreateWindowExW(
            0,                     
            WC_EDIT,                  
            L"",                    
            WS_VISIBLE | WS_CHILD | WS_BORDER | ES_MULTILINE | ES_READONLY | WS_VSCROLL | WS_HSCROLL, 
            320,                       
            10,                       
            450,                 
            400,                        
            hwnd,                      
            (HMENU)IDC_INFOTEXT,        
            NULL,                
            NULL                       
        );
        PopulateTreeViewWithDevices(hTreeView);
    }
    break;
    case WM_NOTIFY:
        if (((LPNMHDR)lParam)->hwndFrom == hTreeView)
        {
            LPNMTREEVIEW pnmTreeView = (LPNMTREEVIEW)lParam;
            if (pnmTreeView->hdr.code == NM_DBLCLK) {
                TVITEM item;
                item.mask = TVIF_PARAM | TVIF_HANDLE;
                item.hItem = TreeView_GetSelection(hTreeView);
                TreeView_GetItem(hTreeView, &item);
                int deviceIndex = (int)item.lParam;

                HTREEITEM selectedItem = TreeView_GetSelection(hTreeView);
                HTREEITEM hUsbRootItem = TreeView_GetChild(hTreeView, NULL);
                HTREEITEM hBluetoothRootItem = TreeView_GetNextSibling(hTreeView, hUsbRootItem);

                if (selectedItem != NULL)
                {
                    if (TreeView_GetParent(hTreeView, selectedItem) == hUsbRootItem)
                    {
                        DisplayDeviceInfo(hInfoText, deviceIndex, 0);
                    }
                    else if (TreeView_GetParent(hTreeView, selectedItem) == hBluetoothRootItem)
                    {
                        DisplayDeviceInfo(hInfoText, deviceIndex, 1);
                    }
                }
            }
        }
        break;
    case WM_DESTROY:
        DeleteObject(hBrushWhite);
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

    HRESULT hres;
    hres = InitWMI(&pLoc, &pSvc);

    HWND hwnd = CreateWindowExW(0, L"DeviceInfoApp", L"Device Information", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 450, NULL, NULL, hInstance, NULL);
    ShowWindow(hwnd, nShowCmd);
   
    if (FAILED(hres)) return 1;

    while (GetMessage(&msg, NULL, 0, 0)) 
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    CoUninitialize();
    return 0;
}