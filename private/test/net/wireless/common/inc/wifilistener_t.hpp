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
// Definitions and declarations for the WiFiListener_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_WiFiListener_t_
#define _DEFINED_WiFiListener_t_
#pragma once

#include "WiFiTypes.hpp"
#include <MemBuffer_t.hpp>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// WiFiListener recieves notifcations from the WiFi sub-system.
// All notifications are stored in a circular buffer so applcations
// can review the notifications at whatever pace they want without
// danger of overflowing the notification queue or, conversely,
// missing a notification sent before expected.
//
class WiFiListenator_t;
class WiFiNotification_t;
class WiFiListener_t
{
public:

    // Constructor and destructor:
    //
    WiFiListener_t(
        const TCHAR *pAdapterName);
     virtual
   ~WiFiListener_t(void);
   
    // Retrieves the adapter name:
    //
    const TCHAR *
    GetAdapterName(void) const { return m_AdapterName; }

    // Is the object still valid?
    //
    bool
    IsValid(void) const
    {
        return m_hMessageEvent != NULL
            && m_MemoryMarker == sm_ExpectedMemoryMarker;
    }

    // (Re)registers to receive notifications:
    // RegisterAll registers to receive all possible notifications.
    // Register registers for notifications from the specified source(s).
    // Calling Register with source==0 cuts off the notification flow.
    //
    virtual DWORD
    RegisterAll(void) = 0;
    virtual DWORD
    Register(
        DWORD  NotifSource,                   // Source==0 cuts off notification flow
        BOOL   bIgnoreDuplicate = FALSE,      // TRUE = don't send duplicate notifications
        DWORD *pPrevNotifSource = NULL) = 0;  // !NULL = get previous notify-source (if any)
        
protected:

    // Adds the specified notification to the list:
    // If necessary, calls CreateNotification to let the derived class
    // generate the appropriate type of notification object.
    //
    DWORD
    AppendNotification(
        WiFiServiceType_e ServiceType,
        const void       *pMessage,
        DWORD              MessageBytes);

    // Called by AppendNotification() to let derived classes construct
    // the appropriate type of notification object:
    //
    virtual DWORD
    CreateNotification(
        WiFiServiceType_e    ServiceType,
        WiFiNotification_t **ppNotify) = 0;

private:

    // Marker indicating the object is still valid:
    //
    static const DWORD sm_ExpectedMemoryMarker = 0xCafeFeedUL;
    DWORD m_MemoryMarker;

    // Wireless adapter name:
    //
    TCHAR m_AdapterName[MAX_PATH];

    // Critical section to be locked before updating the notification list:
    //
    ce::critical_section m_ListenerLock;

    // Circular notification-message buffer:
    //
    static const int sm_MaxBufferedMessages = 64;
    WiFiNotification_t *m_pMessages[sm_MaxBufferedMessages];

    // Last notification written:
    //
    LONG m_LatestMessageId;

    // Event signalling a new notification's arrival:
    //
    HANDLE m_hMessageEvent;

    // Make the iterator a friend so it can call the following methods:
    //
    friend class WiFiListenator_t;

    // "Flushes" the notification-buffer by synchronizing the iterator's
    // position with the listener's:
    //
    void
    Flush(
        LONG *pIteratorPosition);

    // Retrieves the next notification:
    // Waits at most the specified time (in milliseconds) for a notification
    // to arrive. Returns ERROR_TIMEOUT if no notification arrives within the
    // time limit.
    //
    DWORD
    GetNext(
        LONG                *pIteratorPosition,
        WiFiNotification_t **ppMessage,
        LONG                 MaxWaitMs);

    // Retrieves the previous notification:
    // Returns ERROR_NOT_FOUND if the iterator has reached the beginning
    // of the circular list.
    //
    DWORD
    GetPrevious(
        LONG                *pIteratorPosition,
        WiFiNotification_t **ppMessage);

    // Copy and assignment are deliberately disabled:
    //
    WiFiListener_t(const WiFiListener_t &src);
    WiFiListener_t &operator = (const WiFiListener_t &src);
};

// ----------------------------------------------------------------------------
//
// WiFiListenator (WiFiListener Iterator) maintains an application's
// current position within the circular notification-list provided by
// a WiFiListener and provides methods for moving forward and backward
// within the list.
//
class WiFiListenator_t
{
public:

    // Constructor and destructor:
    //
    WiFiListenator_t(WiFiListener_t *pListener);
   ~WiFiListenator_t(void);

    // "Flushes" the notification-buffer by synchronizing the iterator's
    // position with the listener's:
    //
    void
    Flush(void);

    // Retrieves the next notification from the listener:
    // Waits at most the specified time (in milliseconds) for a notification
    // to arrive. Returns ERROR_TIMEOUT if no notification arrives within the
    // time limit.
    //
    DWORD
    GetNext(LONG MaxWaitMs = 1000);

    // Retrieves the previous notification from the listener:
    // Returns ERROR_NOT_FOUND if we've reached the beginning of the
    // circular list.
    //
    DWORD
    GetPrevious(void);

    // Determines whether we are currently positioned at a valid notification:
    //
    bool
    IsValid(void) const { return NULL != m_pMessage; }

    // Gets a pointer to the current notification:
    //
    const WiFiNotification_t *
    operator -> (void) const { return m_pMessage; }

private:

    // Notification receiver:
    //
    WiFiListener_t *m_pListener;

    // Current list position:
    //
    LONG m_Position;

    // Current notification (if any):
    //
    WiFiNotification_t *m_pMessage;
};

// ----------------------------------------------------------------------------
//
// WiFiNotification provides a generic type for transporting notification
// messages from the wireless services.
//
class WiFiNotification_t
{

private:
   
   WiFiNotification_t& operator = (const WiFiNotification_t& other);
   
public:

    // Minimum/maximum legal notification sizes:
    //
    static const DWORD MinimumNotificationSize = sizeof(DWORD);
    static const DWORD MaximumNotificationSize = 128 * 1024;

    // Constructor and destructor:
    //
    WiFiNotification_t(WiFiServiceType_e ServiceType);
    virtual
   ~WiFiNotification_t(void);

    // Creates a copy of the current object:
    //
    virtual WiFiNotification_t *
    Clone(void) const = 0;
    
    // Gets the type of the wireless service which sent the notification:
    //
    WiFiServiceType_e
    GetServiceType(void) const { return m_ServiceType; }

    // Gets the notification data:
    //
    DWORD
    GetLength(void) const { return m_NotifyData.Length(); }
    const void *
    GetData(void) const { return m_NotifyData.GetShared(); }

    // Adds the specified notification message:
    // The base class method just adds the raw data to the buffer.
    //
    virtual DWORD
    AddNotification(
        const WiFiListener_t &Listener,
        const void           *pMessage,
        DWORD                 MessageBytes);

protected:

    // Wireless service type:
    //
    const WiFiServiceType_e m_ServiceType;

    // Notification data:
    //
    MemBuffer_t m_NotifyData;
    
};

};
};

#endif /* _DEFINED_WiFiListener_t_ */
// ----------------------------------------------------------------------------
