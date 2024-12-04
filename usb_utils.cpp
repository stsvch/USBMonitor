#include "usb_utils.h"

BOOL ChangeUSBDeviceState(const WCHAR* deviceID, BOOL connected)
{
    // �������� ������ ���� ������������� ���������
    HDEVINFO hDevInfo = SetupDiGetClassDevs(NULL, NULL, NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (hDevInfo == INVALID_HANDLE_VALUE) return FALSE; // ���� �� ������� �������� ������ ���������, ���������� FALSE

    SP_DEVINFO_DATA devInfoData;
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA); // �������������� ��������� ������ ����������

    DWORD index = 0;
    // ���������� ��� ���������� � ������
    while (SetupDiEnumDeviceInfo(hDevInfo, index++, &devInfoData))
    {
        WCHAR currentDeviceID[MAX_DEVICE_ID_LEN];
        // �������� ������� DeviceID ����������
        if (SetupDiGetDeviceInstanceIdW(hDevInfo, &devInfoData, currentDeviceID, MAX_DEVICE_ID_LEN, NULL))
        {
            // ���������� ������� DeviceID � ��������� deviceID
            if (_wcsicmp(currentDeviceID, deviceID) == 0)
            {
                // ���������� �������, ������ ����� �������� ��� ���������
                SP_PROPCHANGE_PARAMS propChangeParams;
                propChangeParams.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER); // ������������� ���������
                propChangeParams.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE; // ���������, ��� �������� �������� ����������
                propChangeParams.StateChange = connected ? DICS_DISABLE : DICS_ENABLE; // �������� ��������: �������� ��� ��������� ����������
                propChangeParams.Scope = DICS_FLAG_GLOBAL; // ��������� ��������� �� ���� �������� ����������
                propChangeParams.HwProfile = 0; // �� ��������� ���������� ������� ����������

                // ������������� ��������� ��� ��������� ��������� ����������
                if (!SetupDiSetClassInstallParams(hDevInfo, &devInfoData,
                    (SP_CLASSINSTALL_HEADER*)&propChangeParams,
                    sizeof(SP_PROPCHANGE_PARAMS)))
                {
                    SetupDiDestroyDeviceInfoList(hDevInfo); // ����������� �������
                    return FALSE; // ������ ��� ��������� ����������
                }

                // ��������� ��������� ��������� ����������
                if (!SetupDiChangeState(hDevInfo, &devInfoData))
                {
                    SetupDiDestroyDeviceInfoList(hDevInfo); // ����������� �������
                    return FALSE; // ������ ��� ��������� ��������� ����������
                }

                SetupDiDestroyDeviceInfoList(hDevInfo); // ����������� �������
                return TRUE; // ���������� ������� �������� ���������
            }
        }
    }
    SetupDiDestroyDeviceInfoList(hDevInfo); // ����������� �������
    return FALSE; // ���������� � ����� ID �� �������
}

DeviceInfo* ListConnectedUSBDevices(int* deviceCount)
{
    *deviceCount = 0;  // �������������� ���������� ���������

    // �������� ������ ���� ���������, ������������ ����� USB
    HDEVINFO deviceInfoSet = SetupDiGetClassDevs(
        NULL,
        L"USB",  // ��������� ���������� �� ������ "USB"
        NULL,
        DIGCF_PRESENT | DIGCF_ALLCLASSES  // �������� ������ ����������, ������� ������������ � �������
    );

    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        return NULL;  // ���� �� ������� �������� ������ ���������, ���������� NULL
    }

    SP_DEVINFO_DATA deviceInfoData;
    deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);  // �������������� ��������� ������ ��� ����������

    int capacity = 10;  // ��������� ����������� ������� ���������
    DeviceInfo* devices = new DeviceInfo[capacity];  // ������� ������ ��� �������� ���������� � �����������

    // ���������� ��� ���������� � ������
    for (int i = 0; SetupDiEnumDeviceInfo(deviceInfoSet, i, &deviceInfoData); ++i)
    {
        // �������������� ������ ��� �������� ���������� � ������� ����������
        WCHAR deviceDesc[MAX_BUFFER_SIZE] = L"Unknown Device";
        WCHAR deviceClass[MAX_BUFFER_SIZE] = L"Unknown Class";
        WCHAR deviceDriver[MAX_BUFFER_SIZE] = L"Unknown Driver";
        WCHAR deviceClassGUID[MAX_BUFFER_SIZE] = L"Unknown GUID";

        // �������� �������� ����������
        if (SetupDiGetDeviceRegistryProperty(
            deviceInfoSet,
            &deviceInfoData,
            SPDRP_DEVICEDESC,  // ��������: �������� ����������
            NULL,
            (PBYTE)deviceDesc,
            sizeof(deviceDesc),
            NULL))
        {
        }

        // �������� ����� ���������� (��������, USB)
        if (SetupDiGetDeviceRegistryProperty(
            deviceInfoSet,
            &deviceInfoData,
            SPDRP_CLASS,  // ��������: ����� ����������
            NULL,
            (PBYTE)deviceClass,
            sizeof(deviceClass),
            NULL))
        {
        }

        // �������� GUID ������ ����������
        if (SetupDiGetDeviceRegistryProperty(
            deviceInfoSet,
            &deviceInfoData,
            SPDRP_CLASSGUID,  // ��������: GUID ������ ����������
            NULL,
            (PBYTE)deviceClassGUID,
            sizeof(deviceClassGUID),
            NULL))
        {
        }

        // ��������� ��������� ���������� (��������, ������������� � �����������)
        if (wcsstr(deviceDesc, L"Hub") != nullptr ||
            wcsstr(deviceDesc, L"Controller") != nullptr)
        {
            continue;  // ���������� ����������, ��������� � ������ � �������������
        }

        // ���������� ����������, ������� �� �������� USB
        if (wcsstr(deviceClass, L"USB") == nullptr)
        {
            continue;  // ���������� ����������, �� ����������� � ������ USB
        }

        // ��������� �������� ����������, ����� ����������, �������� �� ��� �������
        DWORD capabilities = 0;
        if (SetupDiGetDeviceRegistryProperty(
            deviceInfoSet,
            &deviceInfoData,
            SPDRP_CAPABILITIES,  // ��������: ����������� ����������
            NULL,
            (PBYTE)&capabilities,
            sizeof(capabilities),
            NULL))
        {
            // ���� ���������� �� �������� �������, ���������� ���
            if ((capabilities & CM_DEVCAP_REMOVABLE) == 0) {
                continue;  // ���������� ����������, ������� �� �������� ��������
            }
        }

        // �������� ���������� ������������� ���������� (Device ID)
        WCHAR deviceID[MAX_BUFFER_SIZE] = L"Unknown Device ID";
        if (SetupDiGetDeviceInstanceId(
            deviceInfoSet,
            &deviceInfoData,
            deviceID,
            sizeof(deviceID),
            NULL))
        {
        }

        // ���� ���������� ��������� ��������� ������� ����������� �������, ����������� ���
        if (*deviceCount >= capacity) {
            capacity *= 2;  // ��������� ����������� �������
            DeviceInfo* newDevices = new DeviceInfo[capacity];  // ������� ����� ������ � ������� ������������
            memcpy(newDevices, devices, sizeof(DeviceInfo) * (*deviceCount));  // �������� ������ ������ � ����� ������
            delete[] devices;  // ����������� ������ ������
            devices = newDevices;  // �������������� ��������� �� ����� ������
        }

        // ��������� ���������� � ������� ���������� � ������
        DeviceInfo& device = devices[*deviceCount];
        wcscpy_s(device.Caption, deviceDesc);  // �������� �������� ����������
        wcscpy_s(device.DeviceID, deviceID);  // �������� ID ����������

        ++(*deviceCount);  // ����������� ������� ���������� ���������
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);  // ����������� �������, ��������� � ������� ���������
    return devices;  // ���������� ������ � ����������� � ������������ �����������
}