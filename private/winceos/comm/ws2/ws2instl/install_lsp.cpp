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
#include <winsock2.h>
#include <ws2spi.h>
#include <types.h>
#include <ws2bth.h>
#include <winreg.h>
#include <list.hxx>
#include <functional>
#include <algorithm>
#include <CReg.hxx>
extern "C"
{
    void InstallThirdPartyLSP();
}
// Function installs all third party LSP's
// By calling there doInstall entry point
//XXX: Should maybe move these defines elsewhere?
typedef HRESULT (*PFN_INSTALL_SP ) ();
#define REG_PATH _T("\\COMM\\WS2\\LSP")
#define DLL_NAME _T("Dll")
#define LOAD_ORDER_NAME _T("Order")
#define INSTALL_ENTRY_POINT  _T("Entry")


class SPInstaller
{
    private:
        SPInstaller (const  SPInstaller& s)
        {
            return;
        }

    public:
        SPInstaller(size_t order):
            loadOrder(order)
        {
        }
    bool Init(HKEY hSPNode)
    {
        BOOL bRet = reg.Open(hSPNode,NULL);
        return (bRet != FALSE);
    }
    void Install() 
    {
        if (!reg.IsOK())
        {
            assert ("!SPInstaller !IsOK" && 0);
            return;
        }


        LPCTSTR wszSPDLLPath = reg.ValueSZ(DLL_NAME);
        if (!wszSPDLLPath)
        {
            assert("node had no DLL_NAME value" && 0);
            return;
        }

        //  try to load SP to get its install member.
        ce::auto_hlibrary hSPDLL = LoadLibrary(wszSPDLLPath);
        if (!hSPDLL.valid()) 
        {
            assert("can't load the dll " && 0);
            return;
        }

        static const WCHAR defaultInstallFunction[]= L"DllRegisterServer";
        LPWSTR wszInstallFunction  = (LPWSTR) reg.ValueSZ(INSTALL_ENTRY_POINT);
        if (!wszInstallFunction)
        {
            wszInstallFunction = (WCHAR*)defaultInstallFunction;
        }

        PFN_INSTALL_SP pfnInstallSP = (PFN_INSTALL_SP) GetProcAddress(hSPDLL,wszInstallFunction);
        if (!pfnInstallSP)
        {
            assert ("can't GetProcAddress install entrypoint " && 0); 
            return;
        }
        // wraps our pfnInstallSP in a __try block.
        CallUnsafeFunction(pfnInstallSP);
        return;
    }
    void CallUnsafeFunction(PFN_INSTALL_SP pfnInstallSP)
    {
        __try
        {
            pfnInstallSP();
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            assert("exception calling pfnInstallSP" && 0);
        }
    }
    friend bool operator<(const size_t& lhs, const SPInstaller& rhs );
private:
    CReg        reg;
    size_t      loadOrder;
};

bool operator<(const size_t& lhs, const SPInstaller& rhs )
{
    return (lhs < rhs.loadOrder);
}

void InstallThirdPartyLSP()
{
    using namespace std;

    // open the reg key 
    
    CReg regSPRoot(HKEY_LOCAL_MACHINE,REG_PATH);
    if (!regSPRoot.IsOK())
    {
        return;
    }

    ce::list <SPInstaller> listSPs;

    for(;;)
    {
        BOOL bRet;

        // For every child of hKeysToInstall
        WCHAR wszSPName[MAX_PATH+1];
        const DWORD cbNameOfLSP = sizeof(wszSPName)/sizeof(wszSPName[0]);
        bRet  = regSPRoot.EnumKey(wszSPName,cbNameOfLSP);
        if (bRet == FALSE)
        {
            break; //  We're done when no more keys to enumerate.
        }

        CReg regSPNode (regSPRoot,wszSPName);
        if (!regSPNode.IsOK())
        {
            assert("Regkey supposed to have a child" && 0 );
            continue;
        }


        // Get the LOADOrder for this entry.
        // If not set, pick the largest value possible.

        const size_t loadOrder =   regSPNode.ValueDW(LOAD_ORDER_NAME,(DWORD)-1); // get max symbol

        // Create a SPInstaller, in the correct order of the list.

        ce::list<SPInstaller>::iterator it = 
            listSPs.insert( 
                    std::upper_bound(listSPs.begin(), listSPs.end(), loadOrder),  
                    loadOrder /*Create SPInstaller from loadOrder*/
                    );

        if (it == listSPs.end())
        {
            assert("OOM Installation failed" && 0);
            return;
        }
        bRet = it->Init(regSPNode);
        if (bRet == FALSE)
        {
            assert("SPInstaller->Init Failed" && 0 );
            // erase the item
            listSPs.erase(it);
        }
    }

    // Install every  SP.
    for_each(listSPs.begin(), listSPs.end(),
            mem_fun_ref_t<void,SPInstaller> (&SPInstaller::Install));

    return;

}
