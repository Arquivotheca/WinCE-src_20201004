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
// ----------------------------------------------------------------------------
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
// ----------------------------------------------------------------------------
//
// Definitions and declarations for the WLANListener_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_WLANListener_t_
#define _DEFINED_WLANListener_t_
#pragma once

#include "WLANTypes.hpp"
#if (WLAN_VERSION > 0)

#include "WiFiListener_t.hpp"
#include "WLANAPIIntf_t.hpp"

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// WLANListener recieves notifcations from the WLAN sub-system.
// All notifications are stored in a circular buffer so applcations
// can review the notifications at whatever pace they want without
// danger of overflowing the notification queue or, conversely,
// missing a notification sent before expected.
//
class WLANListener_t : public WiFiListener_t
{
public:

    // Constructor and destructor:
    //
    WLANListener_t(
        const TCHAR *pAdapterName,
        HANDLE       hWLAN);
  __override virtual
   ~WLANListener_t(void);

    // (Re)registers to receive notifications:
    // RegisterAll registers to receive all possible notifications.
    // Register registers for notifications from the specified source(s).
    // Calling Register with source==0 cuts off the notification flow.
    //
  __override virtual DWORD
    RegisterAll(void);
  __override virtual DWORD
    Register(
        DWORD  NotifSource,               // Source==0 cuts off notification flow
        BOOL   bIgnoreDuplicate = FALSE,  // TRUE = don't send duplicate notifications
        DWORD *pPrevNotifSource = NULL);  // !NULL = get previous notify-source (if any)

protected:

    // Called by AppendNotification() to let derived classes construct
    // the appropriate type of notification object:
    //
  __override virtual DWORD
    CreateNotification(
        WiFiServiceType_e    ServiceType,
        WiFiNotification_t **ppNotify);

private:

    // WLAN API interface:
    //
    WLANAPIIntf_t m_API;

    // WLAN service handle:
    //
    HANDLE m_hWLAN;

    // Current set of notification-sources:
    //
    DWORD m_CurrentSource;

    // Callback function for recieving notifications:
    //
    static VOID
    WLANListenerCallback(
        PWLAN_NOTIFICATION_DATA pNotification,
        PVOID                   pContext);
};

// ----------------------------------------------------------------------------
//
// WLANNotification extends WiFiNotification to customize it for
// transporting notifications produced by the WLAN sub-system.
//
class WLANNotification_t : public WiFiNotification_t
{

private:
   
   WLANNotification_t& operator = (const WLANNotification_t& other);
   
public:

    // Constructor and destructor:
    //
    WLANNotification_t(void);

    // Creates a copy of the current object:
    //
  __override virtual WiFiNotification_t *
    Clone(void) const;

    // Adds the specified notification message:
    //
  __override virtual DWORD
    AddNotification(
        const WiFiListener_t &Listener,
        const void           *pMessage,
        DWORD                 MessageBytes);

    // Translates the notification object (if any) to the type
    // sent by WLAN:
    const WLAN_NOTIFICATION_DATA *
    operator -> (void) const
    {
        return static_cast<const WLAN_NOTIFICATION_DATA *>(GetData());
    }
};

};
};

#endif /* if (WLAN_VERSION > 0) */
#endif /* _DEFINED_WLANListener_t_ */
// ----------------------------------------------------------------------------
