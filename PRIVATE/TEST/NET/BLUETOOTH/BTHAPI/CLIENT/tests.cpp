//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
      @doc Bluetooth BthXxx API Test Client

THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright (c) 1999-2000 Microsoft Corporation

  @test bthapisvr | Ji Li (jil) | Bluetooth BthXxx API Test Client |

	This program depends on L2CAPAPI

  @tcindex BTHAPITST

*/
/*

Revision History:
	V0.1   16-May-2003	Original version

-------------------------------------------------------------*/

#include <windows.h>
#include <winsock2.h>
#include <winbase.h>
#include <katoex.h>
#include <tux.h>
#include <tchar.h>
#include <cmdline.h>
#include <svsutil.hxx>
#include <bt_buffer.h>
#include <bt_api.h>
#include <bt_ddi.h>
#include <ws2bth.h>
#include <bthapi.h>
#include <bt_sdp.h>
#include "logwrap.h"
#include <auto_xxx.hxx>
#include <service.h>


/* NOTE the below valid value rule for sniff mode params when changing the constants below
   Sniff Mode interval must be between 0x0002 and 0xFFFE inclusive and must be an even number.
   if ((sniff_mode_min < 0x0002) || (sniff_mode_max < 0x0002) || (sniff_mode_min > 0xFFFE) || (sniff_mode_max > 0xFFFE) ||
       (sniff_mode_min > sniff_mode_max) || (sniff_mode_min % 2) || (sniff_mode_max % 2) || (sniff_attempt == 0)) {
        return ERROR_INVALID_PARAMETER;
    }
*/


#define TEST_SNIFF_MAX       0x640 // 1 sec max
#define TEST_SNIFF_MIN       0x4B0 // .75 sec min
#define TEST_SNIFF_ATTEMPT   0x20
#define TEST_SNIFF_TO        0x20

/* NOTE the below valid value rule for park mode params when changing the constants below
    // Beacon interval must be between 0x000E and 0xFFFE inclusive and must be an even number.
    if ((beacon_min < 0x000E) || (beacon_max < 0x000E) || (beacon_min > 0xFFFE) || (beacon_max > 0xFFFE) ||
        (beacon_min > beacon_max) || (beacon_min % 2) || (beacon_max % 2)) {
        return ERROR_INVALID_PARAMETER;
    }
*/

#define TEST_PARK_BEACON_MAX 0x3E80 // 10 sec max
#define TEST_PARK_BEACON_MIN 0x0640 // 1 sec min 

/* NOTE the below valid value rule for hold mode params when changing the constants below
    // Hold Mode interval must be between 0x0002 and 0xFFFE inclusive and must be an even number.  The max
    // value for the min hold interval is 0xFF00.
    if ((hold_mode_min < 0x0002) || (hold_mode_max < 0x0002) || (hold_mode_min > 0xFF00) || (hold_mode_max > 0xFFFE) ||
        (hold_mode_min > hold_mode_max) || (hold_mode_min % 2) || (hold_mode_max % 2)) {
        return ERROR_INVALID_PARAMETER;
    }
*/

#define TEST_HOLD_MAX 0x3E80 // 10 sec max
#define TEST_HOLD_MIN 0x0640 // 1 sec min 

#define BTHAPIBASE 1000

#define ARRAY_COUNT(x)	(sizeof(x)/sizeof(x[0]))

// Global
extern BT_ADDR g_ServerAddress;
extern BOOL g_fHasServer;

// TEST_ENTRY
#define TEST_ENTRY \
	if (uMsg == TPM_QUERY_THREAD_COUNT) \
	{ \
		((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = lpFTE->dwUserData; \
		return SPR_HANDLED; \
	} \
	else if (uMsg != TPM_EXECUTE) \
	{ \
		return TPR_NOT_HANDLED; \
	}


// this class is used to get BT card details
class BtVersionInfo
{
private:
	int valid;
	
public:
	unsigned char hci_version;
	unsigned short hci_revision;
	unsigned char lmp_version;
	unsigned short lmp_subversion;
	unsigned short manufacturer;
	unsigned char lmp_features[8];

	BtVersionInfo(): hci_version(0), hci_revision(0), lmp_version(0), lmp_subversion(0), manufacturer(0)
	{
		memset (lmp_features, 0, sizeof(lmp_features));
		valid = FALSE;
	};

	// check if this module is same or older than a specified version
	BOOL EqualOrOlderThan(
		unsigned short ucManufacturer, 
		unsigned char uclmp_version, 
		unsigned short uclmp_subversion)
	{
		ASSERT(valid);

		if (manufacturer != ucManufacturer)
			return FALSE;

		if (lmp_version < uclmp_version)
			return TRUE;
		else if (lmp_version == uclmp_version && lmp_subversion <= uclmp_subversion)
			return TRUE;

		return FALSE;
	}

	// Read the local version
	int ReadLocal()
	{
		int iResult = BthReadLocalVersion(
			&hci_version, 
			&hci_revision, 
			&lmp_version, 
			&lmp_subversion, 
			&manufacturer, 
			&lmp_features[0]);

		valid = (iResult == ERROR_SUCCESS) ? TRUE : FALSE;

		return iResult;
	}

	// Read the remote version
	int ReadRemote (BT_ADDR * pba)
	{
		hci_version = 0;
		hci_revision = 0;
		
		int iResult = BthReadRemoteVersion(
			pba, 
			&lmp_version, 
			&lmp_subversion, 
			&manufacturer, 
			&lmp_features[0]);

		valid = (iResult == ERROR_SUCCESS) ? TRUE : FALSE;

		return iResult;
	}

	BOOL HoldSupported()
	{
		ASSERT(valid);

		return ((lmp_features[0] & 0x40) ? TRUE : FALSE);		
	}

	BOOL SniffSupported()
	{
		ASSERT(valid);

		return ((lmp_features[0] & 0x80) ? TRUE : FALSE);		
	}

	BOOL ParkSupported()
	{
		ASSERT(valid);

		return ((lmp_features[1] & 0x01) ? TRUE : FALSE);		
	}

	BOOL RssiSupported()
	{
		ASSERT(valid);

		return ((lmp_features[1] & 0x02) ? TRUE : FALSE);		
	}

	BOOL ScoSupported()
	{
		ASSERT(valid);

		return ((lmp_features[1] & 0x08) ? TRUE : FALSE);		
	}

	// print the fields to the debug output
	void OutStr()
	{
		ASSERT(valid);
		
		QAMessage(TEXT("*** VERSION:"));
		QAMessage(TEXT("HCI Version/Revision: %d.%d"), hci_version, hci_revision);
		QAMessage(TEXT("LMP Version/Subversion: %d.%d"), lmp_version, lmp_subversion);
		QAMessage(TEXT("Manufacturer ID : %d"), manufacturer);

		switch (manufacturer) {
		case 0:
			QAMessage(TEXT("Manufacturer: Ericsson"));
			break;
		case 1:
			QAMessage(TEXT("Manufacturer: Nokia"));
			break;
		case 2:
			QAMessage(TEXT("Manufacturer: Intel"));
			break;
		case 3:
			QAMessage(TEXT("Manufacturer: IBM"));
			break;
		case 4:
			QAMessage(TEXT("Manufacturer: Toshiba"));
			break;
		case 5:
			QAMessage(TEXT("Manufacturer: 3COM"));
			break;
		case 6:
			QAMessage(TEXT("Manufacturer: Microsoft"));
			break;
		case 7:
			QAMessage(TEXT("Manufacturer: Lucent"));
			break;
		case 8:
			QAMessage(TEXT("Manufacturer: Motorola"));
			break;
		case 9:
			QAMessage(TEXT("Manufacturer: Infineon"));
			break;
		case 10:
			QAMessage(TEXT("Manufacturer: CSR"));
			break;
		case 11:
			QAMessage(TEXT("Manufacturer: Silicon Wave"));
			break;
		case 12:
			QAMessage(TEXT("Manufacturer: Digianswer"));
			break;
		default:
			QAMessage(TEXT("Manufacturer: UNKNOWN"));
			break;
		}
				
		DumpFeatures(lmp_features);

	}

	void DumpFeatures (unsigned char *pf) 
	{
		QAMessage (L"*** LMP FEATURES:");
		if ((*pf) & 0x01)
			QAMessage (L"3-slot packets");
		else
			QAMessage (L"NO: 3-slot packets");
			
		if ((*pf) & 0x02)
			QAMessage (L"5-slot packets");
		else
			QAMessage (L"NO: 5-slot packets");
			
		if ((*pf) & 0x04)
			QAMessage (L"encryption");
		else
			QAMessage (L"NO: encryption");
			
		if ((*pf) & 0x08)
			QAMessage (L"slot offset");
		else
			QAMessage (L"NO: slot offset");
			
		if ((*pf) & 0x10)
			QAMessage (L"timing accuracy");
		else
			QAMessage (L"NO: timing accuracy");
			
		if ((*pf) & 0x20)
			QAMessage (L"switch");
		else
			QAMessage (L"NO: switch");
			
		if ((*pf) & 0x40)
			QAMessage (L"hold");
		else
			QAMessage (L"NO: hold");
			
		if ((*pf) & 0x80)
			QAMessage (L"sniff");
		else
			QAMessage (L"NO: sniff");
			
		++pf;
		if ((*pf) & 0x01)
			QAMessage (L"park");
		else
			QAMessage (L"NO: park");
			
		if ((*pf) & 0x02)
			QAMessage (L"RSSI");
		else
			QAMessage (L"NO: RSSI");
			
		if ((*pf) & 0x04)
			QAMessage (L"channel-quality driven rate");
		else
			QAMessage (L"NO: channel-quality driven rate");
			
		if ((*pf) & 0x08)
			QAMessage (L"SCO");
		else
			QAMessage (L"NO: SCO");
			
		if ((*pf) & 0x10)
			QAMessage (L"HV2");
		else
			QAMessage (L"NO: HV2");
			
		if ((*pf) & 0x20)
			QAMessage (L"HV3");
		else
			QAMessage (L"NO: HV3");
			
		if ((*pf) & 0x40)
			QAMessage (L"u-law log");
		else
			QAMessage (L"NO: u-law log");
			
		if ((*pf) & 0x80)
			QAMessage (L"a-law log");
		else
			QAMessage (L"NO: a-law log");
			
		++pf;
		if ((*pf) & 0x01)
			QAMessage (L"CVSD");
		else
			QAMessage (L"NO: CVSD");
			
		if ((*pf) & 0x02)
			QAMessage (L"paging scheme");
		else
			QAMessage (L"NO: paging scheme");
			
		if ((*pf) & 0x04)
			QAMessage (L"power control");
		else
			QAMessage (L"NO: power control");
			
		if ((*pf) & 0x08)
			QAMessage (L"transparent SCO data");
		else
			QAMessage (L"NO: transparent SCO data");
			
		if ((*pf) & 0x10)
			QAMessage (L"flow lag bit 0");
		else
			QAMessage (L"NO: flow lag bit 0");
		
		if ((*pf) & 0x20)
			QAMessage (L"flow lag bit 0");
		else
			QAMessage (L"NO: flow lag bit 0");
			
		if ((*pf) & 0x40)
			QAMessage (L"flow lag bit 0");
		else
			QAMessage (L"NO: flow lag bit 0");
	}
};


int GetConnectionHandle(BT_ADDR * pbt, unsigned short * phandle, UINT fLinkType, BOOL * found)
{
	ASSERT(pbt);
	ASSERT(phandle);

	int iResult = 0;
	*phandle = 0;
	*found= FALSE;

	DWORD dwBufferSize = 16;
	int cConnectionReturned = 0;
	ce::auto_array_ptr<BASEBAND_CONNECTION> buffer= new BASEBAND_CONNECTION [dwBufferSize];

	do
	{
		iResult = BthGetBasebandConnections(dwBufferSize, buffer, &cConnectionReturned);

		if (iResult == ERROR_INSUFFICIENT_BUFFER)
		{
			buffer.close();
			buffer = new BASEBAND_CONNECTION [cConnectionReturned];
			dwBufferSize = cConnectionReturned;
		}
		
	} while (iResult == ERROR_INSUFFICIENT_BUFFER);
	
	if (iResult != ERROR_SUCCESS)
	{
		QAMessage(TEXT("BthGetBasebandConnections() returned %d"), iResult);
		return iResult;			
	}

	for (int i = 0; i < cConnectionReturned; i++)
	{
		if (buffer[i].baAddress == *pbt && buffer[i].fLinkType == fLinkType)
		{
			QAMessage(TEXT("Already connected to remote BT address"));
			*phandle = buffer[i].hConnection;
			*found = TRUE;
			return ERROR_SUCCESS;
		}
	}

	return ERROR_SUCCESS;
}


// retries until it connects
int BthCreateACLConnectionRetry (BT_ADDR * pbt, unsigned short * phandle, int retries)
{
	int iResult = 0;
	BOOL found = FALSE;

	ASSERT(retries > 0);

	iResult = GetConnectionHandle(pbt, phandle, BTH_LINK_TYPE_ACL, &found);

	if (iResult != ERROR_SUCCESS)
	{
		QAMessage(TEXT("IsConnected() returned %d"), iResult);
		return iResult;			
	}

	if (found)
		return ERROR_SUCCESS; // found

	// no existing ACL connection, create it

	do
	{
		iResult = BthCreateACLConnection (pbt, phandle);

		if (iResult == ERROR_TIMEOUT)
			QAMessage(TEXT("BthCreateACLConnection() timed out..."));
		else
			return iResult;
		
		retries--;
	} while (retries > 0);

	return iResult;
};


// retries the connection if it fails with ERROR_TIMEOUT
int BthCreateSCOConnectionRetry (BT_ADDR * pbt, unsigned short * phandle, int retries)
{
	int iResult = 0;

	ASSERT(retries > 0);

	do
	{
		iResult = BthCreateSCOConnection (pbt, phandle);

		if (iResult == ERROR_TIMEOUT)
			QAMessage(TEXT("BthCreateSCOConnection() timed out, trying again..."));
		else 
			return iResult;
		
		retries--;
	} while (retries > 0);

	return iResult;
};

//
//TestId        : 001
//Title         : Get Hardware Status Test
//Description   : BthGetHardwareStatus()
//
TESTPROCAPI t001(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	int iResult;
	int iStatus;

	iResult = BthGetHardwareStatus(&iStatus);
	if (iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("BthGetHardwareStatus failed, error = %d"), iResult);
	}
	else
	{
		switch (iStatus)
		{
		case HCI_HARDWARE_UNKNOWN:
			QAMessage(TEXT("Bluetooth hardware status is %s"), TEXT("HCI_HARDWARE_UNKNOWN"));
			break;
		case HCI_HARDWARE_NOT_PRESENT:
			QAMessage(TEXT("Bluetooth hardware status is %s"), TEXT("HCI_HARDWARE_NOT_PRESENT"));
			break;
		case HCI_HARDWARE_INITIALIZING:
			QAMessage(TEXT("Bluetooth hardware status is %s"), TEXT("HCI_HARDWARE_INITIALIZING"));
			break;
		case HCI_HARDWARE_RUNNING:
			QAMessage(TEXT("Bluetooth hardware status is %s"), TEXT("HCI_HARDWARE_RUNNING"));
			break;
		case HCI_HARDWARE_SHUTDOWN:
			QAMessage(TEXT("Bluetooth hardware status is %s"), TEXT("HCI_HARDWARE_SHUTDOWN"));
			break;
		case HCI_HARDWARE_ERROR:
			QAMessage(TEXT("Bluetooth hardware status is %s"), TEXT("HCI_HARDWARE_ERROR"));
			break;
		default:
			QAWarning(TEXT("Unknown hardware status %d"), iStatus);
			break;
		}

		return TPR_PASS;
	}

	return TPR_FAIL;
}

//
//TestId        : 002
//Title         : Read Local Bluetooth Address Test
//Description   : BthReadLocalAddr()
//
TESTPROCAPI t002(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	int iResult;
	BT_ADDR ba, zero;

	iResult = BthReadLocalAddr(&ba);
	if (iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("BthReadLocalAddr failed, error = %d"), iResult);
	}
	else
	{
		QAMessage(TEXT("Local Bluetooth address is 0x%04x%08x"), GET_NAP(ba), GET_SAP(ba));
		memset(&zero, 0, sizeof(zero));
		if (memcmp(&ba, &zero, sizeof(zero)) != 0)
		{
			return TPR_PASS;
		}
		else
		{
			QAError(TEXT("Failed to read local Bluetooth address, check if hardware is working"));
		}
	}

	return TPR_FAIL;
}

//
//TestId        : 003
//Title         : ScanEableMask Test
//Description   : BthReadScanEnableMask(); BthWriteScanEnableMask()
//
TESTPROCAPI t003(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	int iResult;
	unsigned char mask, curMask, oldMask;

	iResult = BthReadScanEnableMask(&oldMask);
	if (iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("BthReadScanEnableMask() failed, error = %d"), iResult);
	}
	else
	{
		QAMessage(TEXT("Current ScanMask is 0x%02x"), oldMask);
		QAMessage(TEXT("    0x00 - No scans are enabled"));
		QAMessage(TEXT("    0x01 - Inquiry scan is enabled and page scan is disabled"));
		QAMessage(TEXT("    0x02 - Inquiry scan disabled and page scan is enabled"));
		QAMessage(TEXT("    0x03 - Inquiry scan enabled and page scan is enabled"));
		for (mask = 0; mask < 4; mask ++)
		{
			QAMessage(TEXT("Write ScanMask 0x%02x"), mask);
			iResult = BthWriteScanEnableMask(mask);
			if (iResult != ERROR_SUCCESS)
			{
				QAError(TEXT("BthWriteScanEnableMask(0x%02x) failed, error = %d"), mask, iResult);
				break;
			}
			iResult = BthReadScanEnableMask(&curMask);
			if (iResult != ERROR_SUCCESS)
			{
				QAError(TEXT("BthReadScanEnableMask() failed, error = %d"), iResult);
				break;
			}
			QAMessage(TEXT("ScanMask is now 0x%02x"), curMask);
			if (curMask != mask)
			{
				QAError(TEXT("ScanMask did not change as expected"));
				break;
			}
		}

		iResult = BthWriteScanEnableMask(oldMask);
		if (iResult != ERROR_SUCCESS)
		{
			QAError(TEXT("BthWriteScanEnableMask(0x%02x) failed, error = %d"), oldMask, iResult);
		}
		else
		{
			QAMessage(TEXT("ScanMask is restored to 0x%02x"), oldMask);
		}
	}

	if ((mask == 4) && (iResult == ERROR_SUCCESS))
	{
		return TPR_PASS;
	}
	else
	{
		return TPR_FAIL;
	}
}

//
//TestId        : 004
//Title         : AuthenticationEnable Test
//Description   : BthReadAuthenticationEnable(); BthWriteAuthenticationEnable()
//
TESTPROCAPI t004(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	int iResult;
	unsigned char auth, curAuth, oldAuth;

	iResult = BthReadAuthenticationEnable(&oldAuth);
	if (iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("BthReadAuthenticationEnable() failed, error = %d"), iResult);
	}
	else
	{
		QAMessage(TEXT("Current AuthenticationEnable is 0x%02x"), oldAuth);
		QAMessage(TEXT("    0x00 - Authentication is disabled"));
		QAMessage(TEXT("    0x01 - Authentication is enabled for all connections"));
		for (auth = 0; auth < 2; auth ++)
		{
			QAMessage(TEXT("Write AuthenticationEnable 0x%02x"), auth);
			iResult = BthWriteAuthenticationEnable(auth);
			if (iResult != ERROR_SUCCESS)
			{
				QAError(TEXT("BthWriteAuthenticationEnable(0x%02x) failed, error = %d"), auth, iResult);
				break;
			}
			iResult = BthReadAuthenticationEnable(&curAuth);
			if (iResult != ERROR_SUCCESS)
			{
				QAError(TEXT("BthReadAuthenticationEnable() failed, error = %d"), iResult);
				break;
			}
			QAMessage(TEXT("AutjenticationEnable is now 0x%02x"), curAuth);
			if (curAuth != auth)
			{
				QAError(TEXT("AnthenticationEnable did not change as expected"));
				break;
			}
		}

		iResult = BthWriteAuthenticationEnable(oldAuth);
		if (iResult != ERROR_SUCCESS)
		{
			QAError(TEXT("BthWriteAuthenticationEnable(0x%02x) failed, error = %d"), iResult);
		}
		else
		{
			QAMessage(TEXT("AuthenticationEnable is restored to 0x%02x"), oldAuth);
		}
	}

	if ((auth == 2) && (iResult == ERROR_SUCCESS))
	{
		return TPR_PASS;
	}
	else
	{
		return TPR_FAIL;
	}
}

//
//TestId        : 005
//Title         : PageTimeout Test
//Description   : BthReadPageTimeout(); BthWritePageTimeout()
//
TESTPROCAPI t005(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	int iResult;
	unsigned short timeout, curTimeout, oldTimeout;

	iResult = BthReadPageTimeout(&oldTimeout);
	if (iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("BthReadPageTimeout() failed, error = %d"), iResult);
	}
	else
	{
		QAMessage(TEXT("Current PageTimeout is 0x%04x"), oldTimeout);
		for (timeout = 0x0001; timeout < 0x8000; timeout <<= 1)
		{
			QAMessage(TEXT("BthWritePageTimeout(0x%04x)"), timeout);
			iResult = BthWritePageTimeout(timeout);
			if (iResult != ERROR_SUCCESS)
			{
				QAError(TEXT("BthWritePageTimeout(0x%04x) failed, error = %d"), timeout, iResult);
				break;
			}
			iResult = BthReadPageTimeout(&curTimeout);
			if (iResult != ERROR_SUCCESS)
			{
				QAError(TEXT("BthReadPageTimeout() failed, error = %d"), iResult);
				break;
			}
			else
			{
				QAMessage(TEXT("PageTimeout is now 0x%04x"), curTimeout);
			}
			if (curTimeout != timeout)
			{
				QAError(TEXT("PageTimeout did not change as expected"));
				break;
			}
		}

		iResult = BthWritePageTimeout(oldTimeout);
		if (iResult != ERROR_SUCCESS)
		{
			QAError(TEXT("BthWritePageTimeout(0x%04x) failed, error = %d"), oldTimeout, iResult);
		}
		else
		{
			QAMessage(TEXT("PageTimeout is restored to 0x%04x"), oldTimeout);
		}
	}

	if ((timeout == 0x8000) && (iResult == ERROR_SUCCESS))
	{
		return TPR_PASS;
	}
	else
	{
		return TPR_FAIL;
	}
}

//
//TestId        : 006
//Title         : Class of Device Test
//Description   : BthReadCOD(); BthWriteCOD()
//
TESTPROCAPI t006(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	int iResult;
	unsigned int cod, curCod, oldCod;

	iResult = BthReadCOD(&oldCod);
	if (iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("BthReadCOD() failed, error = %d"), iResult);
	}
	else
	{
		QAMessage(TEXT("Current Class of Device is 0x%06x"), oldCod);
		for (cod = 0x000080; cod != 0x800000; cod <<= 1)
		{
			QAMessage(TEXT("BthWriteCOD(0x%06x)"), cod);
			iResult = BthWriteCOD(cod);
			if (iResult != ERROR_SUCCESS)
			{
				QAError(TEXT("BthWriteCOD(0x%06x) failed, error = %d"), cod, iResult);
				break;
			}
			iResult = BthReadCOD(&curCod);
			if (iResult != ERROR_SUCCESS)
			{
				QAError(TEXT("BthReadCOD() failed, error = %d"), iResult);
				break;
			}
			else
			{
				QAMessage(TEXT("Class of Device is now 0x%06x"), curCod);
			}
			if (curCod != cod)
			{
				QAError(TEXT("Class of Device did not change as expected"));
				break;
			}
		}

		iResult = BthWriteCOD(oldCod);
		if (iResult != ERROR_SUCCESS)
		{
			QAError(TEXT("BthWriteCod(0x%06x) failed, error = %d"), oldCod, iResult);
		}
		else
		{
			QAMessage(TEXT("Class of Device is restored to 0x%06x"), oldCod);
		}
	}

	if ((cod == 0x800000) && (iResult == ERROR_SUCCESS))
	{
		return TPR_PASS;
	}
	else
	{
		return TPR_FAIL;
	}
}

//
//TestId        : 007
//Title         : Read Local Version Test
//Description   : BthReadLocalVersion();
//
TESTPROCAPI t007(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	BtVersionInfo info;

	int iResult = info.ReadLocal();

	if (iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("info.ReadLocal() failed, error = %d"), iResult);
	}
	else
	{
		info.OutStr();
		return TPR_PASS;
	}

	return TPR_FAIL;
}

//
//TestId        : 008
//Title         : Set/Revoke PIN Test
//Description   : BthSetPIN(); BthRevokePIN()
//
TESTPROCAPI t008(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	int iResult;
	BT_ADDR ba;
	unsigned char PIN[16];

	memset(&ba, 0xff, sizeof(ba)); // fake Bluetooth address
	memset(&PIN[0], 0x00, sizeof(PIN));
	QAMessage(TEXT("BthSetPIN"));
	iResult = BthSetPIN(&ba, sizeof(PIN), &PIN[0]);
	if (iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("BthSetPIN failed, error = %d"), iResult);
	}
	else
	{
		QAMessage(TEXT("BthRevokePIN"));
		iResult = BthRevokePIN(&ba);
		if (iResult != ERROR_SUCCESS)
		{
			QAError(TEXT("BthSetPIN failed, error = %d"), iResult);
		}
		else
		{
			return TPR_PASS;
		}
	}

	return TPR_FAIL;
}

//
//TestId        : 009
//Title         : Link Key Test
//Description   : BthSetLinkKey(); BthGetLinkKey(); BthRevokeLinkKey()
//
TESTPROCAPI t009(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	int iResult;
	BT_ADDR ba;
	unsigned char getKey[16], setKey[16];
	BOOL fTestPass = TRUE;

	memset(&ba, 0xff, sizeof(ba)); // fake Bluetooth address
	memset(&getKey[0], 0x00, sizeof(getKey));
	memset(&setKey[0], 0xff, sizeof(setKey));
	QAMessage(TEXT("BthGetLinkKey"));
	iResult = BthGetLinkKey(&ba, getKey);
	if (iResult != ERROR_NOT_FOUND)
	{
		QAError(TEXT("BthGetLinkKey did not return ERROR_NOT_FOUND as expected, error = %d"), iResult);
		fTestPass = FALSE;
	}
	else
	{
		QAMessage(TEXT("BthSetLinkKey"));
		iResult = BthSetLinkKey(&ba, setKey);
		if (iResult != ERROR_SUCCESS)
		{
			QAError(TEXT("BthSetLinkKey failed, error = %d"), iResult);
			fTestPass = FALSE;
		}
		else
		{
			QAMessage(TEXT("BthGetLinkKey"));
			iResult = BthGetLinkKey(&ba, getKey);
			if (iResult != ERROR_SUCCESS)
			{
				QAError(TEXT("BthGetLinkKey failed, error = %d"), iResult);
				fTestPass = FALSE;
			}
			else
			{
				if (memcmp(getKey, setKey, sizeof(getKey)) != 0)
				{
					QAError(TEXT("Link key set does not match link key get"));
					fTestPass = FALSE;
				}
				QAMessage(TEXT("BthRevokeLinkKey"));
				iResult = BthRevokeLinkKey(&ba);
				if (iResult != ERROR_SUCCESS)
				{
					QAError(TEXT("BthRevokeLinkKey failed, error = %d"), iResult);
					fTestPass = FALSE;
				}
				else
				{
					QAMessage(TEXT("BthGetLinkKey"));
					iResult = BthGetLinkKey(&ba, getKey);
					if (iResult != ERROR_NOT_FOUND)
					{
						QAError(TEXT("BthGetLinkKey did not return ERROR_NOT_FOUND as expected, error = %d"), iResult);
						fTestPass = FALSE;
					}
				}
			}
		}
	}

	if (fTestPass)
	{
		return TPR_PASS;
	}
	else
	{
		return TPR_FAIL;
	}
}

//
//TestId        : 010
//Title         : Security UI Test
//Description   : BthSetSecurityUI()
//
TESTPROCAPI t010(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	int iResult;
	HANDLE hEvent;
	DWORD dwStoreTimeout, dwProcTimeout;

	hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (hEvent == NULL)
	{
		QAError(TEXT("CreateEvent failed, error = %d"), iResult = GetLastError());
		return TPR_FAIL;
	}
	dwStoreTimeout = 30000; // 30 seconds
	dwProcTimeout = 30000; // 30 seconds
	QAMessage(TEXT("BthSetSecurityUI(0x%08x, 0x%08x, 0x%08x)"), hEvent, dwStoreTimeout, dwProcTimeout);
	iResult = BthSetSecurityUI(hEvent, dwStoreTimeout, dwProcTimeout);
	if (iResult != ERROR_SUCCESS && iResult != ERROR_ALREADY_ASSIGNED)
	{
		QAError(TEXT("BthSetSecurityUI failed, error = %d"), iResult);
	}
	else
	{
		if (iResult == ERROR_SUCCESS)
		{
			QAMessage(TEXT("BthSetSecurityUI(0x%08x, 0x%08x, 0x%08x)"), NULL, dwStoreTimeout, dwProcTimeout);
			iResult = BthSetSecurityUI(NULL, dwStoreTimeout, dwProcTimeout);
			if (iResult != ERROR_SUCCESS)
			{
				QAError(TEXT("BthSetSecurityUI failed, error = %d"), iResult);
				CloseHandle(hEvent);
				return TPR_FAIL;
			}
		}
		CloseHandle(hEvent);
		return TPR_PASS;
	}

	CloseHandle(hEvent);
	return TPR_FAIL;
}

//
//TestId        : 011
//Title         : PIN Request Test
//Description   : BthGetPINRequest(); BthRefusePINRequest()
//
TESTPROCAPI t011(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	int iResult;
	BT_ADDR ba;

	memset(&ba, 0x00, sizeof(ba));
	QAMessage(TEXT("BthGetPINRequest"));
	iResult = BthGetPINRequest(&ba);
	if (iResult != ERROR_NOT_FOUND)
	{
		QAError(TEXT("BthGetPINRequest did not return ERROR_NOT_FOUND as expected, error = %d"), iResult);
	}
	else
	{
		QAMessage(TEXT("BthRefusePINRequest"));
		iResult = BthRefusePINRequest(&ba);
		if (iResult != ERROR_NOT_FOUND)
		{
			QAError(TEXT("BthRefusePINRequest did not return ERROR_NOT_FOUND as expected, error = %d"), iResult);
		}
		else
		{
			return TPR_PASS;
		}
	}

	return TPR_FAIL;
}

//
//TestId        : 012
//Title         : PerformInquiry Test
//Description   : BthPerformInquiry();
//
TESTPROCAPI t012(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	if (!g_fHasServer)
	{
		QAMessage(TEXT("NO server - test case SKIPPED"));
		return TPR_SKIP;
	}

	int iResult;
	unsigned int cDiscoveredDevices = 0;
	BthInquiryResult InquiryList[10];
	BOOL fFoundServer = FALSE;

	QAMessage(TEXT("Perform inquiry LAP = 0x%08x"), BT_ADDR_GIAC);
	iResult = BthPerformInquiry(BT_ADDR_GIAC, 24, 10, 10, &cDiscoveredDevices, &InquiryList[0]);
	if (iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("BthPerformInquiry failed, error = %d"), iResult);
	}
	else
	{
		QAMessage(TEXT("Inquiry result: %ld devices found"), cDiscoveredDevices);
		QAMessage(TEXT("Address        COD      ClockOffset PageScanMode PageScanPeriodMode PageScanRepetitionMode"));
		QAMessage(TEXT("-------------- -------- ------      ----         ----               ----"));
		for (unsigned int i = 0; i < cDiscoveredDevices; i ++)
		{
			if (memcmp(&InquiryList[i].ba, &g_ServerAddress, sizeof(g_ServerAddress)) == 0)
			{
				fFoundServer = TRUE;
			}
			QAMessage(TEXT("0x%04x%08x 0x%06x 0x%04x      0x%02x         0x%02x               0x%02x"), 
				GET_NAP(InquiryList[i].ba), GET_SAP(InquiryList[i].ba),
				InquiryList[i].cod,
				InquiryList[i].clock_offset, InquiryList[i].page_scan_mode, 
				InquiryList[i].page_scan_period_mode, InquiryList[i].page_scan_repetition_mode);
		}
	}

	if (fFoundServer)
	{
		return TPR_PASS;
	}
	else
	{
		QAError(TEXT("Test server (0x%04x%08x) missing from inquiry result"), GET_NAP(g_ServerAddress), GET_SAP(g_ServerAddress));
		return TPR_FAIL;
	}
}

DWORD WINAPI InquiryThread(LPVOID lpParameter)
{
	int iResult;
	BthInquiryResult InquiryList[10];
	unsigned int cDiscoveredDevices = 0;

	QAMessage(TEXT("Perform inquiry LAP = 0x%08x"), BT_ADDR_GIAC);
	iResult = BthPerformInquiry(BT_ADDR_GIAC, 24, 10, 10, &cDiscoveredDevices, &InquiryList[0]);
	if (iResult != ERROR_SUCCESS && iResult != ERROR_CANCELLED)
	{
		QAError(TEXT("BthPerformInquiry failed, error = %d"), iResult);
	}
	else
	{
		QAMessage(TEXT("Inquiry result: %ld devices found"), cDiscoveredDevices);
		QAMessage(TEXT("Address        COD      ClockOffset PageScanMode PageScanPeriodMode PageScanRepetitionMode"));
		QAMessage(TEXT("-------------- -------- ------      ----         ----               ----"));
		for (unsigned int i = 0; i < cDiscoveredDevices; i ++)
		{
			QAMessage(TEXT("0x%04x%08x 0x%06x 0x%04x      0x%02x         0x%02x               0x%02x"), 
				GET_NAP(InquiryList[i].ba), GET_SAP(InquiryList[i].ba),
				InquiryList[i].cod,
				InquiryList[i].clock_offset, InquiryList[i].page_scan_mode, 
				InquiryList[i].page_scan_period_mode, InquiryList[i].page_scan_repetition_mode);
		}
	}
	
	return (DWORD) iResult;
}

//
//TestId        : 013
//Title         : CancelInquiry Test
//Description   : BthCancelInquiry(); BthPerformInquiry();
//
TESTPROCAPI t013(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	if (!g_fHasServer)
	{
		QAMessage(TEXT("NO server - test case SKIPPED"));
		return TPR_SKIP;
	}

	int iResult;
	HANDLE hThread;
	DWORD dwThread;

	hThread = CreateThread(NULL, 0, InquiryThread, NULL, 0, &dwThread);
	if (hThread == NULL)
	{
		QAError(TEXT("CreateThread(InquiryThread) failed, error = %d"), iResult = GetLastError());
	}
	else
	{
		Sleep(1000); // Wait for inquiry
		QAMessage(TEXT("Cancel Inquiry"));
		iResult = BthCancelInquiry();
		if (iResult != ERROR_SUCCESS)
		{
			QAError(TEXT("BthCancelInquiry failed, error = %d"), iResult);
		}
	}
	WaitForSingleObject(hThread, INFINITE);

	if (iResult == ERROR_SUCCESS)
	{
		return TPR_PASS;
	}
	else
	{
		return TPR_FAIL;
	}
}

//
//TestId        : 014
//Title         : InquiryFilter Test
//Description   : BthSetInquiryFilter(); BthClearInquiryFilter; BthPerformInquiry();
//
TESTPROCAPI t014(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	if (!g_fHasServer)
	{
		QAMessage(TEXT("NO server - test case SKIPPED"));
		return TPR_SKIP;
	}

	// verify that feature is supported

	BtVersionInfo info;
	int iResult = info.ReadLocal();

	if (iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("info.ReadLocal() failed, error = %d"), iResult);
		return TPR_FAIL;
	}

	BOOL Skip = info.EqualOrOlderThan(11, 1, 1300);	// Sharp UART card - broken

	if (Skip)
	{
		QAMessage(TEXT("Feature not supported or or broken in BT module"));
		return TPR_SKIP;
	}

	// run test

	BT_ADDR filter;
	unsigned int cDiscoveredDevices = 0;
	BthInquiryResult InquiryList[10];
	BOOL fTestPass = TRUE;

	for (unsigned int i = 0; i < 2; i ++)
	{
		if (i == 1)
		{
			memcpy(&filter, &g_ServerAddress, sizeof(filter));
		}
		else
		{
			memset(&filter, 0xff, sizeof(filter));
		}

		QAMessage(TEXT("Clear inquiry filter to prevent memory error"));
		iResult = BthClearInquiryFilter();
		if (iResult != ERROR_SUCCESS)
		{
			QAError(TEXT("BthClearInquiryFilter failed, error = %d"), iResult);
			fTestPass = FALSE;
			break;
		}
		
		QAMessage(TEXT("Set inquiry filter to 0x%04x%08x"), GET_NAP(filter), GET_SAP(filter));
		iResult = BthSetInquiryFilter(&filter);
		if (iResult != ERROR_SUCCESS)
		{
			QAError(TEXT("BthSetInquiryFilter failed, error = %d"), iResult);
			fTestPass = FALSE;
		}
		else
		{
			QAMessage(TEXT("Perform inquiry LAP = 0x%08x"), BT_ADDR_GIAC);
			iResult = BthPerformInquiry(BT_ADDR_GIAC, 24, 10, 10, &cDiscoveredDevices, &InquiryList[0]);
			if (iResult != ERROR_SUCCESS)
			{
				QAError(TEXT("BthPerformInquiry failed, error = %d"), iResult);
				fTestPass = FALSE;
			}
			else
			{
				QAMessage(TEXT("Inquiry result: %ld devices found"), cDiscoveredDevices);
				QAMessage(TEXT("Address        COD      ClockOffset PageScanMode PageScanPeriodMode PageScanRepetitionMode"));
				QAMessage(TEXT("-------------- -------- ------      ----         ----               ----"));
				for (unsigned int j = 0; j < cDiscoveredDevices; j ++)
				{
					QAMessage(TEXT("0x%04x%08x 0x%06x 0x%04x      0x%02x         0x%02x               0x%02x"), 
						GET_NAP(InquiryList[j].ba), GET_SAP(InquiryList[j].ba),
						InquiryList[j].cod,
						InquiryList[j].clock_offset, InquiryList[j].page_scan_mode, 
						InquiryList[j].page_scan_period_mode, InquiryList[j].page_scan_repetition_mode);
				}
				if (i == 1)
				{
					if (cDiscoveredDevices > 1)
					{
						QAError(TEXT("Found %ld devices. Inquiry filter 0x%04x%08x did not work"), cDiscoveredDevices, GET_NAP(filter), GET_SAP(filter));
						fTestPass = FALSE;
					}
					else if ((cDiscoveredDevices == 0) || (memcmp(&InquiryList[0].ba, &g_ServerAddress, sizeof(g_ServerAddress)) != 0))
					{
						QAError(TEXT("Test server (0x%04x%08x) missing from inquiry result"), GET_NAP(g_ServerAddress), GET_SAP(g_ServerAddress));
						fTestPass = FALSE;
					}
				}
				else
				{
					if (cDiscoveredDevices != 0)
					{
						QAError(TEXT("Found %ld devices. Inquiry filter 0x%04x%08x did not work"), cDiscoveredDevices, GET_NAP(filter), GET_SAP(filter));
						fTestPass = FALSE;
					}
				}
			}
		}
	}
	
	QAMessage(TEXT("Clear inquiry filter"));
	iResult = BthClearInquiryFilter();
	if (iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("BthClearInquiryFilter failed, error = %d"), iResult);
		fTestPass = FALSE;
	}
	else
	{
		QAMessage(TEXT("Perform inquiry LAP = 0x%08x"), BT_ADDR_GIAC);
		iResult = BthPerformInquiry(BT_ADDR_GIAC, 24, 10, 10, &cDiscoveredDevices, &InquiryList[0]);
		if (iResult != ERROR_SUCCESS)
		{
			QAError(TEXT("BthPerformInquiry failed, error = %d"), iResult);
			fTestPass = FALSE;
		}
		else
		{
			for (unsigned int i = 0; i < cDiscoveredDevices; i ++)
			{
				if (memcmp(&InquiryList[i].ba, &g_ServerAddress, sizeof(g_ServerAddress)) == 0)
				{
					break;
				}
			}
			if (i == cDiscoveredDevices)
			{
				QAError(TEXT("Test server (0x%04x%08x) missing from inquiry result"), GET_NAP(g_ServerAddress), GET_SAP(g_ServerAddress));
				fTestPass = FALSE;
			}
		}
	}
	
	if (fTestPass)
	{
		return TPR_PASS;
	}
	else
	{
		return TPR_FAIL;
	}
}

//
//TestId        : 015
//Title         : Remote Name Query Test
//Description   : BthRemoteNameQuery()
//
TESTPROCAPI t015(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	if (!g_fHasServer)
	{
		QAMessage(TEXT("NO server - test case SKIPPED"));
		return TPR_SKIP;
	}

	int iResult;
	unsigned int cRequired = 0;
	WCHAR szName[256];

	QAMessage(TEXT("BthRemoteNameQuery"));
	iResult = BthRemoteNameQuery(&g_ServerAddress, sizeof(szName), &cRequired, &szName[0]);
	if (iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("BthRemoteNameQuery failed, error = %d"), iResult);
	}
	else
	{
		QAMessage(TEXT("Name for 0x%04x%08x is \"%s\""), GET_NAP(g_ServerAddress), GET_SAP(g_ServerAddress), szName);
	}

	if (iResult == ERROR_SUCCESS)
	{
		return TPR_PASS;
	}
	else
	{
		return TPR_FAIL;
	}
}

//
//TestId        : 016
//Title         : ACL Connection Test
//Description   : BthCreateACLConnection(); BthGetBasebandHandles(); BthGetAddress(); BthGetBasebandConnections(); BthCloseConnection()
//
TESTPROCAPI t016(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	if (!g_fHasServer)
	{
		QAMessage(TEXT("NO server - test case SKIPPED"));
		return TPR_SKIP;
	}

	int iResult;
	unsigned short hConnection;
	unsigned short hHandles[10];
	BT_ADDR ba;
	BASEBAND_CONNECTION connections[10];
	int cReturned = 0;
	BOOL fTestPass = TRUE;
	BOOL fFoundServer = FALSE;
	TCHAR tszMode[10];

	QAMessage(TEXT("Create ACL connection to 0x%04x%08x"), GET_NAP(g_ServerAddress), GET_SAP(g_ServerAddress));
	iResult = BthCreateACLConnectionRetry (&g_ServerAddress, &hConnection, 3);
	
	if (iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("BthCreateACLConnection failed, error = %d"), iResult);
		fTestPass = FALSE;
	}
	else
	{
		// BthGetBasebandHandles
		QAMessage(TEXT("Get baseband handles"));
		iResult = BthGetBasebandHandles(sizeof(hHandles)/sizeof(hHandles[0]), &hHandles[0], &cReturned);
		if (iResult != ERROR_SUCCESS)
		{
			QAError(TEXT("BthGetBasebandHandles failed, error = %d"), iResult);
			fTestPass = FALSE;
		}

		QAMessage(TEXT("%d baseband handles found"), cReturned);
		QAMessage(TEXT("Handle BluetoothAddress"));
		QAMessage(TEXT("------ --------------"));
		for (int i = 0; i < cReturned; i ++)
		{
			if (hConnection == hHandles[i])
			{
				fFoundServer = TRUE;
			}
			iResult = BthGetAddress(hHandles[i], &ba);
			if (iResult != ERROR_SUCCESS)
			{
				QAError(TEXT("BthGetBasebandHandles(0x%04x) failed, error = %d"), hHandles[i], iResult);
				fTestPass = FALSE;
			}
			else
			{
				QAMessage(TEXT("0x%04x 0x%04x%08x"), hHandles[i], GET_NAP(ba), GET_SAP(ba));
			}
		}
		if (!fFoundServer)
		{
			fTestPass = FALSE;
		}

		// BthGetBasebandConnections
		cReturned = 0;
		QAMessage(TEXT("Get baseband connections"));
		iResult = BthGetBasebandConnections(sizeof(connections)/sizeof(connections[0]), &connections[0], &cReturned);
		if (iResult != ERROR_SUCCESS)
		{
			QAError(TEXT("BthGetBasebandConnections failed, error = %d"), iResult);
			fTestPass = FALSE;
		}

		fFoundServer = FALSE;
		QAMessage(TEXT("%d baseband connections found"), cReturned);
		QAMessage(TEXT("Connection BluetoothAddress DataPacketsPending LinkType Encrypted Authenticated Mode"));
		QAMessage(TEXT("------     --------------   ----------         ---      ---       ---           -------"));
		for (i = 0; i < cReturned; i ++)
		{
			if (hConnection == connections[i].hConnection)
			{
				fFoundServer = TRUE;
			}
			switch (connections[i].fMode)
			{
			case 0x00: 
				_tcscpy(tszMode, TEXT("ACTIVE "));
				break;
			case 0x01:
				_tcscpy(tszMode, TEXT("HOLD   "));
				break;
			case 0x02:
				_tcscpy(tszMode, TEXT("SNIFF  "));
				break;
			case 0x03:
				_tcscpy(tszMode, TEXT("PARK   "));
			default:
				_tcscpy(tszMode, TEXT("UNKNOWN"));
				break;
			}
			QAMessage(TEXT("0x%04x     0x%04x%08x   0x%08x         %s      %s       %s           %s"),
				connections[i].hConnection, GET_NAP(connections[i].baAddress), GET_SAP(connections[i].baAddress),
				connections[i].cDataPacketsPending, connections[i].fLinkType ? TEXT("ACL") : TEXT("SCO"),
				connections[i].fEncrypted ? TEXT("YES") : TEXT("NO "), 
				connections[i].fAuthenticated ? TEXT("YES") : TEXT("NO "),
				tszMode);
		}
		if (!fFoundServer)
		{
			fTestPass = FALSE;
		}

		QAMessage(TEXT("Terminate ACL connection"));
		iResult = BthCloseConnection(hConnection);
		if (iResult != ERROR_SUCCESS)
		{
			QAError(TEXT("BthCloseConnection failed, error = %d"), iResult);
			fTestPass = FALSE;
		}
	}

	if (fTestPass)
	{
		return TPR_PASS;
	}
	else
	{
		return TPR_FAIL;
	}
}

//
//TestId        : 017
//Title         : Read Remote Version Test
//Description   : BthReadRemoteVersion();
//
TESTPROCAPI t017(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	if (!g_fHasServer)
	{
		QAMessage(TEXT("NO server - test case SKIPPED"));
		return TPR_SKIP;
	}

	int iResult;
	unsigned short hConnection;
	BOOL fTestPass = TRUE;

	QAMessage(TEXT("Create ACL connection to 0x%04x%08x"), GET_NAP(g_ServerAddress), GET_SAP(g_ServerAddress));
	iResult = BthCreateACLConnectionRetry(&g_ServerAddress, &hConnection, 3);
	
	if (iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("BthCreateACLConnection failed, error = %d"), iResult);
		fTestPass = FALSE;
	}
	else
	{
		BtVersionInfo info;

		iResult = info.ReadRemote(&g_ServerAddress);
		
		if (iResult != ERROR_SUCCESS)
		{
			QAError(TEXT("BthReadRemoteVersion failed, error = %d"), iResult);
			fTestPass = FALSE;
		}
		else
		{
			info.OutStr();
		}

		QAMessage(TEXT("Terminate ACL connection"));
		iResult = BthCloseConnection(hConnection);
		if (iResult != ERROR_SUCCESS)
		{
			QAError(TEXT("BthCloseConnection failed, error = %d"), iResult);
			fTestPass = FALSE;
		}
	}

	if (fTestPass)
	{
		return TPR_PASS;
	}
	else
	{
		return TPR_FAIL;
	}
}

//
//TestId        : 018
//Title         : LinkPolicySettings Test
//Description   : BthReadLinkPolicySettings(); BthWriteLinkPolicySettings()
//
TESTPROCAPI t018(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	if (!g_fHasServer)
	{
		QAMessage(TEXT("NO server - test case SKIPPED"));
		return TPR_SKIP;
	}

	int iResult;
	unsigned short hConnection;
	unsigned short policy, curPolicy, oldPolicy;
	BOOL fTestPass = TRUE;

	QAMessage(TEXT("Create ACL connection to 0x%04x%08x"), GET_NAP(g_ServerAddress), GET_SAP(g_ServerAddress));
	iResult = BthCreateACLConnectionRetry (&g_ServerAddress, &hConnection, 3);
	
	if (iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("BthCreateACLConnection failed, error = %d"), iResult);
		fTestPass = FALSE;
	}
	else
	{
		iResult = BthReadLinkPolicySettings(&g_ServerAddress, &oldPolicy);
		if (iResult != ERROR_SUCCESS)
		{
			QAError(TEXT("BthReadLinkPolicySettings(0x%04x%08x) failed, error = %d"), GET_NAP(g_ServerAddress), GET_SAP(g_ServerAddress), iResult);
			fTestPass = FALSE;
		}
		else
		{
			QAMessage(TEXT("Current LinkPolicySettings is 0x%04x"), oldPolicy);
			QAMessage(TEXT("    0x0000 - Disables all LMP modes"));
			QAMessage(TEXT("    0x0001 - Enables master slave switch"));
			QAMessage(TEXT("    0x0002 - Enables Hold mode"));
			QAMessage(TEXT("    0x0004 - Enables Sniff Mode"));
			QAMessage(TEXT("    0x0008 - Enables Park Mode"));
			for (policy = 0; policy < 9; policy ++)
			{
				QAMessage(TEXT("BthWriteLinkPolicySettings(0x%04x%08x, 0x%04x)"), GET_NAP(g_ServerAddress), GET_SAP(g_ServerAddress), policy);
				iResult = BthWriteLinkPolicySettings(&g_ServerAddress, policy);
				if (iResult != ERROR_SUCCESS)
				{
					QAError(TEXT("BthWriteLinkPolicySettings(0x%04x%08x, 0x%04x) failed, error = %d"), GET_NAP(g_ServerAddress), GET_SAP(g_ServerAddress), policy, iResult);
					fTestPass = FALSE;
				}
				iResult = BthReadLinkPolicySettings(&g_ServerAddress, &curPolicy);
				if (iResult != ERROR_SUCCESS)
				{
					QAError(TEXT("BthReadLinkPolicySettings(0x%04x%08x) failed, error = %d"), GET_NAP(g_ServerAddress), GET_SAP(g_ServerAddress), iResult);
					fTestPass = FALSE;
				}
				QAMessage(TEXT("LinkPolicySettings for 0x%04x%08x is now 0x%04x"), GET_NAP(g_ServerAddress), GET_SAP(g_ServerAddress), curPolicy);
				if (curPolicy != policy)
				{
					QAError(TEXT("LinkPolicySettings did not change as expected"));
					fTestPass = FALSE;
				}
			}

			iResult = BthWriteLinkPolicySettings(&g_ServerAddress, oldPolicy);
			if (iResult != ERROR_SUCCESS)
			{
				QAError(TEXT("BthWriteLinkPolicySettings(0x%04x%08x, 0x%04x) failed, error = %d"), GET_NAP(g_ServerAddress), GET_SAP(g_ServerAddress), oldPolicy, iResult);
				fTestPass = FALSE;
			}
			else
			{
				QAMessage(TEXT("LinkPolicySettings is restored to 0x%04x"), oldPolicy);
			}
		}

		QAMessage(TEXT("Terminate ACL connection"));
		iResult = BthCloseConnection(hConnection);
		if (iResult != ERROR_SUCCESS)
		{
			QAError(TEXT("BthCloseConnection failed, error = %d"), iResult);
			fTestPass = FALSE;
		}
	}

	if (fTestPass)
	{
		return TPR_PASS;
	}
	else
	{
		return TPR_FAIL;
	}
}

//
//TestId        : 019
//Title         : ACTIVE Mode Test
//Description   : BthCreateACLConnection(); BthGetCurrentMode(); BthCloseConnection()
//
TESTPROCAPI t019(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	if (!g_fHasServer)
	{
		QAMessage(TEXT("NO server - test case SKIPPED"));
		return TPR_SKIP;
	}

	int iResult;
	BOOL fTestPass = FALSE;
	unsigned short hConnection;
	unsigned char mode;

	QAMessage(TEXT("Create ACL connection to 0x%04x%08x"), GET_NAP(g_ServerAddress), GET_SAP(g_ServerAddress));
	iResult = BthCreateACLConnectionRetry (&g_ServerAddress, &hConnection, 3);

	
	if (iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("BthCreateACLConnection failed, error = %d"), iResult);
	}
	else
	{
		iResult = BthGetCurrentMode(&g_ServerAddress, &mode);
		if (iResult != ERROR_SUCCESS)
		{
			QAError(TEXT("BthGetCurrentMode failed, error = %d"), iResult);
		}
		else
		{
			QAMessage(TEXT("Current mode is 0x%02x"), mode);
			QAMessage(TEXT("    0x00 ACTIVE"));
			QAMessage(TEXT("    0x01 HOLD"));
			QAMessage(TEXT("    0x02 SNIFF"));
			QAMessage(TEXT("    0x03 PARK"));
			
			if (mode != 0x00) // ACTIVE mode
			{
				QAError(TEXT("Current mode is not ACTIVE!"));
			}
			else
			{
				fTestPass = TRUE;
			}

			QAMessage(TEXT("Terminate ACL connection"));
			iResult = BthCloseConnection(hConnection);
			if (iResult != ERROR_SUCCESS)
			{
				QAError(TEXT("BthCloseConnection failed, error = %d"), iResult);
				fTestPass = FALSE;
			}
		}
	}

	if (fTestPass)
	{
		return TPR_PASS;
	}
	else
	{
		return TPR_FAIL;
	}
}

//
//TestId        : 020
//Title         : HOLD Mode Test
//Description   : BthCreateACLConnection(); BthEnterHoldMode(); BthGetCurrentMode(); BthCloseConnection()
//
TESTPROCAPI t020(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	if (!g_fHasServer)
	{
		QAMessage(TEXT("NO server - test case SKIPPED"));
		return TPR_SKIP;
	}

	// verify that feature is supported

	BtVersionInfo info;
	int iResult = info.ReadLocal();

	if (iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("info.ReadLocal() failed, error = %d"), iResult);
		return TPR_FAIL;
	}

	BOOL Skip = (! info.HoldSupported());			// officially not supported
	Skip = Skip || info.EqualOrOlderThan(11, 1, 1300);	// Sharp UART card - broken

	if (Skip)
	{
		QAMessage(TEXT("Feature not supported or or broken in BT module"));
		return TPR_SKIP;
	}

	// run test
	
	BOOL fTestPass = TRUE;
	unsigned short hConnection;
	unsigned short interval;
	unsigned char mode;

	QAMessage(TEXT("Create ACL connection to 0x%04x%08x"), GET_NAP(g_ServerAddress), GET_SAP(g_ServerAddress));
	iResult = BthCreateACLConnectionRetry (&g_ServerAddress, &hConnection, 4);
	
	if (iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("BthCreateACLConnection failed, error = %d"), iResult);
		fTestPass = FALSE;
	}
	else
	{
		QAMessage(TEXT("BthWriteLinkPolicySettings(0x%04x%08x, 0x0002)"), GET_NAP(g_ServerAddress), GET_SAP(g_ServerAddress));
		iResult = BthWriteLinkPolicySettings(&g_ServerAddress, 0x0002);
		if (iResult != ERROR_SUCCESS)
		{
			QAError(TEXT("BthWriteLinkPolicySettings(0x%04x%08x, 0x0002) failed to enable HOLD mode, error = %d"), GET_NAP(g_ServerAddress), GET_SAP(g_ServerAddress), iResult);
			fTestPass = FALSE;
		}
		else
		{ 
			// Enter HOLD mode 
			QAMessage(TEXT("Enter HOLD mode"));
			iResult = BthEnterHoldMode(&g_ServerAddress, TEST_HOLD_MAX, TEST_HOLD_MIN, &interval);
			if (iResult != ERROR_SUCCESS)
			{
				QAError(TEXT("BthEnterHoldMode (0x%04x,0x%04x) failed, error = %d"), TEST_HOLD_MAX, TEST_HOLD_MIN, iResult);
				fTestPass = FALSE;
			}
			else
			{
				iResult = BthGetCurrentMode(&g_ServerAddress, &mode);
				if (iResult != ERROR_SUCCESS)
				{
					QAError(TEXT("BthGetCurrentMode failed, error = %d"), iResult);
					fTestPass = FALSE;
				}
				else
				{
					QAMessage(TEXT("Current mode is 0x%02x"), mode);
					QAMessage(TEXT("    0x00 ACTIVE"));
					QAMessage(TEXT("    0x01 HOLD"));
					QAMessage(TEXT("    0x02 SNIFF"));
					QAMessage(TEXT("    0x03 PARK"));
					if (mode != 0x01) // HOLD mode
					{
						QAError(TEXT("Current mode is not HOLD!"));
						fTestPass = FALSE;
					}
				}

				// Exit HOLD mode
				QAMessage(TEXT("Sleep %ld milliseconds waiting hardware to exit HOLD mode"), DWORD (0.625 * interval) + 3000);
				Sleep(DWORD (0.625 * interval) + 3000); // allow extra 10 ms

				iResult = BthGetCurrentMode(&g_ServerAddress, &mode);
				if (iResult != ERROR_SUCCESS)
				{
					QAError(TEXT("BthGetCurrentMode failed, error = %d"), iResult);
					fTestPass = FALSE;
				}
				else
				{
					QAMessage(TEXT("Current mode is 0x%02x"), mode);
					QAMessage(TEXT("    0x00 ACTIVE"));
					QAMessage(TEXT("    0x01 HOLD"));
					QAMessage(TEXT("    0x02 SNIFF"));
					QAMessage(TEXT("    0x03 PARK"));
					if (mode != 0x00) // ACTIVE mode
					{
						QAError(TEXT("Current mode is not ACTIVE!"));
						fTestPass = FALSE;
					}
				}
			}

			QAMessage(TEXT("Terminate ACL connection"));
			iResult = BthCloseConnection(hConnection);
			if (iResult != ERROR_SUCCESS)
			{
				QAError(TEXT("BthCloseConnection failed, error = %d"), iResult);
				fTestPass = FALSE;
			}
		}
	}

	if (fTestPass)
	{
		return TPR_PASS;
	}
	else
	{
		return TPR_FAIL;
	}
}

//
//TestId        : 021
//Title         : SNIFF Mode Test
//Description   : BthCreateACLConnection(); BthEnterSniffMode(); BthGetCurrentMode(); BthExitSniffMode(); BthCloseConnection()
//
TESTPROCAPI t021(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	if (!g_fHasServer)
	{
		QAMessage(TEXT("NO server - test case SKIPPED"));
		return TPR_SKIP;
	}

	// verify that feature is supported

	BtVersionInfo info;
	int iResult = info.ReadLocal();

	if (iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("info.ReadLocal() failed, error = %d"), iResult);
		return TPR_FAIL;
	}

	BOOL Skip = (! info.SniffSupported());			// officially not supported
	Skip = Skip || info.EqualOrOlderThan(11, 1, 1300);	// Sharp UART card - broken

	if (Skip)
	{
		QAMessage(TEXT("Feature not supported or or broken in BT module"));
		return TPR_SKIP;
	}

	// run test

	BOOL fTestPass = TRUE;
	unsigned short hConnection;
	unsigned short interval;
	unsigned char mode;

	QAMessage(TEXT("Create ACL connection to 0x%04x%08x"), GET_NAP(g_ServerAddress), GET_SAP(g_ServerAddress));
	iResult = BthCreateACLConnectionRetry (&g_ServerAddress, &hConnection, 4);
	
	if (iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("BthCreateACLConnection failed, error = %d"), iResult);
		fTestPass = FALSE;
	}
	else
	{
		QAMessage(TEXT("BthWriteLinkPolicySettings(0x%04x%08x, 0x0004)"), GET_NAP(g_ServerAddress), GET_SAP(g_ServerAddress));
		iResult = BthWriteLinkPolicySettings(&g_ServerAddress, 0x0004);
		if (iResult != ERROR_SUCCESS)
		{
			QAError(TEXT("BthWriteLinkPolicySettings(0x%04x%08x, 0x0004) failed to enable SNIFF mode, error = %d"), GET_NAP(g_ServerAddress), GET_SAP(g_ServerAddress), iResult);
			fTestPass = FALSE;
		}
		else
		{
			// Enter SNIFF mode
			QAMessage(TEXT("Enter SNIFF mode"));
			iResult = BthEnterSniffMode(&g_ServerAddress, TEST_SNIFF_MAX, TEST_SNIFF_MIN, TEST_SNIFF_ATTEMPT, TEST_SNIFF_TO, &interval);
			if (iResult != ERROR_SUCCESS)
			{
				QAError(TEXT("BthEnterSniffMode(0x%04x%08x, 0x%04x, 0x%04x, 0x%04x, 0x%04x) failed, error = %d"), 
					GET_NAP(g_ServerAddress), GET_SAP(g_ServerAddress), TEST_SNIFF_MAX, TEST_SNIFF_MIN, TEST_SNIFF_ATTEMPT, TEST_SNIFF_TO, iResult);
				fTestPass = FALSE;
			}
			else
			{
				iResult = BthGetCurrentMode(&g_ServerAddress, &mode);
				if (iResult != ERROR_SUCCESS)
				{
					QAError(TEXT("BthGetCurrentMode failed, error = %d"), iResult);
					fTestPass = FALSE;
				}
				else
				{
					QAMessage(TEXT("Current mode is 0x%02x"), mode);
					QAMessage(TEXT("    0x00 ACTIVE"));
					QAMessage(TEXT("    0x01 HOLD"));
					QAMessage(TEXT("    0x02 SNIFF"));
					QAMessage(TEXT("    0x03 PARK"));
					if (mode != 0x02) // SNIFF mode
					{
						QAError(TEXT("Current mode is not SNIFF!"));
						fTestPass = FALSE;
					}
				}

				// Exit SNIFF mode
				QAMessage(TEXT("Exit SNIFF mode"));
				iResult = BthExitSniffMode(&g_ServerAddress);
				if (iResult != ERROR_SUCCESS)
				{
					QAError(TEXT("BthExitSniffMode failed, error = %d"), iResult);
					fTestPass = FALSE;
				}

				iResult = BthGetCurrentMode(&g_ServerAddress, &mode);
				if (iResult != ERROR_SUCCESS)
				{
					QAError(TEXT("BthGetCurrentMode failed, error = %d"), iResult);
					fTestPass = FALSE;
				}
				else
				{
					QAMessage(TEXT("Current mode is 0x%02x"), mode);
					QAMessage(TEXT("    0x00 ACTIVE"));
					QAMessage(TEXT("    0x01 HOLD"));
					QAMessage(TEXT("    0x02 SNIFF"));
					QAMessage(TEXT("    0x03 PARK"));
					if (mode != 0x00) // ACTIVE mode
					{
						QAError(TEXT("Current mode is not ACTIVE!"));
						fTestPass = FALSE;
					}
				}
			}

			QAMessage(TEXT("Terminate ACL connection"));
			iResult = BthCloseConnection(hConnection);
			if (iResult != ERROR_SUCCESS)
			{
				QAError(TEXT("BthCloseConnection failed, error = %d"), iResult);
				fTestPass = FALSE;
			}
		}
	}

	if (fTestPass)
	{
		return TPR_PASS;
	}
	else
	{
		return TPR_FAIL;
	}
}

//
//TestId        : 022
//Title         : PARK Mode Test
//Description   : BthCreateACLConnection(); BthEnterParkMode(); BthGetCurrentMode(); BthExitParkMode(); BthCloseConnection()
//
TESTPROCAPI t022(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	if (!g_fHasServer)
	{
		QAMessage(TEXT("NO server - test case SKIPPED"));
		return TPR_SKIP;
	}

	// verify that feature is supported

	BtVersionInfo info;
	int iResult = info.ReadLocal();

	if (iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("info.ReadLocal() failed, error = %d"), iResult);
		return TPR_FAIL;
	}

	BOOL Skip = (! info.ParkSupported());			// officially not supported
	Skip = Skip || info.EqualOrOlderThan(11, 1, 1300);	// Sharp UART card - broken

	if (Skip)
	{
		QAMessage(TEXT("Feature not supported or or broken in BT module"));
		return TPR_SKIP;
	}

	// run test

	BOOL fTestPass = TRUE;
	unsigned short hConnection;
	unsigned short interval;
	unsigned char mode;

	QAMessage(TEXT("Create ACL connection to 0x%04x%08x"), GET_NAP(g_ServerAddress), GET_SAP(g_ServerAddress));
	iResult = BthCreateACLConnectionRetry (&g_ServerAddress, &hConnection, 4);
	
	if (iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("BthCreateACLConnection failed, error = %d"), iResult);
		fTestPass = FALSE;
	}
	else
	{
		QAMessage(TEXT("BthWriteLinkPolicySettings(0x%04x%08x, 0x0008)"), GET_NAP(g_ServerAddress), GET_SAP(g_ServerAddress));
		iResult = BthWriteLinkPolicySettings(&g_ServerAddress, 0x0008);
		if (iResult != ERROR_SUCCESS)
		{
			QAError(TEXT("BthWriteLinkPolicySettings(0x%04x%08x, 0x0008) failed to enable PARK mode, error = %d"), GET_NAP(g_ServerAddress), GET_SAP(g_ServerAddress), iResult);
			fTestPass = FALSE;
		}
		else
		{
			// Enter PARK mode
			QAMessage(TEXT("Enter PARK mode"));
			iResult = BthEnterParkMode(&g_ServerAddress, TEST_PARK_BEACON_MAX, TEST_PARK_BEACON_MIN, &interval);
			if (iResult != ERROR_SUCCESS)
			{
				QAError(TEXT("BthEnterParkMode(0x%04x%08x, 0x%04x, 0x%04x) failed, error = %d"), 
					GET_NAP(g_ServerAddress), GET_SAP(g_ServerAddress), TEST_PARK_BEACON_MAX, TEST_PARK_BEACON_MIN,iResult);
				fTestPass = FALSE;
			}
			else
			{
				iResult = BthGetCurrentMode(&g_ServerAddress, &mode);
				if (iResult != ERROR_SUCCESS)
				{
					QAError(TEXT("BthGetCurrentMode failed, error = %d"), iResult);
					fTestPass = FALSE;
				}
				else
				{
					QAMessage(TEXT("Current mode is 0x%02x"), mode);
					QAMessage(TEXT("    0x00 ACTIVE"));
					QAMessage(TEXT("    0x01 HOLD"));
					QAMessage(TEXT("    0x02 SNIFF"));
					QAMessage(TEXT("    0x03 PARK"));
					if (mode != 0x03) // PARK mode
					{
						QAError(TEXT("Current mode is not PARK!"));
						fTestPass = FALSE;
					}
				}

				// Exit SNIFF mode
				Sleep(3000);  // Give hardware some time
				QAMessage(TEXT("Exit PARK mode"));
				iResult = BthExitParkMode(&g_ServerAddress);
				if (iResult != ERROR_SUCCESS)
				{
					QAError(TEXT("BthExitParkMode failed, error = %d"), iResult);
					fTestPass = FALSE;
				}

				iResult = BthGetCurrentMode(&g_ServerAddress, &mode);
				if (iResult != ERROR_SUCCESS)
				{
					QAError(TEXT("BthGetCurrentMode failed, error = %d"), iResult);
					fTestPass = FALSE;
				}
				else
				{
					QAMessage(TEXT("Current mode is 0x%02x"), mode);
					QAMessage(TEXT("    0x00 ACTIVE"));
					QAMessage(TEXT("    0x01 HOLD"));
					QAMessage(TEXT("    0x02 SNIFF"));
					QAMessage(TEXT("    0x03 PARK"));
					if (mode != 0x00) // ACTIVE mode
					{
						QAError(TEXT("Current mode is not ACTIVE!"));
						fTestPass = FALSE;
					}
				}
			}

			QAMessage(TEXT("Terminate ACL connection"));
			iResult = BthCloseConnection(hConnection);
			if (iResult != ERROR_SUCCESS)
			{
				QAError(TEXT("BthCloseConnection failed, error = %d"), iResult);
				fTestPass = FALSE;
			}
		}
	}

	if (fTestPass)
	{
		return TPR_PASS;
	}
	else
	{
		return TPR_FAIL;
	}
}

//
//TestId        : 023
//Title         : Terminate Idle Connection Test
//Description   : BthCreateACLConnection(); BthTerminateIdleConnections(); BthGetCurrentMode(); BthCloseConnection()
//
TESTPROCAPI t023(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	if (!g_fHasServer)
	{
		QAMessage(TEXT("NO server - test case SKIPPED"));
		return TPR_SKIP;
	}

	int iResult;
	BOOL fTestPass = TRUE;
	unsigned short hConnection;
	//unsigned char mode;

	QAMessage(TEXT("Create ACL connection to 0x%04x%08x"), GET_NAP(g_ServerAddress), GET_SAP(g_ServerAddress));
	iResult = BthCreateACLConnectionRetry (&g_ServerAddress, &hConnection, 3);
	
	if (iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("BthCreateACLConnection failed, error = %d"), iResult);
		fTestPass = FALSE;
	}
	else
	{
		QAMessage(TEXT("Wait 15 seconds for connection to be idle"));
		Sleep(15 * 1000); // Wait for connection to be idle
        QAMessage(TEXT("Terminate Idle ACL connections"));
		iResult = BthTerminateIdleConnections();
		if (iResult != ERROR_SUCCESS)
		{
			QAError(TEXT("BthTerminateIdleConnections failed, error = %d"), iResult);
			fTestPass = FALSE;
		}

		// Make sure the connection is gone
		/*
		iResult = BthGetCurrentMode(&g_ServerAddress, &mode);
		if (iResult == ERROR_SUCCESS)
		{
			QAError(TEXT("BthGetCurrentMode succeeded while failure expeced. Connection was not terminated"));
			fTestPass = FALSE;
		}
		*/
		BthCloseConnection(hConnection);
	}

	if (fTestPass)
	{
		return TPR_PASS;
	}
	else
	{
		return TPR_FAIL;
	}
}

//
//TestId        : 024
//Title         : Authenticate Connection Test
//Description   : BthCreateACLConnection(); BthAuthenticate(); BthCloseConnection()
//
TESTPROCAPI t024(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	if (!g_fHasServer)
	{
		QAMessage(TEXT("NO server - test case SKIPPED"));
		return TPR_SKIP;
	}

	// verify that feature is supported

	BtVersionInfo info;
	int iResult = info.ReadLocal();

	if (iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("info.ReadLocal() failed, error = %d"), iResult);
		return TPR_FAIL;
	}

	BOOL Skip = info.EqualOrOlderThan(11, 1, 1300);	// Sharp UART card - broken

	if (Skip)
	{
		QAMessage(TEXT("Feature not supported or or broken in BT module"));
		return TPR_SKIP;
	}

	// run test

	BOOL fTestPass = TRUE;
	unsigned short hConnection;

	QAMessage(TEXT("Create ACL connection to 0x%04x%08x"), GET_NAP(g_ServerAddress), GET_SAP(g_ServerAddress));
	iResult = BthCreateACLConnectionRetry (&g_ServerAddress, &hConnection, 4);
	
	if (iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("BthCreateACLConnection failed, error = %d"), iResult);
		fTestPass = FALSE;
	}
	else
	{
        QAMessage(TEXT("Authenticate Connection"));
		iResult = BthAuthenticate(&g_ServerAddress);
		if (iResult == ERROR_SUCCESS) // Failure expected
		{
			QAError(TEXT("BthAuthenticate did not fail as expected, error = %d"), iResult);
			fTestPass = FALSE;
		}

		QAMessage(TEXT("Terminate ACL connection"));
		iResult = BthCloseConnection(hConnection);
		if (iResult != ERROR_SUCCESS)
		{
			QAWarning(TEXT("BthCloseConnection failed, error = %d"), iResult);
		}
	}

	if (fTestPass)
	{
		return TPR_PASS;
	}
	else
	{
		return TPR_FAIL;
	}
}

//
//TestId        : 025
//Title         : Encrypt Connection Test
//Description   : BthCreateACLConnection(); BthSetEncryption(); BthCloseConnection()
//
TESTPROCAPI t025(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	if (!g_fHasServer)
	{
		QAMessage(TEXT("NO server - test case SKIPPED"));
		return TPR_SKIP;
	}

	int iResult;
	BOOL fTestPass = TRUE;
	unsigned short hConnection;
	int fOn;

	QAMessage(TEXT("Create ACL connection to 0x%04x%08x"), GET_NAP(g_ServerAddress), GET_SAP(g_ServerAddress));
	iResult = BthCreateACLConnectionRetry (&g_ServerAddress, &hConnection, 4);
	
	if (iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("BthCreateACLConnection failed, error = %d"), iResult);
		fTestPass = FALSE;
	}
	else
	{
		fOn = TRUE;
        QAMessage(TEXT("Encrypt Connection"));
		iResult = BthSetEncryption(&g_ServerAddress, fOn);
		if (iResult == ERROR_SUCCESS) // Failure expected
		{
			QAError(TEXT("BthSetEncryption did not return ERROR_CONNECTION_REFUSED as expected, error = %d"), iResult);
			fTestPass = FALSE;
		}
		else
		{
			fOn = FALSE;
			QAMessage(TEXT("Stop Encrypt Connection"));
			iResult = BthSetEncryption(&g_ServerAddress, fOn);
			if (iResult != ERROR_SUCCESS && iResult != ERROR_NO_USER_SESSION_KEY)
			{
				QAError(TEXT("BthSetEncryption failed, error = %d"), iResult);
				fTestPass = FALSE;
			}
		}

		QAMessage(TEXT("Terminate ACL connection"));
		iResult = BthCloseConnection(hConnection);
		if (iResult != ERROR_SUCCESS)
		{
			QAWarning(TEXT("BthCloseConnection failed, error = %d"), iResult);
		}
	}

	if (fTestPass)
	{
		return TPR_PASS;
	}
	else
	{
		return TPR_FAIL;
	}
}

//
//TestId        : 026
//Title         : SCO Connection Test
//Description   : BthCreateSCOConnection(); BthCloseConnection()
//
TESTPROCAPI t026(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	if (!g_fHasServer)
	{
		QAMessage(TEXT("NO server - test case SKIPPED"));
		return TPR_SKIP;
	}

	// verify that feature is supported

	BtVersionInfo info;
	int iResult = info.ReadLocal();

	if (iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("info.ReadLocal() failed, error = %d"), iResult);
		return TPR_FAIL;
	}
	else if (! info.ScoSupported())
	{
		QAMessage(TEXT("Feature not supported by BT module"));
		return TPR_SKIP;
	}

	// run test

	BOOL found = FALSE;
	BOOL fTestPass = TRUE;
	unsigned short hACLConnection, hSCOConnection;

	//make sure that we're disconnected first
	iResult = GetConnectionHandle(&g_ServerAddress, &hACLConnection, BTH_LINK_TYPE_ACL, &found);

	if (iResult == ERROR_SUCCESS && found)
		BthCloseConnection(hACLConnection);

	QAMessage(TEXT("Create ACL connection to 0x%04x%08x"), GET_NAP(g_ServerAddress), GET_SAP(g_ServerAddress));
	iResult = BthCreateACLConnectionRetry (&g_ServerAddress, &hACLConnection, 4);

	if (iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("BthCreateACLConnection failed, error = %d"), iResult);
		fTestPass = FALSE;
	}
	else
	{
		QAMessage(TEXT("Create SCO connection to 0x%04x%08x"), GET_NAP(g_ServerAddress), GET_SAP(g_ServerAddress));
		iResult = BthCreateSCOConnectionRetry (&g_ServerAddress, &hSCOConnection, 3);
		
		if (iResult != ERROR_CONNECTION_REFUSED &&	// Depends on the setting of server (SYSGEN_AUDIO)
			iResult != ERROR_SUCCESS	&&				// SCO can be accepted or rejectted
			iResult != ERROR_GRACEFUL_DISCONNECT)    // can be returned as a reason by HCI				
		{
			QAError(TEXT("BthCreateSCOConnection did not success or return ERROR_CONNECTION_REFUSED as expected, error = %d"), iResult);
			fTestPass = FALSE;
		}
		else
		{
			QAMessage(TEXT("BthCreateSCOConnection acted as expected"));
		}

		BthCloseConnection(hSCOConnection);

		QAMessage(TEXT("Terminate ACL connection"));
		iResult = BthCloseConnection(hACLConnection);
		
		if (iResult != ERROR_SUCCESS && iResult != ERROR_DEVICE_NOT_CONNECTED)
		{
			QAError(TEXT("BthCloseConnection failed, error = %d"), iResult);
			fTestPass = FALSE;
		}
	}

	if (fTestPass)
	{
		return TPR_PASS;
	}
	else
	{
		return TPR_FAIL;
	}
}

#define MAX_SDPRECSIZE 4096

//
//TestId        : 027
//Title         : Set Service Test
//Description   : BthNsSetService()
//
TESTPROCAPI t027(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	int iResult;
	BOOL fTestPass = TRUE;
	WSAQUERYSET service;
	UCHAR * buffer = new UCHAR[MAX_SDPRECSIZE];
	BLOB blob;
	PBTHNS_SETBLOB updateBlob = (PBTHNS_SETBLOB) buffer;
	UCHAR sdpRecord[] = {	0x35, 0x64, 0x09, 0x00, 0x01, 0x35, 0x06, 0x19,
							0x11, 0x05, 0x19, 0x77, 0x77, 0x09, 0x00, 0x03,
							0x19, 0x77, 0x77, 0x09, 0x00, 0x04, 0x35, 0x15,
							0x35, 0x09, 0x19, 0x01, 0x00, 0x09, 0x00, 0x03,
							0x09, 0x00, 0x01, 0x35, 0x08, 0x19, 0x00, 0x03,
							0x08, 0x18, 0x09, 0x00, 0x01, 0x09, 0x00, 0x05,
							0x35, 0x03, 0x19, 0x10, 0x02, 0x09, 0x00, 0x0a,
							0x45, 0x00, 0x09, 0x00, 0x0b, 0x45, 0x00, 0x09,
							0x00, 0x0c, 0x45, 0x00, 0x09, 0x01, 0x00, 0x25,
							0x18, 0x53, 0x44, 0x50, 0x20, 0x54, 0x65, 0x73,
							0x74, 0x20, 0x53, 0x65, 0x72, 0x76, 0x69, 0x63,
							0x65, 0x20, 0x52, 0x65, 0x63, 0x6f, 0x72, 0x64,
							0x0a, 0x09, 0x01, 0x01, 0x25, 0x00};
	ULONG hRecord = 0;
	ULONG ulSdpVersion = BTH_SDP_VERSION;

	blob.pBlobData = (PBYTE) updateBlob;
	updateBlob->pRecordHandle  = &hRecord;
    updateBlob->pSdpVersion	   = &ulSdpVersion;
	updateBlob->fSecurity      = 0;
	updateBlob->fOptions       = 0;
	updateBlob->ulRecordLength = sizeof(sdpRecord);
	memcpy(updateBlob->pRecord, &sdpRecord[0], sizeof(sdpRecord));
	blob.cbSize    = sizeof(BTHNS_SETBLOB) + updateBlob->ulRecordLength - 1;

	memset(&service, 0, sizeof(service));
	service.dwSize = sizeof(service);
	service.lpBlob = &blob;
	service.dwNameSpace = NS_BTH;

	QAMessage(TEXT("Create SDP record"));
	iResult = BthNsSetService(&service, RNRSERVICE_REGISTER, 0);
	if (iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("BthNsSetService(RNRSERVICE_REGISTER) failed, error = %d"), iResult);
		fTestPass = FALSE;
	}
	else
	{
		QAMessage(TEXT("Remove SDP record, handle = 0x%08x"), hRecord);
		iResult = BthNsSetService(&service, RNRSERVICE_DELETE, 0);
		if (iResult != ERROR_SUCCESS)
		{
			QAError(TEXT("BthNsSetService(RNRSERVICE_DELETE) failed, error = %d"), iResult);
			fTestPass = FALSE;
		}
	}

	delete[] buffer;

	if (fTestPass)
	{
		return TPR_PASS;
	}
	else
	{
		return TPR_FAIL;
	}
}

//
//TestId        : 028
//Title         : Local ServiceSearch/ServiceAttribute Test
//Description   : BthNsLookupServiceBegin(); BthNsLookupServiceNext(); BthNsLookupServiceEnd()
//
TESTPROCAPI t028(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	if (!g_fHasServer)
	{
		QAMessage(TEXT("NO server - test case SKIPPED"));
		return TPR_SKIP;
	}

	int iResult;
	BOOL fTestPass = TRUE;
	WSAQUERYSET service;
	UCHAR * buffer = new UCHAR[MAX_SDPRECSIZE];
	BLOB blob;
	BTHNS_RESTRICTIONBLOB RBlob;
	SOCKADDR_BTH sa;
	CSADDR_INFO	csai;
	LPWSAQUERYSET pwsaResults = (LPWSAQUERYSET) buffer;
	DWORD dwSize  = MAX_SDPRECSIZE; // size of mem pointed by buffer
	HANDLE hLookup;
	unsigned long *pHandles;
	unsigned long ulReturnedHandles;

	memset (&RBlob, 0, sizeof(RBlob));
	RBlob.type = SDP_SERVICE_SEARCH_REQUEST;
	RBlob.uuids[0].uuidType = SDP_ST_UUID16;
	RBlob.uuids[0].u.uuid16 = SDP_PROTOCOL_UUID16;
	blob.cbSize = sizeof(RBlob);
	blob.pBlobData = (BYTE *)&RBlob;

	memset (&sa, 0, sizeof(sa));
	*(BT_ADDR *)(&sa.btAddr) = 0;
	sa.addressFamily = AF_BT;
	memset (&csai, 0, sizeof(csai));
	csai.RemoteAddr.lpSockaddr = (sockaddr *)&sa;
	csai.RemoteAddr.iSockaddrLength = sizeof(sa);

	memset(&service, 0, sizeof(service));
	service.dwSize = sizeof(service);
	service.lpBlob = &blob;
	service.dwNameSpace = NS_BTH;
	service.lpcsaBuffer = &csai;

	QAMessage(TEXT("ServiceSearchRequest"));
	iResult = BthNsLookupServiceBegin(&service, LUP_RES_SERVICE, &hLookup);
	if (iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("BthNsLookupServiceBegin failed, error = %d"), iResult);
		fTestPass = FALSE;
	}
	else
	{
		memset(pwsaResults, 0, sizeof(WSAQUERYSET));
		pwsaResults->dwSize = sizeof(WSAQUERYSET);
		pwsaResults->dwNameSpace = NS_BTH;
		pwsaResults->lpBlob = NULL;

		iResult = BthNsLookupServiceNext(hLookup, 0, &dwSize, pwsaResults);
		if (iResult != ERROR_SUCCESS)
		{
			QAError(TEXT("BthNsLookupServiceNext failed, error = %d"), iResult);
			fTestPass = FALSE;
		}
		else
		{
			ulReturnedHandles = (pwsaResults->lpBlob->cbSize) / sizeof(DWORD);
			pHandles = (unsigned long *) pwsaResults->lpBlob->pBlobData;
			if (ulReturnedHandles == 0)
			{
				QAError(TEXT("No SDP record found for SDP service"));
				fTestPass = FALSE;
			}
		}

		iResult = BthNsLookupServiceEnd(hLookup);
		if (iResult != ERROR_SUCCESS)
		{
			QAError(TEXT("BthNsLookupServiceEnd failed, error = %d"), iResult);
			fTestPass = FALSE;
		}
	}

	if (fTestPass)
	{
		memset (&RBlob, 0, sizeof(RBlob));
		RBlob.type = SDP_SERVICE_ATTRIBUTE_REQUEST;
		RBlob.serviceHandle = pHandles[0];
		RBlob.numRange = 1;
		RBlob.pRange[0].minAttribute = SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST;
		RBlob.pRange[0].maxAttribute = SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST;
		blob.cbSize = sizeof(RBlob);
		blob.pBlobData = (BYTE *)&RBlob;

		memset (&sa, 0, sizeof(sa));
		*(BT_ADDR *)(&sa.btAddr) = g_ServerAddress;
		sa.addressFamily = AF_BT;
		memset (&csai, 0, sizeof(csai));
		csai.RemoteAddr.lpSockaddr = (sockaddr *)&sa;
		csai.RemoteAddr.iSockaddrLength = sizeof(sa);

		memset(&service, 0, sizeof(service));
		service.dwSize = sizeof(service);
		service.lpBlob = &blob;
		service.dwNameSpace = NS_BTH;
		service.lpcsaBuffer = &csai;

		QAMessage(TEXT("ServiceAttributeRequest"));
		iResult = BthNsLookupServiceBegin(&service, 0, &hLookup);
		if (iResult != ERROR_SUCCESS)
		{
			QAError(TEXT("BthNsLookupServiceBegin failed, error = %d"), iResult);
			fTestPass = FALSE;
		}
		else
		{
			memset(pwsaResults, 0, sizeof(WSAQUERYSET));
			pwsaResults->dwSize = sizeof(WSAQUERYSET);
			pwsaResults->dwNameSpace = NS_BTH;
			pwsaResults->lpBlob = NULL;
			dwSize  = MAX_SDPRECSIZE; // size of mem pointed by buffer

			iResult = BthNsLookupServiceNext(hLookup, 0, &dwSize, pwsaResults);
			if (iResult != ERROR_SUCCESS)
			{
				QAError(TEXT("BthNsLookupServiceNext failed, error = %d"), iResult);
				fTestPass = FALSE;
			}

			iResult = BthNsLookupServiceEnd(hLookup);
			if (iResult != ERROR_SUCCESS)
			{
				QAError(TEXT("BthNsLookupServiceEnd failed, error = %d"), iResult);
				fTestPass = FALSE;
			}
		}
	}

	delete[] buffer;

	if (fTestPass)
	{
		return TPR_PASS;
	}
	else
	{
		return TPR_FAIL;
	}
}

//
//TestId        : 029
//Title         : Remote ServiceSearch/ServiceAttribute Test
//Description   : BthNsLookupServiceBegin(); BthNsLookupServiceNext(); BthNsLookupServiceEnd()
//
TESTPROCAPI t029(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	if (!g_fHasServer)
	{
		QAMessage(TEXT("NO server - test case SKIPPED"));
		return TPR_SKIP;
	}

	int iResult;
	BOOL fTestPass = TRUE;
	WSAQUERYSET service;
	UCHAR * buffer = new UCHAR[MAX_SDPRECSIZE];
	BLOB blob;
	BTHNS_RESTRICTIONBLOB RBlob;
	SOCKADDR_BTH sa;
	CSADDR_INFO	csai;
	LPWSAQUERYSET pwsaResults = (LPWSAQUERYSET) buffer;
	DWORD dwSize  = MAX_SDPRECSIZE; // size of mem pointed by buffer
	HANDLE hLookup;
	unsigned long *pHandles;
	unsigned long ulReturnedHandles;

	memset (&RBlob, 0, sizeof(RBlob));
	RBlob.type = SDP_SERVICE_SEARCH_REQUEST;
	RBlob.uuids[0].uuidType = SDP_ST_UUID16;
	RBlob.uuids[0].u.uuid16 = SDP_PROTOCOL_UUID16;
	blob.cbSize = sizeof(RBlob);
	blob.pBlobData = (BYTE *)&RBlob;

	memset (&sa, 0, sizeof(sa));
	*(BT_ADDR *)(&sa.btAddr) = g_ServerAddress;
	sa.addressFamily = AF_BT;
	memset (&csai, 0, sizeof(csai));
	csai.RemoteAddr.lpSockaddr = (sockaddr *)&sa;
	csai.RemoteAddr.iSockaddrLength = sizeof(sa);

	memset(&service, 0, sizeof(service));
	service.dwSize = sizeof(service);
	service.lpBlob = &blob;
	service.dwNameSpace = NS_BTH;
	service.lpcsaBuffer = &csai;

	QAMessage(TEXT("ServiceSearchRequest"));
	iResult = BthNsLookupServiceBegin(&service, 0, &hLookup);
	if (iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("BthNsLookupServiceBegin failed, error = %d"), iResult);
		fTestPass = FALSE;
	}
	else
	{
		memset(pwsaResults, 0, sizeof(WSAQUERYSET));
		pwsaResults->dwSize = sizeof(WSAQUERYSET);
		pwsaResults->dwNameSpace = NS_BTH;
		pwsaResults->lpBlob = NULL;

		iResult = BthNsLookupServiceNext(hLookup, 0, &dwSize, pwsaResults);
		if (iResult != ERROR_SUCCESS)
		{
			QAError(TEXT("BthNsLookupServiceNext failed, error = %d"), iResult);
			fTestPass = FALSE;
		}
		else
		{
			ulReturnedHandles = (pwsaResults->lpBlob->cbSize) / sizeof(DWORD);
			pHandles = (unsigned long *) pwsaResults->lpBlob->pBlobData;
			if (ulReturnedHandles == 0)
			{
				QAError(TEXT("No SDP record found for SDP service"));
				fTestPass = FALSE;
			}
		}

		iResult = BthNsLookupServiceEnd(hLookup);
		if (iResult != ERROR_SUCCESS)
		{
			QAError(TEXT("BthNsLookupServiceEnd failed, error = %d"), iResult);
			fTestPass = FALSE;
		}
	}

	if (fTestPass)
	{
		memset (&RBlob, 0, sizeof(RBlob));
		RBlob.type = SDP_SERVICE_ATTRIBUTE_REQUEST;
		RBlob.serviceHandle = pHandles[0];
		RBlob.numRange = 1;
		RBlob.pRange[0].minAttribute = SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST;
		RBlob.pRange[0].maxAttribute = SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST;
		blob.cbSize = sizeof(RBlob);
		blob.pBlobData = (BYTE *)&RBlob;

		memset (&sa, 0, sizeof(sa));
		*(BT_ADDR *)(&sa.btAddr) = g_ServerAddress;
		sa.addressFamily = AF_BT;
		memset (&csai, 0, sizeof(csai));
		csai.RemoteAddr.lpSockaddr = (sockaddr *)&sa;
		csai.RemoteAddr.iSockaddrLength = sizeof(sa);

		memset(&service, 0, sizeof(service));
		service.dwSize = sizeof(service);
		service.lpBlob = &blob;
		service.dwNameSpace = NS_BTH;
		service.lpcsaBuffer = &csai;

		QAMessage(TEXT("ServiceAttributeRequest"));
		iResult = BthNsLookupServiceBegin(&service, 0, &hLookup);
		if (iResult != ERROR_SUCCESS)
		{
			QAError(TEXT("BthNsLookupServiceBegin failed, error = %d"), iResult);
			fTestPass = FALSE;
		}
		else
		{
			memset(pwsaResults, 0, sizeof(WSAQUERYSET));
			pwsaResults->dwSize = sizeof(WSAQUERYSET);
			pwsaResults->dwNameSpace = NS_BTH;
			pwsaResults->lpBlob = NULL;
			dwSize  = MAX_SDPRECSIZE; // size of mem pointed by buffer

			iResult = BthNsLookupServiceNext(hLookup, 0, &dwSize, pwsaResults);
			if (iResult != ERROR_SUCCESS)
			{
				QAError(TEXT("BthNsLookupServiceNext failed, error = %d"), iResult);
				fTestPass = FALSE;
			}

			iResult = BthNsLookupServiceEnd(hLookup);
			if (iResult != ERROR_SUCCESS)
			{
				QAError(TEXT("BthNsLookupServiceEnd failed, error = %d"), iResult);
				fTestPass = FALSE;
			}
		}
	}

	delete[] buffer;

	if (fTestPass)
	{
		return TPR_PASS;
	}
	else
	{
		return TPR_FAIL;
	}
}

//
//TestId        : 030
//Title         : ServiceSearchAttribute Test
//Description   : BthNsLookupServiceBegin(); BthNsLookupServiceNext(); BthNsLookupServiceEnd()
//
TESTPROCAPI t030(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	if (!g_fHasServer)
	{
		QAMessage(TEXT("NO server - test case SKIPPED"));
		return TPR_SKIP;
	}

	int iResult;
	BOOL fTestPass = TRUE;
	WSAQUERYSET service;
	UCHAR * buffer = new UCHAR[MAX_SDPRECSIZE];
	BLOB blob;
	BTHNS_RESTRICTIONBLOB RBlob;
	SOCKADDR_BTH sa;
	CSADDR_INFO	csai;
	LPWSAQUERYSET pwsaResults = (LPWSAQUERYSET) buffer;
	DWORD dwSize  = MAX_SDPRECSIZE; // size of mem pointed by buffer
	HANDLE hLookup;

	memset (&RBlob, 0, sizeof(RBlob));
	RBlob.type = SDP_SERVICE_SEARCH_ATTRIBUTE_REQUEST;
	RBlob.numRange = 1;
	RBlob.pRange[0].minAttribute = SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST;
	RBlob.pRange[0].maxAttribute = SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST;
	RBlob.uuids[0].uuidType = SDP_ST_UUID16;
	RBlob.uuids[0].u.uuid16 = SDP_PROTOCOL_UUID16;
	blob.cbSize = sizeof(RBlob);
	blob.pBlobData = (BYTE *)&RBlob;

	memset (&sa, 0, sizeof(sa));
	*(BT_ADDR *)(&sa.btAddr) = 0;
	sa.addressFamily = AF_BT;
	memset (&csai, 0, sizeof(csai));
	csai.RemoteAddr.lpSockaddr = (sockaddr *)&sa;
	csai.RemoteAddr.iSockaddrLength = sizeof(sa);

	memset(&service, 0, sizeof(service));
	service.dwSize = sizeof(service);
	service.lpBlob = &blob;
	service.dwNameSpace = NS_BTH;
	service.lpcsaBuffer = &csai;

	QAMessage(TEXT("ServiceSearchAttributeRequest"));
	iResult = BthNsLookupServiceBegin(&service, LUP_RES_SERVICE, &hLookup);
	if (iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("BthNsLookupServiceBegin failed, error = %d"), iResult);
		fTestPass = FALSE;
	}
	else
	{
		memset(pwsaResults, 0, sizeof(WSAQUERYSET));
		pwsaResults->dwSize = sizeof(WSAQUERYSET);
		pwsaResults->dwNameSpace = NS_BTH;
		pwsaResults->lpBlob = NULL;

		iResult = BthNsLookupServiceNext(hLookup, 0, &dwSize, pwsaResults);
		if (iResult != ERROR_SUCCESS)
		{
			QAError(TEXT("BthNsLookupServiceNext failed, error = %d"), iResult);
			fTestPass = FALSE;
		}

		iResult = BthNsLookupServiceEnd(hLookup);
		if (iResult != ERROR_SUCCESS)
		{
			QAError(TEXT("BthNsLookupServiceEnd failed, error = %d"), iResult);
			fTestPass = FALSE;
		}
	}

	delete[] buffer;

	if (fTestPass)
	{
		return TPR_PASS;
	}
	else
	{
		return TPR_FAIL;
	}
}

//
//TestId        : 031
//Title         : ServiceSearchAttribute Test
//Description   : BthNsLookupServiceBegin(); BthNsLookupServiceNext(); BthNsLookupServiceEnd()
//
TESTPROCAPI t031(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	if (!g_fHasServer)
	{
		QAMessage(TEXT("NO server - test case SKIPPED"));
		return TPR_SKIP;
	}

	int iResult;
	BOOL fTestPass = TRUE;
	WSAQUERYSET service;
	UCHAR * buffer = new UCHAR[MAX_SDPRECSIZE];
	BLOB blob;
	BTHNS_RESTRICTIONBLOB RBlob;
	SOCKADDR_BTH sa;
	CSADDR_INFO	csai;
	LPWSAQUERYSET pwsaResults = (LPWSAQUERYSET) buffer;
	DWORD dwSize  = MAX_SDPRECSIZE; // size of mem pointed by buffer
	HANDLE hLookup;

	memset (&RBlob, 0, sizeof(RBlob));
	RBlob.type = SDP_SERVICE_SEARCH_ATTRIBUTE_REQUEST;
	RBlob.numRange = 1;
	RBlob.pRange[0].minAttribute = SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST;
	RBlob.pRange[0].maxAttribute = SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST;
	RBlob.uuids[0].uuidType = SDP_ST_UUID16;
	RBlob.uuids[0].u.uuid16 = SDP_PROTOCOL_UUID16;
	blob.cbSize = sizeof(RBlob);
	blob.pBlobData = (BYTE *)&RBlob;

	memset (&sa, 0, sizeof(sa));
	*(BT_ADDR *)(&sa.btAddr) = g_ServerAddress;
	sa.addressFamily = AF_BT;
	memset (&csai, 0, sizeof(csai));
	csai.RemoteAddr.lpSockaddr = (sockaddr *)&sa;
	csai.RemoteAddr.iSockaddrLength = sizeof(sa);

	memset(&service, 0, sizeof(service));
	service.dwSize = sizeof(service);
	service.lpBlob = &blob;
	service.dwNameSpace = NS_BTH;
	service.lpcsaBuffer = &csai;

	QAMessage(TEXT("ServiceSearchAttributeRequest"));
	iResult = BthNsLookupServiceBegin(&service, 0, &hLookup);
	if (iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("BthNsLookupServiceBegin failed, error = %d"), iResult);
		fTestPass = FALSE;
	}
	else
	{
		memset(pwsaResults, 0, sizeof(WSAQUERYSET));
		pwsaResults->dwSize = sizeof(WSAQUERYSET);
		pwsaResults->dwNameSpace = NS_BTH;
		pwsaResults->lpBlob = NULL;

		iResult = BthNsLookupServiceNext(hLookup, 0, &dwSize, pwsaResults);
		if (iResult != ERROR_SUCCESS)
		{
			QAError(TEXT("BthNsLookupServiceNext failed, error = %d"), iResult);
			fTestPass = FALSE;
		}

		iResult = BthNsLookupServiceEnd(hLookup);
		if (iResult != ERROR_SUCCESS)
		{
			QAError(TEXT("BthNsLookupServiceEnd failed, error = %d"), iResult);
			fTestPass = FALSE;
		}
	}

	delete[] buffer;

	if (fTestPass)
	{
		return TPR_PASS;
	}
	else
	{
		return TPR_FAIL;
	}
}

//
//TestId        : 032
//Title         : Get Remote COD Test
//Description   : BthGetRemoteCOD();
//
TESTPROCAPI t032(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	if (!g_fHasServer)
	{
		QAMessage(TEXT("NO server - test case SKIPPED"));
		return TPR_SKIP;
	}

	int iResult;
	unsigned int COD;
	unsigned short hConnection;
	BOOL fTestPass = TRUE;

	QAMessage(TEXT("Create ACL connection to 0x%04x%08x"), GET_NAP(g_ServerAddress), GET_SAP(g_ServerAddress));
	iResult = BthCreateACLConnectionRetry(&g_ServerAddress, &hConnection, 3);
	
	if (iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("BthCreateACLConnection failed, error = %d"), iResult);
		fTestPass = FALSE;
	}
	else
	{
		iResult = BthGetRemoteCOD(&g_ServerAddress, &COD);
		
		if (iResult != ERROR_SUCCESS)
		{
			QAError(TEXT("BthGetRemoteCOD failed, error = %d"), iResult);
			fTestPass = FALSE;
		}
		else
		{
			QAMessage(TEXT("Remote Bluetooth COD for 0x%04x%08x is: 0x%4.4x"), 
				GET_NAP(g_ServerAddress), GET_SAP(g_ServerAddress), COD);
		}

		QAMessage(TEXT("Terminate ACL connection"));
		iResult = BthCloseConnection(hConnection);
		if (iResult != ERROR_SUCCESS)
		{
			QAError(TEXT("BthCloseConnection failed, error = %d"), iResult);
			fTestPass = FALSE;
		}
	}

	return (fTestPass ? TPR_PASS : TPR_FAIL);

}

//
//TestId        : 033
//Title         : Read RSSI
//Description   : BthReadRSSI();
//
TESTPROCAPI t033(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	if (!g_fHasServer)
	{
		QAMessage(TEXT("NO server - test case SKIPPED"));
		return TPR_SKIP;
	}

	// verify that feature is supported

	BtVersionInfo info;
	int iResult = info.ReadLocal();

	if (iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("info.ReadLocal() failed, error = %d"), iResult);
		return TPR_FAIL;
	}
	else if (! info.RssiSupported())
	{
		QAMessage(TEXT("Feature not supported by BT module"));
		return TPR_SKIP;
	}

	// run test

	unsigned short hConnection;
	BOOL fTestPass = TRUE;

	QAMessage(TEXT("Create ACL connection to 0x%04x%08x"), GET_NAP(g_ServerAddress), GET_SAP(g_ServerAddress));
	iResult = BthCreateACLConnectionRetry(&g_ServerAddress, &hConnection, 3);
	
	if (iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("BthCreateACLConnection failed, error = %d"), iResult);
		fTestPass = FALSE;
	}
	else
	{
		BYTE RSSI = 0;
		
		iResult = BthReadRSSI(&g_ServerAddress, &RSSI);
		
		if (iResult != ERROR_SUCCESS)
		{
			QAError(TEXT("BthReadRSSI failed, error = %d"), iResult);
			fTestPass = FALSE;
		}
		else
		{
			QAMessage(TEXT("RSSI for 0x%04x%08x is: %d"), 
				GET_NAP(g_ServerAddress), GET_SAP(g_ServerAddress), RSSI);
		}

		QAMessage(TEXT("Terminate ACL connection"));
		iResult = BthCloseConnection(hConnection);
		if (iResult != ERROR_SUCCESS)
		{
			QAError(TEXT("BthCloseConnection failed, error = %d"), iResult);
			fTestPass = FALSE;
		}
	}

	return (fTestPass ? TPR_PASS : TPR_FAIL);

}


//
//TestId        : 034
//Title         : GetRole / SwitchRole
//Description   : BthGetRole(), BthSwitchRole;
//
TESTPROCAPI t034(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	if (!g_fHasServer)
	{
		QAMessage(TEXT("NO server - test case SKIPPED"));
		return TPR_SKIP;
	}

	int iResult;
	unsigned short hConnection;
	BOOL fTestPass = TRUE;

	// create an ACL connection to Peer

	QAMessage(TEXT("Create ACL connection to 0x%04x%08x"), GET_NAP(g_ServerAddress), GET_SAP(g_ServerAddress));
	iResult = BthCreateACLConnectionRetry(&g_ServerAddress, &hConnection, 3);
	
	if (iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("BthCreateACLConnection failed, error = %d"), iResult);
		return TPR_FAIL;
	}

	// enable master/slave switch
	QAMessage(TEXT("BthWriteLinkPolicySettings(0x%04x%08x, 0x0001)"), GET_NAP(g_ServerAddress), GET_SAP(g_ServerAddress));
	iResult = BthWriteLinkPolicySettings(&g_ServerAddress, 0x0001);
	if (iResult != ERROR_SUCCESS)
	{
		QAMessage(TEXT("BthWriteLinkPolicySettings(0x%04x%08x, 0x0001) failed to enable master/slave switch, error = %d"), GET_NAP(g_ServerAddress), GET_SAP(g_ServerAddress), iResult);
	}

	USHORT usRole = 0;

	Sleep (1000);

	// verify the APIs
	iResult = BthGetRole(&g_ServerAddress, &usRole);
	if ( iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("BthGetRole failed, error = %d"), iResult);
		return TPR_FAIL;
	}

	QAMessage(TEXT("BthGetRole() returned Role = %d"), usRole);

	USHORT usNewRole = (usRole == 1) ? 0 : 1; // select new role
	
	iResult = BthSwitchRole(&g_ServerAddress, usNewRole);
	if ( iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("BthSwitchRole failed, error = %d"), iResult);
		return TPR_FAIL;
	}

	iResult = BthGetRole(&g_ServerAddress, &usRole);
	if ( iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("BthGetRole failed, error = %d"), iResult);
		return TPR_FAIL;
	}

	QAMessage (TEXT("BthGetRole() return %d"), usRole);

	fTestPass = (usNewRole == usRole) ? TRUE : FALSE;

	// terminate connection

	QAMessage(TEXT("Terminate ACL connection"));
	iResult = BthCloseConnection(hConnection);
	if (iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("BthCloseConnection failed, error = %d"), iResult);
		fTestPass = FALSE;
	}

	return (fTestPass ? TPR_PASS : TPR_FAIL);

}


typedef struct _READER_THREAD_PARAMS
{
	HANDLE hReaderReadyEvent;		// signaled by reader when ready
	HANDLE hExitReaderEvent;		// signaled by main 
	BT_ADDR remote_bt;				// remote BT address used during the tests
	BOOL fTestPass;					// test result

	_READER_THREAD_PARAMS()
	{
		fTestPass = TRUE;
		hReaderReadyEvent = CreateEvent (NULL, FALSE, FALSE, NULL);
		hExitReaderEvent = CreateEvent (NULL, FALSE, FALSE, NULL);
		ASSERT(hReaderReadyEvent);
		ASSERT(hExitReaderEvent);
	};

	~_READER_THREAD_PARAMS()
	{
		CloseHandle(hReaderReadyEvent);
		CloseHandle(hExitReaderEvent);
	};
} READER_THREAD_PARAMS, *PREADER_THREAD_PARAMS;


DWORD WINAPI NotificationReaderThread (LPVOID lpParameter)
{
	MSGQUEUEOPTIONS mqOptions;
	memset (&mqOptions, 0, sizeof(mqOptions));

	mqOptions.dwFlags = 0;
	mqOptions.dwSize = sizeof(mqOptions);
	mqOptions.dwMaxMessages = 10; 
	mqOptions.cbMaxMessage = sizeof(BTEVENT);
	mqOptions.bReadAccess = TRUE;

	// Create message queue to receive BT events on
	HANDLE hMsgQ = CreateMsgQueue(NULL, &mqOptions);
	if (! hMsgQ) 
	{
		QAError(TEXT("NotificationReaderThread: Error creating message queue."));
		goto exit;
	}

	// Listen for all Bluetooth notifications
	HANDLE hBTNotif = RequestBluetoothNotifications(
						BTE_CLASS_CONNECTIONS | BTE_CLASS_DEVICE | BTE_CLASS_PAIRING | BTE_CLASS_STACK, 
						hMsgQ);
	if (! hBTNotif) 
	{
		QAError(TEXT("NotificationReaderThread: Error in call to RequestBluetoothNotifications."));
		goto exit;
	}

	PREADER_THREAD_PARAMS pParams = (PREADER_THREAD_PARAMS) lpParameter;
	SetEvent (pParams -> hReaderReadyEvent);

	QAMessage(TEXT("NotificationReaderThread: Waiting for Bluetooth notifications..."));

	BOOL bDone = FALSE;

	HANDLE events[2];
	events[0] = hMsgQ;
	events[1] = pParams->hExitReaderEvent;

	while (! bDone) 
	{		
		// Wait on the Message Queue handle (not hBtNotif)
		DWORD dwWait = WaitForMultipleObjects ((sizeof(events)/sizeof(events[0])), events, FALSE, INFINITE);
		
		if (WAIT_OBJECT_0 == dwWait) 
		{
			//
			// We have got a Bluetooth event!
			//

			BTEVENT btEvent;
			DWORD dwFlags = 0;
			DWORD dwBytesRead = 0;

			BOOL fRet = ReadMsgQueue (hMsgQ, &btEvent, sizeof(BTEVENT), &dwBytesRead, 10, &dwFlags);
			if (! fRet) 
			{
				QAMessage(TEXT("NotificationReaderThread: ReadMsgQueue() failed. GetLastError() = %d"), GetLastError());
				goto exit;
			} 

			//QAMessage (TEXT("NOTIFIED: Got event with id=%d."), btEvent.dwEventId);
			
			switch (btEvent.dwEventId) 
			{
				case BTE_CONNECTION: 
				{
					BT_CONNECT_EVENT* pbte = (BT_CONNECT_EVENT*) btEvent.baEventData;
					
					QAMessage(L"NOTIFIED (%d): Connection Event: handle=%d, bta=%04x%08x, Link Type=%s, Encryption Mode=%d",
						btEvent.dwEventId,
						pbte->hConnection, 
						GET_NAP(pbte->bta), GET_SAP(pbte->bta), 
						pbte->ucLinkType ? L"ACL" : L"SCO", 
						pbte->ucEncryptMode);

					if (pbte->ucLinkType != 0 && pbte->ucLinkType != 1)
					{
						QAError(TEXT("Unknown link type: %d"), pbte->ucLinkType);
						pParams->fTestPass = FALSE;
					}

					if ((GET_NAP(pbte->bta) != GET_NAP(pParams->remote_bt)) || (GET_SAP(pbte->bta) != GET_SAP(pParams->remote_bt)))
					{
						QAError (TEXT("The following BT address was expected: %04x%08x"),
							GET_NAP(pParams->remote_bt), GET_SAP(pParams->remote_bt));
						pParams->fTestPass = FALSE;
					}

				}
				break;

				case BTE_DISCONNECTION: {
					BT_DISCONNECT_EVENT* pbte = (BT_DISCONNECT_EVENT*) btEvent.baEventData;

					QAMessage(L"NOTIFIED (%d): Disconnection Event: handle=%d, Reason=%d",
						btEvent.dwEventId,
						pbte->hConnection,
						pbte->ucReason);

					if (pbte->ucReason > 0x35)	// current max for 1.2
					{
						QAError(TEXT("Invalid disconnect reason: %d"), pbte->ucReason);
						pParams->fTestPass = FALSE;
					}
				}
				break;

				case BTE_ROLE_SWITCH: {
					BT_ROLE_SWITCH_EVENT* pbte = (BT_ROLE_SWITCH_EVENT*) btEvent.baEventData;

					QAMessage(L"NOTIFIED (%d): Role Switch Event: bta=%04x%08x, Role=%s",
						btEvent.dwEventId,
						GET_NAP(pbte->bta), GET_SAP(pbte->bta), 
						pbte->fRole ? L"Slave" : L"Master");

					if (pbte->fRole != 0 && pbte->fRole != 1)
					{
						QAError(TEXT("Unknown Role: %d"), pbte->fRole);
						pParams->fTestPass = FALSE;
					}

					if ((GET_NAP(pbte->bta) != GET_NAP(pParams->remote_bt)) || (GET_SAP(pbte->bta) != GET_SAP(pParams->remote_bt)))
					{
						QAError (TEXT("The following BT address was expected: %04x%08x"),
							GET_NAP(pParams->remote_bt), GET_SAP(pParams->remote_bt));
						pParams->fTestPass = FALSE;
					}
				}
				break;

				case BTE_MODE_CHANGE: {
					BT_MODE_CHANGE_EVENT* pbte = (BT_MODE_CHANGE_EVENT*) btEvent.baEventData;

					TCHAR mode[64];

					if (pbte->bMode == 0x00)			
						_snwprintf (mode, ARRAY_COUNT(mode), TEXT("Mode=Active"));
					else if (pbte->bMode == 0x01)
						_snwprintf (mode, ARRAY_COUNT(mode), TEXT("Mode=Hold"));
					else if (pbte->bMode == 0x02)
						_snwprintf (mode, ARRAY_COUNT(mode), TEXT("Mode=Sniff"));
					else if (pbte->bMode == 0x03)
						_snwprintf (mode, ARRAY_COUNT(mode), TEXT("Mode=Park"));
					else
					{
						QAError (TEXT("Unknown mode: %d"), pbte->bMode);
						pParams->fTestPass = FALSE;
						_snwprintf (mode, ARRAY_COUNT(mode), TEXT("Mode=Unknown"));
					}

					QAMessage(L"NOTIFIED (%d): Mode Change Event: handle=%d, bta=%04x%08x, %s, Interval=%d", 
						btEvent.dwEventId,
						pbte->hConnection,
						GET_NAP(pbte->bta), GET_SAP(pbte->bta),
						mode,
						pbte->usInterval);

					if ((GET_NAP(pbte->bta) != GET_NAP(pParams->remote_bt)) || (GET_SAP(pbte->bta) != GET_SAP(pParams->remote_bt)))
					{
						QAError (TEXT("The following BT address was expected: %04x%08x"),
							GET_NAP(pParams->remote_bt), GET_SAP(pParams->remote_bt));
						pParams->fTestPass = FALSE;
					}
		
				}
				break;

				case BTE_PAGE_TIMEOUT: {
					QAMessage(L"NOTIFIED (%d): Page Timeout Changed Event", btEvent.dwEventId);
				}
				break;

				case BTE_KEY_NOTIFY: {
					BT_LINK_KEY_EVENT* pbte = (BT_LINK_KEY_EVENT*) btEvent.baEventData;
					
					QAMessage(L"NOTIFIED (%d): Link Key Notify Event: bta=%04x%08x, Key Type=%d", 
						btEvent.dwEventId,
						GET_NAP(pbte->bta), GET_SAP(pbte->bta),
						pbte->key_type);

					if (pbte->key_type > 2)
					{
						QAError(TEXT("Invalid Key Type: %d"), pbte->key_type);
						pParams->fTestPass = FALSE;
					}

					if ((GET_NAP(pbte->bta) != GET_NAP(pParams->remote_bt)) || (GET_SAP(pbte->bta) != GET_SAP(pParams->remote_bt)))
					{
						QAError (TEXT("The following BT address was expected: %04x%08x"),
							GET_NAP(pParams->remote_bt), GET_SAP(pParams->remote_bt));
						pParams->fTestPass = FALSE;
					}
				}
				break;

				case BTE_KEY_REVOKED: {
					BT_LINK_KEY_EVENT* pbte = (BT_LINK_KEY_EVENT*) btEvent.baEventData;
					
					QAMessage(L"NOTIFIED (%d): Link Key Revoked Event: bta=%04x%08x",
						btEvent.dwEventId,
						GET_NAP(pbte->bta), GET_SAP(pbte->bta));

					if ((GET_NAP(pbte->bta) != GET_NAP(pParams->remote_bt)) || (GET_SAP(pbte->bta) != GET_SAP(pParams->remote_bt)))
					{
						QAError (TEXT("The following BT address was expected: %04x%08x"),
							GET_NAP(pParams->remote_bt), GET_SAP(pParams->remote_bt));
						pParams->fTestPass = FALSE;
					}
				}
				break;

				case BTE_LOCAL_NAME: {
					QAMessage(L"NOTIFIED (%d): Name Changed Event", btEvent.dwEventId);
				}
				break;

				case BTE_COD: {
					QAMessage(L"NOTIFIED (%d): COD Changed Event", btEvent.dwEventId);
				}
				break;

				case BTE_STACK_UP: {
					QAMessage(L"NOTIFIED (%d): Stack Up Event", btEvent.dwEventId);
				}
				break;

				case BTE_STACK_DOWN: {
					QAMessage(L"NOTIFIED (%d): Stack Down Event", btEvent.dwEventId);
				}
				break;

				default:
					QAMessage(L"NOTIFIED: Unknown Event: %d", btEvent.dwEventId);
					break;
			}
		}
		else if (WAIT_OBJECT_0 + 1 == dwWait)
		{
			QAMessage(TEXT("NotificationReaderThread: Exit thread requested"));
			break;
		}
		else 
		{
			QAMessage(TEXT("NotificationReaderThread: Error - Unexpected return value from WaitForSingleObject!"));
			goto exit;
		}

	}

exit:

	// Stop listening for Bluetooth notifications
	if (hBTNotif && (! StopBluetoothNotifications(hBTNotif))) 
	{
		QAError(L"StopBluetoothNotifications returned FALSE.\r\n");
		pParams->fTestPass = FALSE;
	}

	if (hMsgQ)
		CloseMsgQueue(hMsgQ);

	return 0;

}

//
//TestId        : 035
//Title         : RequestBluetoothNotifications /StopBluetoothNotifications
//Description   : RequestBluetoothNotifications /StopBluetoothNotifications;
//
TESTPROCAPI t035(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	if (!g_fHasServer)
	{
		QAMessage(TEXT("NO server - test case SKIPPED"));
		return TPR_SKIP;
	}

	// get the HW version

	BtVersionInfo hwInfo;
	int iResult = hwInfo.ReadLocal();

	if (iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("info.ReadLocal() failed, error = %d"), iResult);
		return TPR_FAIL;
	}

	// run the test

	BOOL fTestPass = TRUE;
	ce::auto_hfile hDev; // BDT0: handle

	// Create the notification reader thread and wait for it to be ready
	READER_THREAD_PARAMS ThreadParams;
	ThreadParams.remote_bt = g_ServerAddress;
	
	ce::auto_handle hThread = CreateThread(NULL, 0, NotificationReaderThread, &ThreadParams, 0, NULL);

	if (!hThread)
	{
		QAError(TEXT("CreateThread() failed for NotificationReaderThread"));
		return TPR_FAIL;
	}
	
	WaitForSingleObject(ThreadParams.hReaderReadyEvent, INFINITE);

	// Connect ACL

	QAMessage(TEXT("Create ACL connection"));
	unsigned short hConnection;
	iResult = BthCreateACLConnectionRetry(&g_ServerAddress, &hConnection, 3);

	if (iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("BthCreateACLConnection failed, error = %d"), iResult);
		fTestPass = FALSE;
		goto exit;
	}

	// enable sniff mode and master/slave switch
	QAMessage(TEXT("BthWriteLinkPolicySettings(0x%04x%08x, 0x0005)"), GET_NAP(g_ServerAddress), GET_SAP(g_ServerAddress));
	iResult = BthWriteLinkPolicySettings(&g_ServerAddress, 0x0005);
	if (iResult != ERROR_SUCCESS)
	{
		QAMessage(TEXT("BthWriteLinkPolicySettings(0x%04x%08x, 0x0005) failed, error = %d"), 
			GET_NAP(g_ServerAddress), GET_SAP(g_ServerAddress), iResult);
	}
	else
	{
		BOOL skipSniff = hwInfo.EqualOrOlderThan(11, 1, 1300); // Sharp UART
		
		if (! skipSniff)
		{
			// Enter Sniff mode
			QAMessage(TEXT("Enter SNIFF mode"));
			unsigned short interval;
			iResult = BthEnterSniffMode(&g_ServerAddress, TEST_SNIFF_MAX, TEST_SNIFF_MIN, TEST_SNIFF_ATTEMPT, TEST_SNIFF_TO, &interval);
			if (iResult != ERROR_SUCCESS)
			{
				QAError(TEXT("BthEnterSniffMode(0x%04x%08x, 0x%04x, 0x%04x, 0x%04x, 0x%04x) failed, error = %d"), 
					GET_NAP(g_ServerAddress), GET_SAP(g_ServerAddress), TEST_SNIFF_MAX, TEST_SNIFF_MIN, TEST_SNIFF_ATTEMPT, TEST_SNIFF_TO, iResult);
			}

			// Exit SNIFF mode
			QAMessage(TEXT("Exit SNIFF mode"));
			iResult = BthExitSniffMode(&g_ServerAddress);
			if (iResult != ERROR_SUCCESS)
			{
				QAMessage(TEXT("BthExitSniffMode failed, error = %d"), iResult);
			}
		}

		// switch role to slave
		iResult = BthSwitchRole(&g_ServerAddress, 1);
		if ( iResult != ERROR_SUCCESS)
		{
			QAMessage(TEXT("BthSwitchRole failed, error = %d"), iResult);
		}

		// switch role to master
		iResult = BthSwitchRole(&g_ServerAddress, 0);
		if ( iResult != ERROR_SUCCESS)
		{
			QAMessage(TEXT("BthSwitchRole failed, error = %d"), iResult);
		}
	}

	// Write page timeout
	QAMessage(TEXT("Change Page timeout"));
	iResult = BthWritePageTimeout(0x2000);
	if (iResult != ERROR_SUCCESS)
	{
		QAMessage(TEXT("BthWritePageTimeout failed, error = %d"), iResult);
	}

	// Set COD
	unsigned int cod, oldCod;

	iResult = BthReadCOD(&oldCod);
	if (iResult != ERROR_SUCCESS)
	{
		QAMessage(TEXT("BthReadCOD() failed, error = %d"), iResult);
	}
	else
	{
		QAMessage(TEXT("Current Class of Device is 0x%06x"), oldCod);

		cod = 0x000080;
		QAMessage(TEXT("BthWriteCOD(0x%06x)"), cod);
		iResult = BthWriteCOD(cod);
		if (iResult != ERROR_SUCCESS)
		{
			QAMessage(TEXT("BthWriteCOD(0x%06x) failed, error = %d"), cod, iResult);
		}
				
		iResult = BthWriteCOD(oldCod);
		if (iResult != ERROR_SUCCESS)
		{
			QAMessage(TEXT("BthWriteCod(0x%06x) failed, error = %d"), oldCod, iResult);
		}
	}

	// terminate ACL connection

	QAMessage(TEXT("Terminate ACL connection"));
	iResult = BthCloseConnection(hConnection);
	if (iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("BthCloseConnection failed, error = %d"), iResult);
		fTestPass = FALSE;
		goto exit;
	}

	// shutdown and restart the stack

	// handle hDev will be closed on function exit (see EDG bug#107687)
	hDev = CreateFile (L"BTD0:", GENERIC_READ | GENERIC_WRITE,
									FILE_SHARE_READ | FILE_SHARE_WRITE,
									NULL, OPEN_EXISTING, 0, NULL);

	if (hDev == INVALID_HANDLE_VALUE) 
	{
		QAMessage (L"BTD0: Device not loaded!\n");
	}
	else
	{
		WCHAR *argPtr = TEXT("card");

		QAMessage(TEXT("Shutting down stack"));
		int iErr = DeviceIoControl (hDev, IOCTL_SERVICE_STOP, argPtr, sizeof(WCHAR) * (wcslen (argPtr) + 1), NULL, NULL, NULL, NULL);
		QAMessage (L"DeviceIoControl() for shutdown returned = %d", iErr);

		Sleep (5000);

		QAMessage(TEXT("Startup stack"));
		iErr = DeviceIoControl (hDev, IOCTL_SERVICE_START, argPtr, sizeof(WCHAR) * (wcslen (argPtr) + 1), NULL, NULL, NULL, NULL);
		QAMessage (L"DeviceIoControl() for startup returned = %d", iErr);

		// wait for stack to come up again

		int timeout = 10;

		for (int i = 0; i < timeout; i++)
		{
			int status;

			QAMessage(TEXT("Waiting for stack to come up..."));
			
			iResult = BthGetHardwareStatus(&status);

			if (iResult != ERROR_SUCCESS && iResult != ERROR_SERVICE_NOT_ACTIVE)
			{
				QAError(TEXT("BthGetHardwareStatus failed, error = %d"), iResult);
				fTestPass = FALSE;
				goto exit;
			}
			else if (status == HCI_HARDWARE_RUNNING)
			{
				QAMessage(TEXT("BthGetHardwareStatus returned HCI_HARDWARE_RUNNING"));
				break;
			}
			else
				Sleep(1000);			
		}

		if (i >= timeout)
		{
			QAError(TEXT("Stack didn't come up in <= %d seconds"), timeout);
			fTestPass = FALSE;
		}
			
	}

exit:

	// signal exit and wait for reader thread
	QAMessage(TEXT("Terminating reader thread..."));
	SetEvent (ThreadParams.hExitReaderEvent);
	WaitForSingleObject(hThread, INFINITE);

	fTestPass = (fTestPass && ThreadParams.fTestPass); // Reader thread must have passed as well

	return (fTestPass ? TPR_PASS : TPR_FAIL);
}

//
//TestId        : 036
//Title         : Does not connect after auth failure
//Description   : 
//
TESTPROCAPI t036(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	// verify that feature is supported

	BtVersionInfo hwInfo;
	int iResult = hwInfo.ReadLocal();

	if (iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("info.ReadLocal() failed, error = %d"), iResult);
		return TPR_FAIL;
	}

	BOOL Skip = hwInfo.EqualOrOlderThan(11, 1, 1300);	// Sharp UART card - broken

	if (Skip)
	{
		QAMessage(TEXT("Feature not supported or or broken in BT module"));
		return TPR_SKIP;
	}

	// run test

	QAMessage(TEXT("Enable authentication for all connections"));
	iResult = BthWriteAuthenticationEnable(0x01);
	
	if (iResult != ERROR_SUCCESS)
	{
		QAError(TEXT("BthWriteAuthenticationEnable(0x%02x) failed, error = %d"), iResult);
		return TPR_FAIL;
	}

	QAMessage(TEXT("Create ACL connection"));
	unsigned short hConnection;
	iResult = BthCreateACLConnectionRetry(&g_ServerAddress, &hConnection, 3);

	// PIN dialog will appear on UI but none will enter a PIN, so connection should fail.
	
	if (iResult == ERROR_SUCCESS)
	{
		QAError(TEXT("BthCreateACLConnection didn't fail as expected"));
		iResult = BthWriteAuthenticationEnable(0x00);
		return TPR_FAIL;
	}
	else
	{
		QAMessage(TEXT("BthCreateACLConnection failed as expected, error = %d"), iResult);
	}

	BthWriteAuthenticationEnable(0x00);

	return TPR_PASS;
}

//
//TestId        : 037
//Title         : Closing BTD0 immdiately after create Handle 
//Description   : regression test for Bug 117286
//
TESTPROCAPI t037(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;
	int res = 0;

	QAMessage(TEXT("Create BTD0 file"));
	HANDLE hDev = CreateFile (L"BTD0:", GENERIC_READ | GENERIC_WRITE,
								FILE_SHARE_READ | FILE_SHARE_WRITE,
								NULL, OPEN_EXISTING, 0, NULL);

	if (hDev == INVALID_HANDLE_VALUE) {
		QAMessage(TEXT("Device not loaded!\n"));
		return TPR_FAIL;
	}

	QAMessage(TEXT("Close BTD0 file"));
	__try {
		//use server info
		res = CloseHandle (hDev);
	} __except(1) {
		DEBUGMSG(1, (L"Exception in test 1037.  Unable to close handle\n"));
		return TPR_FAIL;
	}
	

	QAMessage(TEXT("Pass"));
	return TPR_PASS;
}
// --------------------- Function Table used by TUX ----------------------------

FUNCTION_TABLE_ENTRY g_lpFTE[] = {
	TEXT("Bluetooth BthXxx API Tests:"								), 0, 0, 0,					NULL,
	TEXT(  "Get Hardware Status Test"								), 1, 1, BTHAPIBASE + 1,	t001,
	TEXT(  "Read Local Bluetooth Address Test"						), 1, 1, BTHAPIBASE + 2,	t002,
	TEXT(  "ScanEableMask Test"										), 1, 1, BTHAPIBASE + 3,	t003,
	TEXT(  "AuthenticationEnable Test"								), 1, 1, BTHAPIBASE + 4,	t004,
	TEXT(  "PageTimeout Test"										), 1, 1, BTHAPIBASE + 5,	t005,
	TEXT(  "Class of Device Test"									), 1, 1, BTHAPIBASE + 6,	t006,
	TEXT(  "Read Local Version Test"								), 1, 1, BTHAPIBASE + 7,	t007,
	TEXT(  "Set/Revoke PIN Test"									), 1, 1, BTHAPIBASE + 8,	t008,
	TEXT(  "Link Key Test"											), 1, 1, BTHAPIBASE + 9,	t009,
	TEXT(  "Security UI Test"										), 1, 1, BTHAPIBASE + 10,	t010,
	TEXT(  "PIN Request Test"										), 1, 1, BTHAPIBASE + 11,	t011,
	TEXT(  "PerformInquiry Test"									), 1, 1, BTHAPIBASE + 12,	t012,
	TEXT(  "CancelInquiry Test"										), 1, 1, BTHAPIBASE + 13,	t013,
	TEXT(  "InquiryFilter Test"										), 1, 1, BTHAPIBASE + 14,	t014,
	TEXT(  "Remote Name Query Test"									), 1, 1, BTHAPIBASE + 15,	t015,
	TEXT(  "ACL Connection Test"									), 1, 1, BTHAPIBASE + 16,	t016,
	TEXT(  "Read Remote Version Test"								), 1, 1, BTHAPIBASE + 17,	t017,
	TEXT(  "LinkPolicySettings Test"								), 1, 1, BTHAPIBASE + 18,	t018,
	TEXT(  "ACTIVE Mode Test"										), 1, 1, BTHAPIBASE + 19,	t019,
	TEXT(  "HOLD Mode Test"											), 1, 1, BTHAPIBASE + 20,	t020,
	TEXT(  "SNIFF Mode Test"										), 1, 1, BTHAPIBASE + 21,	t021,
	TEXT(  "PARK Mode Test"											), 1, 1, BTHAPIBASE + 22,	t022,
	TEXT(  "Terminate Idle Connection Test"							), 1, 1, BTHAPIBASE + 23,	t023,
	TEXT(  "Authenticate Connection Test"							), 1, 1, BTHAPIBASE + 24,	t024,
	TEXT(  "Encrypt Connection Test"								), 1, 1, BTHAPIBASE + 25,	t025,
	TEXT(  "SCO Connection Test"									), 1, 1, BTHAPIBASE + 26,	t026,
	TEXT(  "Set Service Test"										), 1, 1, BTHAPIBASE + 27,	t027,
	TEXT(  "Local ServiceSearch/ServiceAttribute Test"				), 1, 1, BTHAPIBASE + 28,	t028,
	TEXT(  "Remote ServiceSearch/ServiceAttribute Test"				), 1, 1, BTHAPIBASE + 29,	t029,
	TEXT(  "Local ServiceSearchAttribute Test"						), 1, 1, BTHAPIBASE + 30,	t030,
	TEXT(  "Remote ServiceSearchAttribute Test"						), 1, 1, BTHAPIBASE + 31,	t031,
	TEXT(  "Get Remote COD"										), 1, 1, BTHAPIBASE + 32,	t032,
	TEXT(  "Read RSSI"											), 1, 1, BTHAPIBASE + 33,	t033,
	TEXT(  "GetRole / SwitchRole"									), 1, 1, BTHAPIBASE + 34,	t034,
	TEXT(  "RequestBluetoothNotifications / StopBluetoothNotifications"		), 1, 1, BTHAPIBASE + 35,	t035,
	TEXT(  "Does not connect after auth failure"							), 1, 1, BTHAPIBASE + 36,	t036,
	TEXT(  "Closing BTD0 immdiately after create Handle"				), 1, 1, BTHAPIBASE + 37,	t037,

	NULL,															0, 0, 0,					NULL  // marks end of list
};
 
