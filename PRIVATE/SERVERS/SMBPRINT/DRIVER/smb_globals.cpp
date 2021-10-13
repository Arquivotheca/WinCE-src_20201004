//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "SMB_Globals.h"
#include "Utils.h"

ShareManager        *SMB_Globals::g_pShareManager = NULL;
UniqueID            *SMB_Globals::g_pUniqueFID = NULL;
ConnectionManager   *SMB_Globals::g_pConnectionManager;
UINT                 SMB_Globals::g_uiMaxJobsInPrintQueue = 10;
char                 SMB_Globals::CName[16];
AbstractFileSystem  *SMB_Globals::g_pAbstractFileSystem = NULL;
UINT                 SMB_Globals::g_uiMaxConnections = 5;
MemMappedBuffer      SMB_Globals::g_PrinterMemMapBuffers;
BYTE                 SMB_Globals::g_ServerGUID[4 * 4];
CHAR                 SMB_Globals::g_szWorkGroup[MAX_PATH];

#ifdef DEBUG
CMD_TABLE g_SMBCmdNames[] = {
    TEXT("SMBmkdir"),        0x00,
    TEXT("SMBrmdir"),        0x01,
    TEXT("SMBopen"),         0x02,
    TEXT("SMBcreate"),       0x03,
    TEXT("SMBclose"),        0x04,
    TEXT("SMBflush"),        0x05,
    TEXT("SMBdelete"),       0x06,
    TEXT("SMBmv"),           0x07,
    TEXT("SMBgetatr"),       0x08,
    TEXT("SMBsetatr"),       0x09,
    TEXT("SMBread"),         0x0A,
    TEXT("SMBwrite"),        0x0B,
    TEXT("SMBlock"),         0x0C,
    TEXT("SMBunlock"),       0x0D,
    TEXT("SMBctemp"),        0x0E,
    TEXT("SMBmknew"),        0x0F,
    TEXT("SMBchkpth"),       0x10,
    TEXT("SMBexit"),         0x11,
    TEXT("SMBlseek"),        0x12,
    TEXT("SMBlockread"),     0x13,
    TEXT("SMBwriteunlock"),  0x14,
    TEXT("SMBreadBraw"),     0x1A,
    TEXT("SMBreadBmpx"),     0x1B,
    TEXT("SMBreadBs"),       0x1C,
    TEXT("SMBwriteBraw"),    0x1D,
    TEXT("SMBwriteBmpx"),    0x1E,
    TEXT("SMBwriteBs"),      0x1F,
    TEXT("SMBwriteC"),       0x20,
    TEXT("SMBqrysrv"),       0x21,
    TEXT("SMBsetattrE"),     0x22,
    TEXT("SMBgetattrE"),     0x23,
    TEXT("SMBlockingX"),     0x24,
    TEXT("SMBtrans"),        0x25,
    TEXT("SMBtranss"),       0x26,
    TEXT("SMBioctl"),        0x27,
    TEXT("SMBioctls"),       0x28,
    TEXT("SMBcopy"),         0x29,
    TEXT("SMBmove"),         0x2A,
    TEXT("SMBecho"),         0x2B,
    TEXT("SMBwriteclose"),   0x2C,
    TEXT("SMBopenX"),        0x2D,
    TEXT("SMBreadX"),        0x2E,
    TEXT("SMBwriteX"),       0x2F,
    TEXT("SMBnewsize"),      0x30,
    TEXT("SMBcloseTD"),      0x31,
    TEXT("SMBtrans2"),       0x32,
    TEXT("SMBtrans2s"),      0x33,
    TEXT("SMBfindclose"),    0x34,
    TEXT("SMBfindnclose"),   0x35,
    TEXT("SMBlogon"),        0x60,
    TEXT("SMBbind"),         0x61,
    TEXT("SMBunbind"),       0x62,
    TEXT("SMBgetaccess"),    0x63,
    TEXT("SMBlink"),         0x64,
    TEXT("SMBfork"),         0x65,
    TEXT("SMBioctl"),        0x66,
    TEXT("SMBcopy"),         0x67,
    TEXT("SMBgetpath"),      0x68,
    TEXT("SMBreadh"),        0x69,
    TEXT("SMBmove"),         0x6A,
    TEXT("SMBrdchk"),        0x6B,
    TEXT("SMBmknod"),        0x6C,
    TEXT("SMBrlink"),        0x6D,
    TEXT("SMBgetlatr"),      0x6E,
    TEXT("SMBtcon"),         0x70,
    TEXT("SMBtreedis"),      0x71,
    TEXT("SMBnegprot"),      0x72,
    TEXT("SMBsesssetupX"),   0x73,
    TEXT("SMBulogoffX"),     0x74,
    TEXT("SMBtconX"),        0x75,
    TEXT("SMBdskattr"),      0x80,
    TEXT("SMBsearch"),       0x81,
    TEXT("SMBfind"),         0x82,
    TEXT("SMBfindunique"),   0x83,
    TEXT("SMBfclose"),       0x84,
    TEXT("SMBsplopen"),      0xC0,
    TEXT("SMBsplwr"),        0xC1,
    TEXT("SMBsplclose"),     0xC2,
    TEXT("SMBsplretq"),      0xC3,
    TEXT("SMBsends"),        0xD0,
    TEXT("SMBsendb"),        0xD1,
    TEXT("SMBfwdname"),      0xD2,
    TEXT("SMBcancelf"),      0xD3,
    TEXT("SMBgetmac"),       0xD4,
    TEXT("SMBsendstrt"),     0xD5,
    TEXT("SMBsendend"),      0xD6,
    TEXT("SMBsendtxt"),      0xD7,
    NULL, 0
};

WCHAR *GetCMDName(BYTE opCode)
{
    UINT uiIdx = 0;
    while(g_SMBCmdNames[uiIdx].ct_name != NULL) {
        if(opCode == g_SMBCmdNames[uiIdx].ct_code)
            return g_SMBCmdNames[uiIdx].ct_name;
        uiIdx ++;
    }
    return L"Unknown";
}

void _TRACEMSG (WCHAR *lpszFormat, ...)
{
    va_list args;
    va_start (args, lpszFormat);

    WCHAR szBigBuffer[2000];
    wvsprintf (szBigBuffer, lpszFormat, args);
    swprintf(szBigBuffer, L"%s\n\r", szBigBuffer);
    va_end (args);    
    OutputDebugString (szBigBuffer);
}

#endif



#ifdef DEBUG
LONG       SMB_Globals::g_PacketID;
#endif

HRESULT SMB_Globals::GetCName(BYTE **pName, UINT *pLen)
{
    if(NULL == CName[0])
        return E_FAIL;
        
    *pName = (BYTE *)SMB_Globals::CName;
    *pLen = strlen(SMB_Globals::CName);
    return S_OK;
}

#ifdef DEBUG
//reference SMB_Globals.h for zone #'s
DBGPARAM dpCurSettings = {
    TEXT("SMB_Server"), { 
    TEXT("Init"),TEXT("Errors"),TEXT("Warnings"),TEXT("Netbios Transport"),
    TEXT("TCPIP Transport"), TEXT("SMBs"),TEXT("File Server"),TEXT("Undefined"),
    TEXT("Details"), TEXT("Stats") ,TEXT("Security"),TEXT("Print Queue"),
    TEXT("Undefined"),TEXT("Undefined"),TEXT("Undefined"), TEXT("Quit") },
    0x0003
    //0x0846
};
#endif


// 
//  Utility
//
unsigned short us_rand (void) {
    unsigned short usRes = (unsigned short)rand();
    if (rand() > RAND_MAX / 2)
        usRes |= 0x8000;

    return usRes;
}

typedef int (*CeGenerateGUID_t) (GUID *);

void GenerateGUID (GUID *pGuid) {
    HMODULE hLib = LoadLibrary (L"lpcrt.dll");
    if (hLib) {
        CeGenerateGUID_t CeGenerateGUID = (CeGenerateGUID_t)GetProcAddress (hLib, L"CeGenerateGUID");
        int fRet = (CeGenerateGUID && CeGenerateGUID (pGuid) == 0);
        FreeLibrary (hLib);

        if (fRet)
            return;
    }

    srand (GetTickCount ());
    pGuid->Data1 = (us_rand () << 16) | us_rand();
    pGuid->Data2 = us_rand();
    pGuid->Data3 = us_rand();
    for (int i = 0 ; i < 8 ; i += 2) {
        unsigned short usRand = us_rand();
        pGuid->Data4[i]     = (unsigned char)(usRand & 0xff);
        pGuid->Data4[i + 1] = (unsigned char)(usRand >> 8);
    }
}


void *operator new(size_t size) {
    //RETAILMSG(1, (L"NEW: %d\n", size));
    return malloc(size);
}




