//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifdef DEBUG
#ifdef DEBUG_DEBUGGER

#define COM2_BASE           0x02F8

#define comTxBuffer         0x00
#define comRxBuffer         0x00
#define comDivisorLow       0x00
#define comDivisorHigh      0x01
#define comIntEnable        0x01
#define comIntId            0x02
#define comFIFOControl      0x02
#define comLineControl      0x03
#define comModemControl     0x04
#define comLineStatus       0x05
#define comModemStatus      0x06

#define LS_TSR_EMPTY        0x40
#define LS_THR_EMPTY        0x20
#define LS_RX_BREAK         0x10
#define LS_RX_FRAMING_ERR   0x08
#define LS_RX_PARITY_ERR    0x04
#define LS_RX_OVERRUN       0x02
#define LS_RX_DATA_READY    0x01

#define LS_RX_ERRORS        ( LS_RX_FRAMING_ERR | LS_RX_PARITY_ERR | LS_RX_OVERRUN )

//   14400 = 8
//   16457 = 7 +/-
//   19200 = 6
//   23040 = 5
//   28800 = 4
//   38400 = 3
//   57600 = 2
//  115200 = 1

int __cdecl _inp(unsigned short);
int __cdecl _outp(unsigned short, int);

#pragma intrinsic(_inp, _outp)

#endif
#endif
