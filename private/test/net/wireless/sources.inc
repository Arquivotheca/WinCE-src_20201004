!if 0
Copyright (c) Microsoft Corporation.  All rights reserved.
!endif
!if 0
Use of this source code is subject to the terms of the Microsoft
premium shared source license agreement under which you licensed
this source code. If you did not accept the terms of the license
agreement, you are not authorized to use this source code.
For the terms of the license, please see the license agreement
signed by you and Microsoft.
THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
!endif

WIFI_ROOT=$(TEST_ROOT)\net\wireless
!message wireless\sources.inc: WIFI_ROOT = $(WIFI_ROOT)

#############################################################################
#
# Libraries and header-files:
#

_PUB_COMM_OAK_INC=$(_PUBLICROOT)\common\oak\inc

# OS Utils
_CEOSUTIL=$(_CEOSUTIL)

# Stress Utils
_STRESS_UTILS=$(_STRESSUTILS) 

#WlanProfile util lib
_WLANPROFILE_UTILS=$(SERVERS_OAK_LIB_PATH)\wlanprfl.lib
_ONEX=$(SERVERS_OAK_LIB_PATH)\onex.lib
_WLANUTIL=$(SERVERS_OAK_LIB_PATH)\wlanutil.lib
_L2CMNALL=$(SERVERS_OAK_LIB_PATH)\l2cmnall.lib


# APControl Libraries: 
#
_APCONTROL_INC=$(WIFI_ROOT)\common\APController\inc
_APCONTROL_LIB=$(TEST_OAK_LIB_PATH)\APController.lib
_APCONTROLSVR_INC=$(WIFI_ROOT)\common\APController\svrinc
_APCONTROLSVR_LIB=$(TEST_OAK_LIB_PATH)\APControlServer.lib

# Wireless Utilities Libraries: 
#
_WIFI_METRICS_INC=$(WIFI_ROOT)\common\WiFiMetric\inc
_WIFI_METRICS_LIB=$(TEST_OAK_LIB_PATH)\WiFiMetricsLib.lib
_WIRELESS_INC=$(WIFI_ROOT)\common\inc
_WIRELESS_LIB=$(TEST_OAK_LIB_PATH)\WiFUtils.lib 


# NDIS Test Libraries: 
#
_NDP_INC=$(TEST_ROOT)\net\ndis\NDP\inc
_NDP_LIB=$(TEST_OAK_LIB_PATH)\ndp_lib.lib
_NDT_INC=$(TEST_ROOT)\net\ndis\NDT\inc
_NUIOLIB_INC=$(TEST_ROOT)\net\ndis\nuio\lib
_NUIOLIB_LIB=$(TEST_OAK_LIB_PATH)\nuiolib.lib

# USB "Clicker" Libraries:
#
_SCBEXAPI_LIB=$(TEST_OAK_LIB_PATH)\SCBExAPI.lib
_USBCLICKER_INC=$(WIFI_ROOT)\common\USBClicker\inc
_USBCLICKER_LIB=$(TEST_OAK_LIB_PATH)\USBClicker.lib

# OS Libraries:
#
_AYGSHELL_LIB=$(_AYGSHELL)
_CELLCORE_LIB=$(_CELLCORE)
_NDIS_LIB=$(_NDIS)
_NTCOMPAT_LIB=$(_NTCOMPAT)
_SECUR32_LIB=$(_SECURE32)

# Common Test Libraries: 
#
_NETCOMMON_INC=$(TEST_ROOT)\net\common\inc
_TESTCOMMON_INC=$(TEST_ROOT)\common\inc

_BACKCHANNEL_LIB=$(TEST_OAK_LIB_PATH)\backchannel.lib
_CCLIB_LIB=$(_CCLIB)
_CELLCORE_INC=$(TEST_ROOT)\net\cellcore\tools\inc
_CMDLINE_LIB=$(_CMDLINE)
_CMNPRINT_LIB=$(TEST_OAK_LIB_PATH)\cmnprint.lib
_FRAMEPARSER_INC=$(TEST_ROOT)\net\common\FrameParser\inc
_FRAMEPARSER_LIB=$(TEST_OAK_LIB_PATH)\FrameParser.lib
_KATO_LIB=$(_KATO)
_LITEXML_INC=$(TEST_ROOT)\common\LiteXML
_LITEXML_LIB=$(TEST_OAK_LIB_PATH)\LiteXML.lib
_NETALL_LIB=$(TEST_OAK_LIB_PATH)\netall.lib
_NETCMN_LIB=$(TEST_OAK_LIB_PATH)\netcmn.lib
_NETLOG_LIB=$(TEST_OAK_LIB_PATH)\netlog.lib
_NETLOGMGR_LIB=$(TEST_OAK_LIB_PATH)\NetlogManager.lib
_NETMAIN_LIB=$(TEST_OAK_LIB_PATH)\netmain.lib
_SCENARIOPARSER_INC=$(TEST_ROOT)\net\common\ScenarioParser\inc
_SCENARIOPARSER_LIB=$(TEST_OAK_LIB_PATH)\ScenarioParser.lib

# Wireless Test Libraries:
#
_ADHOCTESTS_INC=$(WIFI_ROOT)\AdHocTests\inc
_ADHOCTESTS_LIB=$(TEST_OAK_LIB_PATH)\AdHocTestsLib.lib
_AUTHMATRIX_INC=$(WIFI_ROOT)\AuthMatrix\inc
_AUTHMATRIX_LIB=$(TEST_OAK_LIB_PATH)\AuthMatLib.lib
_WIFILTK_INC=$(WIFI_ROOT)\LTK\inc
_WIFILTK_LIB=$(TEST_OAK_LIB_PATH)\WiFiLTKLib.lib
_WIFIBVT_INC=$(WIFI_ROOT)\BVT\inc
_WIFIBVT_LIB=$(TEST_OAK_LIB_PATH)\WiFiBVTLib.lib
_IHVTEST_INC=$(WIFI_ROOT)\nativeWiFi\IHV\inc
_IHVTEST_LIB=$(TEST_OAK_LIB_PATH)\IHVTestExtLib.lib
_EAPAUTHSVR_LIB=$(TEST_OAK_LIB_PATH)\EapAuthServerLib.lib
_EAPHOSTUTILS_LIB=$(TEST_OAK_LIB_PATH)\EapHostutils.lib
_EAPPARSER_LIB=$(TEST_OAK_LIB_PATH)\EAPOLFrameParser.lib
_EAPSIM_INC=$(WIFI_ROOT)\EapSim\inc
_EAPSIM_LIB=$(TEST_OAK_LIB_PATH)\EapSimLib.lib
_EXPBACKTESTS_LIB=$(TEST_OAK_LIB_PATH)\ExpBackoffLib.lib
_MSMAPITESTS_LIB=$(TEST_OAK_LIB_PATH)\MSMAPILib.lib
_PMKTESTS_INC=$(WIFI_ROOT)\PMKTests\inc
_PMKTESTS_LIB=$(TEST_OAK_LIB_PATH)\PMKTestsLib.lib
_ROAMMGR_INC=$(WIFI_ROOT)\RoamMgr\inc
_ROAMMGR_LIB=$(TEST_OAK_LIB_PATH)\RoamMgrTestLib.lib
_QOSTESTS_INC=$(WIFI_ROOT)\QoSTests\inc
_QOSTESTS_LIB=$(TEST_OAK_LIB_PATH)\QoSTestsLib.lib
_SSIDTESTS_INC=$(WIFI_ROOT)\SSIDTests\inc
_SSIDTESTS_LIB=$(TEST_OAK_LIB_PATH)\SSIDTestsLib.lib
_WIFIDETECTION_INC=$(WIFI_ROOT)\utils\WiFiDetection\inc
_WIFIDETECTION_LIB=$(TEST_OAK_LIB_PATH)\WiFiDetectLib.lib
_WIFILTK_INC=$(WIFI_ROOT)\LTK\inc
_WIFILTK_LIB=$(TEST_OAK_LIB_PATH)\WiFiLTKLib.lib
_WIFIPOWER_INC=$(WIFI_ROOT)\power\lib
_WIFIPOWER_LIB=$(TEST_OAK_LIB_PATH)\WiFiPowerLib.lib
_WIFIPOWERTEST_LIB=$(TEST_OAK_LIB_PATH)\WifiPowerTestLib.lib
_WIFIROAM_INC=$(WIFI_ROOT)\WiFiRoam\inc
_WIFIROAM_LIB=$(TEST_OAK_LIB_PATH)\WiFiRoamtLib.lib
_WZCTESTS_INC=$(WIFI_ROOT)\WZCTests\inc
_WZCTESTS_LIB=$(TEST_OAK_LIB_PATH)\wzctestslib.lib
_XWIFI_INC=$(WIFI_ROOT)\xwifi\inc
_XWIFI_LIB=$(TEST_OAK_LIB_PATH)\xwifilib.lib



#############################################################################
#
# Build-specific definitions:
#
!INCLUDE $(WIFI_ROOT)\sources.$(_TGTOS)
