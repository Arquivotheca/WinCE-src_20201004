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
//+---------------------------------------------------------------------------------
//
//
// File:
//      stackwalk.h
//
// Contents:
//
//      Declaration of StackWalker class
//
//----------------------------------------------------------------------------------

#if !defined(__STACKWALK_H_INCLUDED__) && !defined(UNDER_CE)
#define __STACKWALK_H_INCLUDED__

#include <imagehlp.h>

class Symbol 
{
    friend class StackWalker;

public:
    ~Symbol();

    const char *moduleName() const 
    {
        return _moduleName;
    }
        
    const char *symbolName() const 
    {
        return _symbolName;
    }

    DWORD displacement() const 
    {
        return _displacement;
    }
        
    Symbol *next() const 
    {
        return _next;
    }
        
    void AppendDisplacement(char * sz)
    {
        char szDisp[16];
        wsprintfA(szDisp, " + 0x%X", _displacement);
        lstrcatA(sz, szDisp);
    }


private:
    Symbol(const char* moduleName, const char* symbolName, DWORD displacement);
    void Append(Symbol*);
    
    char*    _moduleName;
    char*    _symbolName;
    DWORD    _displacement;

    Symbol*    _next;
};


class StackWalker 
{
public:
    StackWalker(HANDLE hProcess);
    ~StackWalker();

    Symbol* ResolveAddress(DWORD addr);
    Symbol* CreateStackTrace(CONTEXT*);
    BOOL GetCallStack(Symbol * symbol, int nChars, WCHAR * sz);
    int GetCallStackSize(Symbol* symbol);

private:
    static DWORD __stdcall GetModuleBase(HANDLE hProcess, DWORD address);
    static DWORD LoadModule(HANDLE hProcess, DWORD address);

private:
    typedef BOOL (__stdcall *SymGetModuleInfoFunc)(HANDLE hProcess,
                                           DWORD dwAddr,
                                           PIMAGEHLP_MODULE ModuleInfo);
                                           
    typedef BOOL (__stdcall *SymGetSymFromAddrFunc)(HANDLE hProcess,
                                            DWORD dwAddr,
                                            PDWORD pdwDisplacement,
                                            PIMAGEHLP_SYMBOL Symbol);
                                            
    typedef DWORD (__stdcall *SymLoadModuleFunc)(HANDLE hProcess,
                                         HANDLE hFile,
                                         PSTR ImageName,
                                         PSTR ModuleName,
                                         DWORD BaseOfDll,
                                         DWORD SizeOfDll);
                                         
    typedef BOOL (__stdcall *StackWalkFunc)(DWORD MachineType,
                                    HANDLE hProcess,
                                    HANDLE hThread,
                                    LPSTACKFRAME StackFrame,
                                    LPVOID ContextRecord,
                                    PREAD_PROCESS_MEMORY_ROUTINE ReadMemoryRoutine,
                                    PFUNCTION_TABLE_ACCESS_ROUTINE FunctionTableAccessRoutine,
                                    PGET_MODULE_BASE_ROUTINE GetModuleBaseRoutine,
                                    PTRANSLATE_ADDRESS_ROUTINE TranslateAddress);
                                    
    typedef BOOL (__stdcall *UndecorateSymbolNameFunc)(LPSTR DecName,
                                         LPSTR UnDecName,
                                         DWORD UnDecNameLength,
                                         DWORD Flags);

private:
    HMODULE                           _imageHlpDLL;
    HANDLE                            _hProcess;
    EXCEPTION_POINTERS                m_exceptionpts;

    static SymGetModuleInfoFunc       _SymGetModuleInfo;
    static SymGetSymFromAddrFunc      _SymGetSymFromAddr;
    static SymLoadModuleFunc          _SymLoadModule;
    static StackWalkFunc              _StackWalk;
    static UndecorateSymbolNameFunc   _UndecorateSymbolName;
    static PFUNCTION_TABLE_ACCESS_ROUTINE    _SymFunctionTableAccess;
};

#endif  // __STACKWALK_H_INCLUDED__
