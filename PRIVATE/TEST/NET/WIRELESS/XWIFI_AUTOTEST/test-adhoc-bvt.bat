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
d *** starting joining -> static ***
tux -o -d xwifi_autotest -c "-ssid AD-NO-ENCRYPTION:-adhoc:-auth:open: -test ip:100.100.x.x -static ip:100.100.0.2" 
tux -o -d xwifi_autotest -c "-ssid AD-WEP40-OPEN:-adhoc:-auth:open:-wep:1/0x1234567890 -test ip:100.100.x.x -static ip:100.100.0.2" 
tux -o -d xwifi_autotest -c "-ssid AD-WEP104-OPEN:-adhoc:-auth:open:-wep:1/0x12345678901234567890123456 -test ip:100.100.x.x -static ip:100.100.0.2" 
tux -o -d xwifi_autotest -c "-ssid AD-WEP40-SHARED:-adhoc:-auth:shared:-wep:1/0x1234567890 -test ip:100.100.x.x -static ip:100.100.0.2" 
tux -o -d xwifi_autotest -c "-ssid AD-WEP104-SHARED:-adhoc:-auth:shared:-wep:1/0x12345678901234567890123456 -test ip:100.100.x.x -static ip:100.100.0.2" 
d *** starting joining -> DHCP ***
tux -o -d xwifi_autotest -c "-ssid AD-NO-ENCRYPTION(DHCP):-adhoc:-auth:open: -test ip:10.10.x.x " 
tux -o -d xwifi_autotest -c "-ssid AD-WEP40-OPEN(DHCP):-adhoc:-auth:open:-wep:1/0x1234567890 -test ip:10.10.x.x " 
tux -o -d xwifi_autotest -c "-ssid AD-WEP104-OPEN(DHCP):-adhoc:-auth:open:-wep:1/0x12345678901234567890123456 -test ip:10.10.x.x " 
tux -o -d xwifi_autotest -c "-ssid AD-WEP40-SHARED(DHCP):-adhoc:-auth:shared:-wep:1/0x1234567890 -test ip:10.10.x.x " 
tux -o -d xwifi_autotest -c "-ssid AD-WEP104-SHARED(DHCP):-adhoc:-auth:shared:-wep:1/0x12345678901234567890123456 -test ip:10.10.x.x " 
d *** starting joining -> autoip ***
tux -o -d xwifi_autotest -c "-ssid AD-NO-ENCRYPTION:-adhoc:-auth:open: -test ip:autoip " 
tux -o -d xwifi_autotest -c "-ssid AD-WEP40-OPEN:-adhoc:-auth:open:-wep:1/0x1234567890 -test ip:autoip " 
tux -o -d xwifi_autotest -c "-ssid AD-WEP104-OPEN:-adhoc:-auth:open:-wep:1/0x12345678901234567890123456 -test ip:autoip " 
tux -o -d xwifi_autotest -c "-ssid AD-WEP40-SHARED:-adhoc:-auth:shared:-wep:1/0x1234567890 -test ip:autoip " 
tux -o -d xwifi_autotest -c "-ssid AD-WEP104-SHARED:-adhoc:-auth:shared:-wep:1/0x12345678901234567890123456 -test ip:autoip " 
d *** starting new adhoc net ***
tux -o -d xwifi_autotest -c "-ssid NAD-NO-ENCRYPTION:-adhoc:-auth:open: -test ip:autoip " 
tux -o -d xwifi_autotest -c "-ssid NAD-WEP40-OPEN:-adhoc:-auth:open:-wep:1/0x1234567890 -test ip:autoip " 
tux -o -d xwifi_autotest -c "-ssid NAD-WEP104-OPEN:-adhoc:-auth:open:-wep:1/0x12345678901234567890123456 -test ip:autoip " 
tux -o -d xwifi_autotest -c "-ssid NAD-WEP40-SHARED:-adhoc:-auth:shared:-wep:1/0x1234567890 -test ip:autoip " 
tux -o -d xwifi_autotest -c "-ssid NAD-WEP104-SHARED:-adhoc:-auth:shared:-wep:1/0x12345678901234567890123456 -test ip:autoip " 
