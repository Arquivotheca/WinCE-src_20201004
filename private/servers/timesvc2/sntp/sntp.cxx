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
/*--
Module Name: sntp.cxx
Abstract: service sntp module
--*/
#include <windows.h>
#include <stdio.h>
#include <winsock2.h>

#include <ws2tcpip.h>

#include <svsutil.hxx>
#include <service.h>
#include <notify.h>

#include "sntp.h"

#define NOTIFICATION_WAIT_SEC    30

#if ! defined(DNS_MAX_NAME_BUFFER_LENGTH)
#define DNS_MAX_NAME_BUFFER_LENGTH      (256)
#endif

#define NTP_PORT        123
#define NTP_PORT_A        "123"

#define BASE_KEY        L"services\\timesvc"

#define MIN_REFRESH        (5*60*1000)
#define MIN_MULTICAST    (5*60*1000)
#define MIN_TIMEUPDATE    (5*1000)

#define MAX_STACKS    5
#define MAX_SERVERS    10
#define MAX_MCASTS    10

#define MAX_MSZ        1024

static HANDLE hNotifyThread = NULL;
static HANDLE hExitEvent = NULL;

typedef BOOL (*tCeRunAppAtEvent)(WCHAR *pwszAppName, LONG lWhichEvent);
typedef DWORD (*tNotifyAddrChange)(PHANDLE Handle, LPOVERLAPPED overlapped);

#define NTPTIMEOFFSET (0x014F373BFDE04000)
#define FIVETOTHESEVETH (0x001312D)

/*
                         1                   2                   3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |LI | VN  |Mode |    Stratum    |     Poll      |   Precision   |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                          Root Delay                           |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                       Root Dispersion                         |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                     Reference Identifier                      |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                                                               |
      |                   Reference Timestamp (64)                    |
      |                                                               |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                                                               |
      |                   Originate Timestamp (64)                    |
      |                                                               |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                                                               |
      |                    Receive Timestamp (64)                     |
      |                                                               |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                                                               |
      |                    Transmit Timestamp (64)                    |
      |                                                               |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                 Key Identifier (optional) (32)                |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                                                               |
      |                                                               |
      |                 Message Digest (optional) (128)               |
      |                                                               |
      |                                                               |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/

///////////////////////////////////////////////////////////////////////
//
//        Base Classes
//
//
class NTP_REQUEST {
private:
    union {
        struct {
            unsigned char i_lvm, i_stratum, i_poll, i_prec;
            unsigned char i_root_delay[4];
            unsigned char i_root_dispersion[4];
            unsigned char i_ref_identifier[4];
            unsigned char i_ref_stamp[8];
            unsigned char i_orig_stamp[8];
            unsigned char i_recv_stamp[8];
            unsigned char i_trans_stamp[8];
        };
        unsigned __int64 ll;
    };

public:
    unsigned int li (void) {
        return (i_lvm >> 6) & 0x03;
    }

    void set_li (unsigned int l) {
        i_lvm = (i_lvm & 0x3f) | ((l & 0x3) << 6);
    }

    unsigned int vn (void) {
        return (i_lvm >> 3) & 0x07;
    }

    void set_vn (unsigned int v) {
        i_lvm = (i_lvm & 0xc7) | ((v & 0x7) << 3);
    }

    unsigned int mode (void) {
        return i_lvm & 0x07;
    }

    void set_mode (unsigned int m) {
        i_lvm = (i_lvm & 0xf8) | (m & 0x7);
    }

    unsigned int stratum (void) {
        return i_stratum;
    }

    void set_stratum (unsigned int s) {
        i_stratum = s;
    }

    unsigned int poll (void) {
        return i_poll;
    }

    void set_poll (unsigned int p) {
        i_poll = p;
    }

    unsigned int precision (void) {
        return i_prec;
    }

    void set_precision (unsigned int p) {
        i_prec = p;
    }

    unsigned int root_delay (void) {
        return (i_root_delay[0] << 24) | (i_root_delay[1] << 16) | (i_root_delay[2] << 8) | i_root_delay[3];
    }

    void set_root_delay (unsigned int rd) {
        i_root_delay[0] = (unsigned char)(rd >> 24);
        i_root_delay[1] = (unsigned char)(rd >> 16);
        i_root_delay[2] = (unsigned char)(rd >>  8);
        i_root_delay[3] = (unsigned char) rd;
    }

    unsigned int root_dispersion (void) {
        return (i_root_dispersion[0] << 24) | (i_root_dispersion[1] << 16) | (i_root_dispersion[2] << 8) | i_root_dispersion[3];
    }

    void set_root_dispersion (unsigned int rd) {
        i_root_dispersion[0] = (unsigned char)(rd >> 24);
        i_root_dispersion[1] = (unsigned char)(rd >> 16);
        i_root_dispersion[2] = (unsigned char)(rd >>  8);
        i_root_dispersion[3] = (unsigned char) rd;
    }

    unsigned int ref_id (void) {
        return (i_ref_identifier[0] << 24) | (i_ref_identifier[1] << 16) | (i_ref_identifier[2] << 8) | i_ref_identifier[3];
    }

    void set_ref_id (unsigned int rid) {
        i_ref_identifier[0] = (unsigned char)(rid >> 24);
        i_ref_identifier[1] = (unsigned char)(rid >> 16);
        i_ref_identifier[2] = (unsigned char)(rid >>  8);
        i_ref_identifier[3] = (unsigned char) rid;
    }

    unsigned __int64 ref_stamp (void) {
        return *(unsigned __int64 *)i_ref_stamp;
    }

    void set_ref_stamp (unsigned __int64 ll) {
        *(unsigned __int64 *)i_ref_stamp = ll;
    }

    unsigned __int64 orig_stamp (void) {
        return *(unsigned __int64 *)i_orig_stamp;
    }

    void set_orig_stamp (unsigned __int64 ll) {
        *(unsigned __int64 *)i_orig_stamp = ll;
    }

    unsigned __int64 recv_stamp (void) {
        return *(unsigned __int64 *)i_recv_stamp;
    }

    void set_recv_stamp (unsigned __int64 ll) {
        *(unsigned __int64 *)i_recv_stamp = ll;
    }

    unsigned __int64 trans_stamp (void) {
        return *(unsigned __int64 *)i_trans_stamp;
    }

    void set_trans_stamp (unsigned __int64 ll) {
        *(unsigned __int64 *)i_trans_stamp = ll;
    }
};

enum When {
    Now,
    Shortly,
    Later
};

class TimeState : public SVSSynch {
    SVSThreadPool    *pEvents;

    SOCKET        saServer[MAX_STACKS];
    int        iSock;

    DWORD        dwRefreshMS;
    DWORD           dwRecoveryRefreshMS;
    DWORD        dwAdjustThreshMS;
    DWORD           dwMulticastPeriodMS;

    SVSCookie        ckNextRefresh;
    SVSCookie        ckNextMulticast;

    char    sntp_servers[MAX_SERVERS][DNS_MAX_NAME_BUFFER_LENGTH];
    char    sntp_mcasts[MAX_MCASTS][DNS_MAX_NAME_BUFFER_LENGTH];

    int        cServers;
    int        cMcasts;

    union {
        struct {
            unsigned int fStarted           : 1;
            unsigned int fHaveClient        : 1;
            unsigned int fHaveServer        : 1;
            unsigned int fSystemTimeCorrect : 1;
            unsigned int fRefreshRequired   : 1;
            unsigned int fForceTimeToServer : 1;
        };
        unsigned int uiFlags;
    };

    void ReInit (void) {
        DEBUGMSG(ZONE_TRACE, (L"[TIMESVC] State reinitialized\r\n"));

        pEvents = 0;
                for (int i = 0 ; i < SVSUTIL_ARRLEN(saServer) ; ++i)
            saServer[i] = INVALID_SOCKET;

        iSock = 0;


        llLastSyncTimeXX = 0;
        ckNextRefresh = ckNextMulticast = 0;

        memset (sntp_servers, 0, sizeof(sntp_servers));
        memset (sntp_mcasts,  0, sizeof(sntp_mcasts));
        cServers = 0;
        cMcasts  = 0;

        dwRefreshMS = dwRecoveryRefreshMS = dwAdjustThreshMS = dwMulticastPeriodMS = 0;

        uiFlags = 0;
    }

public:
    unsigned __int64        llLastSyncTimeXX;

    int IsStarted (void) { return fStarted; }
    int LastUpdateFailed (void) { return fRefreshRequired; }

    int RefreshConfig (void);
    int ForcedUpdate (void);
    int UpdateNowOrLater (enum When, int fForceTime = FALSE);
    int TimeChanged (void);

    int Start (void);
    SVSThreadPool *Stop (void);

    TimeState (void) {
        ReInit ();
    }

    int IsTimeAccurate(void) {
        // Trust the time if we have a local clock, or if 
        // we have a client and there is no refresh required (its up-to-date).
        return fSystemTimeCorrect || (! fRefreshRequired && fHaveClient);
    };

    friend DWORD WINAPI RxThread (LPVOID lpUnused);
    friend DWORD WINAPI TimeRefreshThread (LPVOID lpUnused);
    friend DWORD WINAPI MulticastThread (LPVOID lpUnused);
    friend DWORD WINAPI GetTimeOffsetOnServer (LPVOID lpArg);
};

struct GetTimeOffset {
    __int64         llOffset;
    unsigned char   cchServer[DNS_MAX_NAME_BUFFER_LENGTH];
};

static int GetAddressList (HKEY hk, WCHAR *szValue, char *pItems, int cItems, int cItemSize, int *piCount) {
    *piCount = 0;

    union {
        DWORD   dw;
        in_addr ia;
        WCHAR   szM[MAX_MSZ];
    } u;

    DWORD dwType;
    DWORD dwSize = sizeof(u);

    if (ERROR_SUCCESS != RegQueryValueEx (hk, szValue, NULL, &dwType, (LPBYTE)&u, &dwSize))
        return ERROR_SUCCESS;

    u.szM[MAX_MSZ-1] = '\0';
    u.szM[MAX_MSZ-2] = '\0';

    if (dwType == REG_DWORD) {    // Legacy: ip address
        if (dwSize == sizeof(DWORD)) {
            *piCount = 1;
            strcpy (pItems, inet_ntoa (u.ia));
            return ERROR_SUCCESS;
        }
    }

    if (dwType == REG_SZ) {        // Legacy: single name
        int cc = WideCharToMultiByte (CP_ACP, 0, u.szM, -1, pItems, cItemSize, NULL, NULL);
        if (cc > 0) {
            *piCount = 1;
            return ERROR_SUCCESS;
        }
    }

    if (dwType == REG_MULTI_SZ) {    // List of names/addresses
        WCHAR *p = u.szM;
        while (*p) {
            int cc = cItems ? WideCharToMultiByte (CP_ACP, 0, p, -1, pItems, cItemSize, NULL, NULL) : 0;
            if (cc == 0) {
                *piCount = 0;
                break;
            }

            ++*piCount;
            pItems += cItemSize;
            cItems--;

            p += wcslen (p) + 1;
        }

        return ERROR_SUCCESS;
    }

    return ERROR_BAD_CONFIGURATION;
}


///////////////////////////////////////////////////////////////////////
//
//        Base Classes - implementation
//
//
int TimeState::RefreshConfig (void) {
    DEBUGMSG(ZONE_INIT, (L"[TIMESVC] Refreshing configuration\r\n"));

    HKEY hk;
    int iErr = ERROR_BAD_CONFIGURATION;

    ReInit ();

    if (ERROR_SUCCESS == RegOpenKeyEx (HKEY_LOCAL_MACHINE, BASE_KEY, 0, KEY_READ, &hk)) {
        iErr = GetAddressList (hk, L"server", (char *)sntp_servers, SVSUTIL_ARRLEN(sntp_servers), SVSUTIL_ARRLEN(sntp_servers[0]), &cServers);

        if (cServers) {
            iErr = ERROR_SUCCESS;

            DWORD dw;
            DWORD dwType = 0;
            DWORD dwSize = sizeof(dw);

            if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"AutoUpdate", NULL, &dwType, (LPBYTE)&dw, &dwSize)) &&
                    (dwType == REG_DWORD) && (dwSize == sizeof(dw)) && dw)
                fHaveClient = TRUE;

            if (fHaveClient) {
                dwType = 0;
                dwSize = sizeof(dwRefreshMS);
                if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"refresh", NULL, &dwType, (LPBYTE)&dwRefreshMS, &dwSize)) &&
                    (dwType == REG_DWORD) && (dwSize == sizeof(DWORD)) && (dwRefreshMS >= MIN_REFRESH)) {
                    ;
                } else {
                    iErr = ERROR_BAD_CONFIGURATION;    // Require refresh key if we have a client
                    RETAILMSG(1, (L"[TIMESVC] Configuration error: refresh rate incorrect. Aborting.\r\n"));
                }
            }

            if ((iErr == ERROR_SUCCESS) && fHaveClient) {
                iErr = ERROR_BAD_CONFIGURATION;    // Require accelerated refresh key if we have a client

                dwType = 0;
                dwSize = sizeof(dwRecoveryRefreshMS);
                if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"recoveryrefresh", NULL, &dwType, (LPBYTE)&dwRecoveryRefreshMS, &dwSize)) &&
                    (dwType == REG_DWORD) && (dwSize == sizeof(DWORD)) && (dwRecoveryRefreshMS >= MIN_REFRESH) && (dwRecoveryRefreshMS <= dwRefreshMS)) {
                    iErr = ERROR_SUCCESS;
                } else {
                    RETAILMSG(1, (L"[TIMESVC] Configuration error: accelerated refresh rate incorrect. Aborting.\r\n"));
                }
            }

            if ((iErr == ERROR_SUCCESS) && fHaveClient) {
                iErr = ERROR_BAD_CONFIGURATION;    // Require threshold if we have a client

                dwType = 0;
                dwSize = sizeof(dw);

                if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"threshold", NULL, &dwType, (LPBYTE)&dw, &dwSize)) &&
                    (dwType == REG_DWORD) && (dwSize == sizeof(dw))) {
                    dwAdjustThreshMS = dw;

                    iErr = ERROR_SUCCESS;
                } else {
                    RETAILMSG(1, (L"[TIMESVC] Configuration error: time adjustment threshold incorrect. Aborting.\r\n"));
                }

            }
        } else {
            iErr = ERROR_SUCCESS;
            fSystemTimeCorrect = TRUE;
        }

        DWORD dw;
        DWORD dwType = 0;
        DWORD dwSize = sizeof(dw);

        if ((iErr == ERROR_SUCCESS) && (ERROR_SUCCESS == RegQueryValueEx (hk, L"ServerRole", NULL, &dwType, (LPBYTE)&dw, &dwSize)) &&
                            (dwType == REG_DWORD) && (dwSize == sizeof(dw)) && dw)
            fHaveServer = TRUE;

        dwType = 0;
        dwSize = sizeof(dw);

        if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"trustlocalclock", NULL, &dwType, (LPBYTE)&dw, &dwSize)) &&
                            (dwType == REG_DWORD) && (dwSize == sizeof(dw)) && dw)
            fSystemTimeCorrect = TRUE;

        if (fHaveServer) {
            iErr = GetAddressList (hk, L"multicast", (char *)sntp_mcasts, SVSUTIL_ARRLEN(sntp_mcasts), SVSUTIL_ARRLEN(sntp_mcasts[0]), &cMcasts);
            if ((iErr == ERROR_SUCCESS) && cMcasts) {
                if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"multicastperiod", NULL, &dwType, (LPBYTE)&dw, &dwSize)) &&
                            (dwType == REG_DWORD) && (dwSize == sizeof(dw)) && (dw > MIN_MULTICAST))
                    dwMulticastPeriodMS = dw;
                else {
                    iErr = ERROR_BAD_CONFIGURATION;
                    RETAILMSG(1, (L"[TIMESVC] Configuration error: Multicast period required. Aborting.\r\n"));
                }
            }
        }
        RegCloseKey (hk);
    } else {
        fHaveClient = TRUE;
        fHaveServer = TRUE;

        iErr = ERROR_SUCCESS;
        strcpy ((char *)sntp_servers[0], "time.windows.com");
        cServers = 1;

        sntp_mcasts[0][0] = '\0';
        cMcasts  = 0;        // No multicasts

        dwRefreshMS = 14*24*60*60*1000;            // Synchronize clock every two weeks
        dwRecoveryRefreshMS = 30*60*1000;         // Try every half hour if it fails
        dwAdjustThreshMS = 24*60*60*1000;                // Allow one day

        fSystemTimeCorrect = TRUE;                // Server provides time even if time synch failed
    }

    if (fHaveServer) {
    }
    for (int i = 0 ; i < cServers ; ++i) {
        DEBUGMSG(ZONE_INIT, (L"[TIMESVC] Configuration: sntp server           : %a\r\n", sntp_servers[i]));
    }

    DEBUGMSG(ZONE_INIT, (L"[TIMESVC] Configuration: client                : %s\r\n", fHaveClient ? L"enabled" : L"disabled"));
    DEBUGMSG(ZONE_INIT, (L"[TIMESVC] Configuration: server                : %s\r\n", fHaveServer ? L"enabled" : L"disabled"));
    DEBUGMSG(ZONE_INIT, (L"[TIMESVC] Configuration: regular refresh       : %d ms (%d day(s))\r\n", dwRefreshMS, dwRefreshMS/(24*60*60*1000)));
    DEBUGMSG(ZONE_INIT, (L"[TIMESVC] Configuration: accelerated refresh   : %d ms (%d day(s))\r\n", dwRecoveryRefreshMS, dwRecoveryRefreshMS/(24*60*60*1000)));
    DEBUGMSG(ZONE_INIT, (L"[TIMESVC] Configuration: adjustment threshold  : %d ms\r\n", dwAdjustThreshMS));

    if (fHaveServer) {
        DEBUGMSG(ZONE_INIT, (L"[TIMESVC] Configuration: system clock          : %s\r\n", fSystemTimeCorrect ? L"presumed correct" : L"presumed wrong if not updates"));
        for (int i = 0 ; i < cMcasts ; ++i) {
            DEBUGMSG(ZONE_INIT, (L"[TIMESVC] Configuration: multicast address     : %a\r\n", sntp_mcasts[i]));
        }
        DEBUGMSG(ZONE_INIT, (L"[TIMESVC] Configuration: multicast period      : %d ms\r\n", dwMulticastPeriodMS));
    }

    return iErr;
}

int TimeState::ForcedUpdate (void) {
    fRefreshRequired = TRUE;

    DEBUGMSG(ZONE_CLIENT, (L"[TIMESVC] Client: updating time NOW\r\n"));

    if (ckNextRefresh)
        pEvents->UnScheduleEvent (ckNextRefresh);

    if (! fSystemTimeCorrect)
        fForceTimeToServer = TRUE;

    if (! (ckNextRefresh = pEvents->ScheduleEvent (TimeRefreshThread, NULL, 0)))
        return ERROR_OUTOFMEMORY;

    return ERROR_SUCCESS;
}

int TimeState::UpdateNowOrLater (enum When when, int fForceTime) {
    if (! fHaveClient) {
        DEBUGMSG(ZONE_CLIENT, (L"[TIMESVC] Client: not enabled; update request ignored\r\n"));
        return ERROR_SUCCESS;
    }

    DWORD dwTimeout;
    if (when == Now) {
        fRefreshRequired = TRUE;
        dwTimeout = NULL;
    } else if (when == Shortly) {
        SVSUTIL_ASSERT (fRefreshRequired);
        dwTimeout = dwRecoveryRefreshMS;
    } else
        dwTimeout = dwRefreshMS;

    DEBUGMSG(ZONE_CLIENT, (L"[TIMESVC] Client: schedule time update in %d ms %s\r\n", dwTimeout, fForceTime ? L"(require refresh)" : L""));

    if (ckNextRefresh)
        pEvents->UnScheduleEvent (ckNextRefresh);

    if (fForceTime || (! fSystemTimeCorrect))
        fForceTimeToServer = TRUE;

    if (! (ckNextRefresh = pEvents->ScheduleEvent (TimeRefreshThread, NULL, dwTimeout)))
        return ERROR_OUTOFMEMORY;

    return ERROR_SUCCESS;
}

int TimeState::TimeChanged (void) {
    DEBUGMSG(ZONE_TRACE, (L"[TIMESVC] Server: Time changed - multicast now?\r\n"));
    if (cMcasts == 0)
        return 0;

    if (ckNextMulticast)
        pEvents->UnScheduleEvent (ckNextMulticast);

    if (! (ckNextMulticast = pEvents->ScheduleEvent (MulticastThread, NULL, 0)))
        return ERROR_OUTOFMEMORY;

    return ERROR_SUCCESS;
}

int TimeState::Start (void) {
    DEBUGMSG(ZONE_INIT, (L"[TIMESVC] Service starting\r\n"));

    pEvents = new SVSThreadPool (5);
    if (! pEvents) {
        DEBUGMSG(ZONE_ERROR, (L"[TIMESVC] Start error: Out of memory allocating threads\r\n"));
        return ERROR_OUTOFMEMORY;
    }

    if (fHaveServer) {
        ADDRINFO aiHints;
        ADDRINFO *paiLocal = NULL;

        memset(&aiHints, 0, sizeof(aiHints));
        aiHints.ai_family = PF_UNSPEC;
        aiHints.ai_socktype = SOCK_DGRAM;
        aiHints.ai_flags = AI_NUMERICHOST | AI_PASSIVE;

        iSock = 0;

        int iErr = ERROR_SUCCESS;

        if (0 != getaddrinfo(NULL, NTP_PORT_A, &aiHints, &paiLocal)) {
            iErr = GetLastError ();
            DEBUGMSG(ZONE_ERROR,(L"[TIMESVC] Start error: getaddrinfo() fails, GLE=0x%08x\r\n",iErr));
        } else {
            for (ADDRINFO *paiTrav = paiLocal; paiTrav && (iSock < SVSUTIL_ARRLEN(saServer)) ; paiTrav = paiTrav->ai_next) {
                if (INVALID_SOCKET == (saServer[iSock] = socket(paiTrav->ai_family, paiTrav->ai_socktype, paiTrav->ai_protocol)))
                    continue;

                if (SOCKET_ERROR == bind(saServer[iSock], paiTrav->ai_addr, paiTrav->ai_addrlen)) {
                    iErr = GetLastError ();
                    DEBUGMSG(ZONE_ERROR,(L"[TIMESVC] Start error: failed to bind socket, GLE=0x%08x\r\n",iErr));
                    closesocket (saServer[iSock]);
                    continue;
                }

                ++iSock;
            }
        }

        if (paiLocal)
            freeaddrinfo(paiLocal);

        if (iSock == 0) {
            DEBUGMSG(ZONE_ERROR, (L"[TIMESVC] Start error: failure to register time server (error %d)\r\n", iErr));

            delete pEvents;
            ReInit ();

            return iErr;
        }


        if ( ! pEvents->ScheduleEvent (RxThread, NULL)) {
            for (int i = 0 ; i < iSock ; ++i)
                closesocket (saServer[i]);

            SVSThreadPool *pp = pEvents;
            ReInit ();

            DEBUGMSG(ZONE_ERROR, (L"[TIMESVC] Start error: Out of memory scheduling server thread\r\n"));

            Unlock ();
            delete pp;
            Lock ();

            return ERROR_OUTOFMEMORY;
        }
    }

    if (ERROR_SUCCESS != UpdateNowOrLater (Now)) {
        for (int i = 0 ; i < iSock ; ++i)
            closesocket (saServer[i]);

        SVSThreadPool *pp = pEvents;

        DEBUGMSG(ZONE_ERROR, (L"[TIMESVC] Start error: Out of memory scheduling time update\r\n"));

        ReInit ();

        Unlock ();
        delete pp;
        Lock ();

        return ERROR_OUTOFMEMORY;
    }

    fStarted = TRUE;

    DEBUGMSG(ZONE_INIT, (L"[TIMESVC] Service started successfully\r\n"));

    return ERROR_SUCCESS;
}

SVSThreadPool *TimeState::Stop (void) {
    DEBUGMSG(ZONE_INIT, (L"[TIMESVC] Stopping service\r\n"));
    fStarted = FALSE;

    for (int i = 0 ; i < iSock ; ++i) {
        closesocket (saServer[i]);
        saServer[i] = INVALID_SOCKET;
    }

    iSock = 0;

    SVSThreadPool *pp = pEvents;
    pEvents = NULL;
    return pp;
}

///////////////////////////////////////////////////////////////////////
//
//        Globals
//
//
static TimeState *gpTS = NULL;

///////////////////////////////////////////////////////////////////////
//
//        Service functions
//
//    Note: NTP time conversion functions derived from XP's NTP sources (ntpbase.*)
//
static inline unsigned char EndianSwap (unsigned char x) {
    return x;
}

static inline unsigned short EndianSwap (unsigned short x) {
    return (EndianSwap ((unsigned char)x) << 8) | EndianSwap ((unsigned char)(x >> 8));
}

static inline unsigned int EndianSwap (unsigned int x) {
    return (EndianSwap ((unsigned short)x) << 16) | EndianSwap ((unsigned short)(x >> 16));
}

static inline unsigned __int64 EndianSwap (unsigned __int64 x) {
    return (((unsigned __int64)EndianSwap ((unsigned int)x)) << 32) | EndianSwap ((unsigned int)(x >> 32));
}

static unsigned __int64 NtTimeEpochFromNtpTimeEpoch(unsigned __int64 te) {
    //return (qwNtpTime*(10**7)/(2**32))+NTPTIMEOFFSET
    // ==>
    //return (qwNtpTime*( 5**7)/(2**25))+NTPTIMEOFFSET
    // ==>
    //return ((qwNTPtime*FIVETOTHESEVETH)>>25)+NTPTIMEOFFSET;  
    // ==>
    // Note: 'After' division, we round (instead of truncate) the result for better precision
    unsigned __int64 qwNtpTime=EndianSwap(te);
    unsigned __int64 qwTemp=((qwNtpTime&0x00000000FFFFFFFF)*FIVETOTHESEVETH)+0x0000000001000000; //rounding step: if 25th bit is set, round up;

    return (qwTemp>>25) + ((qwNtpTime&0xFFFFFFFF00000000)>>25)*FIVETOTHESEVETH + NTPTIMEOFFSET;
}

static unsigned __int64 NtpTimeEpochFromNtTimeEpoch(unsigned __int64 te) {
    //return (qwNtTime-NTPTIMEOFFSET)*(2**32)/(10**7);
    // ==>
    //return (qwNtTime-NTPTIMEOFFSET)*(2**25)/(5**7);
    // ==>
    //return ((qwNtTime-NTPTIMEOFFSET)<<25)/FIVETOTHESEVETH);
    // ==>
    // Note: The high bit is lost (and assumed to be zero) but 
    //       it will not be set for another 29,000 years (around year 31587). No big loss.
    // Note: 'After' division, we truncate the result because the precision of NTP already excessive
    unsigned __int64 qwTemp=(te-NTPTIMEOFFSET)<<1; 
    unsigned __int64 qwHigh=qwTemp>>8;
    unsigned __int64 qwLow=(qwHigh%FIVETOTHESEVETH)<<32 | (qwTemp&0x00000000000000FF)<<24;

    return EndianSwap(((qwHigh/FIVETOTHESEVETH)<<32) | (qwLow/FIVETOTHESEVETH));
}

static void GetCurrTimeNtp (unsigned __int64 *ptimeXX) {
    SYSTEMTIME st;
    GetSystemTime (&st);

    union {
        FILETIME ft;
        unsigned __int64 ui64ft;
    };

    SystemTimeToFileTime (&st, &ft);

    *ptimeXX = NtpTimeEpochFromNtTimeEpoch (ui64ft);
}

static int Exec (LPTHREAD_START_ROUTINE pfunc, void *pvControlBlock = NULL) {
    HANDLE h = CreateThread (NULL, 0, pfunc, pvControlBlock, 0, NULL);
    if (! h)
        return GetLastError ();

    WaitForSingleObject (h, INFINITE);

    DWORD dw = ERROR_INTERNAL_ERROR;
    GetExitCodeThread (h, &dw);
    CloseHandle (h);

    return (int)dw;
}

#if defined (DEBUG) || defined (_DEBUG)
static void DumpPacket (NTP_REQUEST *pPacket) {
    unsigned int li = pPacket->li ();
    unsigned int vn = pPacket->vn ();
    unsigned int mode = pPacket->mode ();
    unsigned int poll = pPacket->poll ();
    unsigned int stratum = pPacket->stratum ();
    unsigned int precision = pPacket->precision ();
    unsigned int root_delay = pPacket->root_delay ();
    unsigned int root_dispersion = pPacket->root_dispersion ();
    unsigned int ref_id = pPacket->ref_id ();
    unsigned __int64 ref_stamp = pPacket->ref_stamp ();
    unsigned __int64 orig_stamp = pPacket->orig_stamp ();
    unsigned __int64 recv_stamp = pPacket->recv_stamp ();
    unsigned __int64 trans_stamp = pPacket->trans_stamp ();

    DEBUGMSG(ZONE_PACKETS, (L"\r\nSNTP Packet:\r\n"));

    WCHAR *pText = L"";

    switch (li) {
        case 0:
            pText = L"No warning";
            break;
        case 1:
            pText = L"Last minute has 61 seconds";
            break;
        case 2:
            pText = L"Last minute has 59 seconds";
            break;
        case 3:
            pText = L"Alarm condition (clock not synchronized)";
            break;
        default:
            pText = L"Illegal or reserved code";
            break;
    }

    DEBUGMSG(ZONE_PACKETS, (L"Leap      : %d (%s)\r\n", li, pText));
    DEBUGMSG(ZONE_PACKETS, (L"Version   : %d\r\n", vn));

    pText = L"";
    switch (mode) {
        case 1:
            pText = L"symmetric active";
            break;
        case 2:
            pText = L"symmetric passive";
            break;
        case 3:
            pText = L"client";
            break;
        case 4:
            pText = L"server";
            break;
        case 5:
            pText = L"broadcast";
            break;
        case 6:
            pText = L"NTP control";
            break;
        case 7:
            pText = L"private use";
            break;
        default:
            pText = L"illegal or reserved code";
            break;
    }

    DEBUGMSG(ZONE_PACKETS, (L"Mode      : %d (%s)\r\n", mode, pText));
    DEBUGMSG(ZONE_PACKETS, (L"Stratum   : %d\r\n", stratum));
    DEBUGMSG(ZONE_PACKETS, (L"Poll      : %d\r\n", poll));
    DEBUGMSG(ZONE_PACKETS, (L"Precision : %d\r\n", poll));
    DEBUGMSG(ZONE_PACKETS, (L"Root delay: 0x%08x (%d sec)\r\n", root_delay, ((int)root_delay)/4));
    DEBUGMSG(ZONE_PACKETS, (L"Root disp : 0x%08x (%d sec)\r\n", root_dispersion, ((int)root_dispersion)/4));
    in_addr ia;
    ia.S_un.S_addr = ref_id;
    DEBUGMSG(ZONE_PACKETS, (L"Refid     : %08x (or %a)\r\n", ref_id, inet_ntoa (ia)));

    union {
        unsigned __int64 ui64;
        FILETIME ft;
    };

    SYSTEMTIME xst;

    ui64 = NtTimeEpochFromNtpTimeEpoch (ref_stamp);
    FileTimeToSystemTime ((FILETIME *)&ui64, &xst);
    DEBUGMSG(ZONE_PACKETS, (L"Reference time: %02d/%02d/%d %02d:%02d:%02d.%03d\n", xst.wMonth, xst.wDay, xst.wYear, xst.wHour, xst.wMinute, xst.wSecond, xst.wMilliseconds));

    ui64 = NtTimeEpochFromNtpTimeEpoch (orig_stamp);
    FileTimeToSystemTime ((FILETIME *)&ui64, &xst);
    DEBUGMSG(ZONE_PACKETS, (L"Origination time: %02d/%02d/%d %02d:%02d:%02d.%03d\n", xst.wMonth, xst.wDay, xst.wYear, xst.wHour, xst.wMinute, xst.wSecond, xst.wMilliseconds));

    ui64 = NtTimeEpochFromNtpTimeEpoch (recv_stamp);
    FileTimeToSystemTime ((FILETIME *)&ui64, &xst);
    DEBUGMSG(ZONE_PACKETS, (L"Received time: %02d/%02d/%d %02d:%02d:%02d.%03d\n", xst.wMonth, xst.wDay, xst.wYear, xst.wHour, xst.wMinute, xst.wSecond, xst.wMilliseconds));

    ui64 = NtTimeEpochFromNtpTimeEpoch (trans_stamp);
    FileTimeToSystemTime ((FILETIME *)&ui64, &xst);
    DEBUGMSG(ZONE_PACKETS, (L"Transmitted time: %02d/%02d/%d %02d:%02d:%02d.%03d\n", xst.wMonth, xst.wDay, xst.wYear, xst.wHour, xst.wMinute, xst.wSecond, xst.wMilliseconds));
    DEBUGMSG(ZONE_PACKETS, (L"\r\n\r\n"));
}
#endif

///////////////////////////////////////////////////////////////////////
//
//        Threads
//
//
static DWORD WINAPI MulticastThread (LPVOID lpUnused) {
    DEBUGMSG(ZONE_SERVER, (L"[TIMESVC] Multicast Event\r\n"));
    int fSuccess = FALSE;

    if (! gpTS) {
        DEBUGMSG(ZONE_ERROR, (L"[TIMESVC] Multicast: service not initialized!\r\n"));
        return 0;
    }

    gpTS->Lock ();

    if (! gpTS->IsStarted ()) {
        DEBUGMSG(ZONE_ERROR, (L"[TIMESVC] Time Refresh: service not active!\r\n"));
        gpTS->Unlock ();

        return 0;
    }

    int fTimeAccurate = gpTS->IsTimeAccurate();

    unsigned int uiPollInterval = gpTS->dwMulticastPeriodMS / 1000;
    unsigned int poll = 1;

    while (poll < uiPollInterval)
        poll <<= 1;
    poll >>= 1;

    unsigned int ref = (unsigned int)gpTS->llLastSyncTimeXX & 0xffffffff;
    unsigned __int64 ref_ts = gpTS->llLastSyncTimeXX;

    gpTS->Unlock ();

    if (! fTimeAccurate) {
        DEBUGMSG(ZONE_SERVER, (L"[TIMESVC] Multicast server does not have accurate time\r\n"));
        return 0;
    }

    NTP_REQUEST dg;
    memset (&dg, 0, sizeof(dg));
    dg.set_vn (4);
    dg.set_mode (5);
    dg.set_stratum (2);
    dg.set_poll (poll);
    dg.set_precision((unsigned int)-7);
    dg.set_ref_id (ref);
    dg.set_ref_stamp (ref_ts);

    int icast = 0;

    for ( ; ; ) {
        if (! gpTS) {
            DEBUGMSG(ZONE_ERROR, (L"[TIMESVC] Multicast: service not initialized!\r\n"));
            return 0;
        }

        gpTS->Lock ();

        if (! gpTS->IsStarted ()) {
            DEBUGMSG(ZONE_ERROR, (L"[TIMESVC] Multicast: service not active!\r\n"));
            gpTS->Unlock ();

            return 0;
        }

        if (icast >= gpTS->cMcasts)
            break;

        char hostname[DNS_MAX_NAME_BUFFER_LENGTH];
        strcpy (hostname, gpTS->sntp_mcasts[icast++]);

        gpTS->Unlock ();

        DEBUGMSG(ZONE_SERVER, (L"[TIMESVC] Multicast: multicasting to %a\r\n", hostname));

        ADDRINFO aiHints;
        ADDRINFO *paiLocal = NULL;

        memset(&aiHints, 0, sizeof(aiHints));
        aiHints.ai_family = PF_UNSPEC;
        aiHints.ai_socktype = SOCK_DGRAM;
        aiHints.ai_flags = 0; // Or AI_NUMERICHOST; ?

        if (0 != getaddrinfo(hostname, NTP_PORT_A, &aiHints, &paiLocal)) {
            DEBUGMSG(ZONE_ERROR, (L"[TIMESVC] Multicast: host %a is not reachable\r\n", hostname));
            continue;
        }

        for (ADDRINFO *paiTrav = paiLocal; paiTrav ; paiTrav = paiTrav->ai_next) {
            SOCKET s;
            if (INVALID_SOCKET == (s = socket(paiTrav->ai_family, paiTrav->ai_socktype, paiTrav->ai_protocol)))
                continue;

            int on = 1;
            if (0 != setsockopt (s, SOL_SOCKET, SO_BROADCAST, (char *)&on, sizeof(on))) {
                DEBUGMSG(ZONE_ERROR, (L"[TIMESVC] Multicast: could not set broadcast option, error %d\n", GetLastError ()));
            }

            unsigned __int64 llTXX;
            GetCurrTimeNtp (&llTXX);

            dg.set_trans_stamp (llTXX);

            int iRet = sendto (s, (char *)&dg, sizeof(dg), 0, paiTrav->ai_addr, paiTrav->ai_addrlen);

            closesocket (s);

            if (sizeof(dg) == iRet)
                break;

            DEBUGMSG(ZONE_ERROR, (L"[TIMESVC] Multicast failed: error %d; retrying different address family\n", GetLastError ()));
        }

        if (paiLocal)
            freeaddrinfo(paiLocal);
    }

    if (gpTS->dwMulticastPeriodMS > 1000)
        gpTS->ckNextMulticast = gpTS->pEvents->ScheduleEvent (MulticastThread, NULL, gpTS->dwMulticastPeriodMS);

    gpTS->Unlock ();

    DEBUGMSG(ZONE_SERVER, (L"[TIMESVC] Multicast event processing completed\r\n"));
    return 0;
}

static int GetOffsetFromServer (char *hostname, __int64 *pllOffset) {
    ADDRINFO aiHints;
    ADDRINFO *paiLocal = NULL;

    memset(&aiHints, 0, sizeof(aiHints));
    aiHints.ai_family = PF_UNSPEC;
    aiHints.ai_socktype = SOCK_DGRAM;
    aiHints.ai_flags = 0; // Or AI_NUMERICHOST; ?

    if (0 != getaddrinfo(hostname, NTP_PORT_A, &aiHints, &paiLocal)) {
        DEBUGMSG(ZONE_ERROR, (L"[TIMESVC] Time Refresh: host %a is not reachable\r\n", hostname));
        return FALSE;
    }

    for (ADDRINFO *paiTrav = paiLocal; paiTrav ; paiTrav = paiTrav->ai_next) {
        SOCKET s;
        if (INVALID_SOCKET == (s = socket(paiTrav->ai_family, paiTrav->ai_socktype, paiTrav->ai_protocol)))
            continue;

        for (int i = 0 ; i < 3 ; ++i) {
            DEBUGMSG(ZONE_CLIENT, (L"[TIMESVC] Time Refresh: querying server %a\r\n", hostname));

            NTP_REQUEST dg;
            memset (&dg, 0, sizeof(dg));
            dg.set_vn (4);
            dg.set_mode (3);

            unsigned __int64 llT1XX;
            GetCurrTimeNtp (&llT1XX);

            dg.set_trans_stamp (llT1XX);

#if defined (DEBUG) || defined (_DEBUG)
            DEBUGMSG (ZONE_PACKETS, (L"[TIMESVC] Sending SNTP request\r\n"));
            DumpPacket (&dg);
#endif

            if (sizeof (dg) == sendto (s, (char *)&dg, sizeof(dg), 0, paiTrav->ai_addr, paiTrav->ai_addrlen)) {
                DEBUGMSG(ZONE_CLIENT, (L"[TIMESVC] Time Refresh: sent request, awaiting response\r\n"));
                fd_set f;
                FD_ZERO (&f);
                FD_SET (s, &f);

                timeval tv;
                tv.tv_sec  = 3;
                tv.tv_usec = 0;

                if (select (0, &f, NULL, NULL, &tv) > 0) {
                    SOCKADDR_STORAGE sa;
                    int salen = sizeof(sa);
                    if (sizeof(dg) == recvfrom (s, (char *)&dg, sizeof(dg), 0, (sockaddr *)&sa, &salen)) {
                        unsigned __int64 llT4XX;
                        GetCurrTimeNtp (&llT4XX);

#if defined (DEBUG) || defined (_DEBUG)
                        DEBUGMSG (ZONE_PACKETS, (L"[TIMESVC] Received SNTP response\r\n"));
                        DumpPacket (&dg);
#endif

                        int fSuccess = FALSE;

                        if ((dg.li () != 3) && (llT1XX == dg.orig_stamp ())) {
                            unsigned __int64 llT2XX = dg.recv_stamp ();
                            unsigned __int64 llT3XX = dg.trans_stamp ();

                            unsigned __int64 llT1 = NtTimeEpochFromNtpTimeEpoch (llT1XX);
                            unsigned __int64 llT2 = NtTimeEpochFromNtpTimeEpoch (llT2XX);
                            unsigned __int64 llT3 = NtTimeEpochFromNtpTimeEpoch (llT3XX);
                            unsigned __int64 llT4 = NtTimeEpochFromNtpTimeEpoch (llT4XX);

                            *pllOffset = ((__int64)((llT2 - llT1) + (llT3 - llT4))) / 2;

                            fSuccess = TRUE;
                        } else {
                            DEBUGMSG(ZONE_ERROR, (L"[TIMESVC] Time Refresh: sntp server not synchronized\r\n"));
                        }

                        closesocket (s);

                        if (paiLocal)
                            freeaddrinfo(paiLocal);

                        return fSuccess;
                    } else {
                        DEBUGMSG(ZONE_ERROR, (L"[TIMESVC] Time Refresh: sntp server datagram size incorrect (or authentication requested)\r\n"));
                    }
                } else {
                    DEBUGMSG(ZONE_ERROR, (L"[TIMESVC] Time Refresh: sntp server response timeout (no SNTP on server?)\r\n"));
                }
            } else {
                DEBUGMSG(ZONE_ERROR, (L"[TIMESVC] Time Refresh: host %a unreachable\r\n", hostname));
            }

            closesocket (s);
        }
    }

    if (paiLocal)
        freeaddrinfo(paiLocal);

    return FALSE;
}

static int RefreshTimeFromServer (char *hostname, int fForceTime, DWORD dwMaxAdjustS, int *pfTimeChanged) {
    *pfTimeChanged = FALSE;

    __int64 llOffset;

    if (GetOffsetFromServer (hostname, &llOffset)) {
        int iOffsetSec = (int)(llOffset / 10000000);
        if (fForceTime || ((DWORD)abs (iOffsetSec) < dwMaxAdjustS)) {
            SYSTEMTIME st, st2;
            unsigned __int64 llTime;

            GetSystemTime (&st);
            SystemTimeToFileTime (&st, (FILETIME *)&llTime);
            llTime += llOffset;
            FileTimeToSystemTime ((FILETIME *)&llTime, &st2);
            SetSystemTime (&st2);

            DEBUGMSG(ZONE_CLIENT, (L"[TIMESVC] Time Refresh: time accepted. offset = %d s.\r\n", iOffsetSec));
            DEBUGMSG(ZONE_CLIENT, (L"[TIMESVC] Time Refresh: old time: %02d/%02d/%d %02d:%02d:%02d.%03d\r\n", st.wMonth, st.wDay, st.wYear, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds));
            DEBUGMSG(ZONE_CLIENT, (L"[TIMESVC] Time Refresh: new time: %02d/%02d/%d %02d:%02d:%02d.%03d\r\n", st2.wMonth, st2.wDay, st2.wYear, st2.wHour, st2.wMinute, st2.wSecond, st2.wMilliseconds));

            if ((DWORD)abs (iOffsetSec) > MIN_TIMEUPDATE)
                *pfTimeChanged = TRUE;

            return TRUE;
        } else {
            DEBUGMSG(ZONE_ERROR, (L"[TIMESVC] Time Refresh: time indicated by server is not within allowed adjustment boundaries (offset = %d s)\r\n", iOffsetSec));
        }
    }

    return FALSE;
}

static DWORD WINAPI TimeRefreshThread (LPVOID lpUnused) {
    DEBUGMSG(ZONE_CLIENT, (L"[TIMESVC] Refresh Event\r\n"));

    int fSuccess = FALSE;
    int fTimeChanged = FALSE;
    int iSrv = 0;

    while (! fSuccess) {
        if (! gpTS) {
            DEBUGMSG(ZONE_ERROR, (L"[TIMESVC] Time Refresh: service not initialized!\r\n"));
            return 0;
        }

        gpTS->Lock ();

        if (! gpTS->IsStarted ()) {
            DEBUGMSG(ZONE_ERROR, (L"[TIMESVC] Time Refresh: service not active!\r\n"));
            gpTS->Unlock ();

            return 0;
        }

        if (iSrv >= gpTS->cServers) {
            DEBUGMSG(ZONE_WARNING, (L"[TIMESVC] Time Refresh: all servers queried, but time not updated.\r\n"));
            gpTS->Unlock ();

            break;
        }

        int fForceTime = gpTS->fForceTimeToServer;
        DWORD dwMaxAdjustS = gpTS->dwAdjustThreshMS / 1000;

        char hostname[DNS_MAX_NAME_BUFFER_LENGTH];
        memcpy (hostname, gpTS->sntp_servers[iSrv++], sizeof(hostname));

        gpTS->Unlock ();

        fSuccess = RefreshTimeFromServer (hostname, fForceTime, dwMaxAdjustS, &fTimeChanged);
    }

    if (gpTS) {
        gpTS->Lock ();

        if (gpTS->IsStarted ()) {
            if (fSuccess) {
                GetCurrTimeNtp (&gpTS->llLastSyncTimeXX);
                gpTS->fRefreshRequired = FALSE;

                gpTS->UpdateNowOrLater (Later);

                gpTS->fForceTimeToServer = FALSE;

                DEBUGMSG(ZONE_CLIENT, (L"[TIMESVC] Time successfully refreshed\r\n"));

                if (fTimeChanged || (gpTS->ckNextMulticast == 0))
                    gpTS->TimeChanged ();
            } else {
                DEBUGMSG(ZONE_ERROR, (L"[TIMESVC] Time Refresh failed, rescheduling\r\n"));
                gpTS->fRefreshRequired = TRUE;
                gpTS->UpdateNowOrLater (Shortly);

                if(gpTS->ckNextMulticast == 0) 
                    gpTS->TimeChanged();
            }
        }

        gpTS->Unlock ();
    }

    DEBUGMSG(ZONE_CLIENT, (L"[TIMESVC] Time Refresh event processing completed\r\n"));

    return 0;
}

static DWORD WINAPI RxThread (LPVOID lpUnused) {
    DEBUGMSG(ZONE_INIT, (L"[TIMESVC] Server thread started\r\n"));

    for ( ; ; ) {
        if (! gpTS) {
            DEBUGMSG(ZONE_ERROR, (L"[TIMESVC] Server: Not initialized, quitting\r\n"));
            return 0;
        }

        gpTS->Lock ();

        if (! gpTS->IsStarted ()) {
            DEBUGMSG(ZONE_ERROR, (L"[TIMESVC] Server: Not active, quitting\r\n"));
            gpTS->Unlock ();

            return 0;
        }



        fd_set sockSet;
        FD_ZERO(&sockSet);
        for (int i = 0; i < gpTS->iSock; ++i)
            FD_SET(gpTS->saServer[i], &sockSet);

        gpTS->Unlock ();


        int iSockReady = select (0,&sockSet,NULL,NULL,NULL);
        if (iSockReady == 0 || iSockReady == SOCKET_ERROR) {
            DEBUGMSG(ZONE_ERROR, (L"[TIMESVC] Server: select failed, error=%d\r\n", GetLastError ()));
            break;
        }

        for (i = 0; i < iSockReady; ++i) {
            NTP_REQUEST    dg;

            SOCKADDR_STORAGE sa;
            int salen = sizeof(sa);
            int iRecv = recvfrom (sockSet.fd_array[i], (char *)&dg, sizeof(dg), 0, (sockaddr *)&sa, &salen);

            if (iRecv <= 0) {
                DEBUGMSG(ZONE_ERROR, (L"[TIMESVC] Server: receive failed, error=%d\r\n", GetLastError ()));
                break;
            }

            if (iRecv != sizeof(dg)) {
                DEBUGMSG(ZONE_SERVER, (L"[TIMESVC] Server: size mismatch, packet ignored\r\n"));
                continue;
            }

#if defined (DEBUG) || defined (_DEBUG)
            DEBUGMSG(ZONE_PACKETS, (L"[TIMESVC] Received SNTP request\r\n"));
            DumpPacket (&dg);
#endif

            if (dg.mode () == 3) {    //client
                dg.set_mode (4);    //server
            } else if (dg.mode () == 1) {    // symmetric, active
                dg.set_mode(1);                // symmetric, passive
            } else {
                DEBUGMSG(ZONE_SERVER, (L"[TIMESVC] Server: Not a request, aborting\r\n"));
                continue;
            }

            dg.set_root_delay (0);
            dg.set_root_dispersion (0);
            dg.set_precision((unsigned int)-7);

            unsigned __int64 llOrigTimeXX = dg.trans_stamp ();
            dg.set_orig_stamp (llOrigTimeXX);

            unsigned __int64 llRecvTimeXX;
            GetCurrTimeNtp (&llRecvTimeXX);

            dg.set_recv_stamp (llRecvTimeXX);
            dg.set_trans_stamp (llRecvTimeXX);

            if (! gpTS)
                break;

            gpTS->Lock ();
            dg.set_ref_stamp (gpTS->llLastSyncTimeXX);
            dg.set_ref_id ((unsigned int)gpTS->llLastSyncTimeXX);

            if (gpTS->IsTimeAccurate()) {
                dg.set_li (0);
                dg.set_stratum (2);
            } else {
                dg.set_li (3);
                dg.set_stratum (15);
            }
            gpTS->Unlock ();

            sendto (sockSet.fd_array[i], (char *)&dg, sizeof(dg), 0, (sockaddr *)&sa, salen);

#if defined (DEBUG) || defined (_DEBUG)
            DEBUGMSG(ZONE_PACKETS, (L"[TIMESVC] Sent SNTP response\r\n"));
            DumpPacket (&dg);
#endif
        }
    }

    DEBUGMSG(ZONE_INIT, (L"[TIMESVC] Server Thread exited\r\n"));
    return 0;
}

///////////////////////////////////////////////////////////////////////
//
//        Interface, initialization, management
//
//
static DWORD WINAPI SyncTime (LPVOID dwUnused) {
    DEBUGMSG(ZONE_TRACE, (L"[TIMESVC] Time update request from OS\r\n"));

    int iErr = ERROR_SERVICE_DOES_NOT_EXIST;
    if (gpTS) {
        gpTS->Lock ();
        iErr = ERROR_SUCCESS;

        if (! gpTS->IsStarted())
            iErr = ERROR_SERVICE_NOT_ACTIVE;

        if (iErr == ERROR_SUCCESS)
            iErr = gpTS->UpdateNowOrLater (Now);

        gpTS->Unlock ();
    }

    DEBUGMSG(ZONE_TRACE, (L"[TIMESVC] Time update request : %d\r\n", iErr));

    return iErr;
}

static DWORD WINAPI ForceSyncTime (LPVOID dwUnused) {
    DEBUGMSG(ZONE_TRACE, (L"[TIMESVC] Forced time update request from OS\r\n"));

    int iErr = ERROR_SERVICE_DOES_NOT_EXIST;
    if (gpTS) {
        gpTS->Lock ();
        iErr = ERROR_SUCCESS;

        if (! gpTS->IsStarted())
            iErr = ERROR_SERVICE_NOT_ACTIVE;

        if (iErr == ERROR_SUCCESS) {
            iErr = gpTS->ForcedUpdate ();
        }

        gpTS->Unlock ();
    }

    DEBUGMSG(ZONE_TRACE, (L"[TIMESVC] Forced time update request : %d\r\n", iErr));

    return iErr;
}

static DWORD WINAPI GetTimeOffsetOnServer (LPVOID lpArg) {
    DEBUGMSG(ZONE_TRACE, (L"[TIMESVC] Time offset request from OS\r\n"));

    GetTimeOffset *pArg = (GetTimeOffset *)lpArg;

    int fTimeChanged = FALSE;
    int iSrv = 0;

    for ( ; ; ) {
        if (! gpTS) {
            DEBUGMSG(ZONE_ERROR, (L"[TIMESVC] GetServerOffset: service not initialized!\r\n"));
            return ERROR_SERVICE_DOES_NOT_EXIST;
        }

        char hostname[DNS_MAX_NAME_BUFFER_LENGTH];
        if (pArg->cchServer[0] == '\0') {
            int iErr = ERROR_SUCCESS;

            gpTS->Lock ();

            if (gpTS->IsStarted ()) {
                if (iSrv < gpTS->cServers) {
                    memcpy (hostname, gpTS->sntp_servers[iSrv++], sizeof(hostname));
                } else {
                    DEBUGMSG(ZONE_WARNING, (L"[TIMESVC] GetServerOffset: all servers queried, but no connection.\r\n"));
                    iErr = ERROR_HOST_UNREACHABLE;
                }
            } else {
                DEBUGMSG(ZONE_ERROR, (L"[TIMESVC] GetServerOffset: service not active!\r\n"));
                iErr = ERROR_SERVICE_NOT_ACTIVE;
            }

            gpTS->Unlock ();

            if (iErr != ERROR_SUCCESS)
                return iErr;
        } else
            memcpy (hostname, pArg->cchServer, sizeof(hostname));

        if (GetOffsetFromServer (hostname, &pArg->llOffset))
            return ERROR_SUCCESS;

        if (pArg->cchServer[0] != '\0')
            break;
    }

    DEBUGMSG(ZONE_TRACE, (L"[TIMESVC] GetServerOffset : could not connect (ERROR_HOST_UNREACHABLE)\r\n"));

    return ERROR_HOST_UNREACHABLE;
}

static DWORD WINAPI SetTime (LPVOID dwUnused) {
    DEBUGMSG(ZONE_TRACE, (L"[TIMESVC] Force time update request from OS\r\n"));

    int iErr = ERROR_SERVICE_DOES_NOT_EXIST;
    if (gpTS) {
        gpTS->Lock ();
        iErr = ERROR_SUCCESS;

        if (! gpTS->IsStarted())
            iErr = ERROR_SERVICE_NOT_ACTIVE;

        if (iErr == ERROR_SUCCESS)
            iErr = gpTS->UpdateNowOrLater (Now, TRUE);

        gpTS->Unlock ();
    }

    DEBUGMSG(ZONE_TRACE, (L"[TIMESVC] Force time update request : %d\r\n", iErr));

    return iErr;
}

static DWORD WINAPI NetworkChange (LPVOID dwUnused) {
    DEBUGMSG(ZONE_TRACE, (L"[TIMESVC] Network up/down event from OS\r\n"));

    int iErr = ERROR_SERVICE_DOES_NOT_EXIST;
    if (gpTS) {
        gpTS->Lock ();
        iErr = ERROR_SUCCESS;

        if (! gpTS->IsStarted())
            iErr = ERROR_SERVICE_NOT_ACTIVE;

        if ((iErr == ERROR_SUCCESS) && (gpTS->LastUpdateFailed ()))
            iErr = gpTS->UpdateNowOrLater (Now);

        gpTS->Unlock ();
    }

    DEBUGMSG(ZONE_TRACE, (L"[TIMESVC] Time update request : %d\r\n", iErr));

    return iErr;
}

static DWORD WINAPI StartServer (LPVOID lpUnused) {
    DEBUGMSG(ZONE_TRACE, (L"[TIMESVC] Start Server request from OS\r\n"));

    int iErr = ERROR_SERVICE_DOES_NOT_EXIST;
    if (gpTS) {
        gpTS->Lock ();
        iErr = ERROR_SUCCESS;

        if (gpTS->IsStarted())
            iErr = ERROR_ALREADY_INITIALIZED;

        WSADATA wsd;
        WSAStartup (MAKEWORD(1,1), &wsd);

        if (iErr == ERROR_SUCCESS)
            iErr = gpTS->RefreshConfig ();
    
        if (iErr == ERROR_SUCCESS)
            iErr = gpTS->Start ();

        gpTS->Unlock ();
    }

    DEBUGMSG(ZONE_TRACE, (L"[TIMESVC] Start Server request : %d\r\n", iErr));

    return iErr;
}

static DWORD WINAPI StopServer (LPVOID lpUnused) {
    DEBUGMSG(ZONE_TRACE, (L"[TIMESVC] Stop Server request from OS\r\n"));

    int iErr = ERROR_SERVICE_DOES_NOT_EXIST;
    if (gpTS) {
        gpTS->Lock ();
        iErr = ERROR_SUCCESS;

        if (! gpTS->IsStarted())
            iErr = ERROR_SERVICE_NOT_ACTIVE;

        SVSThreadPool *pp = (iErr == ERROR_SUCCESS) ? gpTS->Stop () : NULL;

        gpTS->Unlock ();

        if (pp) {
            delete pp;
            WSACleanup ();
        }
    }

    DEBUGMSG(ZONE_TRACE, (L"[TIMESVC] Stop Server request : %d\r\n", iErr));

    return iErr;
}

static DWORD WINAPI ExternalTimeUpdate (LPVOID lpUnused) {
    DEBUGMSG(ZONE_TRACE, (L"[TIMESVC] External time update indicator from OS\r\n"));

    int iErr = ERROR_SERVICE_DOES_NOT_EXIST;
    if (gpTS) {
        gpTS->Lock ();
        iErr = ERROR_SUCCESS;

        if (! gpTS->IsStarted()) {
            iErr = ERROR_SERVICE_NOT_ACTIVE;
        } else {
            iErr = gpTS->TimeChanged ();
        }

        gpTS->Unlock ();
    }

    DEBUGMSG(ZONE_TRACE, (L"[TIMESVC] External time update indicator : %d\r\n", iErr));

    return iErr;
}

///////////////////////////////////////////////////////////////////////
//
//        OS interface
//
//
static DWORD WINAPI NotifyThread (LPVOID lpUnused) {
    DEBUGMSG(ZONE_INIT, (L"[TIMESVC] NotifyThread started\r\n"));

    HMODULE hCoreDll = LoadLibrary (L"coredll.dll");
    tCeRunAppAtEvent pCeRunAppAtEvent = (tCeRunAppAtEvent)GetProcAddress (hCoreDll, L"CeRunAppAtEvent");
    HANDLE hWakeup  = CreateEvent (NULL, FALSE, FALSE, L"timesvc\\wakeup");
    HANDLE hTimeSet = CreateEvent (NULL, FALSE, FALSE, L"timesvc\\timeset");
    HANDLE hNotifyInitialized = OpenEvent (EVENT_ALL_ACCESS, FALSE, NOTIFICATION_EVENTNAME_API_SET_READY);
    if (! (pCeRunAppAtEvent && hWakeup && hTimeSet && hNotifyInitialized)) {
        DEBUGMSG(ZONE_ERROR, (L"[TIMESVC] NotifyThread: could not initialize, aborting\r\n"));

        FreeLibrary (hCoreDll);
        if (hWakeup)
            CloseHandle (hWakeup);

        if (hTimeSet)
            CloseHandle (hTimeSet);

        if (hNotifyInitialized)
            CloseHandle (hNotifyInitialized);

        return 0;
    }

    // Clean up...
    int fNotificationsPresent = (WAIT_OBJECT_0 == WaitForSingleObject (hNotifyInitialized, NOTIFICATION_WAIT_SEC*1000));
    CloseHandle (hNotifyInitialized);

    if (! fNotificationsPresent) {
        CloseHandle (hWakeup);
        CloseHandle (hTimeSet);
        return 0;
    }

    pCeRunAppAtEvent (NAMED_EVENT_PREFIX_TEXT L"timesvc\\wakeup", 0);
    pCeRunAppAtEvent (NAMED_EVENT_PREFIX_TEXT L"timesvc\\timeset", 0);
    pCeRunAppAtEvent (NAMED_EVENT_PREFIX_TEXT L"timesvc\\wakeup", NOTIFICATION_EVENT_WAKEUP);
    pCeRunAppAtEvent (NAMED_EVENT_PREFIX_TEXT L"timesvc\\timeset", NOTIFICATION_EVENT_TIME_CHANGE);

    HMODULE hiphlp = LoadLibrary (L"iphlpapi.dll");
    tNotifyAddrChange pNotifyAddrChange = hiphlp ? (tNotifyAddrChange)GetProcAddress (hiphlp, L"NotifyAddrChange") : NULL;
    HANDLE hNetwork = NULL;
    if (pNotifyAddrChange)
        pNotifyAddrChange(&hNetwork, NULL);

    for ( ; ; ) {
        HANDLE ah[4];
        ah[0] = hExitEvent;
        ah[1] = hWakeup;
        ah[2] = hTimeSet;
        ah[3] = hNetwork;

        DWORD cEvents = hNetwork ? 4 : 3;

        DWORD dwRes = WaitForMultipleObjects (cEvents, ah, FALSE, INFINITE);

        if (dwRes == (WAIT_OBJECT_0+1))        // Wakeup
            Exec (SyncTime);
        else if (dwRes == (WAIT_OBJECT_0+2)) // Time reset
            Exec (ExternalTimeUpdate);
        else if (dwRes == (WAIT_OBJECT_0+3)) // Network address changed
            Exec (NetworkChange);
        else
            break;
    }

    pCeRunAppAtEvent (NAMED_EVENT_PREFIX_TEXT L"timesvc\\wakeup", 0);
    pCeRunAppAtEvent (NAMED_EVENT_PREFIX_TEXT L"timesvc\\timeset", 0);
    CloseHandle (hWakeup);
    CloseHandle (hTimeSet);

    if (hiphlp)
        FreeLibrary (hiphlp);

    FreeLibrary (hCoreDll);

    DEBUGMSG(ZONE_INIT, (L"Notify Thread exited\r\n"));

    return 0;
}

static int StartOsNotifications (void) {
    if (hExitEvent)
        return FALSE;

    hExitEvent = CreateEvent (NULL, TRUE, FALSE, NULL);
    hNotifyThread = NULL;

    if (hExitEvent) {
        hNotifyThread = CreateThread (NULL, 0, NotifyThread, NULL, 0, NULL);

        if (! hNotifyThread) {
            CloseHandle (hExitEvent);
            hExitEvent = NULL;
        }
    }

    return hNotifyThread != NULL;
}

static int StopOsNotifications (void) {
    if (hNotifyThread) {
        SetEvent (hExitEvent);
        CloseHandle (hExitEvent);
        hExitEvent = NULL;

        WaitForSingleObject (hNotifyThread, INFINITE);
        CloseHandle (hNotifyThread);
        hNotifyThread = NULL;
    }

    return TRUE;
}

//
//    Public interface
//
int InitializeSNTP (HINSTANCE hMod) {
    svsutil_Initialize ();
    DEBUGREGISTER(hMod);

    DEBUGMSG(ZONE_INIT, (L"[TIMESVC] Module loaded\r\n"));

    gpTS = new TimeState;
    return (gpTS != NULL) ? ERROR_SUCCESS : ERROR_OUTOFMEMORY;
}

void DestroySNTP (void) {
    delete gpTS;
    gpTS = NULL;

    DEBUGMSG(ZONE_INIT, (L"[TIMESVC] Module unloaded\r\n"));

    svsutil_DeInitialize ();

}

int StartSNTP (void) {
    int iErr = Exec (StartServer);
    if (iErr == ERROR_SUCCESS)
        StartOsNotifications ();

    return iErr;
}

int StopSNTP (void) {
    int iErr = Exec (StopServer);
    if (iErr == ERROR_SUCCESS)
        StopOsNotifications ();

    return iErr;
}

int RefreshSNTP (void) {
    int iErr = Exec (StopServer);

    if (iErr == ERROR_SUCCESS)
        iErr = Exec (StartServer);

    return iErr;
}

DWORD GetStateSNTP (void) {
    DWORD dwState = SERVICE_STATE_UNINITIALIZED;
    if (gpTS) {
        gpTS->Lock ();
        dwState = gpTS->IsStarted () ? SERVICE_STATE_ON : SERVICE_STATE_OFF;
        gpTS->Unlock ();
    }

    return dwState;
}

int ServiceControlSNTP (PBYTE pBufIn, DWORD dwLenIn, PBYTE pBufOut, DWORD dwLenOut, PDWORD pdwActualOut, int *pfProcessed) {
    int iErr = ERROR_INVALID_PARAMETER;

    *pfProcessed = FALSE;

    if ((dwLenIn == sizeof(L"sync")) && (wcsicmp ((WCHAR *)pBufIn, L"sync") == 0)) {
        *pfProcessed = TRUE;
        return Exec (ForceSyncTime);
    } else if ((dwLenIn == sizeof(L"set")) && (wcsicmp ((WCHAR *)pBufIn, L"set") == 0)) {
        *pfProcessed = TRUE;
        return Exec (SetTime);
    } else if ((dwLenIn == sizeof(L"update")) && (wcsicmp ((WCHAR *)pBufIn, L"update") == 0)) {
        *pfProcessed = TRUE;
        return Exec (ExternalTimeUpdate);
    } else if ((dwLenIn >= sizeof(L"gettimeoffset")) && (wcsnicmp ((WCHAR *)pBufIn, L"gettimeoffset", (sizeof(L"gettimeoffset") - sizeof(L""))/ sizeof(WCHAR)) == 0)) {
        *pfProcessed = TRUE;
        // Check for '\0' sentinel
        if (((WCHAR *)pBufIn)[(dwLenIn-1)/sizeof(WCHAR)] != '\0')
            return ERROR_INVALID_PARAMETER;

        if ((dwLenOut != sizeof (__int64)) || (! pdwActualOut))
            return ERROR_INVALID_PARAMETER;

        GetTimeOffset *pArg = (GetTimeOffset *)LocalAlloc (LMEM_FIXED, sizeof(GetTimeOffset));
        if (! pArg)
            return GetLastError ();

        pArg->llOffset = 0;
        pArg->cchServer[0] = '\0';

        dwLenIn -= (sizeof(L"gettimeoffset") - sizeof(L""));
        pBufIn  += (sizeof(L"gettimeoffset") - sizeof(L""));

        if (dwLenIn > sizeof(WCHAR)) {
            if (*(WCHAR *)pBufIn != ' ') {
                LocalFree (pArg);
                return ERROR_INVALID_PARAMETER;
            }

            dwLenIn += sizeof(WCHAR);
            pBufIn  += sizeof(WCHAR);

            if ((dwLenIn < 2*sizeof(WCHAR)) || (*(WCHAR *)pBufIn == '\0')) {
                LocalFree (pArg);
                return ERROR_INVALID_PARAMETER;
            }

            if (0 == WideCharToMultiByte (CP_ACP, 0, (WCHAR *)pBufIn, -1, (LPSTR)pArg->cchServer, sizeof(pArg->cchServer), NULL, NULL)) {
                LocalFree (pArg);
                return GetLastError ();
            }
        } else {
            SVSUTIL_ASSERT (*(WCHAR *)pBufIn == '\0');
        }

        iErr = Exec (GetTimeOffsetOnServer, pArg);
        if (iErr == ERROR_SUCCESS) {
            memcpy (pBufOut, &pArg->llOffset, sizeof(pArg->llOffset));
            *pdwActualOut = sizeof(pArg->llOffset);
        }

        LocalFree (pArg);
    }

    return iErr;
}
