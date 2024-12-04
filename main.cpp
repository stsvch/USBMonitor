#include "wmi_utils.h"
#include "usb_utils.h"
#include <windows.h>
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")

#define IDC_TREEVIEW 1
#define IDC_INFOTEXT 2
#define IDC_SBUTTON 3

HWND hSButton = NULL;
HWND hInfoText = NULL;
HTREEITEM selectedItem = NULL;
DeviceInfo usbDeviceProfile[256];
DeviceInfo bluetoothDeviceProfile[256];
int usbDeviceProfileCount = 0;
int bluetoothDeviceProfileCount = 0;
IWbemLocator* pLoc = NULL;
IWbemServices* pSvc = NULL;
int selectedIndex=-2;


void ClearUSBList()
{
    usbDeviceProfileCount = 0;
}

void ClearBluetoothList()
{
    bluetoothDeviceProfileCount = 0;
}

void ClearTree(HWND hwndTree)
{
    HTREEITEM hItem = TreeView_GetRoot(hwndTree);
    while (hItem != NULL) 
    {
        HTREEITEM hNextItem = TreeView_GetNextItem(hwndTree, hItem, TVGN_NEXT); 
        TreeView_DeleteItem(hwndTree, hItem);
        hItem = hNextItem; 
    }
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

void DisplayFullInfo(const DeviceInfo* deviceInfo, const WCHAR* entityQuery, const WCHAR* driverQuery, BOOL isUSB) 
{
    if (!deviceInfo || !hInfoText) return;

    WCHAR infoBuffer[MAX_BUFFER_SIZE * 10]; 
    wcscpy_s(infoBuffer, MAX_BUFFER_SIZE * 10, L"");

    int resultCount = 0;
    Map* deviceDetails = FullQueryDevices(pSvc, entityQuery, &resultCount);
    swprintf_s(infoBuffer, MAX_BUFFER_SIZE, L"%s: %s\r\n", L"Connected", deviceInfo->IsConnected ? L"True" : L"False");
    if(deviceInfo->IsConnected)
    {
        SetWindowText((HWND)hSButton, L"Отключить");
    }
    else
    {
        SetWindowText((HWND)hSButton, L"Подключить");
    }

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
    isUSB? ShowWindow((HWND)hSButton, SW_SHOW): ShowWindow((HWND)hSButton, SW_HIDE);
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

        DisplayFullInfo(deviceInfo, entityQuery, driverQuery, FALSE);
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

        DisplayFullInfo(deviceInfo, entityQuery, driverQuery, TRUE);
    }
}

HTREEITEM AddTreeViewItem(HWND hTreeView, HTREEITEM hParent, const WCHAR* text, int deviceIndex, BOOL isUsb)
{
    TVINSERTSTRUCTW tvinsert;
    tvinsert.hParent = hParent;
    tvinsert.hInsertAfter = TVI_LAST;

    tvinsert.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
    tvinsert.item.pszText = (LPWSTR)text;
    tvinsert.item.lParam = (LPARAM)deviceIndex;

    if (isUsb)
    {
        tvinsert.item.iImage = 0;        
        tvinsert.item.iSelectedImage = 0; 
    }
    else
    {
        tvinsert.item.iImage = 1;        
        tvinsert.item.iSelectedImage = 1; 
    }

    return TreeView_InsertItem(hTreeView, &tvinsert);
}

void GetUSBDevices()
{
    ClearUSBList();
    DeviceInfo* tempDevices = ListConnectedUSBDevices(&usbDeviceProfileCount);
    if (tempDevices != nullptr)
    {
        for (size_t i = 0; i < usbDeviceProfileCount && i < 256; ++i) 
        {
            usbDeviceProfile[i] = tempDevices[i];
            WCHAR escapedID[MAX_BUFFER_SIZE];
            wcscpy_s(escapedID, MAX_BUFFER_SIZE, usbDeviceProfile[i].DeviceID);
            EscapeBackslashes(escapedID);
            usbDeviceProfile[i].IsConnected = IsUsbDeviceConnected(pSvc, escapedID);

            GetDiskPathFromDeviceID(pSvc, escapedID);
               

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

    if (tempDevices != NULL) 
    {
        for (size_t i = 0; i < bluetoothDeviceProfileCount && i < 256; i++)
        {
            bluetoothDeviceProfile[i] = tempDevices[i];
            bluetoothDeviceProfile[i].IsConnected = IsBluetoothDeviceConnected(bluetoothDeviceProfile[i].Caption);
        }
        delete[] tempDevices; 
    }
}

void PopulateTreeViewWithDevices(HWND hTreeView)
{
    ShowWindow((HWND)hSButton, SW_HIDE);
    TreeView_DeleteAllItems(hTreeView);
    GetUSBDevices();
    HTREEITEM hUsbRootItem = AddTreeViewItem(hTreeView, NULL, L"USB устройства", -1, TRUE);
    for (int i = 0; i < usbDeviceProfileCount; i++)
    {
        HTREEITEM hDeviceItem = AddTreeViewItem(hTreeView, hUsbRootItem, usbDeviceProfile[i].Caption, i, TRUE);
    }
    GetBluetoothDevices();
    HTREEITEM hBluetoothRootItem = AddTreeViewItem(hTreeView, NULL, L"Bluetooth устройства", -1, FALSE);
    for (int i = 0; i < bluetoothDeviceProfileCount; i++)
    {
        HTREEITEM hDeviceItem = AddTreeViewItem(hTreeView, hBluetoothRootItem, bluetoothDeviceProfile[i].Caption, i, FALSE);
    }
}

void ChangeState(BOOL isUSBDevice)
{
    if (isUSBDevice)
    {
        if (selectedIndex < 0 || selectedIndex >= usbDeviceProfileCount) return;

        DeviceInfo* deviceInfo = &usbDeviceProfile[selectedIndex];
        if (ChangeUSBDeviceState(deviceInfo->DeviceID, deviceInfo->IsConnected))
        {
            deviceInfo->IsConnected = !deviceInfo->IsConnected;
        }
        DisplayDeviceInfo(hInfoText, selectedIndex, FALSE);
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static HWND hTreeView;
    static HTREEITEM hUsbRootItem;
    static HTREEITEM hBluetoothRootItem;
    static HBRUSH hBrushWhite;
    static HIMAGELIST hImageList;

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
            350,                        
            hwnd,                      
            (HMENU)IDC_INFOTEXT,        
            NULL,                
            NULL                       
        );
        HFONT hFont = CreateFontW(
            16,                        // Высота шрифта
            0,                         // Ширина шрифта (0 — автоматически)
            0,                         // Угол наклона шрифта
            0,                         // Ориентация текста
            FW_NORMAL,                 // Толщина шрифта
            FALSE,                     // Не наклонный шрифт
            FALSE,                     // Не подчеркивание
            FALSE,                     // Без зачеркивания
            DEFAULT_CHARSET,           // Кодировка
            OUT_DEFAULT_PRECIS,        // Точность
            CLIP_DEFAULT_PRECIS,       // Поведение при выходе за границы
            DEFAULT_QUALITY,           // Качество
            DEFAULT_PITCH,             // Вид шрифта
            L"Arial"                   // Название шрифта
        );
        hSButton = CreateWindowExW(0,
            L"BUTTON", 
            L"",  
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  
            330, 370, 120, 30,  
            hwnd, 
            (HMENU)IDC_SBUTTON, 
            NULL,  
            NULL
        );
        ShowWindow((HWND)hSButton, SW_HIDE);
        SendMessage(hInfoText, WM_SETFONT, (WPARAM)hFont, TRUE);
        hImageList = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 2, 2);
        ImageList_SetBkColor(hImageList, CLR_NONE);

        HICON hIcon1 = (HICON)LoadImage(NULL, L"usb.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
        HICON hIcon2 = (HICON)LoadImage(NULL, L"b.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE);

        ImageList_AddIcon(hImageList, hIcon1);
        ImageList_AddIcon(hImageList, hIcon2);

        SendMessage(hTreeView, TVM_SETIMAGELIST, TVSIL_NORMAL, (LPARAM)hImageList);
        PopulateTreeViewWithDevices(hTreeView);
    }
    break;
    case WM_COMMAND: 
    {
        if (LOWORD(wParam) == IDC_SBUTTON)
        {  
            MessageBox(hwnd, L"Кнопка нажата", L"Сообщение", MB_OK);

            HTREEITEM hUsbRootItem = TreeView_GetChild(hTreeView, NULL);
            HTREEITEM hBluetoothRootItem = TreeView_GetNextSibling(hTreeView, hUsbRootItem);

            if (selectedItem != NULL)
            {
                if (TreeView_GetParent(hTreeView, selectedItem) == hUsbRootItem)
                {
                    ChangeState(TRUE);
                }
                else if (TreeView_GetParent(hTreeView, selectedItem) == hBluetoothRootItem)
                {
                    ChangeState(FALSE);
                }
            }
        }
    }
    break;
    case WM_DEVICECHANGE:
    {
 //       std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        SetWindowTextW(hInfoText, L"");
        ClearTree(hTreeView);
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
                selectedIndex = (int)item.lParam;

                selectedItem = TreeView_GetSelection(hTreeView);
                HTREEITEM hUsbRootItem = TreeView_GetChild(hTreeView, NULL);
                HTREEITEM hBluetoothRootItem = TreeView_GetNextSibling(hTreeView, hUsbRootItem);

                if (selectedItem != NULL)
                {
                    if (TreeView_GetParent(hTreeView, selectedItem) == hUsbRootItem)
                    {
                        DisplayDeviceInfo(hInfoText, selectedIndex, FALSE);
                    }
                    else if (TreeView_GetParent(hTreeView, selectedItem) == hBluetoothRootItem)
                    {
                        DisplayDeviceInfo(hInfoText, selectedIndex, TRUE);
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