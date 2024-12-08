#include "driver_utils.h"

void RemoveDriver(const WCHAR* deviceId) {
    HDEVINFO hDevInfo;
    SP_DEVINFO_DATA DeviceInfoData;

    // �������� ������ ���������
    hDevInfo = SetupDiGetClassDevs(NULL, deviceId, NULL, DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (hDevInfo == INVALID_HANDLE_VALUE) {
        MessageBoxW(NULL, L"������ ��������� ������ ���������", L"������", MB_OK | MB_ICONERROR);
        return;
    }

    DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    // ���������, ���������� �� ����������
    if (!SetupDiEnumDeviceInfo(hDevInfo, 0, &DeviceInfoData)) {
        MessageBoxW(NULL, L"���������� �� ������� ��� ��������.", L"������", MB_OK | MB_ICONERROR);
        SetupDiDestroyDeviceInfoList(hDevInfo);
        return;
    }

    // ������� ����������
    if (!SetupDiRemoveDevice(hDevInfo, &DeviceInfoData)) {
        DWORD dwError = GetLastError();
        WCHAR messageBuffer[256];
        StringCchPrintfW(messageBuffer, 256, L"������ �������� ��������. ��� ������: %lu", dwError);
        MessageBoxW(NULL, messageBuffer, L"������", MB_OK | MB_ICONERROR);
    }
    else {
        MessageBoxW(NULL, L"���������� ������� �������.", L"����������", MB_OK | MB_ICONINFORMATION);
    }

    // ������� ������ ���������
    SetupDiDestroyDeviceInfoList(hDevInfo);

}

void InstallDriver(const WCHAR* deviceId, const WCHAR* infPath) {
    HDEVINFO hDevInfo;
    SP_DEVINFO_DATA DeviceInfoData;
    DWORD dwError;
    BOOL rebootRequired = FALSE;


    // ��������� ��������� ���������
    hDevInfo = SetupDiGetClassDevs(NULL, deviceId, NULL, DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (hDevInfo == INVALID_HANDLE_VALUE) {
        MessageBoxW(NULL, L"������ ��������� ������ ��������� ����� ��������", L"������", MB_OK | MB_ICONERROR);
        return;
    }

    DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    // ���������, ���������� �� ����������
    if (!SetupDiEnumDeviceInfo(hDevInfo, 0, &DeviceInfoData)) {
        dwError = GetLastError();
        WCHAR messageBuffer[256];
        StringCchPrintfW(messageBuffer, 256, L"���������� �� ������� ����� ��������. ��� ������: %lu", dwError);
        MessageBoxW(NULL, messageBuffer, L"������", MB_OK | MB_ICONERROR);
        SetupDiDestroyDeviceInfoList(hDevInfo);
        return;
    }

    // �������� ���������� �������
    if (!UpdateDriverForPlugAndPlayDevicesW(
        NULL,           // HWND: ���� ��� ������ ���������� ���� (NULL ��� ��������������� ������)
        deviceId,       // ID ����������
        infPath,        // ���� � INF-�����
        INSTALLFLAG_FORCE | INSTALLFLAG_NONINTERACTIVE, // �������������� ����������
        &rebootRequired // ��������� �� ������������
    )) {
        dwError = GetLastError();
        WCHAR messageBuffer[256];
        StringCchPrintfW(messageBuffer, 256, L"������ ��������� ��������. ��� ������: %lu", dwError);
        MessageBoxW(NULL, messageBuffer, L"������", MB_OK | MB_ICONERROR);
    }
    else {
        MessageBoxW(NULL, L"������� ������� ����������!", L"�����", MB_OK | MB_ICONINFORMATION);
        SaveDriverHistory(deviceId, infPath);
    }

    // ������� ������ ���������
    SetupDiDestroyDeviceInfoList(hDevInfo);
}
WCHAR selectedFilePath[260] = { 0 };

// ������� ��� ������ ��������
void RollbackDriver(const WCHAR* deviceIdentifier)
{
    HDEVINFO deviceInfoSet = SetupDiGetClassDevsW(NULL, NULL, NULL, DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        MessageBoxW(NULL, L"Failed to get device info set.", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    SP_DEVINFO_DATA deviceInfoData;
    deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    BOOL found = FALSE;

    WCHAR infPath[MAX_PATH];

    for (DWORD i = 0; SetupDiEnumDeviceInfo(deviceInfoSet, i, &deviceInfoData); i++) {
        WCHAR deviceID[256];
        if (SetupDiGetDeviceInstanceIdW(deviceInfoSet, &deviceInfoData, deviceID, sizeof(deviceID) / sizeof(WCHAR), NULL)) {
            if (wcsstr(deviceID, deviceIdentifier) != NULL) {
                found = TRUE;

                // �������� ������� �������� ��� ����������
                if (!LoadDriverHistory(deviceIdentifier, infPath, MAX_PATH)) {
                    MessageBoxW(NULL, L"No history found for rollback.", L"Error", MB_OK | MB_ICONERROR);
                    break;
                }

                // ������ �������� �� ���������� ������
                BOOL updateResult = UpdateDriverForPlugAndPlayDevicesW(
                    NULL,                // HWND (����)
                    deviceID,            // ������������� ����������
                    infPath,             // ���� � INF-�����
                    INSTALLFLAG_FORCE,   // �������������� ���������
                    NULL                 // ��������� ��� ������������
                );

                if (updateResult) {
                    MessageBoxW(NULL, L"Driver rollback successful.", L"Success", MB_OK | MB_ICONINFORMATION);
                }
                else {
                    WCHAR errorMsg[256];
                    swprintf_s(errorMsg, sizeof(errorMsg) / sizeof(WCHAR), L"Failed to rollback driver. Error: %lu", GetLastError());
                    MessageBoxW(NULL, errorMsg, L"Error", MB_OK | MB_ICONERROR);
                }
                break;
            }
        }
    }

    if (!found) {
        MessageBoxW(NULL, L"Device not found.", L"Error", MB_OK | MB_ICONERROR);
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);
}

INT_PTR CALLBACK DriverSelectionProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    static WCHAR** drivers;
    static int driverCount;

    switch (message) {
    case WM_INITDIALOG:
        drivers = (WCHAR**)lParam; // �������� ������ ���������
        driverCount = 0;

        // ��������� ������ ���������
        while (drivers[driverCount] != NULL) {
            SendDlgItemMessage(hDlg, IDC_LISTBOX, LB_ADDSTRING, 0, (LPARAM)drivers[driverCount]);
            driverCount++;
        }
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) {
            int selected = (int)SendDlgItemMessage(hDlg, IDC_LISTBOX, LB_GETCURSEL, 0, 0);
            if (selected != LB_ERR) {
                EndDialog(hDlg, selected); // ���������� ������ ���������� ��������
            }
            else {
                EndDialog(hDlg, -1); // ������ �� �������
            }
        }
        else if (LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, -1); // ������
        }
        else if (LOWORD(wParam) == IDC_BROWSE) {
            // ��������� ������ ������ �����
            OPENFILENAME ofn;
            ZeroMemory(&ofn, sizeof(ofn));
            WCHAR szFile[260] = { 0 };

            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hDlg;
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile);
            ofn.lpstrFilter = L"INF Files\0*.INF\0All Files\0*.*\0";
            ofn.nFilterIndex = 1;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

            if (GetOpenFileName(&ofn)) {
                wcscpy_s(selectedFilePath, szFile); // ��������� ����
                EndDialog(hDlg, -2); // ��������� ��� ��� �����
            }
        }
        break;

    case WM_CLOSE:
        EndDialog(hDlg, -1);
        break;
    }
    return (INT_PTR)FALSE;
}

LPWSTR ShowDriverSelectionDialog(HWND hwnd, LPCWSTR deviceId) {
    HDEVINFO deviceInfoSet;
    SP_DEVINFO_DATA deviceInfoData;
    SP_DRVINFO_DATA driverInfoData;
    WCHAR** drivers = NULL; // ������������ ������ ����������
    int driverCount = 0;

    // ��������� ������ ���� ��������� � �������
    deviceInfoSet = SetupDiGetClassDevs(NULL, NULL, NULL, DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        MessageBox(hwnd, L"Failed to get the list of devices.", L"Error", MB_OK | MB_ICONERROR);
        return NULL;
    }

    deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    int deviceIndex = 0;

    // ������� ���� ���������
    while (SetupDiEnumDeviceInfo(deviceInfoSet, deviceIndex, &deviceInfoData)) {
        deviceIndex++;

        // ��������� ID �������� ����������
        WCHAR currentDeviceId[MAX_DEVICE_ID_LEN];
        if (SetupDiGetDeviceInstanceId(deviceInfoSet, &deviceInfoData, currentDeviceId, MAX_DEVICE_ID_LEN, NULL)) {
            // ��������� �������� ID ���������� � ���������� deviceId
            if (_wcsicmp(currentDeviceId, deviceId) == 0) {
                // ���� ID ���������, ������������ ��������
                if (SetupDiBuildDriverInfoList(deviceInfoSet, &deviceInfoData, SPDIT_COMPATDRIVER)) {
                    int driverIndex = 0;
                    driverInfoData.cbSize = sizeof(SP_DRVINFO_DATA);

                    // ������� ����������� ���������
                    while (SetupDiEnumDriverInfo(deviceInfoSet, &deviceInfoData, SPDIT_COMPATDRIVER, driverIndex, &driverInfoData)) {
                        driverIndex++;

                        // ��������� ��������� ���������� � ��������
                        DWORD requiredSize = 0;
                        SetupDiGetDriverInfoDetail(deviceInfoSet, &deviceInfoData, &driverInfoData, NULL, 0, &requiredSize);

                        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                            SP_DRVINFO_DETAIL_DATA* driverDetailData = (SP_DRVINFO_DETAIL_DATA*)malloc(requiredSize);
                            if (driverDetailData) {
                                driverDetailData->cbSize = sizeof(SP_DRVINFO_DETAIL_DATA);
                                if (SetupDiGetDriverInfoDetail(deviceInfoSet, &deviceInfoData, &driverInfoData, driverDetailData, requiredSize, NULL)) {
                                    // ���������� ����� ����� INF
                                    WCHAR* driverInfo = (WCHAR*)malloc((lstrlenW(driverDetailData->InfFileName) + 1) * sizeof(WCHAR));
                                    if (driverInfo) {
                                        wcscpy_s(driverInfo, lstrlenW(driverDetailData->InfFileName) + 1, driverDetailData->InfFileName);
                                        drivers = (WCHAR**)realloc(drivers, (driverCount + 1) * sizeof(WCHAR*));
                                        drivers[driverCount] = driverInfo;
                                        driverCount++;
                                    }
                                }
                                free(driverDetailData);
                            }
                        }
                    }
                }
            }
        }
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);

    // �������� ����������� ���������
    if (driverCount == 0) {
        MessageBox(hwnd, L"No available drivers for selection.", L"Information", MB_OK | MB_ICONINFORMATION);
        return NULL;
    }

    // ���������� ������������ NULL � ������
    drivers = (WCHAR**)realloc(drivers, (driverCount + 1) * sizeof(WCHAR*));
    drivers[driverCount] = NULL;

    // �������� ������� ������ ��������
    int selectedDriverIndex = DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DRIVER_SELECTION), hwnd, DriverSelectionProc, (LPARAM)drivers);

    LPWSTR selectedDriver = NULL;
    if (selectedDriverIndex == -2) {
        // ���� �� OpenFileDialog
        selectedDriver = (LPWSTR)malloc((lstrlenW(selectedFilePath) + 1) * sizeof(WCHAR));
        if (selectedDriver) {
            wcscpy_s(selectedDriver, lstrlenW(selectedFilePath) + 1, selectedFilePath);
        }
    }
    else if (selectedDriverIndex >= 0 && selectedDriverIndex < driverCount) {
        // ���� �� ������
        selectedDriver = (LPWSTR)malloc((lstrlenW(drivers[selectedDriverIndex]) + 1) * sizeof(WCHAR));
        if (selectedDriver) {
            wcscpy_s(selectedDriver, lstrlenW(drivers[selectedDriverIndex]) + 1, drivers[selectedDriverIndex]);
        }
    }

    // ������������ ������
    for (int i = 0; i < driverCount; i++) {
        free(drivers[i]);
    }
    free(drivers);

    return selectedDriver;
}
