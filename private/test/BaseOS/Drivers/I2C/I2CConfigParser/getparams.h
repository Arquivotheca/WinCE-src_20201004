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
//  TUXTEST TUX DLL
//
//  Module: I2CTest.h
//          Declares all global variables and test function prototypes EXCEPT
//          when included by globals.cpp, in which case it DEFINES global
//          variables, including the function table.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////
#ifndef __GETPARAMS_H__
#define __GETPARAMS_H__


#define I2C_CONFIG_FILE_TAG_BUS_NAME                    L"BusName"
#define I2C_CONFIG_FILE_TAG_DATA                        L"Data"
#define I2C_CONFIG_FILE_TAG_OPERATION                   L"Operation"
#define I2C_CONFIG_FILE_TAG_READ                        L"Read"
#define I2C_CONFIG_FILE_TAG_WRITE                       L"Write"
#define I2C_CONFIG_FILE_TAG_LOOPBACK                    L"Loopback"
#define I2C_CONFIG_FILE_TAG_SUBORDINATE_ADDRESS         L"SubordinateAddress"
#define I2C_CONFIG_FILE_TAG_SUBORDINATE_ADDRESS_SIZE    L"SubordinateAdressSize"
#define I2C_CONFIG_FILE_TAG_REGISTER_ADDRESS            L"RegisterAddress"
#define I2C_CONFIG_FILE_TAG_REGISTER_ADDRESS_SIZE       L"RegisterAdressSize"
#define I2C_CONFIG_FILE_TAG_LOOPBACK_ADDRESS            L"LoopbackAddress"
#define I2C_CONFIG_FILE_TAG_NUMBER_OF_BYTES             L"NumberOfBytes"
#define I2C_CONFIG_FILE_TAG_SPEED                       L"Speed"
#define I2C_CONFIG_FILE_TAG_TIMEOUT                     L"TimeOut"
#define I2C_CONFIG_FILE_TAG_TRANSACTION                 L"Transaction"


#endif
