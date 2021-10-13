//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#ifndef __AP_ENTRY_CONFIG
#define __AP_ENTRY_CONFIG

#include "Windot11.h"
#include "80211hdr.h"

#define MAX_SUPPORTED_RATES 8

// IOCTL command types
#define VWA_IOCTL_ADD_AP_ENTRY			1
#define VWA_IOCTL_REMOVE_AP_ENTRY		2
#define VWA_IOCTL_GET_AP_ENTRY			4
#define VWA_IOCTL_SET_AP_VISIBILE		8
#define VWA_IOCTL_SET_AP_INVISIBLE		16
#define VWA_IOCTL_GET_INDEX_FROM_SSID	32
#define VWA_IOCTL_SET_ALL_VISIBLE		64
#define VWA_IOCTL_SET_ALL_INVISIBLE		128

// Structure to be used by applications in usermode to pass to the global AP Simulator
class APEntryConfig
{
public:

	APEntryConfig();
	APEntryConfig(
		DOT11_SSID ssid,
		DOT11_MAC_ADDRESS bssid,
		DOT11_BSS_TYPE bssType,
		BOOLEAN isVisible,
		DOT11_AUTH_ALGORITHM authAlgo,
		DOT11_CIPHER_ALGORITHM ucipherAlgo,
		DOT11_CIPHER_ALGORITHM mcipherAlgo,
		BOOLEAN isEAPEnabled,
		LONG rssi,
		ULONG linkQuality,
		UINT numSupportedRates,
		UCHAR *supportedRates,
		USHORT beaconPeriod,
		DOT11_CAPABILITY capability,
		CHAR * key,
		USHORT cbKey);
	~APEntryConfig();
	APEntryConfig(const APEntryConfig& src);
	APEntryConfig& operator=(const APEntryConfig& src);
	// AP's SSID
	DOT11_SSID m_SSID;

	// AP's BSSID
	DOT11_MAC_ADDRESS m_BSSID;

	// BSS type
	DOT11_BSS_TYPE m_BSSType;

	// Visibility
	BOOLEAN m_visibility;

	// Auth algorithm
	DOT11_AUTH_ALGORITHM m_AuthAlgo;

	// Encryption algorithm
	DOT11_CIPHER_ALGORITHM m_UnicastCipherAlgo;

	// Encryption algorithm
	DOT11_CIPHER_ALGORITHM m_MulticastCipherAlgo;

	// EAP enabled
	BOOLEAN m_EAPEnabled;

	// Signal Strength
	LONG m_RSSI;

	// Link Quality
	ULONG m_LinkQuality;

	// Supported Rates
	UINT m_NumSupportedRates;
	UCHAR m_SupportedRates[MAX_SUPPORTED_RATES];

	// Beacon Period
	USHORT m_BeaconPeriod;

	// Capability
	DOT11_CAPABILITY m_Capability;

	// Security key, if any
	CHAR *m_szKey;

	// Security key length, in bytes
	USHORT m_cbKey;

	//Is the security key valid?
	BOOL m_IsValidKey;

private:
	BOOL ValidateAndTransformKey(CHAR *chKey, PUSHORT pcbKey, DOT11_CIPHER_ALGORITHM cipherAlgo);
	BOOL ValidateAndTransformWEPKey(CHAR *chKey, PUSHORT pcbKey, USHORT expectedHexLength);
	BOOL IsHex(CHAR *chKey, USHORT cbKey);
	VOID AsciiToHex(CHAR *chKey, USHORT cbKey);
};

#endif