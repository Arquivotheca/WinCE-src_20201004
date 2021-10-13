//******************************************************************************
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************

/*++
Module Name:  
	pcmciawr.h

Abstract:

    Definition of Pcmcia Legacy API wrappers

--*/


#ifndef __PCMCIA_WRAPPER_
#define __PCMCIA_WRAPPER_

    BOOL PCMCIAWR_Init(LPCTSTR lpDllName) ;
    VOID PCMCIAWR_DeInit();
    //Access Function
    CARD_CLIENT_HANDLE CardRegisterClient(CLIENT_CALLBACK CallBackFn, PCARD_REGISTER_PARMS pParms);
    STATUS CardDeregisterClient(CARD_CLIENT_HANDLE hCardClient);

    STATUS CardGetFirstTuple(PCARD_TUPLE_PARMS pGetTupleParms);
    STATUS CardGetNextTuple(PCARD_TUPLE_PARMS pGetTupleParms);
    STATUS CardGetTupleData(PCARD_DATA_PARMS pGetTupleData);
    STATUS CardGetParsedTuple(CARD_SOCKET_HANDLE hSocket, UINT8 uDesiredTuple, PVOID pBuf, PUINT32 pnItems);

    STATUS CardRequestExclusive(CARD_CLIENT_HANDLE hCardClient, CARD_SOCKET_HANDLE hSocket);
    STATUS CardReleaseExclusive(CARD_CLIENT_HANDLE hCardClient, CARD_SOCKET_HANDLE hSocket);
    STATUS CardRequestDisable(CARD_CLIENT_HANDLE hCardClient, CARD_SOCKET_HANDLE hSocket);
    STATUS CardResetFunction(CARD_CLIENT_HANDLE hCardClient, CARD_SOCKET_HANDLE hSock);

    CARD_WINDOW_HANDLE CardRequestWindow(CARD_CLIENT_HANDLE hCardClient, PCARD_WINDOW_PARMS pCardWinParms);
    STATUS CardReleaseWindow(CARD_WINDOW_HANDLE hCardWin);   
    PVOID CardMapWindow(CARD_WINDOW_HANDLE hCardWindow, UINT32 uCardAddress, UINT32 uSize, PUINT32 pGranularity);
    STATUS CardMapWindowPhysical(CARD_WINDOW_HANDLE hCardWindow, PCARD_WINDOW_ADDRESS pCardWindowAddr) ;
        
    STATUS CardGetStatus(PCARD_STATUS pStatus);

    STATUS CardRequestConfiguration(CARD_CLIENT_HANDLE hCardClient, PCARD_CONFIG_INFO pParms);
    STATUS CardModifyConfiguration(CARD_CLIENT_HANDLE hCardClient, CARD_SOCKET_HANDLE hSock,
                          PUINT16 fAttributes);
    STATUS CardReleaseConfiguration(CARD_CLIENT_HANDLE hCardClient, CARD_SOCKET_HANDLE hSock);
    STATUS CardRequestConfigRegisterPhAddr(CARD_CLIENT_HANDLE hCardClient,CARD_SOCKET_HANDLE hSock,PCARD_WINDOW_ADDRESS pCardWindowAddr,PDWORD pOffset);
    STATUS CardAccessConfigurationRegister(CARD_CLIENT_HANDLE hCardClient,
                                           CARD_SOCKET_HANDLE hSock,UINT8 rw_flag,
                                           UINT8 offset,UINT8 *pValue);
    // Only Support By Lagacy Driver
    STATUS CardReleaseIRQ(CARD_CLIENT_HANDLE hCardClient, CARD_SOCKET_HANDLE hSocket);
    STATUS CardRequestIRQLine(CARD_CLIENT_HANDLE hCardClient, CARD_SOCKET_HANDLE hSocket, UINT16 uSupportedIrqBit, PDWORD pdwIrqNumber, PDWORD pdwSysIrqNumber);
    STATUS CardRequestIRQ(CARD_CLIENT_HANDLE hCardClient, CARD_SOCKET_HANDLE hSocket,
                                  CARD_ISR ISRFunction, UINT32 uISRContextData);

    STATUS CardGetEventMask(CARD_CLIENT_HANDLE hCardClient, PCARD_EVENT_MASK_PARMS pMaskParms);
    STATUS CardSetEventMask(CARD_CLIENT_HANDLE hCardClient, PCARD_EVENT_MASK_PARMS pMaskParms);

#endif
