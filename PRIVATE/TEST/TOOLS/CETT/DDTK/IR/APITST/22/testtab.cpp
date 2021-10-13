//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//

#include <windows.h>
#include <tchar.h>
#include <katoex.h>
#include <tux.h>
#include <shellproc.h>
#include "nohost.h"
#include "withhost.h"
#include "connect.h"
#include "enumdev.h"

// BASE is a unique value assigned to a given tester or component.  This value,
// when combined with each of the following test's unique IDs, allows every 
// test case within the entire team to be uniquely identified.

#define BASE 0x00000000

// Our function table that we pass to Tux

extern FUNCTION_TABLE_ENTRY g_lpFTE[] = 
{
   TEXT("Create & Bind"					),	   0,   0,              0, NULL,
   TEXT(   "socket create ok"			),     1,   0,       BASE+  1, SocketCreateTest,
   TEXT(   "socket create invalid"      ),     1,   0,       BASE+  2, SocketCreateInvalidTest,
   TEXT(   "socket close"				),     1,   0,       BASE+  3, SocketCloseTest,
   TEXT(   "COM Port Usage Test"        ),     1,   0,       BASE+  4, COMPortUsageTest,
   TEXT(   "socket memory leak"         ),     1,   0,       BASE+  5, SocketMemoryLeakTest,
   TEXT(   "bind ok"					),     1,   0,       BASE+  6, BindTest,
   TEXT(   "bind invalid family"        ),     1,   0,       BASE+  7, BindInvalidFamilyTest,
   TEXT(   "bind twice to same socket"  ),     1,   0,       BASE+  9, BindTwiceTest,
   TEXT(   "bind same name to 2 sockets"),     1,   0,       BASE+ 10, BindSameNameTo2SocketsTest,
   TEXT(   "bind to NULL socket"        ),     1,   0,       BASE+ 11, BindNullSocketTest,
   TEXT(   "bind NULL address"			),     1,   0,       BASE+ 12, BindNullAddrTest,
   TEXT(   "bind huge address"			),     1,   0,       BASE+ 13, BindHugeAddrLengthTest,
   TEXT(   "bind various addresses"     ),     1,   0,       BASE+ 14, BindVariousAddrTest,
   TEXT(   "bind getsockname"           ),     1,   0,       BASE+ 15, GetBoundSockNameTest,
   TEXT(   "bind-close-bind same name"  ),     1,   0,       BASE+ 16, BindCloseBindTest,
   TEXT(   "IrSIR open fail test"       ),     1,   0,       BASE+ 17, IrSIROpenFailTest,
   TEXT(   "bind memory leak test"      ),     1,   0,       BASE+ 18, BindMemoryLeakTest,
   TEXT("IAS"				            ),     0,   0,              0, NULL,
   TEXT(   "simple IRLMP_IAS_SET"       ),     1,   0,       BASE+ 20, SetIASTest,
   TEXT(   "invalid IRLMP_IAS_SET"      ),     1,   0,       BASE+ 21, SetIASInvalidTest,
   TEXT(   "integer IRLMP_IAS_SET"      ),     1,   0,       BASE+ 22, SetIASVariousIntAttribTest,
   TEXT(   "Octet Seq IRLMP_IAS_SET"    ),     1,   0,       BASE+ 23, SetIASVariousOctetSeqAttribTest,
   TEXT(   "User String IRLMP_IAS_SET"  ),     1,   0,       BASE+ 24, SetIASVariousUsrStrAttribTest,
   TEXT(   "ClassName Len IRLMP_IAS_SET"),     1,   0,       BASE+ 25, SetIASClassNameLengthTest,
   TEXT(   "AttribName Len IRLMP_IAS_SET"),    1,   0,       BASE+ 26, SetIASAttribNameLengthTest,
   TEXT(   "Attrib Count IRLMP_IAS_SET" ),     1,   0,       BASE+ 27, SetIASMaxAttributeCountTest,
   TEXT(   "IAS_SET Memory Leak test"   ),     1,   0,       BASE+ 28, SetIASMemoryLeakTest,
   TEXT("LASP-SEL"                      ),     0,   0,              0, NULL,
   TEXT(   "bind hard coded lsap-sel"   ),     1,   0,       BASE+ 30, BindHardCodedLsapSelTest,
   TEXT(   "bind random lsap-sel"       ),     1,   0,       BASE+ 31, BindRandomLsapSelTest,
   TEXT(   "bind same hard coded lsap-sel"),   1,   0,       BASE+ 32, BindSameHardCodedLsapSelTest,
   TEXT("ISA_SET & IAS_QUERY"           ),	   0,   0,              0, NULL,
   TEXT(   "confirm integer IAS_SET"    ),     1,   0,       BASE+ 42, ConfirmSetIASValidInt,
   TEXT(   "confirm octseq IAS_SET"     ),     1,   0,       BASE+ 43, ConfirmSetIASValidOctetSeq,
   TEXT(   "confirm usrstr IAS_SET"     ),     1,   0,       BASE+ 44, ConfirmSetIASValidUsrStr,
   TEXT(   "confirm integer IAS_QUERY"  ),     1,   0,       BASE+ 45, ConfirmQueryIASValidInt,
   TEXT(   "confirm octseq IAS_QUERY"   ),     1,   0,       BASE+ 46, ConfirmQueryIASValidOctetSeq,
   TEXT(   "confirm usrstr IAS_QUERY"   ),     1,   0,       BASE+ 47, ConfirmQueryIASValidUsrStr,
   TEXT(   "confirm attrib delete"      ),     1,   0,       BASE+ 48, ConfirmDeleteIASAttribute,
   TEXT(   "confirm classNameLen query" ),     1,   0,       BASE+ 49, ConfirmQueryClassNameLen,
   TEXT(   "confirm attribNameLen query"),     1,   0,       BASE+ 50, ConfirmQueryAttribNameLen,
   TEXT("Remote Bind"			        ),     0,   0,              0, NULL,
   TEXT(   "bind and remote connect"    ),     1,   0,       BASE+ 60, BasicRemoteConnectTest,
   TEXT(   "bind all name lengths"      ),     1,   0,       BASE+ 61, RemoteConnectNameLengthTest,
   TEXT(   "bind all name values"       ),     1,   0,       BASE+ 62, RemoteConnectNameValueTest,
   TEXT(   "bind sub and super str"     ),     1,   0,       BASE+ 63, RemoteConnectSubAndSuperStringTest,
   TEXT(   "bind/unbind then connect"   ),     1,   0,       BASE+ 64, RemoteConnectToUnboundNameTest,
   TEXT(   "connect-close before accept"),     1,   0,       BASE+ 65, RemoteConnectAndCloseBeforeAcceptTest,
   TEXT(   "bind/conn hard coded lsap-sel"),   1,   0,       BASE+ 66, RemoteConnectHardCodedLsapSelTest,
   //TEXT(   "remote conn memory leak"    ),     1,   0,       BASE+ 67, RemoteConnectMemoryLeakTest,
   TEXT("Remote Connect"		        ),     0,   0,              0, NULL,
   TEXT(   "connect to remote bind"     ),     1,   0,       BASE+ 70, BasicLocalConnectTest,
   TEXT(   "connect all name lengths"   ),     1,   0,       BASE+ 71, LocalConnectNameLengthTest,
   TEXT(   "connect all name values"    ),     1,   0,       BASE+ 72, LocalConnectNameValueTest,
   TEXT(   "connect sub and super str"  ),     1,   0,       BASE+ 73, LocalConnectSubAndSuperStringTest,
   TEXT(   "connect to bad deviceID"    ),     1,   0,       BASE+ 74, LocalConnectToUnknownDevice,
   TEXT(   "IrSir COM open fail then connect"),1,   0,       BASE+ 75, AfterIrSirCOMOpenFailedTest,
   TEXT(   "connect hard coded lsap-sel"),     1,   0,       BASE+ 76, LocalConnectHardCodedLsapSelTest,
   //TEXT(   "local connect memory leak"  ),     1,   0,       BASE+ 77, LocalConnectMemoryLeakTest,
   TEXT(   "close connect in progress"  ),     1,   0,       BASE+ 78, CloseConnectInProgressTest,
   TEXT(   "connect twice with same socket"),  1,   0,       BASE+ 79, ConnectAgainTest,

   TEXT(   "enum devices disc and conn" ),     1,   0,       BASE+ 80, EnumDeviceConnectedTest,
   TEXT(   "enum devices while connected"),    1,   0,       BASE+ 81, DiscoveryWhileConnectedTest,

   // Run the COM Port Usage test again to make sure that nothing is dangling
   //TEXT(   "COM Port Usage Test"        ),     1,   0,       BASE+ 99, COMPortUsageTest,
   NULL,                                       0,   0,              0, NULL  // marks end of list
};

// END OF FILE