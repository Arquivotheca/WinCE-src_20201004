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

#include "serverAdmin.h"


//
// server name check level
//
SERVERNAME_HARDENING_LEVEL  SmbServerNameHardeningLevel = SmbServerNamePartialCheck;


VOID
RtlInitUnicodeString(
    OUT PUNICODE_STRING DestinationString,
    IN PCWSTR SourceString OPTIONAL
    )

/*++

Routine Description:

    The RtlInitUnicodeString function initializes an NT counted
    unicode string.  The DestinationString is initialized to point to
    the SourceString and the Length and MaximumLength fields of
    DestinationString are initialized to the length of the SourceString,
    which is zero if SourceString is not specified.

Arguments:

    DestinationString - Pointer to the counted string to initialize

    SourceString - Optional pointer to a null terminated unicode string that
        the counted string is to point to.


Return Value:

    None.

--*/

{
    ULONG Length;

    DestinationString->Buffer = (PWSTR)SourceString;
    if (ARGUMENT_PRESENT( SourceString ))
    {
        Length = wcslen( SourceString ) * sizeof( WCHAR );
        DestinationString->Length = (USHORT)Length;
        DestinationString->MaximumLength = (USHORT)(Length + sizeof(UNICODE_NULL));
    }
    else
    {
        DestinationString->MaximumLength = 0;
        DestinationString->Length = 0;
    }
}



//
// Equal Unicode String or not
//
BOOLEAN
RtlEqualUnicodeString(
    IN PUNICODE_STRING string1,
    IN PUNICODE_STRING string2,
    IN BOOLEAN IgnoredCaseSensitive
)
/*++

 Routine Description:

    To compare the two unicode string according to the CaseSensitive flag

 Arguments:

        String1 - Pointer to the first string.

    String2 - Pointer to the second string.

    IgnoredCaseSensitive: TRUE when we ignored case sensitive when doing the 
    comparation.

 Return Values:

    TRUE: if both unicode string are equal.
    FALSE: otherwise.
    
 --*/
{
    ULONG len1, len2, Count, minLength;
    WCHAR *uc1, *uc2;
    len1 = string1->Length;
    len2 = string2->Length;
    uc1 = NULL;
    uc2 = NULL;

    if (len1 != len2)
    {
        return FALSE;
    }

    minLength = (len1 <= len2) ? len1 : len2;
    
    if (IgnoredCaseSensitive)
    {
        Count = 0;

        CharUpperW((LPWSTR)string1->Buffer);
        CharUpperW((LPWSTR)string2->Buffer);

        // Can't use wcsnicmp
        while (Count < (minLength/sizeof(WCHAR)))
        {
            uc1= (WCHAR*)(string1->Buffer + Count);
            uc2 = (WCHAR*)(string2->Buffer + Count);
            if (*uc1 != *uc2) {
                return FALSE;
            }
            Count++;
        }
        return TRUE;
    }
    else
    {
        if (0 != wcsncmp(string1->Buffer, string2->Buffer, minLength))
        {
            return FALSE;
        }
        else 
        {
            return TRUE;
        }
    }

}


NTSTATUS
SrvAdminAllocateNameList(
    PUNICODE_STRING Aliases,
    PUNICODE_STRING DefaultAliases,
    CRITICAL_SECTION *Lock,
    PWSTR           **NameList
)
/*++

 Routine Description:

    Get name list from Aliases and DefaultAliases and put into NameList,
    Aliases must be in format as:
    name1\0name2\0name3 or "name1 name2 name3"
    NameList must be in format as:
    pwstr1|pwstr2|pwstr3|NULL|name1\0name2\0name3\0

 Arguments:

    Aliases - names need to be saved in Namelist. Aliases->Length can be updated due to extra L''
    DefaultAliases - names need to saved in NameList, could be NULL. Length can be updated for extra L'' 
    Lock -
    NameList - returned name list

 Return Value:

    STATUS_SUCCESS
    STATUS_INVALID_PARAMETER - IpSrc is not valid IP address
    STATUS_BUFFER_TOO_SMALL - IpDes->Buffer is too small
    STATUS_INTERNAL_ERROR - There is no alias available.

 Notes:

    None.

 --*/
{
    ULONG numberOfAliases = 0;
    ULONG aliasBufferLength;
    PWCHAR aliasBuffer = NULL;
    ULONG index;
    NTSTATUS status;
    PCHAR newBuf = NULL;
    ULONG allocationSize;
    PWCHAR ptr = NULL;
    PWSTR *ptrEntry;

    if (Aliases == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    while ((Aliases->Length) &&
            (Aliases->Buffer[ Aliases->Length/sizeof(WCHAR) - 1] == L'\0' || 
             Aliases->Buffer[Aliases->Length/sizeof(WCHAR) - 1] == L' ' ))
    {
        Aliases->Length -= sizeof(WCHAR);
    }

    aliasBufferLength = Aliases->Length;
    aliasBuffer = Aliases->Buffer;

    //
    // If aliasBufferLength is not 0, at least we have one name. We cannot assume the
    // last alias is ended by either L'\0' or L' ', just adding them if nessasary.
    //
    if (aliasBufferLength > 0)
    {
        numberOfAliases++;
    }

    for (index = 0; index < aliasBufferLength; index += sizeof(WCHAR))
    {
        if ((*aliasBuffer == L'\0') ||
            (*aliasBuffer == L' ') ||
            (*aliasBuffer == L',') )
        {
            numberOfAliases++;
            *aliasBuffer = L'\0';
        }
        aliasBuffer++;
    }

    if (DefaultAliases)
    {
        while ((DefaultAliases->Length) &&
               (DefaultAliases->Buffer[DefaultAliases->Length/sizeof(WCHAR)-1] == L'\0' ||
                DefaultAliases->Buffer[DefaultAliases->Length/sizeof(WCHAR)-1] == L' ' ))
        {
            DefaultAliases->Length -= sizeof(WCHAR);
        }

        aliasBufferLength = DefaultAliases->Length;
        aliasBuffer = DefaultAliases->Buffer;

        //
        // If aliasBufferLength is not 0, at least we have one entry. We cannot assume the last 
        // aliase is ended by either L'\0' or L' '
        //
        if (aliasBufferLength > 0)
        {
            numberOfAliases++;
        }

        for (index = 0; index < aliasBufferLength; index += sizeof(WCHAR))
        {
            if( *aliasBuffer == L'\0' ||
                *aliasBuffer == L' ' || 
                *aliasBuffer == L',' )
            {
                numberOfAliases++;
                *aliasBuffer = L'\0';
            }
            aliasBuffer++;
        }
    }

    if (!numberOfAliases)
    {
        //
        // There is no alias available. SrvLibAllocateNameList currently is used for server
        // name check and ip address check, both of them at least contains one entry 
        // (localhost or 127.0.0.1)
        //
        return STATUS_INTERNAL_ERROR;
    }

    //
    // Append NULL after the buffer
    //
    aliasBufferLength = Aliases->Length + sizeof(WCHAR);
    if (DefaultAliases)
    {
        //
        // Insert NULL between DefaultAliases and Aliase
        //
        aliasBufferLength += DefaultAliases->Length + sizeof(WCHAR);
    }

    allocationSize = aliasBufferLength + (numberOfAliases + 1) * sizeof(PWSTR);

    newBuf = (PCHAR)malloc(allocationSize);
    if (!newBuf)
    {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    RtlZeroMemory( newBuf, allocationSize );

    //
    // Copy the aliases
    // 
    aliasBuffer = (PWCHAR)(newBuf + (numberOfAliases + 1) * sizeof(PWSTR) );

    RtlCopyMemory( aliasBuffer, Aliases->Buffer, Aliases->Length );

    ptr = aliasBuffer + Aliases->Length/sizeof(WCHAR);
    *ptr++ = L'\0';

    if (DefaultAliases)
    {
        RtlCopyMemory( ptr, DefaultAliases->Buffer, DefaultAliases->Length );
        ptr += DefaultAliases->Length/sizeof(WCHAR);
        *ptr = L'\0';
    }

    //
    // Build the array of pointers. If numberOfAliases is 1,
    // it means that the list is empty.
    //
    ptrEntry = (PWSTR *) newBuf;
    if (numberOfAliases > 1)
    {
        *ptrEntry++ = aliasBuffer++;

        //
        // Skip the first character and Last L'\0'
        //
        for (index = 2*sizeof(WCHAR); index < aliasBufferLength; index +=sizeof(WCHAR))
        {
            if (*aliasBuffer++ == L'\0')
            {
                *ptrEntry++ = aliasBuffer;
            }
        }
    }

    *ptrEntry = NULL;

    EnterCriticalSection( Lock );

    // Free it if there already have list.
    if (*NameList != NULL)
    {
       free(*NameList);
    }

    *NameList = (PWSTR *)newBuf;

    LeaveCriticalSection( Lock );

    status = STATUS_SUCCESS;

exit:

    return status;
    
}



static NTSTATUS
IsValidIPv4AddressW(
    PWSTR Name
)
{
/*++

Routine Description:

    Check IPv4 address validation.

Arguments:

    Name : checked IPv4 address

Return Value:

    STATUS_SUCCESS
    STATUS_INVALID_PARAMETER - Name is not in valid format.

Notes:

    Remember that IPv4 address must have 4 parts, separated by periods, and that each part must
    be in the range of 0 to 255. Also note that "000" for a part would be valid, as would "0".

--*/

    UINT UIndex = 0;
    UINT dotCounter = 0;
    UINT UStarter = 0;

    WCHAR Part1[4]; 
    WCHAR Part2[4]; 
    WCHAR Part3[4]; 
    WCHAR Part4[4];

    UINT UPart1;
    UINT UPart2;
    UINT UPart3;
    UINT UPart4;
    
    RtlZeroMemory( Part1, sizeof(Part1) );
    RtlZeroMemory( Part2, sizeof(Part2) );
    RtlZeroMemory( Part3, sizeof(Part3) );
    RtlZeroMemory( Part4, sizeof(Part4) );  

    while (L'\0' != Name[UIndex])
    {
    
        if (L'.' == Name[UIndex])
        {

            switch (dotCounter)
            {
                case 0:
                    wcsncpy( Part1, &Name[UStarter], UIndex );
                    UPart1 = _wtoi(Part1);
                    
                    if ( UPart1 <= 0 || UPart1 > 255 )
                    {
                        return STATUS_INVALID_PARAMETER;
                    }
                    
                    break;
                
                case 1:
                    wcsncpy( Part2, &Name[UStarter+1], (UIndex-UStarter-1) );
                    UPart2 = _wtoi(Part2);

                    if ( UPart2 < 0 || UPart2 > 255 )
                    {
                        return STATUS_INVALID_PARAMETER;
                    }
                    
                    break;
                    
                case 2:
                    wcsncpy( Part3, &Name[UStarter+1], (UIndex-UStarter-1) );   
                    UPart3 = _wtoi(Part3);
                    
                    if ( UPart3 < 0 || UPart3 > 255 )
                    {
                        return STATUS_INVALID_PARAMETER;
                    }
                    
                    break;
                    
                case 3:
                    wcsncpy( Part4, &Name[UStarter+1], (UIndex-UStarter-1) );   
                    UPart4 = _wtoi(Part4);
                    
                    if ( UPart4 < 0 || UPart4 > 255 )
                    {
                        return STATUS_INVALID_PARAMETER;
                    }
                    break;
            }
            
            UStarter = UIndex;
            
            dotCounter++;

            UIndex++;

        } else {
            UIndex++;
        }
    }

    if( 3 != dotCounter )
    {
        return STATUS_INVALID_PARAMETER;
    }
    
    return STATUS_SUCCESS;

}
    


BOOLEAN
SrvAdminIsNetworkAddress(
    PUNICODE_STRING NetworkName,
    BOOLEAN CheckIPv4
)
{
    WCHAR    Name[DNS_MAX_NAME_LENGTH+1];
    NTSTATUS status;


    RtlZeroMemory( Name, sizeof(Name));
    RtlCopyMemory( Name, NetworkName->Buffer, min(NetworkName->Length, DNS_MAX_NAME_LENGTH*sizeof(WCHAR)));

    if (CheckIPv4)
    {
        status = IsValidIPv4AddressW( Name );

        if (NT_SUCCESS(status))
        {
            return TRUE;
        }
    }

    return FALSE;

}

BOOLEAN
SrvAdminSearchNameFromList(
    PUNICODE_STRING Name,
    PWSTR *NameList
)
/* ++

 Routine Description:

    Search Name in NameList

 Arguments:

    Name
    NameList

 Return Value:

    TRUE - Find server name in the list
    FALSE

 Notes:

       None.

-- */
{
    PWSTR NameEntry = NULL;
    UNICODE_STRING tempStr;

    if (!NameList || !Name)
    {
        return FALSE;
    }

    for (NameEntry = *NameList; NameEntry != NULL; NameList++)
    {
        RtlInitUnicodeString(&tempStr, NameEntry);

        if (TRUE == RtlEqualUnicodeString(&tempStr, Name, TRUE))
        {
            return TRUE;
        }
    }

    return FALSE;
}



NTSTATUS
SrvAdminParseSpnName (
    PWSTR SpnName,
    ULONG SpnNameLength,
    PUNICODE_STRING Server,
    PUNICODE_STRING IpAddress,
    BOOLEAN *IsIpAddress
)
/*++

Routine Description:

    Parse SpnName 1)(CIFS/foo.domain2.com/...) or 2)(CIFS/10.1.1.1/...).
    If it is FQDN, put foo.domain2.com to Server and set IsIPAddress FALSE;
    If it is IPv4 address (Not Ipv6 currently supported), put 10.1.1.1 to IpAddress and set IsIpAddress TRUE.

Arguments:

    SpnName     - SpnName got from QCA
    SpnNameLength   - SpnName length in bytes, no including the NULL char.
    Server      - Server name, return "foo.domain2.com" in case 1)
    IpAddress   - IP address, return "10.1.1.1" in case 2)
    IsIpAddress - Indicate whether it is IP address, return FALSE in case 1) and TRUE in case 2)

Return Value:

    STATUS_SUCCESS
    STATUS_INVALID_PARAMETER - SpnName is not in valid format, like there is no '/'

Notes:

    Server or IpAddress buffer is from SpnName, malloc here, free leaving to caller.
    
--*/
{
    // check string from first to first '/', if any character is literal, then case 1)
    // put the content from 1st char to '/' to Server
    // else in case 2), put the content from 1st char to '/' to IpAddress.

    ULONG spnNameSize = SpnNameLength/sizeof(WCHAR);
    UNICODE_STRING serverName = {0};
    ULONG serverNameLength;
    NTSTATUS status = STATUS_SUCCESS;
    UINT UIndex = 0 ;

    PWSTR CIFSServiceName = NULL;

    UNICODE_STRING serviceName = {0};
    UNICODE_STRING spnServiceName = {0};

    BOOLEAN bSlashAppeared = FALSE;

    CIFSServiceName = _wcsdup(L"CIFS");
    if (!CIFSServiceName)
    {
        status = STATUS_INSUFFICIENT_RESOURCES;
        return status;
    }

    serviceName.Buffer = CIFSServiceName;
    serviceName.Length = serviceName.MaximumLength = wcslen(CIFSServiceName) * sizeof(WCHAR);

    if (!spnNameSize)
    {
        status = STATUS_INVALID_PARAMETER;
        goto exit;
    }
    
    *IsIpAddress = FALSE;

    for (UIndex = 0; UIndex < spnNameSize; UIndex++)
    {
        if (SpnName[UIndex] == L'/' ) break;
    }
    spnNameSize -= UIndex;

    spnServiceName.Length = spnServiceName.MaximumLength = UIndex * sizeof(WCHAR);
    spnServiceName.Buffer = SpnName;

    // Check string in the first slash.
    if ( !RtlEqualUnicodeString( &spnServiceName, &serviceName, TRUE) ||
        spnNameSize == 0 )
    {
        status = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    //
    // skip '/'
    //
    UIndex++;
    spnNameSize--;

    serverNameLength = 0;
    serverName.Buffer = &SpnName[UIndex];

    serverName.Length = serverName.MaximumLength = (USHORT)spnNameSize * sizeof(WCHAR);

    if ( TRUE == SrvAdminIsNetworkAddress( &serverName, TRUE ) )
    {
        //
        // It is an IPv4 address, 
        //
        *IsIpAddress = TRUE;
        IpAddress->Buffer = (PWCHAR)malloc(serverName.Length);
        if (!IpAddress->Buffer)
        {
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto exit;
        }

        RtlZeroMemory( IpAddress->Buffer, serverName.Length );  
        
        RtlCopyMemory(IpAddress->Buffer, serverName.Buffer, serverName.Length);
        IpAddress->Length = IpAddress->MaximumLength = serverName.Length;

        Server->Buffer = NULL;
        Server->Length = Server->MaximumLength = 0;
        
    }
    else 
    {
        //
        // server name (may be FQDN)
        //

        *IsIpAddress = FALSE;
        Server->Buffer = (PWCHAR)malloc(serverName.Length);
        if (!Server->Buffer)
        {
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto exit;
        }

        RtlZeroMemory( Server->Buffer, serverName.Length );

        RtlCopyMemory(Server->Buffer, serverName.Buffer, serverName.Length);
        Server->Length = Server->MaximumLength = serverName.Length;

        IpAddress->Buffer = NULL;
        IpAddress->Length = IpAddress->MaximumLength = 0;
    }

exit:

    if (CIFSServiceName)
    {
        free(CIFSServiceName);
        CIFSServiceName = NULL;
    }
    return status;
    
}

NTSTATUS
SrvAdminCheckSpn(
    __in PWSTR SpnName,
    __in SERVERNAME_HARDENING_LEVEL SpnNameCheckLevel
)
/*++

Routine Description:
   Check SPN to make sure SpnName is in SrvAdminServerNameList/SrvAdminAllowedServerNameList/
   SrvAdminIpAddressList, otherwise, there could be SMB relay attack and smb session setup
   should fail.

Arguments:
   SpnName  - SpnName got from QCA
   SpnNameCheckLevel

Return Value:
   STATUS_SUCCESS
   STATUS_INVALID_PARAMETER
   STATUS_ACCESS_DENIED

Notes:

   None.

--*/
{
    BOOLEAN isIpAddress;
    BOOLEAN isAcquiredLock = FALSE;
    UNICODE_STRING serverName;
    UNICODE_STRING ipAddress;
    NTSTATUS status = STATUS_SUCCESS;
    ULONG spnNameLength = 0;

    if (!SpnName)
    {
        return STATUS_INVALID_PARAMETER;
    }

    spnNameLength = wcslen( SpnName );

    // 
    // If no check is needed, we treat it as success
    //
    if (SpnNameCheckLevel == SmbServerNameNoCheck)
    {
        goto exit;
    }

    //
    // If only partial check is requried and client does not provide server name,
    // we treat it as success
    //
    if (SpnNameCheckLevel == SmbServerNamePartialCheck && spnNameLength == 0)
    {
        goto exit;
    }

    if (spnNameLength == 0)
    {
        //
        // Fail for full check
        //
        status = STATUS_ACCESS_DENIED;
        TRACEMSG(ZONE_SECURITY, (L" srvAdminCheckSpn: spnNameLength is 0 and hardening level is full check.\n"));
        goto exit;
    }

    status = SrvAdminParseSpnName( SpnName,
                                    spnNameLength * sizeof(WCHAR),
                                    &serverName,
                                    &ipAddress,
                                    &isIpAddress);
    if (status != STATUS_SUCCESS)
    {
        //
        // Fail for full check
        //
        status = STATUS_ACCESS_DENIED;
        TRACEMSG(ZONE_SECURITY, (L" srvAdminCheckSpn: SrvAdminParseSpnName fail to parse the string %s.\n", SpnName));
        goto exit;  
    }   

    EnterCriticalSection(&SMB_Globals::SrvAdminNameLock);
    isAcquiredLock = TRUE;

    if (isIpAddress)
    {
        PUNICODE_STRING pIp = &ipAddress;

        //
        // Search SrvIpAddressList
        // 
        if (!SrvAdminSearchNameFromList(pIp, SMB_Globals::SrvAdminIpAddressList))
        {
            status = STATUS_ACCESS_DENIED;
            TRACEMSG(ZONE_SECURITY, (L" srvAdminCheckSpn: failed to find Ip address %s in SrvAdminSearchNameFromList.\n", pIp));
            goto exit;  
        }
    }
    else
    {
        //
        // Server name could be NetBIOS name or FQDN, check SrvServerNameList (registered in LSA)
        // and SrvAllowedServerNames (configured in registry)
        //
        if (!SrvAdminSearchNameFromList( &serverName, SMB_Globals::SrvAdminServerNameList) &&
            !SrvAdminSearchNameFromList( &serverName, SMB_Globals::SrvAdminAllowedServerNameList))
        {
            //
            // Fail to find the serverName either in SrvServerNameList or SrvAllowedServerNames
            //
            status = STATUS_ACCESS_DENIED;
            TRACEMSG(ZONE_SECURITY, (L" srvAdminCheckSpn: failed to find %s either in SrvAdminSearchNameFromList.\n", serverName.Buffer));
            goto exit;  
        }

    
    }

exit:

    if (serverName.Buffer)
    {
        free(serverName.Buffer);
    }

    if (ipAddress.Buffer)
    {
        free(ipAddress.Buffer);
    }
    
    if (isAcquiredLock)
    {
        LeaveCriticalSection(&SMB_Globals::SrvAdminNameLock);
    }

    return status;
        
}


NTSTATUS MapSecurityError(
    SECURITY_STATUS secStatus
)
/*++
    
    Routine Description:
       Map Security_Status code to NTSTATUS code
    
    Arguments:
       secStatus : Security status code.
    
    Return Value:
       Please refer NTSTATUS code definition here.
    
    Notes:
    
       None.
    
    --*/
{
    switch(secStatus)
    {
        case SEC_E_NOT_SUPPORTED:
            return STATUS_NOT_SUPPORTED;
            
        case SEC_E_OK:
            return STATUS_SUCCESS;

        case SEC_E_TARGET_UNKNOWN:
            return STATUS_BAD_NETWORK_PATH;
                
        default:
            return STATUS_UNSUCCESSFUL;
    }
}


