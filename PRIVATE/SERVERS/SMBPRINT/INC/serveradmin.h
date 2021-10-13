#include "windows.h"
#include <wdm.h>    //for PUNICODE_STRING

#include "SMB_Globals.h"
#include "ShareInfo.h"
#include "FileServer.h"
#include "sspi.h"
#include "security.h"

#define DNS_MAX_NAME_LENGTH             (255)


typedef enum _SERVERNAME_HARDENING_LEVEL {
    SmbServerNameNoCheck,       // 0: Don't enforce SPN check
    SmbServerNamePartialCheck,  // 1: Allow clients who did not provide the target, but fail those who do provide the target and it doesn't match
    SmbServerNameFullCheck  // 2: Only allow clients who supply matching targets
} SERVERNAME_HARDENING_LEVEL, *PSERVERNAME_HARDENING_LEVEL;


NTSTATUS
SrvAdminAllocateNameList(
    PUNICODE_STRING Aliases,
    PUNICODE_STRING DefaultAliases,
    CRITICAL_SECTION *Lock,
    PWSTR           **NameList
);


NTSTATUS
SrvAdminParseSpnName (
    PWSTR SpnName,
    ULONG SpnNameLength,
    PUNICODE_STRING Server,
    PUNICODE_STRING IpAddress,
    BOOLEAN *IsIpAddress
);


NTSTATUS
SrvAdminCheckSpn(
    PWSTR SpnName,
    SERVERNAME_HARDENING_LEVEL SpnNameCheckLevel
);

BOOLEAN
SrvAdminSearchNameFromList(
    PUNICODE_STRING Name,
    PWSTR *NameList
);

//
// Network Address Name Checking
//
BOOLEAN
SrvAdminIsNetworkAddress(
    PUNICODE_STRING NetworkName,
    BOOLEAN CheckIPv4
);

NTSTATUS MapSecurityError(
    SECURITY_STATUS secStatus
);

//
// Equal Unicode String or not
//
BOOLEAN
RtlEqualUnicodeString(
    PUNICODE_STRING string1,
    PUNICODE_STRING string2,
    BOOLEAN isIgnored
);



