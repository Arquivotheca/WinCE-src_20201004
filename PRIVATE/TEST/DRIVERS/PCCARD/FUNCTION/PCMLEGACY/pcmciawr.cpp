//******************************************************************************
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************

/*++
Module Name:  
	pcmciawr.cpp

Abstract:

    implementation of PCMCIA Legacy API wrappers

--*/

#include <windows.h>
#include <types.h>
#include "cardsv2.h"
#include "pcmciawr.h"

PFN_CardRegisterClient      pfnCardRegisterClient = NULL;
PFN_CardDeregisterClient    pfnCardDeregisterClient = NULL;

PFN_CardGetFirstTuple       pfnCardGetFirstTuple = NULL;
PFN_CardGetNextTuple        pfnCardGetNextTuple = NULL;
PFN_CardGetTupleData        pfnCardGetTupleData = NULL;
PFN_CardGetParsedTuple	pfnCardGetParsedTuple = NULL;

PFN_CardRequestExclusive    pfnCardRequestExclusive = NULL;
PFN_CardReleaseExclusive    pfnCardReleaseExclusive = NULL;
PFN_CardRequestDisable      pfnCardRequestDisable = NULL;
PFN_CardResetFunction       pfnCardResetFunction = NULL;

PFN_CardRequestWindow       pfnCardRequestWindow = NULL;
PFN_CardReleaseWindow       pfnCardReleaseWindow = NULL;
PFN_CardMapWindow		  pfnCardMapWindow = NULL;
PFN_CardMapWindowPhysical   pfnCardMapWindowPhysical = NULL;

PFN_CardGetStatus           pfnCardGetStatus = NULL;
PFN_CardRequestConfiguration pfnCardRequestConfiguration = NULL;
PFN_CardModifyConfiguration pfnCardModifyConfiguration = NULL;
PFN_CardReleaseConfiguration pfnCardReleaseConfiguration = NULL;

PFN_CardRequestConfigRegisterPhAddr pfnCardRequestConfigRegisterPhAddr = NULL;
PFN_CardAccessConfigurationRegister pfnCardAccessConfigurationRegister = NULL;

PFN_CardRequestIRQLine      pfnCardRequestIRQLine = NULL;
PFN_CardReleaseIRQ          pfnCardReleaseIRQ = NULL;
PFN_CardRequestIRQ		pfnCardRequestIRQ = NULL;

PFN_CardGetEventMask		pfnCardGetEventMask = NULL;
PFN_CardSetEventMask		pfnCardSetEventMask = NULL;

HINSTANCE hPCMCIADll = NULL;

BOOL PCMCIAWR_Init(LPCTSTR lpDllName)
{
	
    hPCMCIADll = LoadLibrary(lpDllName);
    if (hPCMCIADll == NULL) {
        NKDbgPrintfW(TEXT("PcmciaN: LoadLibrary(%s) failed %d\r\n"),lpDllName,GetLastError());
        return FALSE;
    }
    else {
        pfnCardRegisterClient = (PFN_CardRegisterClient)GetProcAddress(hPCMCIADll, TEXT("CardRegisterClient"));
        pfnCardDeregisterClient =(PFN_CardDeregisterClient)GetProcAddress(hPCMCIADll, TEXT("CardDeregisterClient"));
        
        pfnCardGetFirstTuple =  (PFN_CardGetFirstTuple)GetProcAddress(hPCMCIADll, TEXT("CardGetFirstTuple"));
        pfnCardGetNextTuple =(PFN_CardGetNextTuple)GetProcAddress(hPCMCIADll, TEXT("CardGetNextTuple"));
        pfnCardGetTupleData = (PFN_CardGetTupleData)GetProcAddress(hPCMCIADll, TEXT("CardGetTupleData"));
        pfnCardGetParsedTuple = (PFN_CardGetParsedTuple)GetProcAddress(hPCMCIADll, TEXT("CardGetParsedTuple"));

        pfnCardRequestExclusive = (PFN_CardRequestExclusive)GetProcAddress(hPCMCIADll, TEXT("CardRequestExclusive"));
        pfnCardReleaseExclusive = (PFN_CardReleaseExclusive)GetProcAddress(hPCMCIADll,TEXT("CardReleaseExclusive"));;
        pfnCardRequestDisable = (PFN_CardRequestDisable)GetProcAddress(hPCMCIADll,TEXT("CardRequestDisable")) ;
        
        pfnCardResetFunction = (PFN_CardResetFunction)GetProcAddress(hPCMCIADll,TEXT("CardResetFunction"));

        pfnCardRequestWindow = (PFN_CardRequestWindow)GetProcAddress(hPCMCIADll,TEXT("CardRequestWindow"));
        pfnCardReleaseWindow = (PFN_CardReleaseWindow)GetProcAddress(hPCMCIADll,TEXT("CardReleaseWindow"));
        pfnCardMapWindow = (PFN_CardMapWindow)GetProcAddress(hPCMCIADll,TEXT("CardMapWindow"));
        pfnCardMapWindowPhysical = (PFN_CardMapWindowPhysical)GetProcAddress(hPCMCIADll,TEXT("CardMapWindowPhysical"));

        pfnCardGetStatus = (PFN_CardGetStatus)GetProcAddress(hPCMCIADll,TEXT("CardGetStatus"));; 
        pfnCardRequestConfiguration =(PFN_CardRequestConfiguration)GetProcAddress(hPCMCIADll,TEXT("CardRequestConfiguration"));
        pfnCardModifyConfiguration = (PFN_CardModifyConfiguration)GetProcAddress(hPCMCIADll,TEXT("CardModifyConfiguration"));;
        pfnCardReleaseConfiguration = (PFN_CardReleaseConfiguration)GetProcAddress(hPCMCIADll,TEXT("CardReleaseConfiguration"));
        pfnCardRequestConfigRegisterPhAddr = (PFN_CardRequestConfigRegisterPhAddr)GetProcAddress(hPCMCIADll,TEXT("CardRequestConfigRegisterPhAddr"));
        pfnCardAccessConfigurationRegister = (PFN_CardAccessConfigurationRegister)GetProcAddress(hPCMCIADll,TEXT("CardAccessConfigurationRegister"));
        
        pfnCardRequestIRQLine = (PFN_CardRequestIRQLine)GetProcAddress(hPCMCIADll,TEXT("CardRequestIRQLine"));
        pfnCardRequestIRQ = (PFN_CardRequestIRQ)GetProcAddress(hPCMCIADll,TEXT("CardRequestIRQ"));
        pfnCardReleaseIRQ = (PFN_CardReleaseIRQ)GetProcAddress(hPCMCIADll,TEXT("CardReleaseIRQ"));

        pfnCardGetEventMask = (PFN_CardGetEventMask)GetProcAddress(hPCMCIADll,TEXT("CardGetEventMask"));
        pfnCardSetEventMask = (PFN_CardSetEventMask)GetProcAddress(hPCMCIADll,TEXT("CardSetEventMask"));
		
    }
    return TRUE;
}
VOID PCMCIAWR_DeInit()
{
    if (hPCMCIADll) {
        BOOL bFree=FreeLibrary(hPCMCIADll);
        ASSERT(bFree==TRUE);
    }
	
    pfnCardRegisterClient = NULL;
    pfnCardDeregisterClient = NULL;
    
    pfnCardGetFirstTuple = NULL;
    pfnCardGetNextTuple = NULL;
    pfnCardGetTupleData = NULL;
    pfnCardGetParsedTuple = NULL;

    pfnCardRequestExclusive = NULL;
    pfnCardReleaseExclusive = NULL;
    pfnCardRequestDisable = NULL;
    
    pfnCardResetFunction = NULL;

    pfnCardRequestWindow = NULL;
    pfnCardReleaseWindow = NULL;
    pfnCardMapWindow = NULL;
    pfnCardMapWindowPhysical = NULL;

    pfnCardGetStatus = NULL; 
    pfnCardRequestConfiguration = NULL;
    pfnCardModifyConfiguration = NULL;
    pfnCardReleaseConfiguration = NULL;
    pfnCardRequestConfigRegisterPhAddr = NULL;
    pfnCardAccessConfigurationRegister = NULL;
    
    pfnCardRequestIRQLine = NULL;
    pfnCardRequestIRQ = NULL;
    pfnCardReleaseIRQ = NULL;

    pfnCardGetEventMask = NULL;
    pfnCardSetEventMask = NULL;
	
    hPCMCIADll = NULL;
}
CARD_CLIENT_HANDLE CardRegisterClient(CLIENT_CALLBACK CallBackFn, PCARD_REGISTER_PARMS pParms)
{
    CARD_CLIENT_HANDLE pReturn = NULL;
    if (pfnCardRegisterClient!=NULL) {
        __try {
            pReturn = pfnCardRegisterClient(CallBackFn, pParms);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            pReturn = NULL;
        }
    }
    return pReturn;
}
STATUS CardDeregisterClient(CARD_CLIENT_HANDLE hCardClient)
{
    STATUS status=CERR_BAD_HANDLE;
    if (pfnCardDeregisterClient!=NULL) {
        __try {
            status=pfnCardDeregisterClient(hCardClient);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            status=CERR_BAD_HANDLE;
        }
    }
    return status;
}

STATUS CardGetFirstTuple(PCARD_TUPLE_PARMS pGetTupleParms)
{
    STATUS status=CERR_BAD_HANDLE;
    if (pfnCardGetFirstTuple!=NULL) {
        __try {
            status=pfnCardGetFirstTuple(pGetTupleParms);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            status=CERR_BAD_HANDLE;
        }
    }
    return status;
}
STATUS CardGetNextTuple(PCARD_TUPLE_PARMS pGetTupleParms)
{
    STATUS status=CERR_BAD_HANDLE;
    if (pfnCardGetNextTuple!=NULL) {
        __try {
            status=pfnCardGetNextTuple(pGetTupleParms);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            status=CERR_BAD_HANDLE;
        }
    }
    return status;
}
STATUS CardGetTupleData(PCARD_DATA_PARMS pGetTupleData)
{
    STATUS status=CERR_BAD_HANDLE;
    if (pfnCardGetTupleData!=NULL) {
        __try {
            status=pfnCardGetTupleData(pGetTupleData);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            status=CERR_BAD_HANDLE;
        }
    }
    return status;
}

STATUS CardGetParsedTuple(CARD_SOCKET_HANDLE hSocket, UINT8 uDesiredTuple, PVOID pBuf, PUINT32 pnItems)
{
    STATUS status=CERR_BAD_HANDLE;
    if (pfnCardGetParsedTuple!=NULL) {
        __try {
            status=pfnCardGetParsedTuple(hSocket, uDesiredTuple, pBuf, pnItems);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            status=CERR_BAD_HANDLE;
        }
    }
    return status;
}

STATUS CardRequestExclusive(CARD_CLIENT_HANDLE hCardClient, CARD_SOCKET_HANDLE hSocket)
{
    STATUS status=CERR_BAD_HANDLE;
    if (pfnCardRequestExclusive!=NULL) {
        __try {
            status=pfnCardRequestExclusive(hCardClient,hSocket);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            status=CERR_BAD_HANDLE;
        }
    }
    return status;
    
}
STATUS CardReleaseExclusive(CARD_CLIENT_HANDLE hCardClient, CARD_SOCKET_HANDLE hSocket)
{
    STATUS status=CERR_BAD_HANDLE;
    if (pfnCardReleaseExclusive!=NULL) {
        __try {
            status=pfnCardReleaseExclusive(hCardClient,hSocket);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            status=CERR_BAD_HANDLE;
        }
    }
    return status;    
}
STATUS CardRequestDisable(CARD_CLIENT_HANDLE hCardClient, CARD_SOCKET_HANDLE hSocket)
{
    STATUS status=CERR_BAD_HANDLE;
    if (pfnCardRequestDisable!=NULL) {
        __try {
            status=pfnCardRequestDisable(hCardClient,hSocket);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            status=CERR_BAD_HANDLE;
        }
    }
    return status;    
}

STATUS CardResetFunction(CARD_CLIENT_HANDLE hCardClient, CARD_SOCKET_HANDLE hSock)
{
    STATUS status=CERR_BAD_HANDLE;
    if (pfnCardResetFunction!=NULL) {
        __try {
            status=pfnCardResetFunction(hCardClient,hSock);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            status=CERR_BAD_HANDLE;
        }
    }
    return status;    
}

CARD_WINDOW_HANDLE CardRequestWindow(CARD_CLIENT_HANDLE hCardClient, PCARD_WINDOW_PARMS pCardWinParms)
{
    CARD_WINDOW_HANDLE hWindow = NULL;
    if (pfnCardRequestWindow!=NULL) {
        __try {
             hWindow =pfnCardRequestWindow(hCardClient,pCardWinParms);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
             hWindow = NULL;;
        }
    }
    return hWindow;    
}
STATUS CardReleaseWindow(CARD_WINDOW_HANDLE hCardWin)
{
    STATUS status=CERR_BAD_HANDLE;
    if (pfnCardReleaseWindow!=NULL) {
        __try {
            status=pfnCardReleaseWindow(hCardWin);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            status=CERR_BAD_HANDLE;
        }
    }
    return status;    

}

PVOID CardMapWindow(CARD_WINDOW_HANDLE hCardWindow, UINT32 uCardAddress, UINT32 uSize, PUINT32 pGranularity)
{
    PVOID hWin = NULL;
    if (pfnCardMapWindow!=NULL) {
        __try {
            hWin=pfnCardMapWindow(hCardWindow, uCardAddress, uSize, pGranularity);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            hWin = NULL;
        }
    }
    return hWin;    

}

STATUS CardMapWindowPhysical(CARD_WINDOW_HANDLE hCardWindow, PCARD_WINDOW_ADDRESS pCardWindowAddr) 
{
    STATUS status=CERR_BAD_HANDLE;
    if (pfnCardMapWindowPhysical!=NULL) {
        __try {
            status=pfnCardMapWindowPhysical(hCardWindow,pCardWindowAddr);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            status=CERR_BAD_HANDLE;
        }
    }
    return status;    

}
    
STATUS CardGetStatus(PCARD_STATUS pStatus)
{
    STATUS status=CERR_BAD_HANDLE;
    if (pfnCardGetStatus!=NULL) {
        __try {
            status=pfnCardGetStatus(pStatus);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            status=CERR_BAD_HANDLE;
        }
    }
    return status;    
}

STATUS CardRequestConfiguration(CARD_CLIENT_HANDLE hCardClient, PCARD_CONFIG_INFO pParms)
{
    STATUS status=CERR_BAD_HANDLE;
    if (pfnCardRequestConfiguration!=NULL) {
        __try {
            status=pfnCardRequestConfiguration(hCardClient,pParms);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            status=CERR_BAD_HANDLE;
        }
    }
    return status;    
}
STATUS CardModifyConfiguration(CARD_CLIENT_HANDLE hCardClient, CARD_SOCKET_HANDLE hSock,
                      PUINT16 fAttributes)
{
    STATUS status=CERR_BAD_HANDLE;
    if (pfnCardModifyConfiguration!=NULL) {
        __try {
            status=pfnCardModifyConfiguration(hCardClient, hSock,fAttributes);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            status=CERR_BAD_HANDLE;
        }
    }
    return status;    
}
STATUS CardReleaseConfiguration(CARD_CLIENT_HANDLE hCardClient, CARD_SOCKET_HANDLE hSock)
{
    STATUS status=CERR_BAD_HANDLE;
    if (pfnCardReleaseConfiguration!=NULL) {
        __try {
            status=pfnCardReleaseConfiguration(hCardClient, hSock);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            status=CERR_BAD_HANDLE;
        }
    }
    return status;    
    
}
STATUS CardRequestConfigRegisterPhAddr(CARD_CLIENT_HANDLE hCardClient,CARD_SOCKET_HANDLE hSock,PCARD_WINDOW_ADDRESS pCardWindowAddr,PDWORD pOffset)
{
    STATUS status=CERR_BAD_HANDLE;
    if (pfnCardRequestConfigRegisterPhAddr!=NULL) {
        __try {
            status=pfnCardRequestConfigRegisterPhAddr(hCardClient, hSock, pCardWindowAddr,pOffset);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            status=CERR_BAD_HANDLE;
        }
    }
    return status;        
}

STATUS CardAccessConfigurationRegister(CARD_CLIENT_HANDLE hCardClient,
                                       CARD_SOCKET_HANDLE hSock,UINT8 rw_flag,
                                       UINT8 offset,UINT8 *pValue)
{
    STATUS status=CERR_BAD_HANDLE;
    if (pfnCardAccessConfigurationRegister!=NULL) {
        __try {
            status=pfnCardAccessConfigurationRegister(hCardClient, hSock, rw_flag,offset,pValue);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            status=CERR_BAD_HANDLE;
        }
    }
    return status;        
}
// Only Support By Lagacy Driver
STATUS CardReleaseIRQ(CARD_CLIENT_HANDLE hCardClient, CARD_SOCKET_HANDLE hSocket)
{
    STATUS status=CERR_BAD_HANDLE;
    if (pfnCardReleaseIRQ!=NULL) {
        __try {
            status=pfnCardReleaseIRQ(hCardClient,hSocket);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            status=CERR_BAD_HANDLE;
        }
    }
    return status;    
}
STATUS CardRequestIRQLine(CARD_CLIENT_HANDLE hCardClient, CARD_SOCKET_HANDLE hSocket, UINT16 uSupportedIrqBit, PDWORD pdwIrqNumber, PDWORD  pdwSysIrqNumber)
{
    STATUS status=CERR_BAD_HANDLE;
    if (pfnCardRequestIRQLine!=NULL) {
        __try {
            status=pfnCardRequestIRQLine(hCardClient,hSocket,uSupportedIrqBit, pdwIrqNumber, pdwSysIrqNumber);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            status=CERR_BAD_HANDLE;
        }
    }
    return status;    
}

STATUS CardRequestIRQ(CARD_CLIENT_HANDLE hCardClient, CARD_SOCKET_HANDLE hSocket,
                                  CARD_ISR ISRFunction, UINT32 uISRContextData){
    STATUS status=CERR_BAD_HANDLE;
    if (pfnCardRequestIRQ!=NULL) {
        __try {
            status=pfnCardRequestIRQ(hCardClient,hSocket,ISRFunction, uISRContextData);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            status=CERR_BAD_HANDLE;
        }
    }
    return status;    
}

STATUS CardGetEventMask(CARD_CLIENT_HANDLE hCardClient, PCARD_EVENT_MASK_PARMS pMaskParms)
{
    STATUS status=CERR_BAD_HANDLE;
    if (pfnCardGetEventMask!=NULL) {
        __try {
            status=pfnCardGetEventMask(hCardClient,pMaskParms);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            status=CERR_BAD_HANDLE;
        }
    }
    return status;    
}

STATUS CardSetEventMask(CARD_CLIENT_HANDLE hCardClient, PCARD_EVENT_MASK_PARMS pMaskParms)
{
    STATUS status=CERR_BAD_HANDLE;
    if (pfnCardSetEventMask!=NULL) {
        __try {
            status=pfnCardSetEventMask(hCardClient,pMaskParms);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            status=CERR_BAD_HANDLE;
        }
    }
    return status;    
}

