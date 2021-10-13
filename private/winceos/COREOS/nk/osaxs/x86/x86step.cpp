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
/*++

Module Name:

    x86step.cpp

Abstract:

    X86 single step logic

Environment:

    OsaxsT, OsaxsH

--*/

#ifndef BUILDING_UNIT_TEST
#include "osaxs_p.h"
#include "step.h"
#endif

const DWORD TrapFlag = 0x100;

enum Register {
    EAX,
    ECX,
    EDX,
    EBX,
    ESP,
    EBP,
    ESI,
    EDI
};


struct ModRM
{
    Register DestReg;
    enum
    {
        UNUSED,
        DIRECT,
        INDIRECT
    } BaseRegRule;
    Register BaseReg;
    BOOL fUseIndex;
    Register IdxReg;
    ULONG Scale;
    BOOL fUseDisp;
    ULONG Disp;
};


#ifndef BUILDING_UNIT_TEST
#define ReadInstByte ReadByte
#define ReadEADword ReadDword
#define ReadInstDword ReadDword
#define ReadStack ReadDword
#endif

BOOL DecodeModRM(ModRM *pModRM, ULONG *InstSize)
{
    BOOL fSuccess;
    DWORD dw;
    BOOL f;
    BYTE b;

    f = ReadInstByte(DD_ExceptionState.context->Eip + *InstSize, &b);
    if (!f)
    {
        goto error;
    }

    ++(*InstSize);
    
    ULONG mod = (b >> 6) & 0x3;
    ULONG reg = (b >> 3) & 0x7;
    ULONG rm  = b & 0x7;
    ULONG dispSize = 0;

    pModRM->fUseIndex = false;
    pModRM->fUseDisp  = false;
    pModRM->Disp      = 0;
    pModRM->DestReg   = static_cast<Register>(reg);
    pModRM->BaseRegRule = (mod != 3)? ModRM::INDIRECT : ModRM::DIRECT;

    if (mod == 1)
    {
        dispSize = 1;
    }
    else if (mod == 2)
    {
        dispSize = 4;
    }

    pModRM->BaseReg = static_cast<Register>(rm);
    if (pModRM->BaseReg == EBP && dispSize == 0)
    {
        dispSize = 4;
        pModRM->BaseRegRule = ModRM::UNUSED;
    }
    else if (pModRM->BaseReg == ESP)
    {
        // Decode the SIB
        f = ReadInstByte(DD_ExceptionState.context->Eip + *InstSize, &b);
        if (!f)
        {
            goto error;
        }
        ++(*InstSize);
        
        ULONG scale = (b >> 6) & 0x3;
        ULONG index = (b >> 3) & 0x7;
        ULONG base  = b & 0x7;

        pModRM->Scale     = (1 << scale);
        pModRM->fUseIndex = index != ESP;
        pModRM->IdxReg    = static_cast<Register>(index);

        if (base == EBP && mod == 0)
        {
            pModRM->BaseRegRule = ModRM::UNUSED;
        }
        else
        {
            pModRM->BaseRegRule = ModRM::INDIRECT;
        }

        pModRM->BaseReg = static_cast<Register>(base);
    }

    switch (dispSize)
    {
        case 1:
            f = ReadInstByte(DD_ExceptionState.context->Eip + *InstSize, &b);
            if (!f)
            {
                goto error;
            }
            pModRM->Disp = (char)b;
            pModRM->fUseDisp = TRUE;
            break;
            
        case 4:
            f = ReadInstDword(DD_ExceptionState.context->Eip + *InstSize, &dw);
            if (!f)
            {
                goto error;
            }
            pModRM->Disp = dw;
            pModRM->fUseDisp = TRUE;
            break;
        case 0:
            break;
        default:
            goto error;
    }

    *InstSize += dispSize;
    fSuccess = TRUE;
exit:
    return fSuccess;
error:
    fSuccess = FALSE;
    goto exit;
}


ULONG EvalReg(Register reg)
{
    switch (reg)
    {
        case EAX:   return DD_ExceptionState.context->Eax;
        case ECX:   return DD_ExceptionState.context->Ecx;
        case EDX:   return DD_ExceptionState.context->Edx;
        case EBX:   return DD_ExceptionState.context->Ebx;
        case ESP:   return DD_ExceptionState.context->Esp;
        case EBP:   return DD_ExceptionState.context->Ebp;
        case ESI:   return DD_ExceptionState.context->Esi;
        case EDI:   return DD_ExceptionState.context->Edi;
    }
    return 0;
}


BOOL EvalModRM(ModRM const *pModRM, ULONG *pResult)
{
    BOOL fSuccess;

    if (pModRM->BaseRegRule == ModRM::DIRECT)
    {
        *pResult = EvalReg(pModRM->BaseReg);
    }
    else
    {
        ULONG EA = 0;
        if (pModRM->BaseRegRule == ModRM::INDIRECT)
        {
            EA += EvalReg(pModRM->BaseReg);
        }

        if (pModRM->fUseIndex)
        {
            EA += pModRM->Scale * EvalReg(pModRM->IdxReg);
        }

        if (pModRM->fUseDisp)
        {
            EA += pModRM->Disp;
        }

        DWORD dw;
        BOOL f = ReadEADword(EA, &dw);
        if (!f)
        {
            goto error;
        }

        *pResult = dw;
    }

    fSuccess = TRUE;
exit:
    return fSuccess;
error:
    fSuccess = FALSE;
    goto exit;
}


BOOL X86ComputeNextOffset(DWORD *pNextOffset, DWORD *pInstrSize, BOOL *pfIsCall, BOOL *pfIsRet)
{
    BOOL fSuccess;
    BOOL f;

    *pfIsRet  = FALSE;
    *pfIsCall = FALSE;
    *pInstrSize = 0;
    if (DD_OnEmbeddedBreakpoint())
    {
        ULONG PostBreak = DD_ExceptionState.context->Eip + 1;
        *pNextOffset = PostBreak;
        *pInstrSize  = 1;
        *pfIsCall    = FALSE;
    }
    else
    {
        BYTE b;
        f = ReadInstByte(DD_ExceptionState.context->Eip, &b);
        if (!f)
        {
            goto error;
        }
        switch (b)
        {
            case 0xe8:                      // call
            {
                const ULONG InstSize = 5;
                DWORD relative;
                f = ReadInstDword(DD_ExceptionState.context->Eip + 1, &relative);
                if (!f)
                {
                    goto error;
                }
                ULONG returnaddr = DD_ExceptionState.context->Eip + InstSize;
                
                *pNextOffset = returnaddr + relative;
                *pInstrSize = InstSize;
                *pfIsCall = TRUE;
                *pfIsRet  = FALSE;
                break;
            }

            case 0xff:                      // call
            {
                ModRM modrm;
                ULONG InstSize = 1;
                f = DecodeModRM(&modrm, &InstSize);
                if (!f)
                {
                    goto error;
                }

                ULONG result;
                f = EvalModRM(&modrm, &result);
                if (!f)
                {
                    goto error;
                }

                *pNextOffset = result;
                *pInstrSize  = InstSize;
                *pfIsCall    = TRUE;
                *pfIsRet     = FALSE;
                break;
            }

            case 0xC3:                  // near ret
            case 0xC2:
            {
                ULONG ReturnAddr;
                f = ReadStack(DD_ExceptionState.context->Esp, &ReturnAddr);
                if (!f)
                {
                    goto error;
                }
                *pNextOffset   = ReturnAddr;
                *pfIsCall      = FALSE;
                *pfIsRet       = TRUE;
                if (b == 0xC2)
                {
                    *pInstrSize = 3;
                }
                else
                {
                    *pInstrSize = 1;
                }

                break;
            }
            
            default:
                goto error;
        }
    }

    fSuccess = TRUE;

exit:
    return fSuccess;

error:
    fSuccess = FALSE;
    goto exit;
}



#ifndef BUILDING_UNIT_TEST

HRESULT SingleStep::CpuSetup()
{
    DEBUGGERMSG(OXZONE_STEP, (L"+SS:x86\r\n"));

    ULONG Next;
    ULONG Size;
    BOOL fIsCall;
    BOOL fIsRet;
    HRESULT hr;

    BOOL fCanCompute = X86ComputeNextOffset(&Next, &Size, &fIsCall, &fIsRet);
    if (fCanCompute)
    {
        DEBUGGERMSG(OXZONE_STEP, (L" SS:x86 computed offset Next:%08X, Size:%d, Call:%d, Ret:%d\r\n",Next,Size,fIsCall,fIsRet));
        ULONG Target;
        if (fIsCall || fIsRet)
        {
            Target = DD_TranslateOffset(Next);
            if (Target == Next)
            {
                // Don't bother if it's not a psl.
                DEBUGGERMSG(OXZONE_STEP, (L" SS:x86 call / return default to trap flag\r\n"));
                DD_ExceptionState.context->EFlags |= TrapFlag;
                hr = S_OK;
                goto exit;
            }
        }
        else
        {
            Target = Next;
        }

        hr = SetBP((void *) Target, FALSE, &bp);
        if (FAILED(hr))
        {
            goto error;
        }

        if (fIsCall)
        {
            ULONG SafetyOffset = DD_ExceptionState.context->Eip + Size;
            hr = SetBP((void *) SafetyOffset, FALSE, &safetybp);
            if (FAILED(hr))
            {
                DEBUGGERMSG(OXZONE_ALERT, (L" SS: Failed to set safetybp, hr=%08X\r\n", hr));
                safetybp = NULL;
            }
        }
    }
    else
    {
        DEBUGGERMSG(OXZONE_STEP, (L" SS:x86 default to trap flag\r\n"));
        DD_ExceptionState.context->EFlags |= TrapFlag;
    }
    hr = S_OK;

exit:
    DEBUGGERMSG(OXZONE_STEP, (L"-SS:x86 hr=%08X\r\n", hr));
    return hr;

error:
    DEBUGGERMSG(OXZONE_ALERT, (L" SS:x86 failed %08X\r\n", hr));
    goto exit;
}


HRESULT SingleStep::GetNextPC(ULONG *Pc)
{
    ULONG Next, Size;
    BOOL f, fCall, fRet;
    HRESULT hr;

    f = X86ComputeNextOffset(&Next, &Size, &fCall, &fRet);
    if (f)
    {
        if (fCall || fRet)
        {
            DWORD Translated = DD_TranslateOffset(Next);
            if (Translated == Next)
            {
                hr = KDBG_E_FAIL;
                goto exit;
            }
            Next = Translated;
        }
        *Pc = Next;
    }
    else
    {
        hr = KDBG_E_FAIL;
        goto exit;
    }

    hr = S_OK;
exit:
    return hr;
}

void SingleStep::SimulateDebugBreakExecution(void)
{
    return;
}

#endif

