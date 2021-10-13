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
// Definitions and declarations for the CMWiFiListener_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_CMWiFiListener_t_
#define _DEFINED_CMWiFiListener_t_
#pragma once

#include "CMWiFiTypes.hpp"
#if (CMWIFI_VERSION > 0)

#include "CMWiFiIntf_t.hpp"
#include "WLANListener_t.hpp"

#include <ThreadController_t.hpp>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// CMWiFiListener recieves notifcations from the Connection Manager
// and CSP-WiFi sub-systems.
// All notifications are stored in a circular buffer so applcations
// can review the notifications at whatever pace they want without
// danger of overflowing the notification queue or, conversely,
// missing a notification sent before expected.
//
class CMWiFiListener_t : public WLANListener_t
{
public:

    // Connection Manager notification source IDs:
    //
    static const DWORD NotificationSourceCM  = 0x8000000;
    static const DWORD NotificationSourceAll = NotificationSourceCM
                                             | WLAN_NOTIFICATION_SOURCE_ALL;

    // Constructor and destructor:
    //
    CMWiFiListener_t(
        const TCHAR *pAdapterName,
        HANDLE       hWLAN);
  __override virtual
   ~CMWiFiListener_t(void);

    // Adds the specified type to the set of those CM notification types
    // we want to receive:
    //
    DWORD
    AddCMNotificationType(      // session-state change notifications
        CM_NOTIFICATION_TYPE NotifyType,
        CM_SESSION_HANDLE    hSession);
    DWORD
    AddCMNotificationType(      // connection-state change notifications
        CM_NOTIFICATION_TYPE           NotifyType,
        const CM_CONNECTION_SELECTION &Connection);

    // Removes the specified CM notification type:
    // Returns ERROR_NOT_FOUND if the notification's not in the list.
    //
    DWORD
    RemoveCMNotificationType(
        CM_NOTIFICATION_TYPE NotifyType);

    // Clears the list of CM notification types:
    //
    void
    ClearCMNotificationTypes(void);

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

    // CM API interface:
    //
    CMWiFiIntf_t m_API;

    // Event for signalling CM notifications:
    //
    ce::auto_handle m_hCMEvent;

    // Current set of notification-sources:
    //
    DWORD m_CurrentSource;

    // List of CM notification-types:
    //
    MemBuffer_t m_CMNotifyTypes;

    // Handle to current notification listener:
    //
    CM_NOTIFICATIONS_LISTENER_HANDLE m_hCMListener;

    // ListenerThread runs a sub-thread for recieving notifcations
    // from the Connection Manager.
    //
    class CMWiFiListenerThread_t : public ThreadController_t
    {
    public:

        // Constructor:
        //
        CMWiFiListenerThread_t(
            CMWiFiListener_t *pListener);

    protected:
                
        // Runs the sub-thread for receiving Connection Manager notifications:
        //
      __override virtual DWORD
        Run(void);

    private:

        // Parent Listener:
        //
        CMWiFiListener_t *m_pListener;
    };

    CMWiFiListenerThread_t *m_pThread;
};

// ----------------------------------------------------------------------------
//
// CMWiFiNotification extends WiFiNotification to customize it for
// transporting notifications produced by Connection Manager.
//
class CMWiFiNotification_t : public WiFiNotification_t
{

private:
   
   CMWiFiNotification_t& operator = (const CMWiFiNotification_t& other);

public:

    // Constructor and destructor:
    //
    CMWiFiNotification_t(void);

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
    // sent by Connect Manager:
    const CM_NOTIFICATION *
    operator -> (void) const
    {
        return static_cast<const CM_NOTIFICATION *>(GetData());
    }
};

};
};

#endif /* if (CMWIFI_VERSION > 0) */
#endif /* _DEFINED_CMWiFiListener_t_ */
// ----------------------------------------------------------------------------
