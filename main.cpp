#include "wmi_utils.h"
#include "usb_utils.h"
#include "driver_utils.h"
#include "event.h"
#include "statistic.h"
#include "resource.h"
#include <windows.h>
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")

WCHAR globalEventBuffer[BUFFER_SIZE] = { 0 };
HBRUSH hBrushWhite;
HTREEITEM selectedItem = NULL;
DeviceInfo usbDeviceProfile[256];
int usbDeviceProfileCount = 0;
IWbemLocator* pLoc = NULL;
IWbemServices* pSvc = NULL;
int selectedIndex = -2;
HWND hPages[2];
HWND hTabControl;

void CreatePageDeviceControls(HWND hwndDlg)
{
    CreateWindowExW(
        0,
        WC_EDIT,
        L"",
        WS_VISIBLE | WS_CHILD | WS_BORDER | ES_MULTILINE | ES_READONLY | WS_VSCROLL | WS_HSCROLL,
        10,
        10,
        420,
        300,
        hwndDlg,
        (HMENU)IDC_INFOTEXT,
        NULL,
        NULL
    );

    CreateWindowExW(
        0,
        WC_BUTTON,
        L"",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        10,
        320,
        100,
        30,
        hwndDlg,
        (HMENU)IDC_BUTTON_S,
        NULL,
        NULL
    );
}

void CreatePageDriveerControls(HWND hwndDlg)
{
    CreateWindowExW(
        0,
        WC_EDIT,
        L"",
        WS_VISIBLE | WS_CHILD | WS_BORDER | ES_MULTILINE | ES_READONLY | WS_VSCROLL | WS_HSCROLL,
        10,
        10,
        420,
        300,
        hwndDlg,
        (HMENU)IDC_INFOTEXT,
        NULL,
        NULL
    );

    CreateWindowExW(
        0,
        WC_BUTTON,
        L"Обновить",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        10,
        320,
        100,
        30,
        hwndDlg,
        (HMENU)IDC_BUTTON_U,
        NULL,
        NULL
    );

    CreateWindowExW(
        0,
        WC_BUTTON,
        L"Отключить",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        120,
        320,
        100,
        30,
        hwndDlg,
        (HMENU)IDC_BUTTON_D,
        NULL,
        NULL
    );

    CreateWindowExW(
        0,
        WC_BUTTON,
        L"Откатить",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        230,
        320,
        100,
        30,
        hwndDlg,
        (HMENU)IDC_BUTTON_R,
        NULL,
        NULL
    );
}

INT_PTR CALLBACK Page1Proc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
        CreatePageDeviceControls(hwndDlg);
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_BUTTON_S)
        {
            ChangeUSBDeviceState(usbDeviceProfile[selectedIndex].DeviceID, usbDeviceProfile[selectedIndex].IsConnected);
        }
        return TRUE;
    }
    return FALSE;
}

INT_PTR CALLBACK Page2Proc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
        CreatePageDriveerControls(hwndDlg);
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_BUTTON_D)
        {
            WCHAR baseDeviceID[256];
            ExtractBaseDeviceID(usbDeviceProfile[selectedIndex].DeviceID, baseDeviceID, sizeof(baseDeviceID) / sizeof(WCHAR));
            RemoveDriver(baseDeviceID);
        }
        else if (LOWORD(wParam) == IDC_BUTTON_R)
        {
            WCHAR baseDeviceID[256];
            ExtractBaseDeviceID(usbDeviceProfile[selectedIndex].DeviceID, baseDeviceID, sizeof(baseDeviceID) / sizeof(WCHAR));
            RollbackDriver(baseDeviceID);
        }
        else if (LOWORD(wParam) == IDC_BUTTON_U)
        {
            WCHAR* path = ShowDriverSelectionDialog(hwndDlg, usbDeviceProfile[selectedIndex].DeviceID);
            WCHAR baseDeviceID[256];

            ExtractBaseDeviceID(usbDeviceProfile[selectedIndex].DeviceID, baseDeviceID, sizeof(baseDeviceID) / sizeof(WCHAR));
            InstallDriver(baseDeviceID, path);

        }
        return TRUE;
    }
    return FALSE;
}

void ClearUSBList()
{
    usbDeviceProfileCount = 0;
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

void DisplayFullInfo(const DeviceInfo* deviceInfo, const WCHAR* entityQuery, const WCHAR* driverQuery)
{
    if (!deviceInfo) return;

    WCHAR deviceInfoBuffer[MAX_BUFFER_SIZE * 10];
    WCHAR driverInfoBuffer[MAX_BUFFER_SIZE * 10];
    wcscpy_s(deviceInfoBuffer, MAX_BUFFER_SIZE * 10, L"");
    wcscpy_s(driverInfoBuffer, MAX_BUFFER_SIZE * 10, L"");

    int resultCount = 0;
    Map* deviceDetails = FullQueryDevices(pSvc, entityQuery, &resultCount);

    swprintf_s(deviceInfoBuffer, MAX_BUFFER_SIZE, L"%s: %s\r\n", L"Connected", deviceInfo->IsConnected ? L"True" : L"False");
    if (deviceInfo->IsConnected)
    {
        SetWindowText(GetDlgItem(hPages[0], IDC_BUTTON_S), L"Отключить");
    }
    else
    {
        SetWindowText(GetDlgItem(hPages[0], IDC_BUTTON_S), L"Подключить");
    }

    if (resultCount > 0 && deviceDetails)
    {
        for (int i = 0; i < resultCount; ++i)
        {
            WCHAR additionalInfo[MAX_BUFFER_SIZE];
            swprintf_s(additionalInfo, MAX_BUFFER_SIZE, L"%s: %s\r\n", deviceDetails[i].Key, deviceDetails[i].Value);

            if (wcslen(deviceInfoBuffer) + wcslen(additionalInfo) < MAX_BUFFER_SIZE * 10)
            {
                wcscat_s(deviceInfoBuffer, MAX_BUFFER_SIZE * 10, additionalInfo);
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
        for (int i = 0; i < resultCount; ++i)
        {
            WCHAR additionalDriverInfo[MAX_BUFFER_SIZE];
            swprintf_s(additionalDriverInfo, MAX_BUFFER_SIZE, L"%s: %s\r\n", driverDetails[i].Key, driverDetails[i].Value);

            if (wcslen(driverInfoBuffer) + wcslen(additionalDriverInfo) < MAX_BUFFER_SIZE * 10)
            {
                wcscat_s(driverInfoBuffer, MAX_BUFFER_SIZE * 10, additionalDriverInfo);
            }
            else
            {
                break;
            }
        }
        delete[] driverDetails;
    }

    SetWindowTextW(GetDlgItem(hPages[0], IDC_INFOTEXT), deviceInfoBuffer);
    SetWindowTextW(GetDlgItem(hPages[1], IDC_INFOTEXT), driverInfoBuffer);  
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

LRESULT CALLBACK ModalProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static HWND hListView;
    static std::vector<EventData>* pEvents;
    switch (uMsg)
    {
    case WM_CREATE:
    {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pEvents = (std::vector<EventData>*)pCreate->lpCreateParams;

        hListView = CreateWindowEx(
            0, WC_LISTVIEW, NULL,
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
            10, 10, 780, 460,
            hwnd, (HMENU)1, GetModuleHandle(NULL), NULL);

        LVCOLUMN lvc = {};
        lvc.mask = LVCF_TEXT | LVCF_WIDTH;

        std::vector<std::wstring> columns =
        {
            L"EventID", L"TimeCreated", L"Level", L"Provider Name",
            L"ProcessID", L"ThreadID", L"Channel", L"Task", L"Keywords", L"Data"
        };

        int columnWidths[] = { 80, 150, 60, 150, 80, 80, 100, 80, 150, 250 };

        for (size_t i = 0; i < columns.size(); i++) {
            lvc.pszText = (LPWSTR)columns[i].c_str();
            lvc.cx = columnWidths[i];
            ListView_InsertColumn(hListView, (int)i, &lvc);
        }

        for (size_t i = 0; i < pEvents->size(); i++) 
        {
            const EventData& event = (*pEvents)[i];

            LVITEM lvi = {};
            lvi.mask = LVIF_TEXT;
            lvi.iItem = (int)i;

            lvi.pszText = (LPWSTR)event.eventID.c_str();
            lvi.iSubItem = 0;
            ListView_InsertItem(hListView, &lvi);

            ListView_SetItemText(hListView, (int)i, 1, (LPWSTR)event.timeCreated.c_str());
            ListView_SetItemText(hListView, (int)i, 2, (LPWSTR)event.level.c_str());
            ListView_SetItemText(hListView, (int)i, 3, (LPWSTR)event.providerName.c_str());
            ListView_SetItemText(hListView, (int)i, 4, (LPWSTR)event.processID.c_str());
            ListView_SetItemText(hListView, (int)i, 5, (LPWSTR)event.threadID.c_str());
            ListView_SetItemText(hListView, (int)i, 6, (LPWSTR)event.channel.c_str());
            ListView_SetItemText(hListView, (int)i, 7, (LPWSTR)event.task.c_str());
            ListView_SetItemText(hListView, (int)i, 8, (LPWSTR)event.keywords.c_str());
            ListView_SetItemText(hListView, (int)i, 9, (LPWSTR)event.data.c_str());
        }
        break;
    }

    case WM_CLOSE:
        DestroyWindow(hwnd);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

void ShowModalWindow(HINSTANCE hInstance, HWND hwndParent, const std::vector<EventData>& events) 
{
    WNDCLASS wc = {};
    wc.lpfnWndProc = ModalProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"EventViewer";

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0, L"EventViewer", L"События устройства",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 500,
        hwndParent, NULL, hInstance, (LPVOID)&events);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) 
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnregisterClass(L"EventViewer", hInstance);
}

void GetEventList(HWND hwnd)
{
    std::vector<EventData> events;
    GetEvents(usbDeviceProfile[selectedIndex].DeviceID, events);
    HINSTANCE hInstance = GetModuleHandle(NULL);
    ShowModalWindow(hInstance, NULL, events);
}

void GetStatistic(HWND hwnd) 
{
    double readSpeed = MeasureReadSpeed(usbDeviceProfile[selectedIndex].Path);
    double writeSpeed = MeasureWriteSpeed(usbDeviceProfile[selectedIndex].Path);

    WCHAR message[MAX_BUFFER_SIZE * 4];
    swprintf_s(
        message, sizeof(message) / sizeof(WCHAR),
        L"========== Информация об устройстве ==========\n"
        L"Устройство: %s\n"
        L"Подключение: %s\n"
        L"Путь (Диск): %s\n"
        L"Тип устройства: %s\n\n"
        L"========== Тесты производительности ==========\n"
        L"Скорость чтения: %.2f МБ/с\n"
        L"Скорость записи: %.2f МБ/с\n\n",
        usbDeviceProfile[selectedIndex].Caption,
        usbDeviceProfile[selectedIndex].IsConnected ? L"Да" : L"Нет",
        usbDeviceProfile[selectedIndex].Path,
        usbDeviceProfile[selectedIndex].Type,
        readSpeed,
        writeSpeed
    );

    MessageBox(
        hwnd,            
        message,          
        L"Результаты тестирования устройства", 
        MB_OK | MB_ICONINFORMATION 
    );
}

void GetUSBDevices()
{
    ClearUSBList();
    DeviceInfo* tempDevices = ListConnectedUSBDevices(&usbDeviceProfileCount);
    if (tempDevices != nullptr)
    {
        for (size_t i = 0; i < usbDeviceProfileCount && i < 256; i++)
        {
            usbDeviceProfile[i] = tempDevices[i];
            WCHAR escapedID[MAX_BUFFER_SIZE];
            wcscpy_s(escapedID, MAX_BUFFER_SIZE, usbDeviceProfile[i].DeviceID);
            EscapeBackslashes(escapedID);
            usbDeviceProfile[i].IsConnected = IsUsbDeviceConnected(pSvc, escapedID);
            if (!GetDevicePath(usbDeviceProfile[i].DeviceID, usbDeviceProfile[i].Path))
            {
                ZeroMemory(usbDeviceProfile[i].Path, sizeof(usbDeviceProfile[i].Path));
            }
            GetDeviceType(usbDeviceProfile[i].DeviceID, usbDeviceProfile[i].Type);
        }
        delete[] tempDevices;
    }
}

void PopulateTreeViewWithDevices(HWND hTreeView)
{
    TreeView_DeleteAllItems(hTreeView);
    GetUSBDevices();
    HTREEITEM hUsbRootItem = AddTreeViewItem(hTreeView, NULL, L"USB устройства", -1, TRUE);
    for (int i = 0; i < usbDeviceProfileCount; i++)
    {
        HTREEITEM hDeviceItem = AddTreeViewItem(hTreeView, hUsbRootItem, usbDeviceProfile[i].Caption, i, TRUE);
    }
}

bool GetDeviceInstanceID(LPARAM lParam, wchar_t* outputBuffer, size_t bufferSize)
{
    DEV_BROADCAST_HDR* pHdr = (DEV_BROADCAST_HDR*)lParam;
    DEV_BROADCAST_DEVICEINTERFACE* pDev = (DEV_BROADCAST_DEVICEINTERFACE*)pHdr;
    if (wcslen(pDev->dbcc_name) < bufferSize)
    {
        wcsncpy_s(outputBuffer, bufferSize, pDev->dbcc_name, _TRUNCATE);
        return true;
    }
    else
    {
        wprintf(L"Буфер недостаточного размера\n");
        return false;
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static HWND hTreeView;
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
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case ID_UPDATE:
            PopulateTreeViewWithDevices(hTreeView);
            break;
        case ID_STATISTIC_ERROR:
            GetEventList(hwnd);
            break;
        case ID_STATISTIC_TEST:
            GetStatistic(hwnd);
            break;
        }
        break;
    case WM_CREATE:
    {
        HMENU hMenu = CreateMenu();
        HMENU hSubMenuStatistic = CreatePopupMenu();
        HMENU hSubMenuConfig = CreatePopupMenu();

        AppendMenu(hSubMenuStatistic, MF_STRING, ID_STATISTIC_ERROR, L"События");
        AppendMenu(hSubMenuStatistic, MF_STRING, ID_STATISTIC_TEST, L"Анализ");
        AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hSubMenuStatistic, L"Статистика");
        AppendMenu(hMenu, MF_STRING, ID_UPDATE, L"Обновить");

        SetMenu(hwnd, hMenu);

        hBrushWhite = CreateSolidBrush(RGB(255, 255, 255));

        hTreeView = CreateWindowExW(0, WC_TREEVIEW, L"", WS_VISIBLE | WS_CHILD | WS_BORDER | TVS_HASLINES | TVS_LINESATROOT,
            10, 10, 300, 400, hwnd, (HMENU)IDC_TREEVIEW, NULL, NULL);

        hTabControl = CreateWindowExW(WS_EX_CONTROLPARENT, WC_TABCONTROL, NULL,
            WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS | WS_BORDER,
            320, 10, 450, 400, hwnd, NULL, GetModuleHandle(NULL), NULL);

        TCITEM tie;
        tie.mask = TCIF_TEXT;

        tie.pszText = (LPWSTR)L"Устройство";
        TabCtrl_InsertItem(hTabControl, 0, &tie);

        tie.pszText = (LPWSTR)L"Драйвер";
        TabCtrl_InsertItem(hTabControl, 1, &tie);

        hPages[0] = CreateDialog(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG1), hTabControl, Page1Proc);
        hPages[1] = CreateDialog(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG2), hTabControl, Page2Proc);

        for (int i = 0; i < 2; ++i) 
        {
            SetWindowLongPtr(hPages[i], GWL_STYLE, GetWindowLongPtr(hPages[i], GWL_STYLE) & ~WS_BORDER & ~WS_CAPTION);
            SetWindowPos(hPages[i], NULL, 5, 25, 440, 370, SWP_NOZORDER);
        }

        ShowWindow(hPages[0], SW_SHOW);
        ShowWindow(hPages[1], SW_HIDE);

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

    case WM_NOTIFY:
        if (((LPNMHDR)lParam)->hwndFrom == hTabControl && ((LPNMHDR)lParam)->code == TCN_SELCHANGE)
        {
            int iPage = TabCtrl_GetCurSel(hTabControl);
            for (int i = 0; i < 2; ++i)
            {
                ShowWindow(hPages[i], (i == iPage) ? SW_SHOW : SW_HIDE);
            }
        }
        else if (((LPNMHDR)lParam)->hwndFrom == hTreeView)
        {
            LPNMTREEVIEW pnmTreeView = (LPNMTREEVIEW)lParam;
            if (pnmTreeView->hdr.code == NM_DBLCLK) 
            {
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
                        WCHAR escapedID[MAX_BUFFER_SIZE];
                        wcscpy_s(escapedID, MAX_BUFFER_SIZE, usbDeviceProfile[selectedIndex].DeviceID);
                        EscapeBackslashes(escapedID);

                        WCHAR entityQuery[MAX_BUFFER_SIZE];
                        WCHAR driverQuery[MAX_BUFFER_SIZE];
                        swprintf_s(entityQuery, MAX_BUFFER_SIZE, L"SELECT * FROM Win32_PnPEntity WHERE DeviceID = '%s'", escapedID);
                        swprintf_s(driverQuery, MAX_BUFFER_SIZE, L"SELECT * FROM Win32_PnPSignedDriver WHERE DeviceID = '%s'", escapedID);

                        DisplayFullInfo(&usbDeviceProfile[selectedIndex], entityQuery, driverQuery);
                    }
                    TabCtrl_SetCurSel(hTabControl, 0);
                    ShowWindow(hPages[0], SW_SHOW);
                    ShowWindow(hPages[1], SW_HIDE);
                }
            }
        }
        break;
    case WM_DEVICECHANGE:
    {
        PopulateTreeViewWithDevices(hTreeView);
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

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) 
{
    MSG msg;
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"usb";
    wc.hIcon = (HICON)LoadImage(NULL, L"m.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
    RegisterClass(&wc);

    HRESULT hres;
    hres = InitWMI(&pLoc, &pSvc);

    HWND hwnd = CreateWindowExW(0, L"usb", L"Usb монитор", WS_OVERLAPPEDWINDOW,
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
