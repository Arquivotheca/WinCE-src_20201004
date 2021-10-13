@REM
@REM Copyright (c) Microsoft Corporation.  All rights reserved.
@REM
@REM
@REM Use of this source code is subject to the terms of the Microsoft shared
@REM source or premium shared source license agreement under which you licensed
@REM this source code. If you did not accept the terms of the license agreement,
@REM you are not authorized to use this source code. For the terms of the license,
@REM please see the license agreement between you and Microsoft or, if applicable,
@REM see the SOURCE.RTF on your install media or the root of your tools installation.
@REM THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
@REM
tux -o -d xwifi_autotest -c "-ssid NO-ENCRYPTION(DHCP)::-auth:open: -test ip:100.100.x.x -static ip:100.100.0.2" 
tux -o -d xwifi_autotest -c "-ssid WEP40-OPEN(DHCP)::-auth:open:-wep:1/0x1234567890 -test ip:100.100.x.x -static ip:100.100.0.2" 
tux -o -d xwifi_autotest -c "-ssid WEP104-OPEN(DHCP)::-auth:open:-wep:1/0x12345678901234567890123456 -test ip:100.100.x.x -static ip:100.100.0.2" 
tux -o -d xwifi_autotest -c "-ssid NO-ENCRYPTION(DHCP)::-auth:open:-wep:1/0x12345678901234567890123456 -test ip:100.100.x.x -static ip:100.100.0.2" 
tux -o -d xwifi_autotest -c "-ssid NO-ENCRYPTION(DHCP)::-auth:shared:-wep:1/0x1234567890 -test ip:0.0.0.0 -static ip:100.100.0.2" 
tux -o -d xwifi_autotest -c "-ssid WEP40-OPEN(DHCP)::-auth:open: -test ip:100.100.x.x -static ip:100.100.0.2" 
tux -o -d xwifi_autotest -c "-ssid WEP40-OPEN(DHCP)::-auth:open:-wep:1/wrong -test ip:100.100.x.x -static ip:100.100.0.2" 
tux -o -d xwifi_autotest -c "-ssid WEP40-OPEN(DHCP)::-auth:shared:-wep:1/0x1234567890 -test ip:0.0.0.0 -static ip:100.100.0.2" 
tux -o -d xwifi_autotest -c "-ssid WEP104-OPEN(DHCP)::-auth:open: -test ip:100.100.x.x -static ip:100.100.0.2" 
tux -o -d xwifi_autotest -c "-ssid WEP104-OPEN(DHCP)::-auth:open:-wep:1/0x1234567890 -test ip:100.100.x.x -static ip:100.100.0.2" 
tux -o -d xwifi_autotest -c "-ssid WEP104-OPEN(DHCP)::-auth:shared:-wep:1/0x12345678901234567890123456 -test ip:0.0.0.0 -static ip:100.100.0.2" 
d skip
tux -o -d xwifi_autotest -c "-ssid WEP40-SHARED(DHCP)::-auth:shared:-wep:1/0x1234567890 -test ip:100.100.x.x -static ip:100.100.0.2" 
tux -o -d xwifi_autotest -c "-ssid WEP104-SHARED(DHCP)::-auth:shared:-wep:1/0x12345678901234567890123456 -test ip:100.100.x.x -static ip:100.100.0.2" 
d skip
d skip
tux -o -d xwifi_autotest -c "-ssid WEP40-SHARED(DHCP)::-auth:open: -test ip:0.0.0.0 -static ip:100.100.0.2" 
tux -o -d xwifi_autotest -c "-ssid WEP40-SHARED(DHCP)::-auth:shared:-wep:1/wrong -test ip:0.0.0.0 -static ip:100.100.0.2" 
tux -o -d xwifi_autotest -c "-ssid WEP40-SHARED(DHCP)::-auth:open:-wep:1/0x1234567890 -test ip:0.0.0.0 -static ip:100.100.0.2" 
tux -o -d xwifi_autotest -c "-ssid WEP104-SHARED(DHCP)::-auth:open: -test ip:0.0.0.0 -static ip:100.100.0.2" 
tux -o -d xwifi_autotest -c "-ssid WEP104-SHARED(DHCP)::-auth:shared:-wep:1/wrong -test ip:0.0.0.0 -static ip:100.100.0.2" 
tux -o -d xwifi_autotest -c "-ssid WEP104-SHARED(DHCP)::-auth:open:-wep:1/0x12345678901234567890123456 -test ip:0.0.0.0 -static ip:100.100.0.2" 
d skip
tux -o -d xwifi_autotest -c "-ssid AD-NO-ENCRYPTION(DHCP)::-auth:open: -test ip:0.0.0.0 -static ip:100.100.0.2" 
tux -o -d xwifi_autotest -c "-ssid AD-WEP40-OPEN(DHCP)::-auth:open:-wep:1/0x1234567890 -test ip:0.0.0.0 -static ip:100.100.0.2" 
tux -o -d xwifi_autotest -c "-ssid AD-WEP104-OPEN(DHCP)::-auth:open:-wep:1/0x12345678901234567890123456 -test ip:0.0.0.0 -static ip:100.100.0.2" 
tux -o -d xwifi_autotest -c "-ssid AD-WEP40-SHARED(DHCP)::-auth:shared:-wep:1/0x1234567890 -test ip:0.0.0.0 -static ip:100.100.0.2" 
tux -o -d xwifi_autotest -c "-ssid AD-WEP104-SHARED(DHCP)::-auth:shared:-wep:1/0x12345678901234567890123456 -test ip:0.0.0.0 -static ip:100.100.0.2" 
