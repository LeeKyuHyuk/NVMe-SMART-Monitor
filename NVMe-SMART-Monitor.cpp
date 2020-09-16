#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <Windows.h>
#include <nvme.h>

#define MAX_DEVICE 32
#define IDENTIFY_PAYLOAD (sizeof(NVME_IDENTIFY_CONTROLLER_DATA)) // 4KB Payload
#define SMART_PAYLOAD (sizeof(NVME_HEALTH_INFO_LOG)) // 512B Payload

using namespace std;

struct NvmeDevice {
	TCHAR drive[64];
	UCHAR model[41];
};

class NVMe
{
private:
	HANDLE nvmeDevice;
	vector<NvmeDevice> nvmeDeviceVector;
public:
	NVMe();
	void findNvmeDevice();
	UINT printNvmeDevice();
	BOOL printIdentify(UINT index);
	BOOL printSmart(UINT index);
	void closeHandle();
};

NVMe::NVMe()
{
	nvmeDevice = NULL;
	cout << "=======================================================" << endl;
	cout << "      /$$   /$$ /$$    /$$ /$$      /$$" << endl;
	cout << "     | $$$ | $$| $$   | $$| $$$    /$$$" << endl;
	cout << "     | $$$$| $$| $$   | $$| $$$$  /$$$$  /$$$$$$" << endl;
	cout << "     | $$ $$ $$|  $$ / $$/| $$ $$/$$ $$ /$$__  $$" << endl;
	cout << "     | $$  $$$$ \\  $$ $$/ | $$  $$$| $$| $$$$$$$$" << endl;
	cout << "     | $$\\  $$$  \\  $$$/  | $$\\  $ | $$| $$_____/" << endl;
	cout << "     | $$ \\  $$   \\  $/   | $$ \\/  | $$|  $$$$$$$" << endl;
	cout << "     |__/  \\__/    \\_/    |__/     |__/ \\_______/" << endl << endl;
	cout << "                                 NVMe S.M.A.R.T Monitor" << endl;
	cout << "                                   " << __DATE__ << " " << __TIME__ << endl;
	cout << "                                           @KyuHyuk Lee" << endl;
	cout << "=======================================================" << endl << endl;
	cout << ">> Select NVMe Device, you can see the S.M.A.R.T value." << endl;
	cout << ">> Enter 'q' to exit." << endl << endl;
}

void NVMe::findNvmeDevice()
{
	UINT index = 0;
	TCHAR drive[64];
	LPCTSTR path = TEXT("\\\\.\\PhysicalDrive");
	LPCTSTR format = TEXT("%s%d");

	for (int count = 0; count < MAX_DEVICE; count++)
	{
		wsprintf(drive, format, path, count);
		nvmeDevice = CreateFile(drive, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (nvmeDevice == INVALID_HANDLE_VALUE)
		{
			if (GetLastError() == ERROR_ACCESS_DENIED)
			{
				cout << "Access denied. Please run in administrator mode." << endl;
			}
#if defined DEBUG_PRINT
			else
			{
				cout << "Failed to get handle! Port: " << count << ", Error: " << GetLastError() << endl;
			}
#endif
		}
		else
		{
			ULONG bufferLength = FIELD_OFFSET(STORAGE_PROPERTY_QUERY, AdditionalParameters) + sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA) + IDENTIFY_PAYLOAD;
			PVOID buffer = malloc(bufferLength);

			if (buffer == NULL)
			{
				cout << "DeviceNVMeQueryProtocolDataTest: allocate buffer failed, exit.\n" << endl;
				closeHandle();
				continue;
			}

			/* Initialize query data structure to get Identify Controller Data. */
			ZeroMemory(buffer, bufferLength);
			PSTORAGE_PROPERTY_QUERY query = (PSTORAGE_PROPERTY_QUERY)buffer;
			PSTORAGE_PROTOCOL_DATA_DESCRIPTOR protocolDataDescr = (PSTORAGE_PROTOCOL_DATA_DESCRIPTOR)buffer;
			PSTORAGE_PROTOCOL_SPECIFIC_DATA protocolData = (PSTORAGE_PROTOCOL_SPECIFIC_DATA)query->AdditionalParameters;

			query->PropertyId = StorageAdapterProtocolSpecificProperty;
			query->QueryType = PropertyStandardQuery;

			protocolData->ProtocolType = ProtocolTypeNvme;
			protocolData->DataType = NVMeDataTypeIdentify;
			protocolData->ProtocolDataRequestValue = NVME_IDENTIFY_CNS_CONTROLLER;
			protocolData->ProtocolDataRequestSubValue = 0;
			protocolData->ProtocolDataOffset = sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA);
			protocolData->ProtocolDataLength = IDENTIFY_PAYLOAD;

			/* Send request down. */
			ULONG returnedLength;
			BOOL result = DeviceIoControl(nvmeDevice, IOCTL_STORAGE_QUERY_PROPERTY, buffer, bufferLength, buffer, bufferLength, &returnedLength, NULL);

			/* Validate the returned data. */
			/* https://docs.microsoft.com/en-us/windows/win32/fileio/working-with-nvme-devices */
			if ((protocolDataDescr->Version != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR)) || (protocolDataDescr->Size != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR)))
			{
#if defined DEBUG_PRINT
				cout << "DeviceNVMeQueryProtocolDataTest: Get Identify Controller Data - data descriptor header not valid.\n" << endl;
#endif
				closeHandle();
				continue;
			}

			protocolData = &protocolDataDescr->ProtocolSpecificData;

			if ((protocolData->ProtocolDataOffset < sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA)) || (protocolData->ProtocolDataLength < IDENTIFY_PAYLOAD))
			{
#if defined DEBUG_PRINT
				cout << "DeviceNVMeQueryProtocolDataTest: Get Identify Controller Data - ProtocolData Offset/Length not valid.\n" << endl;
#endif
				closeHandle();
				continue;
			}

			/* Identify Controller Data Parsing */
			PNVME_IDENTIFY_CONTROLLER_DATA identifyControllerData = (PNVME_IDENTIFY_CONTROLLER_DATA)((PCHAR)protocolData + protocolData->ProtocolDataOffset);

			if ((identifyControllerData->VID == 0) || (identifyControllerData->NN == 0))
			{
#if defined DEBUG_PRINT
				cout << "DeviceNVMeQueryProtocolDataTest: Identify Controller Data not valid.\n" << endl;
#endif
				closeHandle();
				continue;
			}
			else
			{
				UCHAR model[sizeof(identifyControllerData->MN) + 1];
				model[sizeof(identifyControllerData->MN)] = '\0';
				NvmeDevice nvmeDevice;
				ZeroMemory(nvmeDevice.drive, sizeof(nvmeDevice.drive));
				memcpy(nvmeDevice.drive, drive, sizeof(drive));
				ZeroMemory(nvmeDevice.model, sizeof(nvmeDevice.model));
				memcpy(nvmeDevice.model, &(identifyControllerData->MN), sizeof(identifyControllerData->MN));
				nvmeDeviceVector.push_back(nvmeDevice);
				index++;
			}
			closeHandle();
		}
	}
}

UINT NVMe::printNvmeDevice()
{
	for (UINT index = 0; index < nvmeDeviceVector.size(); index++)
	{
		cout << "  - [" << index << "] " << nvmeDeviceVector[index].model << endl;
	}
	return nvmeDeviceVector.size();
}

BOOL NVMe::printIdentify(UINT index)
{
	nvmeDevice = CreateFile(nvmeDeviceVector[index].drive, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	ULONG bufferLength = FIELD_OFFSET(STORAGE_PROPERTY_QUERY, AdditionalParameters) + sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA) + IDENTIFY_PAYLOAD;
	PVOID buffer = malloc(bufferLength);

	if (buffer == NULL)
	{
#if defined DEBUG_PRINT
		cout << "DeviceNVMeQueryProtocolDataTest: allocate buffer failed, exit.\n" << endl;
#endif
		closeHandle();
		return FALSE;
	}

	/* Initialize query data structure to get Identify Controller Data. */
	ZeroMemory(buffer, bufferLength);
	PSTORAGE_PROPERTY_QUERY query = (PSTORAGE_PROPERTY_QUERY)buffer;
	PSTORAGE_PROTOCOL_DATA_DESCRIPTOR protocolDataDescr = (PSTORAGE_PROTOCOL_DATA_DESCRIPTOR)buffer;
	PSTORAGE_PROTOCOL_SPECIFIC_DATA protocolData = (PSTORAGE_PROTOCOL_SPECIFIC_DATA)query->AdditionalParameters;

	query->PropertyId = StorageAdapterProtocolSpecificProperty;
	query->QueryType = PropertyStandardQuery;

	protocolData->ProtocolType = ProtocolTypeNvme;
	protocolData->DataType = NVMeDataTypeIdentify;
	protocolData->ProtocolDataRequestValue = NVME_IDENTIFY_CNS_CONTROLLER;
	protocolData->ProtocolDataRequestSubValue = 0;
	protocolData->ProtocolDataOffset = sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA);
	protocolData->ProtocolDataLength = IDENTIFY_PAYLOAD;

	/* Send request down. */
	ULONG returnedLength;
	BOOL result = DeviceIoControl(nvmeDevice, IOCTL_STORAGE_QUERY_PROPERTY, buffer, bufferLength, buffer, bufferLength, &returnedLength, NULL);

	/* Validate the returned data. */
	/* https://docs.microsoft.com/en-us/windows/win32/fileio/working-with-nvme-devices */
	if ((protocolDataDescr->Version != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR)) || (protocolDataDescr->Size != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR)))
	{
#if defined DEBUG_PRINT
		cout << "DeviceNVMeQueryProtocolDataTest: Get Identify Controller Data - data descriptor header not valid.\n" << endl;
#endif
		closeHandle();
		return FALSE;
	}

	protocolData = &protocolDataDescr->ProtocolSpecificData;

	if ((protocolData->ProtocolDataOffset < sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA)) || (protocolData->ProtocolDataLength < IDENTIFY_PAYLOAD))
	{
#if defined DEBUG_PRINT
		cout << "DeviceNVMeQueryProtocolDataTest: Get Identify Controller Data - ProtocolData Offset/Length not valid.\n" << endl;
#endif
		closeHandle();
		return FALSE;
	}

	/* Identify Controller Data Parsing */
	PNVME_IDENTIFY_CONTROLLER_DATA identifyControllerData = (PNVME_IDENTIFY_CONTROLLER_DATA)((PCHAR)protocolData + protocolData->ProtocolDataOffset);

	if ((identifyControllerData->VID == 0) || (identifyControllerData->NN == 0))
	{
#if defined DEBUG_PRINT
		cout << "DeviceNVMeQueryProtocolDataTest: Identify Controller Data not valid.\n" << endl;
#endif
		closeHandle();
		return FALSE;
	}
	else
	{
		UCHAR SN[sizeof(identifyControllerData->SN) + 1];
		SN[sizeof(identifyControllerData->SN)] = '\0';
		UCHAR MN[sizeof(identifyControllerData->MN) + 1];
		MN[sizeof(identifyControllerData->MN)] = '\0';
		UCHAR FW[sizeof(identifyControllerData->FR) + 1];
		FW[sizeof(identifyControllerData->FR)] = '\0';

		memcpy(&SN, &(identifyControllerData->SN), sizeof(identifyControllerData->SN));
		memcpy(&MN, &(identifyControllerData->MN), sizeof(identifyControllerData->MN));
		memcpy(&FW, &(identifyControllerData->FR), sizeof(identifyControllerData->FR));

		cout << "Model Number : " << MN << endl;
		cout << "Serial Number : " << SN << endl;
		cout << "FW Revision : " << FW << endl;
	}
	closeHandle();
	return TRUE;
}

BOOL NVMe::printSmart(UINT index)
{
	nvmeDevice = CreateFile(nvmeDeviceVector[index].drive, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	ULONG bufferLength = FIELD_OFFSET(STORAGE_PROPERTY_QUERY, AdditionalParameters) + sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA) + SMART_PAYLOAD;
	PVOID buffer = malloc(bufferLength);

	if (buffer == NULL)
	{
#if defined DEBUG_PRINT
		cout << "DeviceNVMeQueryProtocolDataTest: allocate buffer failed, exit.\n" << endl;
#endif
		closeHandle();
		return FALSE;
	}

	/* Initialize query data structure to get S.M.A.R.T Data. */
	ZeroMemory(buffer, bufferLength);
	PSTORAGE_PROPERTY_QUERY query = (PSTORAGE_PROPERTY_QUERY)buffer;
	PSTORAGE_PROTOCOL_DATA_DESCRIPTOR protocolDataDescr = (PSTORAGE_PROTOCOL_DATA_DESCRIPTOR)buffer;
	PSTORAGE_PROTOCOL_SPECIFIC_DATA protocolData = (PSTORAGE_PROTOCOL_SPECIFIC_DATA)query->AdditionalParameters;

	query->PropertyId = StorageDeviceProtocolSpecificProperty;
	query->QueryType = PropertyStandardQuery;

	protocolData->ProtocolType = ProtocolTypeNvme;
	protocolData->DataType = NVMeDataTypeLogPage;
	protocolData->ProtocolDataRequestValue = NVME_LOG_PAGE_HEALTH_INFO;
	protocolData->ProtocolDataRequestSubValue = 0;
	protocolData->ProtocolDataOffset = sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA);
	protocolData->ProtocolDataLength = sizeof(NVME_HEALTH_INFO_LOG);

	/* Send request down. */
	ULONG returnedLength;
	BOOL result = DeviceIoControl(nvmeDevice, IOCTL_STORAGE_QUERY_PROPERTY, buffer, bufferLength, buffer, bufferLength, &returnedLength, NULL);

	/* Validate the returned data. */
	/* https://docs.microsoft.com/en-us/windows/win32/fileio/working-with-nvme-devices */
	if ((protocolDataDescr->Version != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR)) || (protocolDataDescr->Size != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR)))
	{
#if defined DEBUG_PRINT
		cout << "DeviceNVMeQueryProtocolDataTest: Get SMART Data - data descriptor header not valid.\n" << endl;
#endif
		closeHandle();
		return FALSE;
	}

	protocolData = &protocolDataDescr->ProtocolSpecificData;

	if ((protocolData->ProtocolDataOffset < sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA)) || (protocolData->ProtocolDataLength < SMART_PAYLOAD))
	{
#if defined DEBUG_PRINT
		cout << "DeviceNVMeQueryProtocolDataTest: Get SMART Data - ProtocolData Offset/Length not valid.\n" << endl;
#endif
		closeHandle();
		return FALSE;
	}

	/* SMART Health Data Parsing */
	PNVME_HEALTH_INFO_LOG SMARTHealthData = (PNVME_HEALTH_INFO_LOG)((PCHAR)protocolData + protocolData->ProtocolDataOffset);

	uint16_t temperature = 0;
	memcpy_s((void*)(&temperature), sizeof(uint16_t), (const void*)(SMARTHealthData->Temperature), sizeof(uint16_t));
	cout << "Temperature : " << temperature - 273 << "กษ" << endl;

	uint8_t availableSpare = SMARTHealthData->AvailableSpare;
	cout << "Available Spare : " << (int)availableSpare << "%" << endl;

	uint8_t availableSpareThreshold = SMARTHealthData->AvailableSpareThreshold;
	cout << "Available Spare Threshold : " << (int)availableSpareThreshold << "%" << endl;

	uint8_t percentageUsed = SMARTHealthData->PercentageUsed;
	cout << "Percentage Used : " << (int)percentageUsed << "%" << endl;


	uint64_t dataUnitReadLow = 0;
	uint64_t dataUnitReadHigh = 0;
	memcpy_s((void*)(&dataUnitReadLow), sizeof(uint64_t), (const void*)(SMARTHealthData->DataUnitRead), sizeof(uint64_t));
	memcpy_s((void*)(&dataUnitReadHigh), sizeof(uint64_t), (const void*)(&(SMARTHealthData->DataUnitRead[8])), sizeof(uint64_t));
	cout << "Data Units Read : " << (long long)((dataUnitReadHigh << 64) + dataUnitReadLow) <<
		" (" << ((long long)((dataUnitReadHigh << 64) + dataUnitReadLow)) * 1000 * 512 / (1024 * 1024 * 1024) << "GB)" << endl;

	uint64_t dataUnitWrittenLow = 0;
	uint64_t dataUnitWrittenHigh = 0;
	memcpy_s((void*)(&dataUnitWrittenLow), sizeof(uint64_t), (const void*)(SMARTHealthData->DataUnitWritten), sizeof(uint64_t));
	memcpy_s((void*)(&dataUnitWrittenHigh), sizeof(uint64_t), (const void*)(&(SMARTHealthData->DataUnitWritten[8])), sizeof(uint64_t));
	cout << "Data Units Written : " << (long long)((dataUnitWrittenHigh << 64) + dataUnitWrittenLow) <<
		" (" << ((long long)((dataUnitWrittenHigh << 64) + dataUnitWrittenLow)) * 1000 * 512 / (1024 * 1024 * 1024) << "GB)" << endl;

	uint64_t hostReadCommandsLow = 0;
	uint64_t hostReadCommandsHigh = 0;
	memcpy_s((void*)(&hostReadCommandsLow), sizeof(uint64_t), (const void*)(SMARTHealthData->HostReadCommands), sizeof(uint64_t));
	memcpy_s((void*)(&hostReadCommandsHigh), sizeof(uint64_t), (const void*)(&(SMARTHealthData->HostReadCommands[8])), sizeof(uint64_t));
	cout << "Host Read Commands : " << (long long)((hostReadCommandsHigh << 64) + hostReadCommandsLow) << endl;

	uint64_t hostWrittenCommandsLow = 0;
	uint64_t hostWrittenCommandsHigh = 0;
	memcpy_s((void*)(&hostWrittenCommandsLow), sizeof(uint64_t), (const void*)(SMARTHealthData->HostWrittenCommands), sizeof(uint64_t));
	memcpy_s((void*)(&hostWrittenCommandsHigh), sizeof(uint64_t), (const void*)(&(SMARTHealthData->HostWrittenCommands[8])), sizeof(uint64_t));
	cout << "Host Write Commands : " << (long long)((hostWrittenCommandsHigh << 64) + hostWrittenCommandsLow) << endl;

	uint64_t controllerBusyTimeLow = 0;
	uint64_t controllerBusyTimeHigh = 0;
	memcpy_s((void*)(&controllerBusyTimeLow), sizeof(uint64_t), (const void*)(SMARTHealthData->ControllerBusyTime), sizeof(uint64_t));
	memcpy_s((void*)(&controllerBusyTimeHigh), sizeof(uint64_t), (const void*)(&(SMARTHealthData->ControllerBusyTime[8])), sizeof(uint64_t));
	cout << "Controller Busy Time : " << (long long)((controllerBusyTimeHigh << 64) + controllerBusyTimeLow) << endl;

	uint64_t powerCycleLow = 0;
	uint64_t powerCycleHigh = 0;
	memcpy_s((void*)(&powerCycleLow), sizeof(uint64_t), (const void*)(SMARTHealthData->PowerCycle), sizeof(uint64_t));
	memcpy_s((void*)(&powerCycleHigh), sizeof(uint64_t), (const void*)(&(SMARTHealthData->PowerCycle[8])), sizeof(uint64_t));
	cout << "Power Cycles : " << (long long)((powerCycleHigh << 64) + powerCycleLow) << endl;

	uint64_t powerOnHoursLow = 0;
	uint64_t powerOnHoursHigh = 0;
	memcpy_s((void*)(&powerOnHoursLow), sizeof(uint64_t), (const void*)(SMARTHealthData->PowerOnHours), sizeof(uint64_t));
	memcpy_s((void*)(&powerOnHoursHigh), sizeof(uint64_t), (const void*)(&(SMARTHealthData->PowerOnHours[8])), sizeof(uint64_t));
	cout << "Power Cycles : " << (long long)((powerOnHoursHigh << 64) + powerOnHoursLow) << endl;

	uint64_t unsafeShutdownsLow = 0;
	uint64_t unsafeShutdownsHigh = 0;
	memcpy_s((void*)(&unsafeShutdownsLow), sizeof(uint64_t), (const void*)(SMARTHealthData->UnsafeShutdowns), sizeof(uint64_t));
	memcpy_s((void*)(&unsafeShutdownsHigh), sizeof(uint64_t), (const void*)(&(SMARTHealthData->UnsafeShutdowns[8])), sizeof(uint64_t));
	cout << "Unsafe Shutdowns : " << (long long)((unsafeShutdownsHigh << 64) + unsafeShutdownsLow) << endl;

	uint64_t mediaErrorsLow = 0;
	uint64_t mediaErrorsHigh = 0;
	memcpy_s((void*)(&mediaErrorsLow), sizeof(uint64_t), (const void*)(SMARTHealthData->MediaErrors), sizeof(uint64_t));
	memcpy_s((void*)(&mediaErrorsHigh), sizeof(uint64_t), (const void*)(&(SMARTHealthData->MediaErrors[8])), sizeof(uint64_t));
	cout << "Media and Data Integrity Errors : " << (long long)((mediaErrorsHigh << 64) + mediaErrorsLow) << endl;

	uint64_t errorInfoLogEntryCountLow = 0;
	uint64_t errorInfoLogEntryCountHigh = 0;
	memcpy_s((void*)(&errorInfoLogEntryCountLow), sizeof(uint64_t), (const void*)(SMARTHealthData->ErrorInfoLogEntryCount), sizeof(uint64_t));
	memcpy_s((void*)(&errorInfoLogEntryCountHigh), sizeof(uint64_t), (const void*)(&(SMARTHealthData->ErrorInfoLogEntryCount[8])), sizeof(uint64_t));
	cout << "Number of Error Information Log Entries : " << (long long)((errorInfoLogEntryCountHigh << 64) + errorInfoLogEntryCountLow) << endl;

	closeHandle();
	return TRUE;
}

void NVMe::closeHandle()
{
	assert(nvmeDevice != NULL);
	BOOL status = CloseHandle(nvmeDevice);
	if (!status)
	{
		cout << "There was a problem with Handle Closing." << endl;
	}
}

void error(HANDLE console, string message)
{
	SetConsoleTextAttribute(console, 12);
	cout << message << endl << endl;
	SetConsoleTextAttribute(console, 15);
}

int main()
{
	NVMe nvme = NVMe();
	nvme.findNvmeDevice();
	string command = "";
	HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
	do
	{
		SetConsoleTextAttribute(console, 10);
		cout << "[+] NVMe Devices" << endl;
		SetConsoleTextAttribute(console, 15);
		UINT totalDevices = nvme.printNvmeDevice();
		cout << endl;
		cout << "[NVMe SMART Monitor] >> ";
		cin >> command;
		try
		{
			if (strcmp(command.c_str(), "q") == 0 || strcmp(command.c_str(), "Q") == 0)
				break;
			UINT index = stoul(command, nullptr, 0);
			if (index < totalDevices)
			{
				cout << "=======================================================" << endl;
				nvme.printIdentify(index);
				cout << endl;
				nvme.printSmart(index);
				cout << "=======================================================" << endl;
			}
			else
				error(console, "NVMe Device that does not exist.");
		}
		catch (...)
		{
			error(console, "Invalid input.");
		}
	} while (TRUE);
	return EXIT_SUCCESS;
}
