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
#ifndef __RASSERVERTEST_H__
#define __RASSERVERTEST_H__

#include "common.h"
#include <tux.h>
#include <cmdline.h>


//
// Global constants for RAS/Tux interaction
//

typedef enum _LineToAdd_t
{
    DefaultDevice0=0, OneVPNLine, OneL2TPLine, OnePPTPLine
} LineToAdd;

#define MAX_ARGS 4
#define MAX_LINE_ADD 16

#define ONE_VPN_LINE _T("OneVPNLine")
#define ONE_L2TP_LINE _T("OneL2TPLine") 
#define ONE_PPTP_LINE _T("OnePPTPLine")


//
// Common
//
#define RASSERVER_INVALID_HANDLE        1
#define RASSERVER_INVALID_IN_BUF        2
#define RASSERVER_INVALID_IN_LEN        3
#define RASSERVER_INVALID_OUT_BUF        4
#define RASSERVER_INVALID_OUT_LEN        5

//
// RASCNTL_SERVER_GET_STATUS 
//
// Obtains the status of the Point-to-Point Protocol (PPP) server and lines.
//
#define RASSERVER_ENABLED_SERVER         11
#define RASSERVER_DISABLED_SERVER         12

//
// RASCNTL_SERVER_ENABLE 
//
// Turns the PPP server on.
//
#define RASSERVER_ALREADY_ENABLED        21
#define RASSERVER_ENABLE                22

//
// RASCNTL_SERVER_DISABLE
//
// Turns the PPP server off.
//

//
// RASCNTL_SERVER_GET_PARAMETERS
//
// Obtains the global server parameters.
//
#define RASSERVER_DISPLAY                41
#define RASSERVER_ENABLED_PARAMS        42
#define RASSERVER_DISABLED_PARAMS       43

//
// RASCNTL_SERVER_SET_PARAMETERS
//
// Sets the global server parameters.
//
#define RASSERVER_SET_VALID_PARAMS        51
#define RASSERVER_SET_INVALID_PARAMS    52

//
// RASCNTL_SERVER_LINE_ADD
//
// Adds a line to be managed by the PPP server. 
//
#define RASSERVER_ALREADY_ADDED            61
#define RASSERVER_INVALID_LINE            62
#define RASSERVER_VALID_LINE            63

//
// RASCNTL_SERVER_LINE_REMOVE
//
// Removes a line being managed by the PPP server.
//
#define RASSERVER_ALREADY_REMOVED        71
#define RASSERVER_INVALID_REMOVED        72
#define RASSERVER_VALID_REMOVED            73
#define RASSERVER_NONEXISTENT_REMOVED    74

//
// RASCNTL_SERVER_LINE_ENABLE
//
// Enables management of a line.
//
#define RASSERVER_INVALID_ENABLED        82
#define RASSERVER_VALID_ENABLED            83
#define RASSERVER_NONEXISTENT_ENABLED    84

//
// RASCNTL_SERVER_LINE_DISABLE
//
// Disables management of a line.
//
#define RASSERVER_ALREADY_DISABLED        91
#define RASSERVER_NONEXISTENT_DISABLED    92
#define RASSERVER_VALID_DISABLED        93
#define RASSERVER_INVALID_DISABLED        94

//
// RASCNTL_SERVER_LINE_GET_PARAMETERS
//
// Obtains a line of parameters.
//

//
// RASCNTL_SERVER_LINE_SET_PARAMETERS
//
// Sets a line of parameters.
//
#define RASSERVER_INVALID_SET_LINE        111
#define RASSERVER_VALID_SET_LINE        112
#define RASSERVER_NONEXIST_LINE            113

//
// RASCNTL_SERVER_USER_SET_CREDENTIALS
//
// Allows a user name or password.
//
#define RASSERVER_INVALID_USER_NAME        121
#define RASSERVER_LONG_USER_NAME        122
#define RASSERVER_LONG_PASSWORD            123
#define RASSERVER_INVALID_PASSWORD        124
#define RASSERVER_INTL_USERNAMES        125
#define RASSERVER_NULL_USER_NAME        126

//
// RASCNTL_SERVER_USER_DELETE_CREDENTIALS
//
// Removes a user name or password.
//
#define RASSERVER_USER_EXISTS            132
#define RASSERVER_USER_NOT_EXISTS        133
#define RASSERVER_USER_INTL_NAME        134

//
// RASCNTL_SERVER_LINE_GET_CONNECTION_INFO
//
// Get info about a line's connection status
//
#define RASSERVER_LINE_ENABLED         141
#define RASSERVER_LINE_ADDED           142
#define RASSERVER_LINE_REMOVED         143
#define RASSERVER_LINE_DISABLED        144

//
// RASCNTL_SERVER_GET_IPV6_NET_PREFIX
//
// Get the pool of IPV6 network prefixes available for allocation
//
#define RASSERVER_INVALID_RANGE        151
#define RASSERVER_VALID_RANGE          152

//
// RASCNTL_SERVER_SET_IPV6_NET_PREFIX
//
// Set the pool of IPV6 network prefixes available for allocation
//
// The two RasServerGetNetPrefix constants above should also suffice for this one
//
#define RASSERVER_INVALID_PREFIX      161

#endif // __RASAPITEST_H__
