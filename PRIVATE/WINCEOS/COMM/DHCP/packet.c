//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*****************************************************************************/
/**                                Microsoft Windows                        **/
/*****************************************************************************/

/*
    packet.c

  DESCRIPTION:
    DHCP packet routines


*/

#include "dhcpp.h"
#include "dhcp.h"
#include "protocol.h"

uint RefDhcpInfo(DhcpInfo *pDhcp);
uint DerefDhcpInfo(DhcpInfo *pDhcp);

#ifdef DEBUG
LPWSTR
PacketType(
    uchar Type
    )
{
    switch (Type) {
    case DHCPDISCOVER:  return L"DISCOVER";
    case DHCPOFFER:     return L"OFFER";
    case DHCPREQUEST:   return L"REQUEST";
    case DHCPINFORM:    return L"INFORM";
    case DHCPDECLINE:   return L"DECLINE";
    case DHCPRELEASE:   return L"RELEASE";
	case DHCPACK:       return L"ACK";
	case DHCPNACK:      return L"NAK";

    default:        return L"UNKNOWN";
    }
}
#endif

extern DhcpInfo	*v_pDelete;

BOOL GetName(char *pName) {
	int		Ret, Status = 0;
	TCHAR	Name[17], OrigName[17];
	HKEY	hKey;
	DWORD	dwLen;

	Ret = FALSE;
	Status = RegOpenKeyEx (HKEY_LOCAL_MACHINE, TEXT("Ident"), 0, 0, &hKey);
	
	if (Status == ERROR_SUCCESS) {
		memset(OrigName, 0, sizeof(OrigName));
		dwLen = sizeof(OrigName);
		Status = RegQueryValueEx(hKey, TEXT("OrigName"), NULL, NULL, 
			(LPBYTE)OrigName, &dwLen);
		
		memset(Name, 0, sizeof(Name));
		dwLen = sizeof(Name);
		if ((Status = RegQueryValueEx(hKey, TEXT("Name"), NULL, NULL,
			(LPBYTE)Name, &dwLen)) == ERROR_SUCCESS) {

			if (_tcsicmp(Name, OrigName)) {

				WideCharToMultiByte(CP_ACP, 0, Name, 16, pName, 
					16, NULL, NULL);
/*
				dwLen >>= 1;
				dwLen--;
				for (i = 0; i < dwLen; i++) {
					if (pName[i] >= 'a' && pName[i] <= 'z') {
						pName[i] = pName[i] - 'a' + 'A';
					}
				}
				
				while (dwLen < 15) {
					pName[dwLen++] = 0x20;
				}
*/				
				Ret = TRUE;
			}
		}
		RegCloseKey (hKey);
	}
	
	return Ret;

}	// GetName()


//
// Return pointer to specified option in Dhcp->pSendOptions if it is present
//
uchar *
FindSendOption(
    DhcpInfo *pDhcp,
    uchar Option
    )
{
    uchar   *ptmp;

    if (pDhcp->pSendOptions) {
        ptmp = pDhcp->pSendOptions;
        while (ptmp < (pDhcp->pSendOptions + pDhcp->cSendOptions)) {
            if (Option == *ptmp) {
                //
                // Option was specified in the send options.
                //
                return ptmp;
            }
            ptmp++;         // advance to length byte
            ptmp += *ptmp;  // advance by option length
            ptmp++;         // advance to next option
        }
    }
    return NULL;
}   // FindSendOption

void BuildDhcpPkt(
	DhcpInfo	*pDhcp,
	DhcpPkt		*pPkt,
	uchar		Type,
	int			Flags, 
    uchar		*aOptionList,
	int			*pcPkt)
{
    int     i, Cookie = MAGIC_COOKIE;
    uchar   *p, *pEnd;
    char    aName[17];
    uchar   *ptmp;

    DEBUGMSG(ZONE_FUNCTION, (TEXT("+BuildDhcpPkt: %s pDhcp 0x%X/0x%X\r\n"),
            PacketType(Type), pDhcp, pDhcp->Socket));

	DEBUGMSG(ZONE_SEND, (L"DHCP: TX %s for %s\n", PacketType(Type), pDhcp->Name));

    pEnd = pPkt->aOptions + OPTIONS_LEN;    // byte right after end of pkt
    if (Flags & NEW_PKT_FL) {
        memset(pPkt, 0, sizeof(DhcpPkt));
        pPkt->Op = 1;
        pPkt->Htype = pDhcp->ClientHWType;
        pPkt->Hlen = pDhcp->ClientHWAddrLen;
        pPkt->Hops = 0;
        // now we getXid here - OM
        pPkt->Xid = GetXid(pDhcp->ClientHWAddr, pDhcp->ClientHWAddrLen);
        pPkt->Xid = net_long(pPkt->Xid);
        // Secs, Flags, Yiaddr, Siaddr, Giaddr should all be set to 0
        pPkt->Secs = pPkt->Flags = pPkt->Ciaddr = pPkt->Yiaddr = pPkt->Siaddr = pPkt->Giaddr = 0;
		//
		// For soft client IDs, have the server send a broadcast IP packet back,
		// since directed IP packets to an invalid IP address just get tossed.
		// For a renewal request, we don't need to use a broadcast because
		// proxy arp entries should have been created.
		//
		if ((pDhcp->ClientIDType == 0) && !(Flags & RENEW_PKT_FL))
			pPkt->Flags = net_short(DHCP_BCAST_FL);
        memcpy(pPkt->aChaddr, pDhcp->ClientHWAddr, min(CHADDR_LEN, pDhcp->ClientHWAddrLen));
        memcpy(pPkt->aOptions, &Cookie, 4);
    }
    if ((Flags & RIP_PKT_FL) == 0)
        pPkt->Ciaddr = pDhcp->IPAddr;

    p = pPkt->aOptions + 4;
    // Set the Message Type
    *p++ = DHCP_MSG_TYPE_OP;
    *p++ = 1;                // option length
    *p++ = Type;

    switch(Type) {
    case DHCPREQUEST:
        // Server Identifier
        if (Flags & SID_PKT_FL) {
            *p++ = DHCP_SERVER_ID_OP;
            *p++ = 4;
            memcpy(p, &pDhcp->DhcpServer, 4);
            p += 4;
        }

        // no break, fall-thru

    case DHCPDISCOVER:

        // Requested Addr
        if (pDhcp->IPAddr && (Flags & RIP_PKT_FL)) {
            *p++ = DHCP_REQ_IP_OP;
            *p++ = 4;
            memcpy(p, &pDhcp->IPAddr, 4);
            p += 4;
        }

        // IP address lease should also be done before DHCPINFORM

        // no break, fall-thru

    case DHCPINFORM:

        if (NULL == FindSendOption(pDhcp, DHCP_CLIENT_ID_OP)) {
            //
            // Set the Client Id using adapter address
            //
            *p++ = DHCP_CLIENT_ID_OP;
            *p++ = 1 + pDhcp->ClientIDLen;	// op len = 1 byte for type + N bytes for HWAddr
            *p++ = pDhcp->ClientIDType;     
            memcpy(p, &pDhcp->ClientID, pDhcp->ClientIDLen);
            p += pDhcp->ClientIDLen;
        }

        if (NULL == FindSendOption(pDhcp, DHCP_HOST_NAME_OP)) {
            //
            // Set the host name option using the local computer name
            //
            if (GetName(aName)) {
                aName[16] = '\0';
                // Host Name Option
                *p++ = DHCP_HOST_NAME_OP;
                for (i = 0; aName[i] && i < 17; i++) {
                    *(p + i + 1) = aName[i];
                }
                *p++ = (char)i;
                p += i;
            }
        }

        // Parameter Request List
        if (aOptionList && aOptionList[0] &&
            aOptionList[0] <= (pEnd - p)) {
            *p++ = DHCP_PARAMETER_REQ_OP;
            memcpy (p, aOptionList, aOptionList[0] + 1);    // +1 for len
            p += aOptionList[0] + 1;    // +1 for length
        }

        if (pDhcp->pSendOptions) {
            memcpy(p, pDhcp->pSendOptions, pDhcp->cSendOptions);
            p += pDhcp->cSendOptions;
        }

        // End Option
        *p++ = DHCP_END_OP;
        
        break;

    case DHCPDECLINE:
        // Requested Addr
        *p++ = DHCP_REQ_IP_OP;
        *p++ = 4;
        memcpy(p, &pDhcp->IPAddr, 4);
        p += 4;

        // no break, fall-thru

    case DHCPRELEASE:

        if (ptmp = FindSendOption(pDhcp, DHCP_CLIENT_ID_OP)) {
            memcpy(p, ptmp, ptmp[1]+2);
            p += ptmp[1]+2;
        } else {
            // Set the Client Id
            *p++ = DHCP_CLIENT_ID_OP;
            *p++ = 1 + pDhcp->ClientIDLen;	// op len = 1 byte for type + N bytes for ClientID
            *p++ = pDhcp->ClientIDType;     
            memcpy(p, &pDhcp->ClientID, pDhcp->ClientIDLen);
            p += pDhcp->ClientIDLen;
        }

        // Server Identifier
        *p++ = DHCP_SERVER_ID_OP;
        *p++ = 4;
        memcpy(p, &pDhcp->DhcpServer, 4);
        p += 4;

        // we should also do Message Option

        // End Option
        *p++ = DHCP_END_OP;

        break;

    default:
        DEBUGMSG(ZONE_ERROR, 
            (TEXT("\t!BuildDhcpPkt: unknown packet type (%d) requested\r\n"),
            Type));
        break;
    }    // switch(Type)

    if (p < pEnd)
        memset(p, DHCP_PAD_OP, pEnd - p);
    
    if (pcPkt)
        *pcPkt = p - (char *)pPkt;

    DEBUGMSG(ZONE_FUNCTION, (TEXT("-BuildDhcpPkt: pDhcp 0x%X/0x%X Len %d\r\n"),
            pDhcp, pDhcp->Socket, (p - (char *)pPkt)));

}    // BuildDhcpPkt()


void Tvert(uchar c, TCHAR *p) {
    if (c >= 100)
        p++;
    if (c >= 10)
        p++;
    *(p+1) = TEXT('\0');

    do {
        *p-- = TEXT('0') + (c % 10);
    } while (c /= 10);

}


#define MIN_LEASE_TIME 1

STATUS
TranslatePkt(
	DhcpInfo	*pDhcp,
	DhcpPkt		*pPkt,
	int			 cPkt,
	uchar		*pType,
    unsigned int Xid)
//
//	Process a received packet
//
{
    STATUS    Status = DHCP_FAILURE;  // assume failure
    int        i, fSaveReg, Cookie = MAGIC_COOKIE;
    uchar    *p, *pEnd, Type = 0;
    TCHAR    szBuf[MAX_REG_STR];
    HKEY    hKey;
    LONG    hRes;
    DWORD    cData;
    int		Overload, ReadOverload, ReadNext, fEnd;

    uint    NewSubnetMask,  fSubnetMask;
    uint    NewGateway,     fGateway;
    uint    NewLease,       fLease;
    uint    NewServer,      fServer;
    uint    NewT1,          fT1;
    uint    NewT2,          fT2;
    uint    NewDNS[2],      fDNS;
    uint    NewWins[2],     fWins;
    uchar * pnew;

    DEBUGMSG(ZONE_RECV, 
        (TEXT("DHCP:+TranslatePkt: pDhcp %X: OP %d, Htype %d, Xid %x/%x\r\n"), 
        pDhcp, pPkt->Op, pPkt->Htype, pPkt->Xid, Xid));

    fSubnetMask = fGateway = fLease = fServer = fT1 = fT2 = fDNS = fWins = fSaveReg = FALSE;

    if (cPkt >= (sizeof(*pPkt) - OPTIONS_LEN + 4) &&
    	2 == pPkt->Op &&
        pDhcp->ClientHWType == pPkt->Htype &&
        Xid == pPkt->Xid &&
        0 == memcmp(&Cookie, pPkt->aOptions, 4) &&
        ((pDhcp->Flags & DHCPCFG_NO_MAC_COMPARE_FL) || (pPkt->Hlen == pDhcp->ClientHWAddrLen && 
            (0 == memcmp(pDhcp->ClientHWAddr, pPkt->aChaddr, pPkt->Hlen))))) {


        Overload = ReadOverload = ReadNext = 0;
        //
        // Scan through the options once without storing config options so we can
        // perform some sanity checks first.
        // (The number of options checked will increase as we get more input from QA and customers)
        //
		do {

            fEnd = 0;
            if (0 == ReadNext) {
            	// read the regular options
                p = &pPkt->aOptions[4];
                pEnd = (uchar *)pPkt + cPkt;
            } else if (1 == ReadNext) {
                // read the file field
                p = pPkt->aFile;
                pEnd = pPkt->aOptions;
        	} else if (2 == ReadNext) {
        	    // read the sname field
        	    p = pPkt->aSname;
        	    pEnd = pPkt->aFile;
    		} else {
    		    ASSERT(0);
    		    goto BreakLoop;
			}
    		ReadOverload += ReadNext;
			
            while (p < pEnd) {
                i = 4;
                pnew = NULL;
                switch (*p) {
                //
                // These are the 4 byte (i=4) cases
                //
                case DHCP_SUBNET_OP:
                    pnew = (uchar *) &NewSubnetMask;
                    fSubnetMask++;
                    break;
                case DHCP_IP_LEASE_OP:
                    pnew = (uchar *) &NewLease;
                    fLease++;
                    break;
                case DHCP_SERVER_ID_OP:
                    pnew = (uchar *) &NewServer;
                    fServer++;
                    break;
                case DHCP_ROUTER_OP:
                    pnew = (uchar *) &NewGateway;
                    fGateway++;
                    break;
                case DHCP_T1_VALUE_OP:
                    pnew = (uchar *) &NewT1;
                    fT1++;
                    break;
                case DHCP_T2_VALUE_OP:
                    pnew = (uchar *) &NewT2;
                    fT2++;
                    break;

                //
                // Up to 8 byte options
                //
                case DHCP_DNS_OP: 
                    NewDNS[0] = 0;
                    NewDNS[1] = 0;
                    if ((i = p[1]) > 8) {
                        i = 8;
                    }
                    pnew = (uchar *)NewDNS;
                    fDNS++;
                    break;

                case DHCP_NBT_SRVR_OP:
                    NewWins[0] = 0;
                    NewWins[1] = 0;
                    if ((i = p[1]) > 8) {
                        i = 8;
                    }
                    pnew = (uchar *)NewWins;
                    fWins++;
                    break;

                //
                // Special cases
                //
                case DHCP_MSG_TYPE_OP:
                    Type = p[2];
                    if (*pType && Type != *pType) {
                        Status = DHCP_SUCCESS;
                        DEBUGMSG(ZONE_RECV, (TEXT("DHCP:TranslatePkt: Type = %d, expecting %d\n"), Type, *pType));
                        goto BreakLoop;
                    }
                    break;

                case DHCP_MSFT_AUTOCONF:
                    if (p[1] != 1) {
                        DEBUGMSG(ZONE_RECV, (TEXT("DHCP:TranslatePkt: AutoConfig Option Len = %d, expecting 1\n"), p[1]));
                        goto BreakLoop;
                    }
                    break;

                case DHCP_OVERLOAD_OP:
                	if ((p + 2) >= pEnd) {
                        DEBUGMSG(ZONE_RECV | ZONE_WARN, 
                        	(TEXT("DHCP:TranslatePkt: Overload Option packet overflow\n")));
                        goto BreakLoop;
            		} 
    				if (p[1] != 1) {
                        DEBUGMSG(ZONE_RECV | ZONE_WARN, 
                        	(TEXT("DHCP:TranslatePkt: Overload Option Len = %d, expecting 1\n"), p[1]));
                        goto BreakLoop;
    				}
    				// don't allow multiple overload options!
    				if (! Overload) {
        				if (p[2] <= 3) {
        				    Overload = p[2];
    				    } else {
                            DEBUGMSG(ZONE_RECV | ZONE_WARN, 
                            	(TEXT("DHCP:TranslatePkt: Overload Option too large %d\n"), Overload));
                        }
    				}
                	break;
    	
                case DHCP_PAD_OP:
                    p++;
                    continue;
                    break;

                case DHCP_END_OP:
                    fEnd = TRUE;
                    goto VerifyNextField;
                    break;
                }

                if (pnew) {
                    // verify length
                    if (p + i > pEnd) {
                        DEBUGMSG(ZONE_RECV, (TEXT("DHCP:TranslatePkt: Length %d too long for option %d\n"), i, *p));
                        goto BreakLoop;
                    }

                    memcpy(pnew, p+2, i);
                }
                p += p[1] + 2;
            }

VerifyNextField:
            // rfc 2131, section 4.1 requires an end option after each field
            if (! fEnd) {
                Status = DHCP_FAILURE;
                DEBUGMSG(ZONE_RECV, 
                    (TEXT("DHCP:TranslatePkt: no end option! ReadNext = %d\n"), ReadNext));
                goto BreakLoop;                
            }
    		if (Overload & 0x1) {
    			ReadNext++;
    		} else if (Overload) {
    			ReadNext += 2;
    		}

		}while (ReadOverload < Overload);

        //
        // Make sure config options seem reasonable
        //
        if (fSubnetMask) {
            if (NewSubnetMask == 0) {
                DEBUGMSG(ZONE_RECV, (TEXT("DHCP:TranslatePkt: Invalid subnet mask %d\n"), NewSubnetMask));
                goto BreakLoop;
            }
        }

		DEBUGMSG(ZONE_RECV, (L"DHCP: RX %s for %s 0x%X\n", PacketType(Type), pDhcp->Name));

        if (fLease) {
            if (NewLease < MIN_LEASE_TIME && Type == DHCPACK) {
                DEBUGMSG(ZONE_RECV, (TEXT("DHCP:TranslatePkt: Invalid lease time %d < %d\n"), NewLease, MIN_LEASE_TIME));
                goto BreakLoop;
            }
        } else {
            if (Type == DHCPACK) {
                DEBUGMSG(ZONE_RECV, (TEXT("DHCP:TranslatePkt: No lease specified in DHCPACK!\n")));
                goto BreakLoop;
            }
        }

        if (fServer) {
            if (NewServer == 0) {
                DEBUGMSG(ZONE_RECV, (TEXT("DHCP:TranslatePkt: Invalid server id %d\n"), NewServer));
                goto BreakLoop;
            }
        }

        //
        // Make sure the ipaddr and subnet mask match if this is a renewal
        //
        if (*pType == DHCPACK && pDhcp->IPAddr) {
            if (pDhcp->IPAddr != pPkt->Yiaddr) {
                DEBUGMSG(ZONE_RECV,(TEXT("DHCP:TranslatePkt: Mismatch in IP addr - rcvd 0x%X, expect 0x%X\r\n"),
                                    pPkt->Yiaddr,pDhcp->IPAddr));
                goto BreakLoop;
            }
            if (pDhcp->SubnetMask && (pDhcp->SubnetMask != NewSubnetMask)) {
                DEBUGMSG(ZONE_RECV,(TEXT("DHCP:TranslatePkt: Mismatch in subnet - rcvd 0x%X, expect 0x%X\r\n"),
                                    NewSubnetMask,pDhcp->SubnetMask));
                goto BreakLoop;
            }
        }

        //
        // Open registry key to save new values
        //
        if (DHCPACK == Type) {
            _tcscpy (szBuf, COMM_REG_KEY);
            _tcscat (szBuf, pDhcp->Name);
            _tcscat (szBuf, TEXT("\\Parms\\TcpIp\\DhcpOptions"));
            hRes = RegOpenKeyEx (HKEY_LOCAL_MACHINE, szBuf, 0, 0, 
                &hKey);
            if (ERROR_SUCCESS == hRes) {
                DEBUGMSG(ZONE_RECV, 
                    (TEXT("DHCP:TranslatePacket: Save in registry\r\n")));
                fSaveReg = TRUE;
            }
        }


        Overload = ReadOverload = ReadNext = 0;
        //
        // Now run through the options again and store away results
        //
		do {
            if (0 == ReadNext) {
            	// read the regular options
                p = &pPkt->aOptions[4];
                pEnd = (uchar *)pPkt + cPkt;
            } else if (1 == ReadNext) {
                // read the file field
                p = pPkt->aFile;
                pEnd = pPkt->aOptions;
        	} else if (2 == ReadNext) {
        	    // read the sname field
        	    p = pPkt->aSname;
        	    pEnd = pPkt->aFile;
    		} else {
    		    ASSERT(0);
    		    goto BreakLoop;
			}
    		ReadOverload += ReadNext;

            while (p < pEnd) {
                switch(*p) {
                case DHCP_PAD_OP:
                    p++;
                    continue;
                    break;
                case DHCP_END_OP:
                    Status = DHCP_SUCCESS;
                    goto NextField;
                    break;
                case DHCP_SUBNET_OP:
                    pDhcp->SubnetMask = NewSubnetMask;
                    break;
                case DHCP_ROUTER_OP:
                    pDhcp->Gateway = NewGateway;
                    break;
                case DHCP_DNS_OP:
                    pDhcp->DNS[0] = NewDNS[0];
                    pDhcp->DNS[1] = NewDNS[1];
                    break;
                case DHCP_DOMAIN_NAME_OP:
                    if (p[1] > MAX_DOMAIN_NAME) {
                        pDhcp->aDomainName[0] = MAX_DOMAIN_NAME;
                        memcpy(pDhcp->aDomainName+1, p+2, MAX_DOMAIN_NAME);
                    } else {
                        memcpy(pDhcp->aDomainName, p+1, p[1] + 1);
                    }
                    break;
                case DHCP_NBT_SRVR_OP:
                    pDhcp->WinsServer[0] = NewWins[0];
                    pDhcp->WinsServer[1] = NewWins[1];
                    break;
                case DHCP_IP_LEASE_OP:
                    pDhcp->Lease = net_long(NewLease);
                    break;
                case DHCP_SERVER_ID_OP:
                    pDhcp->DhcpServer = NewServer;
                    break;
                case DHCP_T1_VALUE_OP:
                    pDhcp->T1 = net_long(NewT1);
                    break;
                case DHCP_T2_VALUE_OP:
                    pDhcp->T2 = net_long(NewT2);
                    break;

                case DHCP_MSFT_AUTOCONF:
                    if (p[2]) {
                        pDhcp->Flags |= DHCPCFG_AUTO_IP_ENABLED_FL;
                    } else {
                        pDhcp->Flags &= ~DHCPCFG_AUTO_IP_ENABLED_FL;
                    }
                    break;

                case DHCP_OVERLOAD_OP:
                    // don't allow multiple overload options!
                    if (! Overload) {
        				if (p[2] <= 3) {
                            Overload = p[2];
                        }
                    }
                    
                    break;

                case DHCP_MSG_TYPE_OP:
                case DHCP_MESSAGE_OP:       // NYI
                case DHCP_NBT_NODE_TYPE_OP: // NYI
                case DHCP_HOST_NAME_OP:     // NYI
                    break;

                default:
                    DEBUGMSG(ZONE_RECV, 
                        (TEXT("DHCP:TranslatePkt: Unexpected option %d\r\n"), *p));
                    break;
                }    // switch(*p)

                if (fSaveReg) {
                    Tvert(*p, szBuf);
                    cData = 0;
                    hRes = RegQueryValueEx(hKey, szBuf, NULL, NULL, NULL, &cData);
                    if (ERROR_SUCCESS == hRes && cData)
                        SetRegBinaryValue(hKey, szBuf, p, p[1] + 2);
                }

                p += p[1] + 2;

            }    // while (...)

NextField:
    		if (Overload & 0x1) {
    			ReadNext++;
    		} else if (Overload) {
    			ReadNext += 2;
    		}

	    }while (ReadOverload < Overload);

        // DEBUGMSG(ZONE_RECV, (TEXT("DHCP:TranslatePkt: No DHCP_END_OP in packet!!\n")));

BreakLoop:        
        if (Status == DHCP_SUCCESS && (Type == DHCPOFFER || Type == DHCPACK)) {
            if (fLease) {
                if (! fT1)
                    pDhcp->T1 = pDhcp->Lease >> 1;      // default half lease time
                if (! fT2)
                    pDhcp->T2 = (pDhcp->Lease * 7) >> 3;    // dft: 7/8 lease time
                memcpy(&pDhcp->IPAddr, &pPkt->Yiaddr, 4);
            }
        }

    } else {
#if DEBUG

		if (cPkt < (sizeof(*pPkt) - OPTIONS_LEN + 4))
			DEBUGMSG(ZONE_RECV, 
				(TEXT("DHCP:Translate: packet too small %d\r\n"),
				cPkt));
		// we can continue--since pPkt is a ptr to our own packet which we know is
		// larger than the minimum dhcp packet

        if (2 != pPkt->Op) 
            DEBUGMSG(ZONE_RECV, (TEXT("DHCP:TranslatePkt: Op %d != 2\n"), 
	            pPkt->Op));
        if (1 != pPkt->Htype)
            DEBUGMSG(ZONE_RECV, 
	            (TEXT("DHCP:TranslatePkt: Htype %d != 1\n"), pPkt->Htype));
        if (Xid != pPkt->Xid)
            DEBUGMSG(ZONE_RECV, 
	            (TEXT("DHCP:TranslatePkt: Xid 0x%x != 0x%x\n"), pPkt->Xid, Xid));
        if (0 != memcmp(&Cookie, pPkt->aOptions, 4))
            DEBUGMSG(ZONE_RECV, 
	            (TEXT("DHCP:TranslatePkt: Cookie %B%B%B%B != %X\n"),
	            pPkt->aOptions[3], pPkt->aOptions[2], pPkt->aOptions[1], 
	            pPkt->aOptions[0], Cookie));

#endif                                 
    }

    if (fSaveReg)
        RegCloseKey(hKey);

    if (pType)
        *pType = Type;
    
    DEBUGMSG(ZONE_RECV, (TEXT("DHCP:-TranslatePkt: pDhcp %X Type %d, Ret: %d\r\n"),
        pDhcp, Type, Status));
    return Status;

}    // TranslatePkt()


// caller must either have a ref on the DhcpInfo or hold the Dhcp->Lock
STATUS DhcpSendDown(DhcpInfo *pDhcp, DhcpPkt *pPkt, int cPkt, int DestIP) {
    SOCKADDR_IN	    SockAddr;
    STATUS          Status;
    int             cSent;
    WSABUF          WsaBuf;
    LPSOCK_INFO		Socket;

	if (cPkt < 300)
		cPkt = 300;

	memset(&SockAddr, 0, sizeof(SockAddr));
	SockAddr.sin_family = AF_INET;
	SockAddr.sin_port = net_short(DHCP_SERVER_PORT);
	SockAddr.sin_addr.S_un.S_addr = DestIP;
    
	WsaBuf.len = cPkt;
	WsaBuf.buf = (char *)pPkt;

	// make sure all paths below free this lock.
    CTEGetLock(&pDhcp->Lock, 0);
    // rely on CS behavior to be able to enter CS while holding CS lock

    if ((DHCPSTATE_MEDIA_DISC_PENDING|DHCPSTATE_DELETE) & pDhcp->SFlags) {
		CTEFreeLock(&pDhcp->Lock, 0);
		Status = DHCP_DELETED;
	} else {
		if (Socket = pDhcp->Socket) {
			Status = CallSock(Send)(Socket, &WsaBuf, 1, &cSent, 0,
				(SOCKADDR *)&SockAddr, sizeof(SockAddr), NULL, NULL, NULL, 
				&pDhcp->Lock);
			// AfdSend will release the dhcp lock above
		} else {
			CTEFreeLock(&pDhcp->Lock, 0);
			Status = DHCP_NOINTERFACE;
		}
	}

    DEBUGMSG(ZONE_SEND, 
        (TEXT("*DhcpSendDown: Dest %d/%x, Sent %d/%d Status %d\r\n"),
        DHCP_SERVER_PORT, pDhcp->IPAddr, cSent, 
        cPkt, Status));

    return Status;
}    // DhcpSendDown()


#define DHCP_TIMEOUT_CONSTANT_RATE   1000
#define DHCP_TIMEOUT_AUTOCFG         1000
#define DHCP_TIMEOUT_NORMAL          4000

int
AddFuzz(
    int cUseFuzz,
    int cTimeout
    )
{
    int cFuzz;
    int cRange;
    int cRet;

    cRet = cTimeout;

    if (cUseFuzz) {
        //
        // Scale the fuzz to the timeout so there are no zero length timeouts.
        //
        if (cTimeout < DHCP_TIMEOUT_NORMAL) {
            cRange = 500;   // random fuzz ranges from +/- 500ms
        } else {
            cRange = 1000;  // random fuzz ranges from +/- 1 second
        }
    
        cFuzz = ((GetTickCount() % (cRange*2)) - cRange);
    
        if (cFuzz > 0) {
            //
            // Slow down sending for positive "fuzz"
            //
            Sleep(cFuzz);
        } else {
            //
            // Reduce select wait time for negative "fuzz"
            //
            cRet = cTimeout + cFuzz;
        }
    }

    return cRet;
}


STATUS
SendDhcpPkt(
    DhcpInfo *  pDhcp,
    DhcpPkt *   pPkt,
    int         cPkt,
    uchar       WaitforType,
    int         Flags
    )
{
    STATUS		    Status = DHCP_FAILURE;
    int             cLoop, cTimeLeft, cTimeout, cMultiplier;
    int             cUseFuzz;
    DWORD           cBeginTime, cZeroTime;
    struct timeval  Timeval;
    LPSOCK_INFO     Sock;
    SOCK_LIST       ReadList;
    DhcpPkt         RcvPkt;
    int             cRcvd, cRcvPkt;
    uchar           Type;
    int             fSend = TRUE;
    int             cMaxLoop;
    int             DestIP;
    WSABUF          WsaBuf;
    DWORD           WsaFlags;

    DEBUGMSG(ZONE_SEND, (TEXT("+SendDhcpPkt: pDhcp %X pPkt %X\r\n"), 
        pDhcp, pPkt));

	// also consider not bailing out
    if (DHCPSTATE_MEDIA_DISC & pDhcp->SFlags)
    	return DHCP_MEDIA_DISC;

    Sock = pDhcp->Socket;

    if ((DHCPACK == WaitforType) &&
        (pDhcp->SFlags & DHCPSTATE_AFD_INTF_UP_FL) &&
        (pDhcp->Flags & DHCPCFG_DIRECT_RENEWAL_FL))
    {
        //
        // Reduce broadcast traffic by directing the REQUEST to the DHCP server
        // if the DhcpDirectRenewal option was turned on for this interface.
        // 
        DestIP = pDhcp->DhcpServer;
    } else if (BCAST_FL & Flags) {
        DestIP = INADDR_BROADCAST;
    } else {
        DestIP = pDhcp->DhcpServer;
    }

    cRcvPkt = sizeof(RcvPkt);

	cMaxLoop = Flags & LOOP_MASK_FL;

    cLoop = 0;

    if (pDhcp->Flags & DHCPCFG_CONSTANT_RATE_FL) {
        //
        // In some wireless environments, the losses are due to weak signal
        // strength and not congestion, so sending the retries at a constant
        // rate provides a better customer experience.
        //
        cMultiplier = 1;
        cTimeout = DHCP_TIMEOUT_CONSTANT_RATE;
        cUseFuzz = 0;
    } else if (pDhcp->Flags & DHCPCFG_AUTO_IP_ENABLED_FL) {
        cMultiplier = 2;
        cTimeout = DHCP_TIMEOUT_AUTOCFG;
        cUseFuzz = 1;
    } else {
        cMultiplier = 2;
        cTimeout = DHCP_TIMEOUT_NORMAL;
        cUseFuzz = 1;
    }

    pDhcp->SFlags &= ~DHCPSTATE_SAW_DHCP_SRV;

	// ref it so it doesn't disappear
    RefDhcpInfo(pDhcp);
    CTEFreeLock(&pDhcp->Lock, 0);

    cZeroTime = GetTickCount();

    do {
        cBeginTime = GetTickCount();
        
        if (fSend) {
            cTimeLeft = AddFuzz(cUseFuzz, cTimeout);
            cTimeout *= cMultiplier;
            pPkt->Secs = (unsigned short)((cBeginTime - cZeroTime) / 1000);
            if (Status = DhcpSendDown(pDhcp, pPkt, cPkt, DestIP))
                break;
        }

        // wait only 8 secs (2 x min time) after last packet
        if ((cLoop != 1) && ((cLoop + 1) == cMaxLoop) && (cTimeLeft > 8000))
            cTimeLeft = 8000;
        
        ASSERT((cTimeLeft >= 0) && (cTimeLeft < 62000));
        if (cTimeLeft < 0)
            cTimeLeft = 0;
        if (cTimeLeft > 62000)
            cTimeLeft = 62000;

        // do select w/ timeout
        Timeval.tv_sec = cTimeLeft / 1000;
        Timeval.tv_usec = (cTimeLeft % 1000)*1000;
        
        ReadList.hSocket = 0;
        ReadList.Socket = Sock;
        ReadList.EventMask = READ_EVENTS;

        Status = CallAfd(Select)(1, &ReadList, 0, NULL, 0, NULL, &Timeval, NULL);

        if (Status)
            break;
        if (ReadList.Socket) {    // there is data
            cRcvPkt = sizeof(RcvPkt);
            
            WsaBuf.buf = (char *)&RcvPkt;
            WsaBuf.len = cRcvPkt;
            WsaFlags = 0;

            CTEGetLock(&pDhcp->Lock, 0);
		    if ((DHCPSTATE_MEDIA_DISC_PENDING|DHCPSTATE_DELETE) & pDhcp->SFlags) {
			    CTEFreeLock(&pDhcp->Lock, 0);
                Status = DHCP_DELETED;
            } else if (pDhcp->Socket) {
            	ASSERT(Sock == pDhcp->Socket);
                Status = CallSock(Recv)(Sock, &WsaBuf, 1, &cRcvd, &WsaFlags, 
                    NULL, NULL, NULL, NULL, NULL, &pDhcp->Lock);
                // AfdRecv will release the dhcp lock above
            } else {
                CTEFreeLock(&pDhcp->Lock, 0);
                Status = WSAENOTSOCK;
            }
            
            if (Status) {
                DEBUGMSG(ZONE_SEND, (TEXT("SendDhcpPkt: Recv() failed %d\n"), Status));
                break;
            }

            Type = WaitforType;

            CTEGetLock(&pDhcp->Lock, 0);
		    if ((DHCPSTATE_MEDIA_DISC_PENDING|DHCPSTATE_DELETE) & pDhcp->SFlags) {
			    CTEFreeLock(&pDhcp->Lock, 0);
                Status = DHCP_DELETED;
                break;
	    	} else {
                pDhcp->SFlags |= DHCPSTATE_SAW_DHCP_SRV;    // There is a DHCP server on the net.
                Status = TranslatePkt(pDhcp, &RcvPkt, cRcvd, &Type, pPkt->Xid);
                CTEFreeLock(&pDhcp->Lock, 0);
    		}

            if (DHCP_SUCCESS == Status) {
                if (Type == WaitforType)
                    break;
                else if (DHCPNACK == Type) {
                    Status = DHCP_NACK;
                    break;
                }
            }
            // this should work as long as the difference is not huge
            cTimeLeft -= GetTickCount() - cBeginTime;
        } else    {    // Select timed out
            DEBUGMSG(ZONE_SEND, (TEXT("SendDhcpPkt: Select() timed out\n")));                        
            Status = DHCP_FAILURE;
            cTimeLeft = 0;
        }

        if (cTimeLeft <= 0) {
            cLoop++;
            fSend = TRUE;
        } else
            fSend = FALSE;

    } while (cLoop < cMaxLoop);

    DEBUGMSG(ZONE_SEND, (TEXT("-SendDhcpPkt: pDhcp %X pPkt %X Ret: %d\r\n"), 
        pDhcp, pPkt, Status));
    
    CTEGetLock(&pDhcp->Lock, 0);
    if ((DHCPSTATE_MEDIA_DISC_PENDING|DHCPSTATE_DELETE) & pDhcp->SFlags)
        Status = DHCP_DELETED;
    
    cBeginTime = DerefDhcpInfo(pDhcp);
    ASSERT(cBeginTime || (DHCP_DELETED == Status));

    return Status;

}    // SendDhcpPkt()


