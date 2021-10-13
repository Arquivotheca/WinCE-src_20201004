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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//

#pragma once

// Structure with all the info from the device

#define MAX_LENGTH_REGION_NAME  64    // By definition
#define MAX_LENGTH_VERSION_STR  20    // Today 16 is enough.  i.e.  3.0.11178.11203
                                        // Defining 20, covering the case of every one of the 
                                        // 4 numbers growing one order of magnitude
#define MAX_LENGTH_DEVICE_NAME  32    // Merlin doesn't allow more than 15 chars in the device name.  Let's double that.
#define MAX_LENGTH_USER_NAME    64    // Could be an e-mail address?
#define MAX_LENGTH_CPU_NAME     12    // Longest string currently returned by GetCpuName() is 9 + '\0'.


#ifdef __cplusplus
extern "C" {
#endif
                                        
typedef struct XIPInfoStruct
{
    DWORD   dwVersion;      //Build Number of this XIP region
    TCHAR   szRegion[MAX_LENGTH_REGION_NAME];       // String to copy XIP region name to
    TCHAR   szVersion[MAX_LENGTH_VERSION_STR];      // String to copy XIP version string to
}XIPINFOSTRUCT, * LPXIPINFOSTRUCT;

                                        
enum EPRODID 
{
    EPROD_UNKNOWN,
    POCKET_PC, 
    SMART_PHONE,
};

enum ELANGID {  //the numbers are the actual LCID
      WWE=0x0409, // USA was probably defined some where else since it gave me syntax error.
      GER=0x0407, 
      JPN=0x0411, 
      CHS=0x0804, 
      CHT=0x0404, 
      KOR=0x0412, 
      FRA=0x040c, 
      ESP=0x040a, 
      ITA=0x0410, 
      PTB=0x0416 
};


enum EProductRelease
{
    EPR_UNKNOWN         = 0,
    EPR_RAPIER          = 99,
    EPR_MERLIN          = 1,
    EPR_OZONE           = 2,
    EPR_OZONE_AKU1      = 3,
    EPR_OZONE_AKU2      = 4,
    EPR_OZONE_UPDATE_1  = 5,
    EPR_OZONE_UPDATE_2  = 6,
    EPR_MAGNETO         = 7,
    EPR_MAGNETO_AKU     = 8,
    EPR_CROSSBOW        = 9,
    EPR_CROSSBOW_AKU    = 10,
    EPR_PHOTON          = 11,
    EPR_PHOTON_AKU      = 12,
};



#define DEVINFO_GOTVERSION          0x00000001
#define DEVINFO_GOTPRODUCT          0x00000002
#define DEVINFO_GOTPLATFORM         0x00000004
#define DEVINFO_GOTSKU              0x00000008
#define DEVINFO_GOTLANGUAGE         0x00000010
#define DEVINFO_GOTDEVICENAME       0x00000020
#define DEVINFO_GOTDEVICEID         0x00000040
#define DEVINFO_GOTUSERNAME         0x00000100
#define DEVINFO_GOTBINARYIMAGENAME  0x00000200






typedef struct DeviceInfoStruct
{
    TCHAR       szPlatform[MAX_PATH];        // Platform i.e. "MACAW" (from OEM info)
    TCHAR       szProduct[MAX_PATH];         // Product i.e. "PocketPC"
    DWORD       dwProdId;                    // Id of Product (mapped to EPRODID)
    TCHAR       szLang[MAX_PATH];            // Lang
    LCID        dwLangId;                    // Lang identificator   
    TCHAR       szSku[MAX_PATH];             // Specifies the Sku type, GSM, Compressed, ShellOnly, etc.
    DWORD       dwRelease;                   // Release Number (EProductRelease)
    DWORD       dwBuild;                     // Build Number
    DWORD       dwOsBuild;
    DWORD       dwOsVersionMajor;
    DWORD       dwOsVersionMinor;
    DWORD       dwPhysicalMemory;
    DWORD       dwCpuType;
    TCHAR       szCpuName[MAX_LENGTH_CPU_NAME];
    BYTE        byDeviceId[16];
    TCHAR       szName[MAX_LENGTH_DEVICE_NAME];
    TCHAR       szUserName[MAX_LENGTH_USER_NAME];   // Server Sync user name or desktop sync machine name
    BOOL        bDebug;                             // Is shell32.exe a debug build?
    TCHAR       szBinaryImageName[MAX_PATH];        // Specifies the binary image name.
    DWORD       dwDevInfoFlags;                     // Start flagging 'got' fields
} DEVICEINFOSTRUCT, * LPDEVICEINFOSTRUCT;


BOOL GetDeviceInfo( LPDEVICEINFOSTRUCT  );
LPCTSTR GetPersistentRoot();



struct SDeviceId
{ 
    BYTE rbyDeviceId[16];
};


#ifdef UNDER_CE
BOOL GetProduct(TCHAR *pszBuff, DWORD dwBufLen);
BOOL GetDeviceId(    struct SDeviceId *o_pDeviceId );
DWORD GetCpuType();
HRESULT GetCpuName(WCHAR *o_pszCPUName, size_t cchCpuName);

// GetVirtualRelease
// Copies the correct release path into pszBuff (always with a trailing \).
// If the file VirtualReleaseOverride.txt exists at the root of the device,
// the first line of this will be used.
// If there is no VirtualReleaseOverride.txt, the flat release dir will be
// used (\release). If there is no \release, then the current dir of the module
// will be used.
//
// Takes pszRelease: 
//      Buffer of length cchRelease characters that will receive the 
//      release directory.
// cchRelease
//      Length of the buffer passed in in characters.
//
// Returns number of characters written on success, 0 on failure
//
// Note: pszRelease will always have a trailing '\'
DWORD GetVirtualRelease(TCHAR *pszRelease, DWORD cchRelease);
#endif


#ifndef DEVINFO_CONSTS_EXTERN
const LPCTSTR c_szShip        = _T("ship");
const LPCTSTR c_szRetail      = _T("retail");
const LPCTSTR c_szDebug       = _T("debug");
const LPCTSTR c_szUnknown     = _T("unknown");
#else
extern LPCTSTR c_szShip;
extern LPCTSTR c_szRetail;
extern LPCTSTR c_szDebug;
extern LPTSTR c_szUnknown;
#endif

#ifdef __cplusplus
}
#endif


