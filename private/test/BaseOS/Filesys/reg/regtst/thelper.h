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
#ifndef __TCSP_HLP__
#define __TCSP_HLP__


#ifdef UNDER_CE
#include <bldver.h>
#include <windbase.h>
#endif  //  UNDER_CE
#include "diskio.h"
#include "fathlp.h"

#define lidGerman    0x0407

#ifndef UNDER_CE
#define ASSERT        assert
#define Random        rand


#ifdef __cplusplus
    extern "C" {
#endif  //  __cplusplus


#endif  //  ndef UNDER_CE


//
// Local functions shared only within this library
//
LPTSTR L_FileTimeToStr(LPTSTR szBuffer, FILETIME FileTime);
LPTSTR L_DwAttribToStr(LPTSTR pszAttribs, DWORD dwAttribs);


//
// Database Helper Stuff
//

//+ ------------------------------------------------------------
//      Database helper Macros
//- ------------------------------------------------------------
#define MYCLOSEHANDLE       CLOSE_HANDLE
#define MY_CLOSE_HANDLE     CLOSE_HANDLE

#define TST_FLAG_READABLE       0x0001  //  used by Hlp_GenStringData
#define TST_FLAG_ALPHA          0x0002
#define TST_FLAG_ALPHA_NUM      0x0003
#define TST_FLAG_NUMERIC        0x0004

#ifdef UNDER_CE

//+ ------------------------------------------------------------
//      Database helper constants and macros
//- ------------------------------------------------------------
#define HLP_VERBOSE             0x0001
#define HLP_SILENT              0x0002
#define TST_FLAG_SILENT         HLP_SILENT
#define TST_FLAG_VERBOSE        HLP_VERBOSE

extern  CEPROPID    g_rgPropIds[];
#define TST_NUM_GLOBAL_PROPS    9

//  Use these as bit flags.
#define TST_FLAG_PROP_IS_SORT_KEY       0x01
#define TST_STRING_PROP_FORMAT          _T("Record_%d_Prop_%d")
#define TST_STRING_PROP_FORMAT_RANDOM   _T("Record_%d_Prop_%d_%d")
#define GET_PROPID(X)                   X & 0xFF

#define TST_BIT_RANDOM_DATA         0x00000001
#define TST_BIT_SEQUENTIAL_DATA     0x00000002
#define TST_BIT_VERBOSE             0x00000004
#define TST_BIT_SILENT              0x00000008

#define TST_DEFAULT_DATA_SIZE       50
#define TST_MAX_DATA                50


#define UNMOUNT(X)      {                       \
    if ((FALSE == CHECK_INVALIDGUID(X)) && (!CeUnmountDBVol(X))) \
        TRACE(TEXT(">>>>> ERROR : CeUnmountDBVol failed 0x%x\r\n"), GetLastError()); \
    CREATE_INVALIDGUID(X); \
}


//+ ------------------------------------------------------------
//      Database helper structs
//- ------------------------------------------------------------
typedef struct _tst_recordinfo {
    DWORD   dwNumProps;        //  Specify the num props in the record.
    PROPID  *pPropIds;         //  Should be used as a PROPID array.
    DWORD   *pdwPropFlags;     //  dwPropFlag array : bit fields.
                                //  TST_FLAG_PROP_IS_SORT_KEY
                                //  TST_BIT_RANDOM_DATA
                                //  TST_BIT_SEQUENTIAL_DATA
                                //  High order WORD specifies sizes of BLOBs, STRINGs etc.
} TEST_RECORDINFO, *PTEST_RECORDINFO;

//+ ------------------------------------------------------------
//      Database helper functions
//- ------------------------------------------------------------
DWORD   Hlp_DumpDatabase(HANDLE hDB, DWORD dwFlags);
DWORD   Hlp_DumpDatabaseOids(HANDLE hDB);
DWORD   Hlp_DumpBuffer(const BYTE* pbBuffer, DWORD cbBuffer);
BOOL    Hlp_DumpStruct_OidInfo(CEOIDINFO OidInfo);
BOOL    Hlp_DumpStruct_FileInfo(CEFILEINFO FileInfo);
BOOL    Hlp_DumpStruct_DirInfo(CEDIRINFO DirInfo);
BOOL    Hlp_DumpStruct_SortOrderSpec(SORTORDERSPEC SortSpec);
BOOL    Hlp_DumpStruct_DBaseInfo(CEDBASEINFO DBInfo);
BOOL    Hlp_DumpStruct_RecordInfo(CERECORDINFO RecInfo);
BOOL    Hlp_DumpStruct_OidInfo(CEOIDINFO OidInfo);
BOOL    Hlp_DumpStruct_SortOrderSpec(SORTORDERSPEC SortSpec);
BOOL    Hlp_DumpOidInfoEx(PCEGUID pceguid, CEOID Oid);
WCHAR*  Hlp_GetDBName(CEOID DBOid);
LPTSTR  Hlp_DBFlagsToStr(DWORD dwFlags, LPTSTR pszFlags);
BOOL    Hlp_DumpOidInfo(CEOID Oid);
BOOL    Hlp_DumpOidInfoEx(PCEGUID pceguid, CEOID Oid);
CEOID   Hlp_GetDBOid(PCEGUID pVolGUID, LPTSTR pszDBName);
BOOL    Hlp_DeleteDBbyName(PCEGUID VolGUID, LPTSTR pszDBName);
DWORD   Hlp_WriteTestRecords(HANDLE hDB, TEST_RECORDINFO Tst_RecInfo, DWORD dwNumRecords, DWORD dwSeed, DWORD dwFlags, DWORD dwReserved);
DWORD   Hlp_WriteTestRecordsEx(   HANDLE hDB,
                                CEOID  RecordOid,
                                TEST_RECORDINFO Tst_RecInfo,
                                DWORD dwNumRecords,
                                DWORD dwSeed,
                                DWORD dwFlags,
                                DWORD dwReserved);

LPTSTR  Hlp_PropidToStr(CEPROPID Propid, LPTSTR pszTmp);
void    Hlp_DumpProp(CEPROPVAL PropVal);
DWORD   Hlp_DumpRecord(PBYTE pbPropBuffer, DWORD dwPropsRead, DWORD dwFlags);
CEOID   Hlp_CreateDatabase(LPTSTR pszDBName, DWORD dwType, WORD wNumSortOrder, SORTORDERSPEC *mySortOrder);

BOOL    Hlp_GetVolDBCount(CEGUID VolGUID, DWORD *pdwCount);
BOOL    Hlp_GetRecordCountFromSz(LPTSTR pszDbName, DWORD* pdwRecordCount);
BOOL    Hlp_IsIdentical_FileTime(FILETIME ft1, FILETIME ft2);
BOOL    Hlp_IsIdentical_Record(PCEPROPVAL rgPropVal1, WORD wPropsRead1, PCEPROPVAL rgPropVal2, WORD wPropsRead2);
BOOL    Hlp_DBExists(CEGUID *pVolGUID, LPTSTR pszDBName);
BOOL    Hlp_DBExists(LPTSTR pszVolName, LPTSTR pszDBName);


#if CE_MAJOR_VER >= 0x0004
BOOL    Hlp_GetDBNameEx(PCEGUID pVolGUID, CEOID DBOid, LPTSTR pszDBName);
BOOL    Hlp_GetRecordCount(HANDLE hDB, DWORD *pdwRecordCount);
BOOL    Hlp_DumpStruct_DBaseInfoEx(CEDBASEINFOEX DBInfoEx);
BOOL    Hlp_DumpStruct_OidInfoEx(CEOIDINFOEX OidInfoEx);
BOOL    Hlp_DumpOidInfoEx2(PCEGUID pceguid, CEOID Oid);
BOOL    Hlp_DumpStruct_SortOrderSpecEx(SORTORDERSPECEX SortSpecEx);
void    Hlp_Dump_Struct_Test_RecInfo(TEST_RECORDINFO Tst_RecInfo, DWORD dwFlags);
BOOL    Hlp_DumpStruct_By_Handle_DBInfo(BY_HANDLE_DB_INFORMATION hDbInfo);
BOOL    Hlp_Cmp_Struct_dbInfoEx(CEDBASEINFOEX DbInfo, CEDBASEINFOEX DbInfo_expected);
BOOL    Hlp_Cmp_Struct_SortOrderSpecEx(SORTORDERSPECEX SortSpec1, SORTORDERSPECEX SortSpec2);


DWORD   Hlp_EnumDBsEx2(LPTSTR pszVolName);

CEOID   Hlp_CreateDatabaseEx(CEGUID *pVolGUID, CEDBASEINFO *pdbInfo);
CEOID   Hlp_CreateDatabaseEx2(CEGUID *pVolGUID, CEDBASEINFOEX *pdbInfoEx);
BOOL    Hlp_DuplicateData(HANDLE hDB, DWORD dwNumRecords, TEST_RECORDINFO Tst_RecInfo);
WORD    Hlp_GetPropIndex(PCEPROPVAL rgPropVals, WORD wNumProps, PROPID PropID);
BOOL    Hlp_VerifyDB(  PCEGUID pVolGUID,
                        PCEOID pDbOid,
                        CEDBASEINFOEX DbInfo_expected,
                        DWORD dwTstFlags);
BOOL    Hlp_DeleteRecords(HANDLE hDB, DWORD dwDelRecs);
BOOL    Hlp_DeleteRecordsEx2(HANDLE hDB, DWORD dwNumRecords);
BOOL    Hlp_AddProps(HANDLE hDB, TEST_RECORDINFO Tst_RecInfo);
BOOL    Hlp_DeleteProps(HANDLE hDB, TEST_RECORDINFO Tst_RecInfo);

void    Hlp_Free_Struct_TestRecordInfo(TEST_RECORDINFO Tst_RecInfo);
BOOL    Hlp_IsIdentical_ByHandleDbInformation(BY_HANDLE_DB_INFORMATION byH_DbInfo1, BY_HANDLE_DB_INFORMATION byH_DbInfo2);
BOOL    Hlp_GetOidInfoEx2(CEGUID *pVolGUID, CEOID Oid, CEOIDINFOEX *pOidInfoEx);
BOOL    Hlp_IsIndenticalDBInfos(HANDLE hDB_Src, HANDLE hDB_Dest);

#endif  //  version 4

#endif //   Under CE for database specific stuff


//+ ------------------------------------------------------------
//  memory helper functions
//- ------------------------------------------------------------
BOOL    Hlp_BumpProgramMem(int dwNumPages);
DWORD   Hlp_GetAvailMem(void);
BOOL    Hlp_ReduceProgramMem(int dwNumPages);
void    Hlp_DisplayMemoryInfo(void);
DWORD   Hlp_GetAvailMem_Physical(void);
DWORD   Hlp_GetAvailMem_Virtual(void);
DWORD   Hlp_GetFreeStorePages(void);
DWORD   Hlp_GetFreePrgmPages(void);
BOOL    Hlp_Equalize_Store_Prgm_Mem(void);
BOOL    Hlp_EatAll_Store_Mem(void);
BOOL    Hlp_EatAll_Store_MemEx(DWORD dwLeaveMem);
BOOL    Hlp_EatAll_Prgm_MemEx(DWORD dwNumPagesToLeave);
BOOL    Hlp_SetMemPercentage(DWORD dwStorePcent, DWORD dwRamPcent);



//
// File System Stuff
//

//+ ----------------------------------------------------------------------
//      FileSystems helper constants
//- ----------------------------------------------------------------------
#define     MAX_CARD_COUNT            50
#define     HLP_FILL_RANDOM         1
#define     HLP_FILL_SEQUENTIAL     2
#define     HLP_FILL_DONT_CARE      0
#define     HLP_FLAG_UNCOMPRESSED       0x1

#define TST_ID_ATADISK              0xA1A   //  looks like ATA
#define TST_ID_SRAMDISK             0x59AA  //  looks like SRam
#define TST_ID_TRUEFFS              0x1FF5  //  looks like TFFS
#define TST_ID_ATAPI                0xA1A91 //  Looks like AtApi
#define TST_ID_USBDISK              0x05BD  //  Looks like USBD


#define ATADISK_DLL                 _T("ATADISK.DLL")
#define SRAMDISK_DLL                _T("SRAMDISK.DLL")
#define TRUEFFS_DLL                 _T("TRUEFFS.DLL")
#define ATAPI_DLL                   _T("ATAPI.DLL")
#define USBDISK_DLL                 _T("USBDISK6.DLL")

enum DriveTestType
{
    NullType    = 0,
    Profile,
    DeviceType
};

enum MediaType
{
    InavlidType = 0,
    AtapiType,
    FlashType,
    RemovableType
};



//+ ----------------------------------------------------------------------
//      FileSystems helper functions
//- ----------------------------------------------------------------------
LPTSTR Hlp_AttribToStr(DWORD dwAttribs, LPTSTR pszBuffer);
DWORD   Hlp_DumpFileAttrs(LPTSTR pszFileName);
DWORD   Hlp_GetCardCount(void) ;
BOOL    Hlp_FatFormatVolume(int volume, DWORD dwFlags);
BOOL    Hlp_FatGetInfo(int volume, FATINFO *pFI);
HANDLE  Hlp_GetVolHandle(int volume,  MediaType mType);
BOOL Hlp_Get_Storage_Name(LPTSTR pszStorageName, enum MediaType mType);
BOOL    Hlp_DisplayVolumeInfo(int volume);
DWORD   Hlp_GetVolClusterSize(int volume);
int     Hlp_GetVolFreeClusters(int volume);
DWORD   Hlp_WriteTestFile(LPTSTR szFileName, DWORD dwFileSize);
DWORD   Hlp_WriteTestFileEx(LPTSTR pszFileName, DWORD dwFileSize, DWORD dwFlags);
BOOL    Hlp_FillBuffer(__out_ecount(cbBuffer) PBYTE pbBuffer, DWORD cbBuffer, DWORD dwFlags);
BOOL    Hlp_FileExists(LPTSTR pszFileName);
BOOL    Hlp_Get_Localized_StorageCard_Warning(LPTSTR *pszString);
LPTSTR  Hlp_Get_StorageCard_Name(LPTSTR pszStorageCard);
BOOL    Hlp_DeleteTree(LPTSTR szPath, BOOL fDontDeleteRoot);
DWORD   Hlp_GetFileSize(LPTSTR pszFileName);
BOOL    Hlp_DumpStruct_By_Handle_FileInfo(BY_HANDLE_FILE_INFORMATION FileInfo);
BOOL    Hlp_Dump_FileInfoByName(LPTSTR pszFileName);
DWORD   Hlp_ReadFileToBuf(LPTSTR pszFileName, PBYTE *ppbFile, DWORD *pdwFile);
DWORD   Hlp_GetBlockDriverID(DWORD dwDskNum);
#ifdef UNDER_CE
BOOL    Hlp_CopyFilePPSH(LPCTSTR szSrc, LPCTSTR szDst);
#endif
DWORD   Hlp_DumpDirFiles(LPTSTR pszDirName);
DWORD   Hlp_DeleteFiles(LPTSTR pszSearch);
bool    Hlp_FindFirstAFS(LPTSTR s, unsigned cch, DriveTestType = DeviceType);

BOOL    Hlp_DumpStorageCardID(PSTORAGE_IDENTIFICATION pStorageID);

//
// Registry Stuff
//

//+ ----------------------------------------------------------------------
//      Registry helper constants
//- ----------------------------------------------------------------------
#define TST_NUM_REG_ROOTS     3
extern HKEY  g_Reg_Roots[];
#define TST_NUM_REG_TYPES     8
extern DWORD g_RegValueTypes[];

#define REG_CLOSE_KEY(X)     {       \
    if (0!=X) \
    {   \
        if (ERROR_SUCCESS != RegCloseKey(X)) \
            TRACE(TEXT("RegCloseKey failed \n")); \
        else \
            X = 0; \
    } \
}

//+ ---------------------------------------------
//  Registry failure Macros
//- ---------------------------------------------
#define REG_FAIL(X, Y) { \
    TRACE(TEXT("%s error in %s line %u 0x%x "), TEXT(#X), TEXT(__FILE__), __LINE__, Y); \
    Hlp_DumpError(Y); \
    TRACE(TEXT("\n")); \
    goto ErrorReturn; \
} \

#define VERIFY_REG_ERROR(szFunc, dwError, dwErrorExpected) { \
    if (dwError != dwErrorExpected) \
    { \
        TRACE(TEXT("%s error in %s line %u"), TEXT(#szFunc), TEXT(__FILE__), __LINE__); \
        TRACE(TEXT("Expecting ErrorCode 0x%x "), dwErrorExpected); \
        Hlp_DumpError(dwErrorExpected); \
        TRACE(TEXT("Got error code 0x%x "), dwError); \
        Hlp_DumpError(dwError); \
        TRACE(TEXT("\n")); \
        goto ErrorReturn; \
    } \
} \

#endif

//+ ----------------------------------------------------------------------
//      Registry helper functions
//- ----------------------------------------------------------------------
BOOL Hlp_IsValuePresent(HKEY hKey, LPCTSTR pszValName);
BOOL    Hlp_IsKeyPresent(HKEY hRoot, LPCTSTR pszSubKey);
TCHAR* Hlp_HKeyToText(HKEY hKey, __out_ecount(cbBuffer) TCHAR *pszBuffer, DWORD cbBuffer);
BOOL    Hlp_Write_Value(HKEY hRoot, LPCTSTR pszSubKey, LPCTSTR pszValue, DWORD dwType, const BYTE* pbData, DWORD cbData);
BOOL    Hlp_Write_Random_Value(HKEY hKey, LPCTSTR pszSubKey, LPCTSTR pszValName, DWORD *pdwType, __out_ecount(*pcbData) PBYTE pbData, DWORD *pcbData);
LONG    Hlp_Read_Value(HKEY hRoot, LPCTSTR pszSubKey, LPCTSTR pszValue, DWORD *pdwType, __out_ecount(*pcbData) PBYTE pbData, DWORD *pcbData);
TCHAR* Hlp_KeyType_To_Text(DWORD dwType, __out_ecount(cbBuffer) TCHAR *pszBuffer, DWORD cbBuffer);
TCHAR* Hlp_HKeyToTLA(HKEY hKey, __out_ecount(cbBuffer) TCHAR *pszBuffer, DWORD cbBuffer);

BOOL    Hlp_GetNumSubKeys(HKEY hKey, DWORD* pdwNumKeys);
BOOL    Hlp_GetNumValues(HKEY hKey, DWORD* pdwNumVals);
DWORD   Hlp_GetMaxValueDataLen(HKEY hKey);
DWORD   Hlp_GetMaxValueNameChars(HKEY hKey);
BOOL Hlp_GetSystemHiveFileName(__out_ecount(cbBuffer) LPTSTR pszFileName, DWORD cbBuffer);
BOOL Hlp_GetUserHiveFileName(__out_ecount(cbFileName) LPTSTR pszFileName, DWORD cbFileName);

BOOL    Hlp_GenRandomValData(DWORD *pdwType, __out_ecount(*pcbData) PBYTE pbData, DWORD *pcbData);
BOOL    Hlp_DeleteValue(HKEY hRootKey, LPCTSTR pszSubKey, LPCTSTR pszValue);
BOOL    Hlp_DumpSubKeys(HKEY hKey);
BOOL    Hlp_IsRegAlive(void);
BOOL    Hlp_CmpRegKeys(HKEY hKey1, HKEY hKey2);


//+ ======================================================================
//      G E N E R I C    H E L P E R    S T U F F
//- ======================================================================

#ifdef UNDER_CE
void    Hlp_DeviceReset(void);
BOOL    Hlp_Is_German(void);
#endif


//+ ----------------------------------------------------------------------
//      Generic helper functions
//- ----------------------------------------------------------------------
void    TRACE(LPCTSTR szFormat, ...) ;
void    Hlp_PrintTestHeader(LPCTSTR pszTestTitle);
void    Hlp_PrintTestFooter(LPCTSTR pszTestTitle);

BOOL    Hlp_GetSysTimeAsFileTime(FILETIME *retFileTime);
void    Hlp_GenRandomFileTime(FILETIME *myFileTime);
LPTSTR  Hlp_GenStringData(__out_ecount(cChars) LPTSTR pszString, DWORD cChars, DWORD dwFlags);
LPTSTR WINAPI  Hlp_LastErrorToText(DWORD dwLastError, __out_ecount(cbBuffer) LPTSTR pszBuffer, DWORD cbBuffer);
void    Hlp_DumpLastError(void);
void    Hlp_DumpError(DWORD dwError);
DWORD   WINAPI Hlp_MyReadFileExA(LPCTSTR pszFileName, PBYTE *ppbFile, DWORD *pdwFile);
DWORD   WINAPI Hlp_WriteBufToFile( LPCTSTR pszFileName, const BYTE* pbBuffer, DWORD cbBuffer);
BOOL    WINAPI Hlp_PrintLastError(void);
DWORD   WINAPI Hlp_CheckLastError(DWORD dwLastError);
DWORD   Hlp_GetPageSize(void);


//+ ------------------------------------------------------------
//      Useful macros
//- ------------------------------------------------------------

#define MUST_BE_NULL            NULL
#define MAX_TEST_ARGS           50

#define DUMP_LOCN   {TRACE(TEXT("File %s, line %d\r\n"), _T(__FILE__), __LINE__);}

#ifdef UNDER_CE

#define GENERIC_FAIL(X) {   \
    TRACE(_T("%s error in %s line %u 0x%x "), _T(#X), _T(__FILE__), __LINE__, GetLastError()); \
    Hlp_DumpLastError();\
    TRACE(_T("\n"));\
    goto ErrorReturn; \
}

#define NO_FAIL(X) {    \
    TRACE(_T("%s should have failed in %s line %u "), _T(#X), _T(__FILE__), __LINE__);   \
    TRACE(_T("\n"));     \
    goto ErrorReturn;  \
}

#else

#define GENERIC_FAIL(X) {                               \
    printf("%s error 0x%x ", #X, GetLastError());      \
    printf("\n");                                      \
    goto ErrorReturn;                                  \
}

#define NO_FAIL(X) {   printf("%s didn't fail as expected", #X);printf("\n");goto ErrorReturn; }

#endif


#define HEAPVALIDATE()  {                       \
    if( !HeapValidate(GetProcessHeap(),0,0))    \
    {                                           \
        DebugBreak();                           \
    }                                           \
}

#define CHECK_ALLOC(X)      {                   \
    if (NULL == X)                              \
        GENERIC_FAIL(LocalAlloc);              \
    HEAPVALIDATE();                            \
}

#define FREE(X) {                                                               \
    if (X)                                                                      \
    {                                                                           \
        if (LocalFree(X))                                                       \
            TRACE(_T(">>>WARNING: LocalFree Failed. 0x%x\n"), GetLastError()); \
         X=NULL;                                                               \
        HEAPVALIDATE();                                                        \
    }                                                                           \
}

#define CLOSE_HANDLE(X)     {       \
    if (INVALID_HANDLE_VALUE!=X) \
    {   \
        if (!CloseHandle(X)) \
            TRACE(TEXT("CloseHandle failed 0x%x\n"), GetLastError()); \
        else \
            X=INVALID_HANDLE_VALUE; \
    } \
}

#define FINDCLOSE(X)     {       \
    if (INVALID_HANDLE_VALUE!=X) \
    {   \
        if (!FindClose(X)) \
            TRACE(TEXT("FindClose failed 0x%x\n"), GetLastError()); \
        else \
            X=INVALID_HANDLE_VALUE; \
    } \
}

#define VERIFY_LASTERROR(X)     {   \
    if (GetLastError() != X)    \
        TRACE(_T(">>>>>> Expecting LastError 0x%x got 0x%x. %s line %u\n"), X, GetLastError(), _T(__FILE__), __LINE__);   \
}


//+ ------------------------------------------------------------
//      Database helper Macros
//- ------------------------------------------------------------
#define MYCLOSEHANDLE       CLOSE_HANDLE
#define MY_CLOSE_HANDLE     CLOSE_HANDLE


#ifndef UNDER_CE

#ifdef __cplusplus
}       // Balance extern "C" above
#endif

#endif


#define CHARCOUNT(a) (sizeof(a)/sizeof(*a))
bool    Hlp_FindFirstAFS(wchar_t * s, unsigned cch);

