/***********************************************************************************************************************************
//++
// Readme.txt
//++
************************************************************************************************************************************

########################
How to build the driver:
########################

In order to build the driver one should have XP DDK and MS SDK/ MS Visual Studio installed. Use XP DDK build environment to build the driver.
1>Open XP checked/retail build window by clicking "Start->(All)Programs->Development Kits->Windows DDK xxxx.xxxx->Build Environments->Win XP (checked/Free) build environment".
2>Get to the folder which has source code to build NDP driver.
  i.e. cd  \Macallan\private\test\net\NDP\sys\NTsys
3>Run "build -c" at the prompt. This will build the driver (sys and .pdb (symbol)) under the same folder.

#######################
How to debug the driver
#######################
Either use Windbg or Softice to debug the driver.

#######################
How to install the driver
#######################
The current way (as of 5/15/2003) is to install using the network control panel application. Copy "ndp.inf" and "ndp.sys" file to the local folder on the desktop machine. 
In Network Connections UI, select an any adapter/connection and open Properties.
Click Install, then Protocol, then Add, and then Have disk. Then point to the location of the above folder where ndp.inf and ndp.sys have been copied. Select "NDP Protocol driver" and click OK. 

#############################
How to UN-install the driver
#############################
The current way (as of 5/15/2003) is to UN-install using the network control panel application.
In Network Connections UI, select an any adapter/connection and open Properties. Then select "NDP Protocol" from the list of the protocols shown and click "Uninstall".


######################
USAGE
######################
In order to use this driver, use perf_ndis.dll. This dll is located under \Macallan\private\test\net\ndis\NDP\perfdll. This dll is tux based dll. In order to use this dll one would need, tux.exe, kato.dll, tooltalk.dll. Run <s tux -o -d perf_ndis.dll -c "? ?"> to know about the options.


#######################
Some Questions/Answers
#######################
1> Can I use my Wince build environment to build the driver for NT.
Ans:
NO.Building the Protocol driver for NT from "NT build environment" offered by Win CE has some problems. And hence it is decided that we'll build the NT driver using XP ddk and check in the final binaries to SD.The problems seen as of date 22 April 2003 are
1. The version of compiler used in Win CE build is lower than what is expected by XP DDK. The error that one would get is,
Compiling - c:\macallan\private\test\net\ndis\ndp\ntcesys\.\ndp.c for x86
c:\macallan\sdk\nt\inc\wdm.h(24) : error C1189: #error :  Compiler version not s
upported by Windows DDK
2. Also NT DDK is not checked into SD as we have NT SDK.


***************************************************************************************************************************************/