#include "wmi_utils.h"
#include "usb_utils.h"
#include "driver_utils.h"
#include "resource.h"
#include <windows.h>
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")

INT_PTR CALLBACK Page1Proc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK Page2Proc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

HTREEITEM selectedItem = NULL;
DeviceInfo usbDeviceProfile[256];
int usbDeviceProfileCount = 0;
IWbemLocator* pLoc = NULL;
IWbemServices* pSvc = NULL;
int selectedIndex = -2;

HWND hPages[2]; // Массив для страниц вкладок
HWND hTabControl;

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

    // Собираем информацию об устройстве
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

    // Собираем информацию о драйверах
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

    // Устанавливаем текст на вкладках
    SetWindowTextW(GetDlgItem(hPages[0], IDC_INFOTEXT), deviceInfoBuffer);  // Вкладка "Device"
    SetWindowTextW(GetDlgItem(hPages[1], IDC_INFOTEXT), driverInfoBuffer);  // Вкладка "Driver"
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

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static HWND hTreeView;
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

        // Создаем дерево (TreeView)
        hTreeView = CreateWindowExW(0, WC_TREEVIEW, L"", WS_VISIBLE | WS_CHILD | WS_BORDER | TVS_HASLINES | TVS_LINESATROOT,
            10, 10, 300, 400, hwnd, (HMENU)IDC_TREEVIEW, NULL, NULL);

        // Создаем Tab Control
        hTabControl = CreateWindowExW(WS_EX_CONTROLPARENT, WC_TABCONTROL, NULL,
            WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS | WS_BORDER,
            320, 10, 450, 400, hwnd, NULL, GetModuleHandle(NULL), NULL);

        // Настройка Tab Control
        TCITEM tie;
        tie.mask = TCIF_TEXT;

        // Вкладка Device
        tie.pszText = (LPWSTR)L"Device";
        TabCtrl_InsertItem(hTabControl, 0, &tie);

        // Вкладка Driver
        tie.pszText = (LPWSTR)L"Driver";
        TabCtrl_InsertItem(hTabControl, 1, &tie);

        // Создаем страницы как диалоговые окна (дочерние элементы) и убираем лишние стили
        hPages[0] = CreateDialog(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG1), hTabControl, Page1Proc);
        hPages[1] = CreateDialog(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG2), hTabControl, Page2Proc);

        for (int i = 0; i < 2; ++i) 
        {
            SetWindowLongPtr(hPages[i], GWL_STYLE, GetWindowLongPtr(hPages[i], GWL_STYLE) & ~WS_BORDER & ~WS_CAPTION);
            SetWindowPos(hPages[i], NULL, 5, 25, 440, 370, SWP_NOZORDER);
        }

        // Показываем только первую страницу по умолчанию
        ShowWindow(hPages[0], SW_SHOW);
        ShowWindow(hPages[1], SW_HIDE);

        // Создаем список изображений для TreeView
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
            // Обработка переключения вкладок
            int iPage = TabCtrl_GetCurSel(hTabControl);
            for (int i = 0; i < 2; ++i)
            {
                ShowWindow(hPages[i], (i == iPage) ? SW_SHOW : SW_HIDE);
            }
        }
        else if (((LPNMHDR)lParam)->hwndFrom == hTreeView)
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
                        // Выбран элемент USB
                        WCHAR escapedID[MAX_BUFFER_SIZE];
                        wcscpy_s(escapedID, MAX_BUFFER_SIZE, usbDeviceProfile[selectedIndex].DeviceID);
                        EscapeBackslashes(escapedID);

                        WCHAR entityQuery[MAX_BUFFER_SIZE];
                        WCHAR driverQuery[MAX_BUFFER_SIZE];
                        swprintf_s(entityQuery, MAX_BUFFER_SIZE, L"SELECT * FROM Win32_PnPEntity WHERE DeviceID = '%s'", escapedID);
                        swprintf_s(driverQuery, MAX_BUFFER_SIZE, L"SELECT * FROM Win32_PnPSignedDriver WHERE DeviceID = '%s'", escapedID);

                        DisplayFullInfo(&usbDeviceProfile[selectedIndex], entityQuery, driverQuery);
                    }
                    // Показываем первую вкладку по умолчанию
                    TabCtrl_SetCurSel(hTabControl, 0);
                    ShowWindow(hPages[0], SW_SHOW);
                    ShowWindow(hPages[1], SW_HIDE);
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

// Функция для создания общих элементов
void CreatePageDeviceControls(HWND hwndDlg) {
    // Создать текстовое поле с увеличенной высотой
    CreateWindowExW(
        0,
        WC_EDIT,
        L"",
        WS_VISIBLE | WS_CHILD | WS_BORDER | ES_MULTILINE | ES_READONLY | WS_VSCROLL | WS_HSCROLL,
        10,          // Отступ слева
        10,          // Отступ сверху
        420,         // Ширина текстового поля (уменьшили на 30 пикселей для отступов)
        300,         // Увеличенная высота текстового поля
        hwndDlg,
        (HMENU)IDC_INFOTEXT,
        NULL,
        NULL
    );

    // Создать кнопку ниже текстового поля
    CreateWindowExW(
        0,
        WC_BUTTON,
        L"Click Me",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        10,          // Отступ слева
        320,         // Позиция ниже текстового поля
        100,         // Ширина кнопки
        30,          // Высота кнопки
        hwndDlg,
        (HMENU)IDC_BUTTON_S,
        NULL,
        NULL
    );
}

// Функция для создания общих элементов
void CreatePageDriveerControls(HWND hwndDlg) 
{
    // Создать текстовое поле с увеличенной высотой
    CreateWindowExW(
        0,
        WC_EDIT,
        L"",
        WS_VISIBLE | WS_CHILD | WS_BORDER | ES_MULTILINE | ES_READONLY | WS_VSCROLL | WS_HSCROLL,
        10,          // Отступ слева
        10,          // Отступ сверху
        420,         // Ширина текстового поля (уменьшили на 30 пикселей для отступов)
        300,         // Увеличенная высота текстового поля
        hwndDlg,
        (HMENU)IDC_INFOTEXT,
        NULL,
        NULL
    );

    // Создать кнопку ниже текстового поля
    CreateWindowExW(
        0,
        WC_BUTTON,
        L"Обновить",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        10,          // Отступ слева
        320,         // Позиция ниже текстового поля
        100,         // Ширина кнопки
        30,          // Высота кнопки
        hwndDlg,
        (HMENU)IDC_BUTTON_U,
        NULL,
        NULL
    );
    // Создать кнопку ниже текстового поля
    CreateWindowExW(
        0,
        WC_BUTTON,
        L"Отключить",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        120,          // Отступ слева
        320,         // Позиция ниже текстового поля
        100,         // Ширина кнопки
        30,          // Высота кнопки
        hwndDlg,
        (HMENU)IDC_BUTTON_D,
        NULL,
        NULL
    );
    // Создать кнопку ниже текстового поля
    CreateWindowExW(
        0,
        WC_BUTTON,
        L"Откатить",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        230,          // Отступ слева
        320,         // Позиция ниже текстового поля
        100,         // Ширина кнопки
        30,          // Высота кнопки
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
            MessageBox(hwndDlg, L"Button on Page 2 clicked!", L"Info", MB_OK);
        }
        else if (LOWORD(wParam) == IDC_BUTTON_R)
        {

        }
        else if (LOWORD(wParam) == IDC_BUTTON_U)
        {
            WCHAR* path = ShowDriverSelectionDialog(hwndDlg, usbDeviceProfile[selectedIndex].DeviceID);
            WCHAR baseDeviceID[256]; // Буфер для хранения основного Device ID

            ExtractBaseDeviceID(usbDeviceProfile[selectedIndex].DeviceID, baseDeviceID, sizeof(baseDeviceID) / sizeof(WCHAR));

            InstallDriver(baseDeviceID, path);

        }
        return TRUE;
    }
    return FALSE;
}
