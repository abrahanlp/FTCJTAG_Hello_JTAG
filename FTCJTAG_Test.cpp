#include <Windows.h>
#include "FTCJTAG.h"
#include <stdio.h>

//Function Prototypes
FTC_HANDLE FTCJTAG_initialize(DWORD devIndex);
FTC_STATUS FTCJTAG_setGPIO(FTC_HANDLE handle);
FTC_STATUS FTCJTAG_printDLLver(void);
void FTCJTAG_error(FTC_STATUS status, FTC_HANDLE handle);
int count_JTAG_Devices(PReadDataByteBuffer dataBuffer, DWORD byteBufferSize);

// Variables
WriteDataByteBuffer writeBuff = {0xFF};
ReadDataByteBuffer readBuff = {0};
DWORD bytesReceived = 0;

int chain_devices = 0;

int main()
{
    FTC_STATUS ftStatus = FTC_SUCCESS;
    FTC_HANDLE ftHandle = NULL;

    printf("FTDI JTAG C/C++ Hello World\r\n");
    
    ftStatus = FTCJTAG_printDLLver();

    ftHandle = FTCJTAG_initialize(0);
    if (ftHandle == NULL) return -1;

    // Set IR register with all '1' for BYPASS INSTRUCTION
    ftStatus = JTAG_Write(ftHandle, TRUE, 256, &writeBuff, 65535, RUN_TEST_IDLE_STATE);
    if (ftStatus != FTC_SUCCESS) {
        FTCJTAG_error(ftStatus, ftHandle);
    }

    // Fill DR register with 0s
    memset(writeBuff, 0x00, sizeof(writeBuff));
    ftStatus = JTAG_Write(ftHandle, FALSE, 256, &writeBuff, 65535, RUN_TEST_IDLE_STATE);
    if (ftStatus != FTC_SUCCESS) {
        FTCJTAG_error(ftStatus, ftHandle);
    }

    // Fill DR register with 1s
    memset(writeBuff, 0xFF, sizeof(writeBuff));
    ftStatus = JTAG_WriteRead(ftHandle, FALSE, 256, &writeBuff, 65535, &readBuff, &bytesReceived, TEST_LOGIC_STATE);
    if (ftStatus != FTC_SUCCESS) {
        FTCJTAG_error(ftStatus, ftHandle);
    }
    
    // Read how many 0s were shifted
    chain_devices = count_JTAG_Devices(&readBuff, bytesReceived);

    if (chain_devices == 0) {
        printf("No devices detected\r\n");
        return -1;
    }
    else {
        printf("Detected %d devices\r\n", chain_devices);
    }

    // Print devices IDCODE
    for (int i = 0; i < chain_devices; i++) {
        ftStatus = JTAG_WriteRead(ftHandle, FALSE, 32, &writeBuff, 4, &readBuff, &bytesReceived, RUN_TEST_IDLE_STATE);
        if (ftStatus != FTC_SUCCESS) {
            FTCJTAG_error(ftStatus, ftHandle);
        }

        printf("IDCODE %d: 0x", i);
        for (DWORD i = 0; i < bytesReceived; i++) {
            printf("%02X", readBuff[bytesReceived - i - 1]);
        }
        printf("\r\n");
    }
        
    JTAG_Close(ftHandle);

    return 0;
}

int count_JTAG_Devices(PReadDataByteBuffer dataBuffer, DWORD byteBufferSize) {
    int numDevices = 0;

    for (DWORD i = 0; i < byteBufferSize; i++) {
        if ((*dataBuffer)[i] == 0xFF) {
            break;
        }
        else {
            char tmp_data = (*dataBuffer)[i];
            for (int j = 0; j < 8; j++) {
                if ((tmp_data & 1) == 0) {
                    numDevices++;
                    tmp_data >>= 1;
                }
                else {
                    break;
                }
            }
        }
    }

    return numDevices;
}

FTC_HANDLE FTCJTAG_initialize(DWORD devIndex) {
    FTC_HANDLE handle = NULL;
    FTC_STATUS status = FTC_SUCCESS;
    DWORD hiSpeedDevices;
    char devName[100];
    DWORD devLocationID;
    char devChannel[5];
    DWORD devType;

    status = JTAG_GetNumHiSpeedDevices(&hiSpeedDevices);
    printf("Hi speed devices: %d\r\n", hiSpeedDevices);
    if ((status != FTC_SUCCESS) || (hiSpeedDevices == 0)) {
        FTCJTAG_error(status, handle);
    }

    status = JTAG_GetHiSpeedDeviceNameLocIDChannel(devIndex, devName, sizeof(devName), &devLocationID, devChannel, sizeof(devChannel), &devType);
    if (status == FTC_SUCCESS) {
        printf("Device: %d\r\n", devIndex);
        printf("\tName: %s\r\n", devName);
        printf("\tLocation: %d\r\n", devLocationID);
        printf("\tChannel: %s\r\n", devChannel);
        printf("\tType: %d\r\n", devType);
    }
    else {
        FTCJTAG_error(status, handle);
    }

    status = JTAG_OpenHiSpeedDevice(devName, devLocationID, devChannel, &handle);
    if (status != FTC_SUCCESS) {
        FTCJTAG_error(status, handle);
    }

    status = JTAG_InitDevice(handle, 0); //65535
    if (status != FTC_SUCCESS) {
        FTCJTAG_error(status, handle);
    }

    status = FTCJTAG_setGPIO(handle);
    if (status != FTC_SUCCESS) {
        FTCJTAG_error(status, handle);
    }

    return handle;
}

FTC_STATUS FTCJTAG_setGPIO(FTC_HANDLE handle) {
    FTC_INPUT_OUTPUT_PINS LowInputOutputPinsData = {0};
    FTH_INPUT_OUTPUT_PINS HighInputOutputPinsData = {0};

    //Basys 3 configuration
    LowInputOutputPinsData.bPin4InputOutputState = TRUE;
    LowInputOutputPinsData.bPin4LowHighState = TRUE;

    return JTAG_SetHiSpeedDeviceGPIOs(handle, TRUE, &LowInputOutputPinsData, 
                                              TRUE, &HighInputOutputPinsData);
}

FTC_STATUS FTCJTAG_printDLLver(void) {
    char verStr[10];
    FTC_STATUS status = JTAG_GetDllVersion(verStr, sizeof(verStr));
    printf("Dll version: %s\r\n", verStr);

    return status;
}

void FTCJTAG_error(FTC_STATUS status, FTC_HANDLE handle) {
    char lang[] = "EN";
    char errStr[100];

    printf("FTC ERROR: %d ", status);

    JTAG_GetErrorCodeString(lang, status, errStr, sizeof(errStr));
    printf("%s", errStr);

    if (handle != NULL) {
        JTAG_Close(handle);
    }

    exit(-1);
}
