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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
//                                                                     
// --------------------------------------------------------------------

#include "xwifi_tuxmain.h"
#include "xwifi_testproc.h"

// Global CKato logging object.  Set while processing SPM_LOAD_DLL message.
CKato *g_pKato = NULL;

// Global shell info structure.  Set while processing SPM_SHELL_INFO message.
SPS_SHELL_INFO *g_pShellInfo;



// Tux testproc function table
FUNCTION_TABLE_ENTRY g_lpFTE[] = {
    /*
   LPCTSTR  lpDescription; // description of test
   UINT     uDepth;        // depth of item in tree hierarchy
   DWORD    dwUserData;    // user defined data that will be passed to TestProc at runtime
   DWORD    dwUniqueID;    // test-id = uniquely identifies the test - used in loading/saving scripts
   TESTPROC lpTestProc;    // pointer to TestProc function to be called for this test
    */

    {
        L"insert card with initial state. If there is card existing, eject then initialize, then insert card then wait until detected",
        1, 0x1100, 1000, IF1
    },
    {
        L"check adapter XWIFI11B1, iphelper API finds xwifi11b adapter.",
        1, 0x1200, 1001, IF1
    },
    {
        L"card eject -> wait for 0-ip",
        1, 0x1520, 1002, F1
    },
    {
        L"clear the test SSID set",
        1, 0x1300, 1003, IF1
    },
    {
        L"prepare the test SSID set",
        1, 0x1400, 1004, IF1
    },
    {
        L"card insert -> wait/test 0-ip",
        1, 0x1420, 1005, F1
    },
    {
        L"reset Preferred List -> wait/test 0 ip",
        1, 0x1020, 1006, F1
    },
    {
        L"WZC query test set SSID",
        1, 0x1800, 1007, IF1
    },
    {
        L"connect to SSID -> wait/test ip-addr",
        1, 0x1130, 1008, F1
    },
    {
        L"run ipconfig [in order to collect information on the test condition]",
        1, 0x1500, 1009, IF1
    },
    {
        L"run ndisconfig [in order to collect information on the test condition]",
        1, 0x1600, 1010, IF1
    },
    {
        L"run 'ping gateway'. [skip if ip-addr is not DHCP]",
        1, 0x1700, 1011, IF1
    },
    {
        L"reset Preferred List -> wait/test 0 ip",
        1, 0x1020, 1099, F1
    },


    // 11xx
    {
        L"connect to SSID -> wait/test ip-addr",
        1, 0x1130, 1100, F1
    },
    {
        L"reset Preferred List -> wait/test 0 ip",
        1, 0x1020, 1101, F1
    },
    {
        L"connect to SSID -> wait/test ip-addr",
        1, 0x1130, 1102, F1
    },
    {
        L"ipconfig /release' -> wait/test 0-ip",
        1, 0x1B20, 1103, F1
    },
    {
        L"ipconfig /renew' -> wait/test ip-addr",
        1, 0x1A30, 1104, F1
    },
    {
        L"reset Preferred List -> wait/test 0 ip",
        1, 0x1020, 1105, F1
    },
    {
        L"ipconfig /release' -> wait/test 0-ip",
        1, 0x1B20, 1106, F1
    },
    {
        L"ipconfig /renew' -> wait/test 0-ip",
        1, 0x1A20, 1107, F1
    },
    {
        L"connect to SSID -> wait/test ip-addr",
        1, 0x1130, 1108, F1
    },
    {
        L"unbind -> wait for 0-ip",
        1, 0x1320, 1109, F1
    },
    {
        L"bind -> wait/test ip-addr",
        1, 0x1230, 1110, F1
    },
    {
        L"reset Preferred List -> wait/test 0 ip",
        1, 0x1020, 1111, F1
    },
    {
        L"unbind -> wait for 0-ip",
        1, 0x1320, 1112, F1
    },
    {
        L"bind -> wait/test 0-ip",
        1, 0x1220, 1113, F1
    },
    {
        L"connect to SSID -> wait/test ip-addr",
        1, 0x1130, 1114, F1
    },
    {
        L"card eject -> wait for 0-ip",
        1, 0x1520, 1115, F1
    },
    {
        L"card insert -> wait/test ip-addr",
        1, 0x1430, 1116, F1
    },
    {
        L"reset Preferred List -> wait/test 0 ip",
        1, 0x1020, 1117, F1
    },
    {
        L"card eject -> wait for 0-ip",
        1, 0x1520, 1118, F1
    },
    {
        L"card insert -> wait/test 0-ip",
        1, 0x1420, 1119, F1
    },
    {
        L"connect to SSID -> wait/test ip-addr",
        1, 0x1130, 1120, F1
    },
    {
        L"adapter pwr-down -> wait/test 0-ip",
        1, 0x1720, 1121, F1
    },
    {
        L"adapter pwr-up -> wait/test ip-addr",
        1, 0x1630, 1122, F1
    },
    {
        L"reset Preferred List -> wait/test 0 ip",
        1, 0x1020, 1123, F1
    },
    {
        L"adapter pwr-down -> wait/test 0-ip",
        1, 0x1720, 1124, F1
    },
    {
        L"adapter pwr-up -> wait/test 0-ip",
        1, 0x1620, 1125, F1
    },
    {
        L"connect to SSID -> wait/test ip-addr",
        1, 0x1130, 1126, F1
    },
    {
        L"remove from visible SSID list -> (1) infra->wait/test 0-ip (2) adhoc->stay connected",
        1, 0x1920, 1127, F1
    },
    {
        L"back to visible SSID list (AP ON or adhoc back) -> wait/test ip-addr",
        1, 0x1830, 1128, F1
    },
    {
        L"reset Preferred List -> wait/test 0 ip",
        1, 0x1020, 1129, F1
    },
    {
        L"remove from visible SSID list -> infra/adhoc, wait/test 0-ip",
        1, 0x1920, 1130, F1
    },
    {
        L"connect to SSID -> (1) infra->wait/test 0-addr (2) adhoc ->wait/test ip-addr",
        1, 0x1120, 1131, F1
    },
    {
        L"back to visible SSID list (AP ON or adhoc back) -> wait/test ip-addr",
        1, 0x1830, 1132, F1
    },
    {
        L"reset Preferred List -> wait/test 0 ip",
        1, 0x1020, 1133, F1
    },
    {
        L"connect to SSID -> wait/test ip-addr",
        1, 0x1130, 1134, F1
    },
    {
        L"[skip at this moment] roaming AP1-> AP2 -> wait-roaming/test ip-addr",
        1, 0x1C30, 1135, F1
    },
    {
        L"[skip at this moment] roaming AP1<- AP2 -> wait-roaming/test ip-addr",
        1, 0x1D30, 1136, F1
    },
    {
        L"reset Preferred List -> wait/test 0 ip",
        1, 0x1020, 1137, F1
    },
    {
        L"connect to SSID -> wait/test ip-addr",
        1, 0x1130, 1138, F1
    },
    {
        L"connect to SSID again [refresh] -> wait/test ip-addr",
        1, 0x1130, 1139, F1
    },
    {
        L"reset Preferred List -> wait/test 0 ip",
        1, 0x1020, 1199, F1
    },

    // 12xx
    {
        L"[ui] reset Preferred List -> wait for 0-ip -> no UI check",
        1, 0x1020, 1200, F1
    },
    {
        L"[ui] unbind -> wait for 0-ip -> UI OFF [should disappear when adapter goes away]",
        1, 0x1322, 1201, F1
    },
    {
        L"[ui] bind -> wait for 0-ip -> UI ON [should appear since it cannot connect to any SSID]",
        1, 0x1223, 1202, F1
    },
    {
        L"[ui] connect to SSID -> wait for ip-addr -> UI ON [should remain]",
        1, 0x1133, 1203, F1
    },
    {
        L"[ui] unbind -> wait for 0-ip -> UI OFF [should disappear]",
        1, 0x1322, 1204, F1
    },
    {
        L"[ui] bind -> wait for ip-addr -> UI OFF [should not appear since it can connect]",
        1, 0x1232, 1205, F1
    },
    {
        L"[ui] reset Preferred List -> wait for 0-ip -> no UI check",
        1, 0x1020, 1206, F1
    },
    {
        L"[ui] card eject -> wait for 0-ip -> UI OFF [should disappear]",
        1, 0x1522, 1207, F1
    },
    {
        L"[ui] card insert -> wait for 0-ip -> UI ON [should appear since it cannot connect to any SSID]",
        1, 0x1423, 1208, F1
    },
    {
        L"[ui] connect to SSID -> wait for ip-addr -> UI ON [should remain]",
        1, 0x1133, 1209, F1
    },
    {
        L"[ui] card eject -> wait for 0-ip -> UI OFF [should disappear]",
        1, 0x1522, 1210, F1
    },
    {
        L"[ui] card insert -> wait for ip-addr -> UI OFF [should not appear since it can connect]",
        1, 0x1432, 1211, F1
    },
    {
        L"[ui] reset Preferred List -> wait for 0-ip -> no UI check",
        1, 0x1020, 1212, F1
    },
    {
        L"[ui] adapter pwr-down -> wait for 0-ip -> UI OFF [should disappear]",
        1, 0x1722, 1213, F1
    },
    {
        L"[ui] adapter pwr-up -> wait for 0-ip -> UI ON [should appear since it cannot connect to any SSID]",
        1, 0x1623, 1214, F1
    },
    {
        L"[ui] connect to SSID -> wait for ip-addr -> UI ON [should remain]",
        1, 0x1133, 1215, F1
    },
    {
        L"[ui] adapter pwr-down -> wait for 0-ip -> UI OFF [should disappear]",
        1, 0x1722, 1216, F1
    },
    {
        L"[ui] adapter pwr-up -> wait for ip-addr -> UI OFF [should not appear since it can connect]",
        1, 0x1632, 1217, F1
    },
    {
        L"[ui] reset Preferred List -> wait for 0-ip -> no UI check",
        1, 0x1020, 1218, F1
    },
    {
        L"[ui] connect to SSID -> wait for ip-addr -> no UI check",
        1, 0x1130, 1219, F1
    },
    {
        L"[ui] card eject -> wait for 0-ip -> UI OFF [should disappear]",
        1, 0x1522, 1220, F1
    },
    {
        L"[ui] card insert -> wait for ip-addr -> UI OFF [should not appear since it can connect]",
        1, 0x1432, 1221, F1
    },
    {
        L"[ui] AP OFF -> wait for 0-ip -> UI ON [should come since it cannot connect] [this tests UI OFF -> ON case] [skip test if adhoc]",
        1, 0x1923, 1222, F1
    },
    {
        L"[ui] AP ON -> wait for ip-addr -> UI ON [should remain] [skip test if adhoc]",
        1, 0x1833, 1223, F1
    },
    {
        L"[ui] reset Preferred List -> wait for 0-ip -> no UI check",
        1, 0x1020, 1224, F1
    },
    {
        L"[ui] card eject -> wait for 0-ip -> UI OFF [should disappear]",
        1, 0x1522, 1225, F1
    },
    {
        L"[ui] card insert -> wait for 0-ip -> UI ON [should appear since it cannot connect to any SSID]",
        1, 0x1423, 1226, F1
    },
    {
        L"[ui] connect to SSID -> wait for ip-addr -> UI ON [should remain]",
        1, 0x1133, 1227, F1
    },
    {
        L"[ui] AP OFF -> wait for 0-ip -> UI ON [should come since it cannot connect] [this tests UI ON -> ON case] [skip test if adhoc]",
        1, 0x1923, 1228, F1
    },
    {
        L"[ui] AP ON -> wait for ip-addr -> UI ON [should remain] [skip test if adhoc]",
        1, 0x1833, 1229, F1
    },
    {
        L"[ui] reset Preferred List -> wait for 0-ip -> no UI check",
        1, 0x1020, 1230, F1
    },
    {
        L"[ui] connect to SSID -> wait for ip-addr -> no UI check",
        1, 0x1130, 1231, F1
    },
    {
        L"[ui] card eject -> wait for 0-ip -> UI OFF [should disappear]",
        1, 0x1522, 1232, F1
    },
    {
        L"[ui] card insert -> wait for ip-addr -> UI OFF [should not appear since it can connect]",
        1, 0x1432, 1233, F1
    },
    {
        L"[skip at this moment] [ui] roaming AP1-> AP2 -> wait for roaming -> UI OFF [should remain OFF since it can still connect to AP2] [skip test if adhoc]",
        1, 0x1C32, 1234, F1
    },
    {
        L"[skip at this moment] [ui] roaming AP1<- AP2 -> wait for roaming > UI OFF [should remain OFF since it can still connect to AP1] [skip test if adhoc]",
        1, 0x1D32, 1235, F1
    },
    {
        L"[ui] reset Preferred List -> wait for 0-ip -> no UI check",
        1, 0x1020, 1236, F1
    },
    {
        L"[ui] card eject -> wait for 0-ip -> UI OFF [should disappear]",
        1, 0x1522, 1237, F1
    },
    {
        L"[ui] card insert -> wait for 0-ip -> UI ON [should appear since it cannot connect to any SSID]",
        1, 0x1423, 1238, F1
    },
    {
        L"[ui] connect to SSID -> wait for ip-addr -> UI ON [should remain]",
        1, 0x1133, 1239, F1
    },
    {
        L"[skip at this moment] [ui] roaming AP1-> AP2 -> wait for roaming -> UI ON [skip test if adhoc]",
        1, 0x1C33, 1240, F1
    },
    {
        L"[skip at this moment] [ui] roaming AP1<- AP2 -> wait for roaming -> UI ON [skip test if adhoc]",
        1, 0x1D33, 1241, F1
    },
    {
        L"[ui] reset Preferred List -> wait for 0-ip -> no UI check",
        1, 0x1020, 1299, F1
    },

    // 13xx
    {
        L"[ui] connect to SSID -> wait for ip-addr -> no UI check",
        1, 0x1130, 1300, F1
    },
    {
        L"[packet-xmit] UDP broadcast 1 x 16-byte",
        1, 0x10010010, 1301, F2
    },
    {
        L"[packet-xmit] UDP broadcast 1 x 256-byte",
        1, 0x10010100, 1302, F2
    },
    {
        L"[packet-xmit] UDP broadcast 1 x 512-byte",
        1, 0x10010200, 1303, F2
    },
    {
        L"[packet-xmit] UDP broadcast 1 x 1024-byte",
        1, 0x10010400, 1304, F2
    },
    {
        L"[packet-xmit] UDP broadcast 1 x 2048-byte",
        1, 0x10010800, 1305, F2
    },
    {
        L"[packet-xmit] UDP broadcast 1 x 4096-byte",
        1, 0x10011000, 1306, F2
    },
    {
        L"[packet-xmit] UDP broadcast 2 x 512-byte",
        1, 0x10020200, 1307, F2
    },
    {
        L"[ui] reset Preferred List -> wait for 0-ip -> no UI check",
        1, 0x1020, 1308, F1
    },
    {
        L"[packet-xmit] UDP broadcast 1 x 16-byte",
        1, 0x10010010, 1309, F2
    },
    {
        L"[packet-xmit] UDP broadcast 1 x 256-byte",
        1, 0x10010100, 1310, F2
    },
    {
        L"[packet-xmit] UDP broadcast 1 x 512-byte",
        1, 0x10010200, 1311, F2
    },
    {
        L"[packet-xmit] UDP broadcast 1 x 1024-byte",
        1, 0x10010400, 1312, F2
    },
    {
        L"[packet-xmit] UDP broadcast 1 x 2048-byte",
        1, 0x10010800, 1313, F2
    },
    {
        L"[packet-xmit] UDP broadcast 1 x 4096-byte",
        1, 0x10011000, 1314, F2
    },
    {
        L"[packet-xmit] UDP broadcast 2 x 512-byte",
        1, 0x10020200, 1315, F2
    },
    {
        L"[ui] reset Preferred List -> wait for 0-ip -> no UI check",
        1, 0x1020, 1399, F1
    },


    // 14xx stability test 
    //==============
    {
        L"connect to SSID -> wait/test ip-addr",
        1, 0x1130, 1400, F1
    },
    {
        L"card eject -> wait for 0-ip",
        1, 0x1520, 1401, F1
    },
    {
        L"[stability] card insert -> wait for 0 ms -> card eject [in UI OFF situation]",
        1, 0x1100, 1402, F3
    },
    {
        L"[stability] card insert -> wait for 1 ms -> card eject [in UI OFF situation]",
        1, 0x1101, 1403, F3
    },
    {
        L"[stability] card insert -> wait for 10 ms -> card eject [in UI OFF situation]",
        1, 0x1102, 1404, F3
    },
    {
        L"[stability] card insert -> wait for 50 ms -> card eject [in UI OFF situation]",
        1, 0x1103, 1405, F3
    },
    {
        L"[stability] card insert -> wait for 100 ms -> card eject [in UI OFF situation]",
        1, 0x1104, 1406, F3
    },
    {
        L"[stability] card insert -> wait for 500 ms -> card eject [in UI OFF situation]",
        1, 0x1105, 1407, F3
    },
    {
        L"[stability] card insert -> wait for 1000 ms -> card eject [in UI OFF situation]",
        1, 0x1106, 1408, F3
    },
    {
        L"[stability] card insert -> wait for 2000 ms -> card eject [in UI OFF situation]",
        1, 0x1107, 1409, F3
    },
    {
        L"[stability] card insert -> wait for 5000 ms -> card eject [in UI OFF situation]",
        1, 0x1108, 1410, F3
    },
    {
        L"[stability] card insert -> wait for 30000 ms -> card eject [in UI OFF situation]",
        1, 0x1109, 1411, F3
    },

    //==============
    {
        L"card insert -> wait/test ip-addr",
        1, 0x1430, 1412, F1
    },
    {
        L"[ui] reset Preferred List -> wait for 0-ip -> no UI check",
        1, 0x1020, 1413, F1
    },
    {
        L"card eject -> wait for 0-ip",
        1, 0x1520, 1414, F1
    },
    {
        L"[stability] card insert -> wait for 0 ms -> card eject [in UI ON situation]",
        1, 0x1100, 1415, F3
    },
    {
        L"[stability] card insert -> wait for 1 ms -> card eject [in UI ON situation]",
        1, 0x1101, 1416, F3
    },
    {
        L"[stability] card insert -> wait for 10 ms -> card eject [in UI ON situation]",
        1, 0x1102, 1417, F3
    },
    {
        L"[stability] card insert -> wait for 50 ms -> card eject [in UI ON situation]",
        1, 0x1103, 1418, F3
    },
    {
        L"[stability] card insert -> wait for 100 ms -> card eject [in UI ON situation]",
        1, 0x1104, 1419, F3
    },
    {
        L"[stability] card insert -> wait for 500 ms -> card eject [in UI ON situation]",
        1, 0x1105, 1420, F3
    },
    {
        L"[stability] card insert -> wait for 1000 ms -> card eject [in UI ON situation]",
        1, 0x1106, 1421, F3
    },
    {
        L"[stability] card insert -> wait for 2000 ms -> card eject [in UI ON situation]",
        1, 0x1107, 1422, F3
    },
    {
        L"[stability] card insert -> wait for 5000 ms -> card eject [in UI ON situation]",
        1, 0x1108, 1423, F3
    },
    {
        L"[stability] card insert -> wait for 30000 ms -> card eject [in UI ON situation]",
        1, 0x1109, 1424, F3
    },

    //==============
    {
        L"[ui] card insert -> wait for 0-ip -> UI ON [should appear since it cannot connect to any SSID]",
        1, 0x1423, 1425, F1
    },
    {
        L"[stability] connect to SSID -> wait for 0 ms -> disconnect [in UI ON situation]",
        1, 0x1200, 1426, F3
    },
    {
        L"[stability] connect to SSID -> wait for 1 ms -> disconnect [in UI ON situation]",
        1, 0x1201, 1427, F3
    },
    {
        L"[stability] connect to SSID -> wait for 10 ms -> disconnect [in UI ON situation]",
        1, 0x1202, 1428, F3
    },
    {
        L"[stability] connect to SSID -> wait for 50 ms -> disconnect [in UI ON situation]",
        1, 0x1203, 1429, F3
    },
    {
        L"[stability] connect to SSID -> wait for 100 ms -> disconnect [in UI ON situation]",
        1, 0x1204, 1430, F3
    },
    {
        L"[stability] connect to SSID -> wait for 500 ms -> disconnect [in UI ON situation]",
        1, 0x1205, 1431, F3
    },
    {
        L"[stability] connect to SSID -> wait for 1000 ms -> disconnect [in UI ON situation]",
        1, 0x1206, 1432, F3
    },
    {
        L"[stability] connect to SSID -> wait for 2000 ms -> disconnect [in UI ON situation]",
        1, 0x1207, 1433, F3
    },
    {
        L"[stability] connect to SSID -> wait for 5000 ms -> disconnect [in UI ON situation]",
        1, 0x1208, 1434, F3
    },
    {
        L"[stability] connect to SSID -> wait for 30000 ms -> disconnect [in UI ON situation]",
        1, 0x1209, 1435, F3
    },

    //==============
    {
        L"connect to SSID -> wait/test ip-addr",
        1, 0x1130, 1436, F1
    },
    {
        L"[ui] card eject -> wait for 0-ip -> UI OFF [should disappear]",
        1, 0x1522, 1437, F1
    },
    {
        L"[ui] card insert -> wait for ip-addr -> UI OFF [should not appear since it can connect]",
        1, 0x1432, 1438, F1
    },
    {
        L"[stability] connect to SSID -> wait for 0 ms -> disconnect [in UI OFF situation]",
        1, 0x1200, 1439, F3
    },
    {
        L"[stability] connect to SSID -> wait for 1 ms -> disconnect [in UI OFF situation]",
        1, 0x1201, 1440, F3
    },
    {
        L"[stability] connect to SSID -> wait for 10 ms -> disconnect [in UI OFF situation]",
        1, 0x1202, 1441, F3
    },
    {
        L"[stability] connect to SSID -> wait for 50 ms -> disconnect [in UI OFF situation]",
        1, 0x1203, 1442, F3
    },
    {
        L"[stability] connect to SSID -> wait for 100 ms -> disconnect [in UI OFF situation]",
        1, 0x1204, 1443, F3
    },
    {
        L"[stability] connect to SSID -> wait for 500 ms -> disconnect [in UI OFF situation]",
        1, 0x1205, 1444, F3
    },
    {
        L"[stability] connect to SSID -> wait for 1000 ms -> disconnect [in UI OFF situation]",
        1, 0x1206, 1445, F3
    },
    {
        L"[stability] connect to SSID -> wait for 2000 ms -> disconnect [in UI OFF situation]",
        1, 0x1207, 1446, F3
    },
    {
        L"[stability] connect to SSID -> wait for 5000 ms -> disconnect [in UI OFF situation]",
        1, 0x1208, 1447, F3
    },
    {
        L"[stability] connect to SSID -> wait for 30000 ms -> disconnect [in UI OFF situation]",
        1, 0x1209, 1448, F3
    },
    {
        L"[make sure go back to zero state] reset Preferred List -> wait for 0-ip -> no UI check",
        1, 0x1020, 1499, F1
    },


    NULL, 0, 0, 0, NULL
};




// --------------------------------------------------------------------
BOOL WINAPI 
DllMain(
    HANDLE hInstance, 
    ULONG dwReason, 
    LPVOID lpReserved ) 
// --------------------------------------------------------------------    
{
    UNREFERENCED_PARAMETER(lpReserved);
    
    switch( dwReason )
    {
        case DLL_PROCESS_ATTACH:
            Debug(TEXT("%s: DLL_PROCESS_ATTACH\r\n"), MODULE_NAME);
            Debug(TEXT("%s: Disabiling thread library calls\r\n"), MODULE_NAME);
			DisableThreadLibraryCalls((HMODULE)hInstance); // not interested in those here
            return TRUE;

        case DLL_PROCESS_DETACH:
            Debug(TEXT("%s: DLL_PROCESS_DETACH\r\n"), MODULE_NAME);
            return TRUE;
    }
    return FALSE;
}


void ShowUsage();

// --------------------------------------------------------------------
void LOG(LPWSTR szFmt, ...)
// --------------------------------------------------------------------
{
#ifdef DEBUG
    va_list va;

    if(g_pKato)
    {
        va_start(va, szFmt);
        g_pKato->LogV( LOG_DETAIL, szFmt, va);
        va_end(va);
    }
#endif
}





int
WasOption
// look for tux argument then find -c option.
// this -c is our module-specific command line option.
// get this string, then find given option in this string.
// returns option index
// returns index of argv[] found, -1 if not found
(
    IN int argc,       // number of args
    IN WCHAR* argv[],  // arg array
    IN WCHAR* szOption // to find ('t')
)
{
    for(int i=0; i<argc; i++)
    {
        if( ((*argv[i] == L'-') || (*argv[i] == L'/')) &&
            !wcscmp(argv[i]+1, szOption))
            return i;
    }
    return -1;
}   // WasOption()



int
GetOption
// look for argument like '-t 100' or '/t 100'.
// returns index of '100' if option ('t') is found
// returns -1 if not found
(
    int argc,                // number of args
    IN WCHAR* argv[],        // arg array
    IN WCHAR* pszOption,     // to find ('n')
    OUT WCHAR** ppszArgument // option value ('100')
)
{
    if(!ppszArgument)
        return -1;

    int i = WasOption(argc, argv, pszOption);
    if((i < 0) || ((i+1) >= argc))
    {
        *ppszArgument = NULL;
        return -1;
    }

    *ppszArgument = argv[i+1];
    return i+1;
}   // GetOption()







//
// copied from \private\winceos\COREOS\core\corelibc\crtw32\startup\crtstrtw.cxx
//
//  String utility: count number of white spaces
//
static int crtstart_SkipWhiteW (wchar_t *lpString) {
    wchar_t *lpStringSav = lpString;

    while ((*lpString != L'\0') && iswspace (*lpString))
        ++lpString;

    return lpString - lpStringSav;
}

//
//  String utility: count number of non-white spaces
//
static int crtstart_SkipNonWhiteQuotesW (wchar_t *lpString) {
    wchar_t *lpStringSav = lpString;

    int fInQuotes = FALSE;

    while ((*lpString != L'\0') && ((! iswspace (*lpString)) || fInQuotes)) {
        if (*lpString == L'"')
            fInQuotes = ! fInQuotes;

        ++lpString;
    }

    return lpString - lpStringSav;
}

//
//  Remove quotes
//
static void crtstart_RemoveQuotesW (wchar_t *lpString, unsigned iStringLen) {
    wchar_t *lpStringEnd = lpString + iStringLen;
    wchar_t *lpDest = lpString;

    while (
        (*lpString != L'\0') &&
        (lpString<lpStringEnd) &&
        (lpDest<lpStringEnd)
    ) {
        if (*lpString == L'"')
            ++lpString;
        else
            *lpDest++ = *lpString++;
    }

    *lpDest = L'\0';
}

//
//  Count number of characters and arguments
//
static void crtstart_CountSpaceW (unsigned &nargs, unsigned &nchars, wchar_t *lpCmdWalker) {
    for ( ; ; ) {
        lpCmdWalker += crtstart_SkipWhiteW (lpCmdWalker);
        
        if (*lpCmdWalker == L'\0')
            break;

        int szThisString = crtstart_SkipNonWhiteQuotesW (lpCmdWalker);
        
        lpCmdWalker += szThisString;
        nchars      += szThisString + 1;

        ++nargs;

        if (*lpCmdWalker == L'\0')
            break;
    }
}

//
//  Parse argument stream
//

void *g_pvArgBlock  = NULL;
#define MAX_64K 0x00010000

static int crtstart_ParseArgsWW (wchar_t *argv0, wchar_t *lpCmdLine, int &argc, wchar_t ** &argv) {
    unsigned iArgv0Len       = wcslen (argv0) + 1;
    unsigned nargs           = 1;
    unsigned nchars          = iArgv0Len;

    crtstart_CountSpaceW (nargs, nchars, lpCmdLine);
    if(nargs > 1024)
        nargs = 1024;
    if(nchars > MAX_64K)    // max cmd-line length 64k w_chars
        nchars = MAX_64K;

    g_pvArgBlock  = LocalAlloc (LMEM_FIXED, (nargs + 1) * sizeof (wchar_t *) + nchars * sizeof(wchar_t));
    if( g_pvArgBlock == 0 ) {
        RETAILMSG (1, (TEXT("Initialization error %d\r\n"), GetLastError()));
        return FALSE;
    }
    wchar_t *pcTarget    = (wchar_t *)((wchar_t **)g_pvArgBlock + nargs + 1);
    wchar_t *lpCmdWalker = lpCmdLine;
    argv = (wchar_t **)g_pvArgBlock;
    argc = nargs;

    memcpy (pcTarget, argv0, iArgv0Len * sizeof(wchar_t));
    argv[0] = pcTarget;
    pcTarget   += iArgv0Len;

    for (int i=1; i<argc; i++) {
        lpCmdWalker += crtstart_SkipWhiteW (lpCmdWalker);

        if (*lpCmdWalker == L'\0')
            break;

        int szThisString = crtstart_SkipNonWhiteQuotesW (lpCmdWalker);
        memcpy (pcTarget, lpCmdWalker, szThisString * sizeof(wchar_t));
        pcTarget[szThisString] = L'\0';

        crtstart_RemoveQuotesW (pcTarget, wcslen(pcTarget));
        argv[i] = pcTarget;

        pcTarget += szThisString + 1;
        lpCmdWalker += szThisString;

        if (*lpCmdWalker == L'\0')
            break;
    }
    argv[argc] = NULL;
    return TRUE;
}


int     argc;
wchar_t **argv;


// --------------------------------------------------------------------
SHELLPROCAPI 
ShellProc(
    UINT uMsg, 
    SPPARAM spParam ) 
// --------------------------------------------------------------------    
{
    LPSPS_BEGIN_TEST pBT = {0};
    LPSPS_END_TEST pET = {0};

    switch (uMsg) {
    
        // Message: SPM_LOAD_DLL
        //
        case SPM_LOAD_DLL: 
            Debug(_T("ShellProc(SPM_LOAD_DLL, ...) called\r\n"));

            // If we are UNICODE, then tell Tux this by setting the following flag.
            #ifdef UNICODE
                ((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;
            #endif

            // turn on kato debugging
            KatoDebug(1, KATO_MAX_VERBOSITY,KATO_MAX_VERBOSITY,KATO_MAX_LEVEL);

            // Get/Create our global logging object.
            g_pKato = (CKato*)KatoGetDefaultObject();
            return SPR_HANDLED;        

        // Message: SPM_UNLOAD_DLL
        //
        case SPM_UNLOAD_DLL: 
            Debug(_T("ShellProc(SPM_UNLOAD_DLL, ...) called"));
            WSACleanup();
            if(g_pvArgBlock)
                LocalFree(g_pvArgBlock);
            return SPR_HANDLED;      

        // Message: SPM_SHELL_INFO
        //
        case SPM_SHELL_INFO:
            Debug(_T("ShellProc(SPM_SHELL_INFO, ...) called\r\n"));
      
            // Store a pointer to our shell info for later use.
            g_pShellInfo = (LPSPS_SHELL_INFO)spParam;
        
            // handle command line parsing

            if (g_pShellInfo->szDllCmdLine && *g_pShellInfo->szDllCmdLine)
            {
                Debug(L"Command Line: \"%s\".", g_pShellInfo->szDllCmdLine);
                // g_pShellInfo->szDllCmdLine = -c "string" part only.

                if (! crtstart_ParseArgsWW (L"tux", (WCHAR*)g_pShellInfo->szDllCmdLine, argc, argv))
                {
                    Debug(L"cmd line processing error [arg processing failure]");
                    return SPR_FAIL;
                }

                if(WasOption(argc, argv, L"?")>=0)
                {
                    ShowUsage();
                    return SPR_FAIL;
                }
            }
            else
            {
                argv[0] = L'\0';
                argc = 0;
            }
            return SPR_HANDLED;

        // Message: SPM_REGISTER
        //
        case SPM_REGISTER:
            Debug(_T("ShellProc(SPM_REGISTER, ...) called\r\n"));

            WSADATA WsaData;	// Ask for Winsock version 2.2.
            if (WSAStartup(MAKEWORD(2,2), &WsaData))
                Debug(_T("WSAStartup Failed %d\r\n"));
            
            ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;
            #ifdef UNICODE
                return SPR_HANDLED | SPF_UNICODE;
            #else
                return SPR_HANDLED;
            #endif
            
        // Message: SPM_START_SCRIPT
        //
        case SPM_START_SCRIPT:
            Debug(_T("ShellProc(SPM_START_SCRIPT, ...) called\r\n"));

            return SPR_HANDLED;

        // Message: SPM_STOP_SCRIPT
        //
        case SPM_STOP_SCRIPT:
            
            return SPR_HANDLED;

        // Message: SPM_BEGIN_GROUP
        //
        case SPM_BEGIN_GROUP:
            Debug(_T("ShellProc(SPM_BEGIN_GROUP, ...) called\r\n"));
            g_pKato->BeginLevel(0, _T("BEGIN GROUP: XWIFI11B TEST"));
            return SPR_HANDLED;

        // Message: SPM_END_GROUP
        //
        case SPM_END_GROUP:
            Debug(_T("ShellProc(SPM_END_GROUP, ...) called\r\n"));
            g_pKato->EndLevel(_T("END GROUP: XWIFI11B TEST"));
            return SPR_HANDLED;

        // Message: SPM_BEGIN_TEST
        //
        case SPM_BEGIN_TEST:
            Debug(_T("ShellProc(SPM_BEGIN_TEST, ...) called\r\n"));

            // Start our logging level.
            pBT = (LPSPS_BEGIN_TEST)spParam;
            g_pKato->BeginLevel(pBT->lpFTE->dwUniqueID, 
                                _T("BEGIN TEST: \"%s\", Threads=%u, Seed=%u"),
                                pBT->lpFTE->lpDescription, pBT->dwThreadCount,
                                pBT->dwRandomSeed);

            return SPR_HANDLED;

        // Message: SPM_END_TEST
        //
        case SPM_END_TEST:
            Debug(_T("ShellProc(SPM_END_TEST, ...) called\r\n"));

            // End our logging level.
            pET = (LPSPS_END_TEST)spParam;
            g_pKato->EndLevel(_T("END TEST: \"%s\", %s, Time=%u.%03u"),
                              pET->lpFTE->lpDescription,
                              pET->dwResult == TPR_SKIP ? _T("SKIPPED") :
                              pET->dwResult == TPR_PASS ? _T("PASSED") :
                              pET->dwResult == TPR_FAIL ? _T("FAILED") : _T("ABORTED"),
                              pET->dwExecutionTime / 1000, pET->dwExecutionTime % 1000);

            return SPR_HANDLED;

        // Message: SPM_EXCEPTION
        //
        case SPM_EXCEPTION:
            Debug(_T("ShellProc(SPM_EXCEPTION, ...) called\r\n"));
            g_pKato->Log(LOG_EXCEPTION, _T("Exception occurred!"));
            return SPR_HANDLED;

        default:
            Debug(_T("ShellProc received bad message: 0x%X\r\n"), uMsg);
            ASSERT(!"Default case reached in ShellProc!");
            return SPR_NOT_HANDLED;
    }
}




void ShowUsage()
{
    g_pKato->Log(LOG_COMMENT, _T("XWIFI11B: Usage: tux -o -d xwifi-autotest"));
}
