#include "driver_utils.h"

// ��������� �������� �� InfName
void InstallDriver(const WCHAR* infPath, const WCHAR* deviceID, const WCHAR* driverVersion)
{
    BOOL rebootRequired = FALSE;

    // ��������� ��������
    BOOL result = DiInstallDriverW(NULL, infPath, DIIRFLAG_FORCE_INF, &rebootRequired);
    if (result) {
        // �������� ���������
        MessageBoxW(NULL, L"Driver installed successfully.", L"Success", MB_OK | MB_ICONINFORMATION);

        // ���������� ������� ��������
        SaveDriverHistory(deviceID, infPath, driverVersion);

        if (rebootRequired) {
            MessageBoxW(NULL, L"Reboot is required to complete installation.", L"Info", MB_OK | MB_ICONINFORMATION);
        }
    }
    else {
        // ��������� ������
        WCHAR errorMsg[256];
        swprintf_s(errorMsg, sizeof(errorMsg) / sizeof(WCHAR), L"Failed to install driver. Error: %lu", GetLastError());
        MessageBoxW(NULL, errorMsg, L"Error", MB_OK | MB_ICONERROR);
    }
}

//������� �������� �������� � ����������, �� �� ������� ��� �������
// �� �������.
void RemoveDriver(const WCHAR* deviceIdentifier)
{
    HDEVINFO deviceInfoSet = SetupDiGetClassDevsW(NULL, NULL, NULL, DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        MessageBoxW(NULL, L"Failed to get device info set.", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    SP_DEVINFO_DATA deviceInfoData;
    deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    BOOL found = FALSE;

    for (DWORD i = 0; SetupDiEnumDeviceInfo(deviceInfoSet, i, &deviceInfoData); i++) {
        WCHAR deviceID[256];
        if (SetupDiGetDeviceInstanceIdW(deviceInfoSet, &deviceInfoData, deviceID, sizeof(deviceID) / sizeof(WCHAR), NULL)) {
            if (wcsstr(deviceID, deviceIdentifier) != NULL) {
                found = TRUE;

                // �������� �������� ��������
                BOOL removeResult = SetupDiCallClassInstaller(DIF_REMOVE, deviceInfoSet, &deviceInfoData);
                if (removeResult) {
                    MessageBoxW(NULL, L"Driver removed successfully.", L"Success", MB_OK | MB_ICONINFORMATION);
                }
                else {
                    WCHAR errorMsg[256];
                    swprintf_s(errorMsg, sizeof(errorMsg) / sizeof(WCHAR), L"Failed to remove driver. Error: %lu", GetLastError());
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

// �������, ���������� ��� ������� �� ������ ��� ������ ����� � ��������� ��������
void OnBrowseAndInstallDriver(HWND hwnd) 
{
    BROWSEINFO bi = { 0 };
    bi.lpszTitle = L"�������� ����� � ����������"; // ��������� ����������� ���� ������ �����
    LPITEMIDLIST pidl = SHBrowseForFolder(&bi); // ��������� ������ ������ �����

    if (pidl != 0) {
        // �������� ���� � ��������� �����
        WCHAR path[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, path)) {
            MessageBox(hwnd, path, L"��������� �����", MB_OK);

            // ����� ����������� ��������� ��������
            // �������� ���������� �������� �� ��������� �����
            BOOL result = SetupCopyOEMInfW(
                path,           // ���� � ����� � INF ������� ��������
                NULL,           // �������, ���� ���������� (NULL - ��������� �������, ������ C:\Windows\inf)
                SPOST_PATH,     // ��� ��������� (���� � ����� � �������)
                0,              // �������������� ����� (0 - ����������� �����������)
                NULL,           // ����� ��� ����� ����� (�� �����, ������� NULL)
                0,              // ������ ������ (0, ��� ��� ����� �� ������������)
                NULL,           // ����������� ������ ������ (NULL, ��� ��� �� ������������)
                NULL            // ���� � �������������� ����� (NULL, ���� �� ���������)
            );

            if (result) {
                // ��������� �� �������� ��������� ��������
                MessageBox(hwnd, L"������� ������� ����������!", L"�����", MB_OK | MB_ICONINFORMATION);
            }
            else {
                // ��������� ������ ��������� ��������
                DWORD error = GetLastError();
                WCHAR errorMsg[256];
                swprintf_s(errorMsg, L"������ ��������� ��������: %ld", error);
                MessageBox(hwnd, errorMsg, L"������", MB_OK | MB_ICONERROR);
            }
        }
    }
    else {
        // ��������� �� ������, ���� ����� �� ���� �������
        MessageBox(hwnd, L"����� �� ���� �������.", L"������", MB_OK | MB_ICONERROR);
    }
}

// ������� ��� ����������� ����������� ���� ������ �������� � �������� ���� � ���������� ��������
LPWSTR ShowDriverSelectionDialog(HWND hwnd)
{
    HDEVINFO deviceInfoSet;
    SP_DEVINFO_DATA deviceInfoData;
    SP_DRVINFO_DATA driverInfoData;
    WCHAR drivers[1024][MAX_PATH]; // ������ ��� �������� �������� ���������
    int driverCount = 0;

    // �������� ������ ���� ��������� � �������
    deviceInfoSet = SetupDiGetClassDevs(NULL, NULL, NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        MessageBox(hwnd, L"�� ������� �������� ������ ���������.", L"������", MB_OK | MB_ICONERROR);
        return NULL;
    }

    deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    int deviceIndex = 0;

    // ���������� ��� ����������
    while (SetupDiEnumDeviceInfo(deviceInfoSet, deviceIndex, &deviceInfoData)) {
        deviceIndex++;

        // ������� ������ ��������� ��������� ��� �������� ����������
        if (SetupDiBuildDriverInfoList(deviceInfoSet, &deviceInfoData, SPDIT_COMPATDRIVER)) {
            int driverIndex = 0;
            driverInfoData.cbSize = sizeof(SP_DRVINFO_DATA);

            // ���������� ��������� ��������
            while (SetupDiEnumDriverInfo(deviceInfoSet, &deviceInfoData, SPDIT_COMPATDRIVER, driverIndex, &driverInfoData)) {
                driverIndex++;

                // ��������� ���������� � �������� � ������
                if (driverCount < 1024) {
                    wcscpy_s(drivers[driverCount], driverInfoData.Description);
                    driverCount++;
                }
            }
        }
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);

    // ������� ���������� ���� � ���������� ����������
    if (driverCount == 0) {
        MessageBox(hwnd, L"��� ��������� ��������� ��� ������.", L"����������", MB_OK | MB_ICONINFORMATION);
        return NULL;
    }

    // ��������� ������ � ��������� ���� ��������� ���������
    WCHAR driverList[4096] = L"";
    for (int i = 0; i < driverCount; ++i) {
        WCHAR line[128];
        swprintf_s(line, L"%d: %s\n", i + 1, drivers[i]);
        wcscat_s(driverList, line);
    }

    // ���������� ������ ������������
    MessageBox(hwnd, driverList, L"��������� ��������", MB_OK | MB_ICONINFORMATION);

    // ����������� � ������������ ����� ��������
    WCHAR inputBuffer[10];
    int selectedDriverIndex = -1;
    while (selectedDriverIndex < 1 || selectedDriverIndex > driverCount) {
        // �������� ������������ ������ ����� ��������
        int response = MessageBox(hwnd, L"������� ����� ���������� �������� (��������, 1):", L"����� ��������", MB_OKCANCEL | MB_ICONQUESTION);
        if (response == IDCANCEL) {
            return NULL; // ���� ������������ ������� �����
        }

        // �������� ����� ��������
        if (GetWindowText(hwnd, inputBuffer, sizeof(inputBuffer) / sizeof(WCHAR)) != 0) {
            selectedDriverIndex = _wtoi(inputBuffer);
        }

        // ���������, ��� ��������� ������ ��������
        if (selectedDriverIndex < 1 || selectedDriverIndex > driverCount) {
            MessageBox(hwnd, L"������������ ����� ��������. ����������, ���������� �����.", L"������", MB_OK | MB_ICONERROR);
        }
    }

    // ���� ������������ ������ ���������� �������, ���������� ��� ��������
    if (selectedDriverIndex >= 1 && selectedDriverIndex <= driverCount) {
        LPWSTR selectedDriver = (LPWSTR)LocalAlloc(LPTR, (lstrlenW(drivers[selectedDriverIndex - 1]) + 1) * sizeof(WCHAR));
        if (selectedDriver) {
            wcscpy_s(selectedDriver, lstrlenW(drivers[selectedDriverIndex - 1]) + 1, drivers[selectedDriverIndex - 1]);
        }
        return selectedDriver;
    }
    return NULL;
}
