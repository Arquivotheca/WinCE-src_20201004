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
/*
 */
#include <windows.h>
#include <dbgapi.h>

DBGPARAM dpCurSettings =
{
    TEXT("LoadDbg"),
    {
        TEXT(""),TEXT(""),TEXT(""),TEXT(""),
        TEXT(""),TEXT(""),TEXT(""),TEXT(""),
        TEXT(""),TEXT(""),TEXT(""),TEXT(""),
        TEXT(""),TEXT(""),TEXT(""),TEXT("")
    },
    0x0000
};

LPCTSTR g_pszDllKey   = TEXT("SYSTEM\\Debugger");
LPCTSTR g_pszDllValue = TEXT("Library");

const TCHAR AUTO_LAUNCH_VALUE[] = TEXT("Launch03");

typedef enum _REGTASK
{
    RT_UNREGISTER,
    RT_NOTHING,
    RT_REGISTER
}
REGTASK;

LONG RegisterDbg(LPCTSTR pszCmd, LPCTSTR pszDbgName, BOOL fRegister)
/*++

Routine Description:

    Register or unregister the keys and values used when being run
    by filesys.exe at system startup

Arguments:

    pszCmd - name of this EXE to be put in the registry
    pszDbgName - name of the debugger dll
    fRegister - TRUE if registering, FALSE if unregistering

Return Value:

    ERROR_SUCCESS if successful
    WIN32 error code otherwise

--*/
{
    LONG nRet = ERROR_SUCCESS;
    HKEY hKey;
    DWORD dwDisp;

    if (fRegister)
    {
        nRet = RegCreateKeyEx (HKEY_LOCAL_MACHINE,
                g_pszDllKey,
                0,
                TEXT(""),
                0,
                KEY_WRITE,
                NULL,
                &hKey,
                &dwDisp);
        if (ERROR_SUCCESS == nRet)
        {
            nRet = RegSetValueEx(hKey,
                    g_pszDllValue,
                    0,
                    REG_SZ,
                    (BYTE*)pszDbgName,
                    (_tcslen(pszDbgName) + 1) * sizeof(TCHAR));

            DEBUGCHK (nRet == ERROR_SUCCESS);
            RegCloseKey(hKey);
        }
    }
    else
    {
        RegDeleteKey(HKEY_LOCAL_MACHINE, g_pszDllKey);
    }

    if (ERROR_SUCCESS == nRet)
    {
        nRet = RegOpenKeyEx (HKEY_LOCAL_MACHINE,
                TEXT("init"),
                0,
                KEY_WRITE,
                &hKey);
        if (ERROR_SUCCESS == nRet)
        {
            if (fRegister)
            {
                // register
                nRet = RegSetValueEx(hKey, AUTO_LAUNCH_VALUE, 0, REG_SZ, (BYTE*)pszCmd,
                                     (_tcslen(pszCmd) + 1) * sizeof(TCHAR));
            }
            else
            {
                // unregister
                nRet = RegDeleteValue(hKey, AUTO_LAUNCH_VALUE);
                DEBUGCHK (nRet == ERROR_SUCCESS);
            }

            RegCloseKey(hKey);
        }
    }
    
    return nRet;
}


static void DumpReg (void)
{
    TCHAR buffer[MAX_PATH];
    HKEY hkey;
    LONG ret;
    DWORD size;
    DWORD type;

    ret = RegOpenKeyEx (HKEY_LOCAL_MACHINE,
            TEXT("init"),
            0,
            KEY_READ,
            &hkey);
    if (ret == ERROR_SUCCESS)
    {
        size = sizeof (buffer);
        ret = RegQueryValueEx (hkey, 
                AUTO_LAUNCH_VALUE,
                NULL,
                &type,
                (BYTE *) buffer,
                &size);

        if (ret == ERROR_SUCCESS && type == REG_SZ)
        {
            buffer[_countof (buffer) - 1] = 0;
            RETAILMSG (1, (TEXT("loaddbg: HKLM\\init\\launch03 = %s\r\n"), buffer));
        }
        else
        {
            RETAILMSG (1, (TEXT("loaddbg: no registration found.\r\n")));
        }
        RegCloseKey (hkey);
    }
    else
    {
        RETAILMSG (1, (TEXT("loaddbg: Can't open HKLM\\init\r\n")));
    }
}


int _tmain(int argc, LPCTSTR* argv)
/*++

Routine Description:

    main routine

Arguments:

    argc - number of command line args
    argv - array of command-line args

Return Value:

    ERROR_SUCCESS if successful
    WIN32 error code otherwise

--*/
{
    int iArg;
    LONG nRet;
    HKEY hKey;
    DWORD dwType;
    DWORD dwSize;
    LPCTSTR pszDbgName = TEXT("kd.dll");
    LPCTSTR pszExeName = TEXT("loaddbg.exe");
    REGTASK regtask = RT_NOTHING;           // Default to not registering
    BOOL fRun = TRUE;

    DEBUGREGISTER(NULL);

    // get the EXE name from the command line
    if (argc > 0)
    {
        pszExeName = argv[0];
    }

    // get the DLL name from the registry if its there
    nRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, g_pszDllKey, 0, 0, &hKey);
    if (ERROR_SUCCESS == nRet)
    {
        TCHAR szRegDllName[MAX_PATH];
        dwSize = sizeof (szRegDllName);
        nRet = RegQueryValueEx(hKey, g_pszDllValue, NULL, &dwType, (BYTE*)szRegDllName, &dwSize);
        if ((ERROR_SUCCESS == nRet) && (REG_SZ == dwType)) {
            pszDbgName = szRegDllName;
        }

        RegCloseKey(hKey);
    }

    nRet = ERROR_SUCCESS;

    // parse the arguments
    if ((argc > 1) && _istdigit(argv[1][0]))
    {
        // we have been called by filesys upon startup, ignore command-line args
        regtask = RT_NOTHING;
    }
    else
    {
        // process arguments normally
        for (iArg = 1; (iArg < argc) && (ERROR_SUCCESS == nRet); iArg++)
        {
            switch (argv[iArg][0])
            {
                case TEXT('-'):
                case TEXT('/'):
                    if (!_tcsicmp(argv[iArg] + 1, TEXT("norun")))
                    {
                        fRun = FALSE;
                    }
                    else if (!_tcsicmp(argv[iArg] + 1, TEXT("reg")))
                    {
                        regtask = RT_REGISTER;
                    }
                    else if (!_tcsicmp(argv[iArg] + 1, TEXT("unreg")))
                    {
                        regtask = RT_UNREGISTER;
                    }
                    else if (!_tcsicmp (argv[iArg] + 1, TEXT("noreg")))
                    {
                        regtask = RT_NOTHING;
                    }
                    else if (!_tcsicmp (argv[iArg] + 1, TEXT("dumpreg")))
                    {
                        DumpReg ();
                    }
                    else
                    {
                        nRet = ERROR_BAD_ARGUMENTS;
                    }
                    break;
                default:
                    pszDbgName = argv[iArg];
            }
        }
    }
    
    if (ERROR_SUCCESS == nRet)
    {
        if (regtask != RT_NOTHING)
        {
            // perform the registration
            nRet = RegisterDbg(pszExeName, pszDbgName, RT_REGISTER == regtask);
        }

        if (fRun)
        {
            // Load Hdstub
            if (!AttachDebugger (pszDbgName))
            {
                RETAILMSG (1, (L"Loaddbg: Failed to load debugger '%s'.\r\n", pszDbgName));
            }
       }
    }
    
    return nRet;
}
