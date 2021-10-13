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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
/*


*/
#ifndef __CMSGQUE_H_INCLUDED__
#define __CMSGQUE_H_INCLUDED__

#include <dbt.h>
#include <gesture_priv.h>

//  Thread code in the kernel knows that this is special.
#define TLS_MSG_QUEUE_INDEX 0


class CWindow;
class MsgQueue;
struct NCDATA;
class TouchCursor_t;
struct crtGlobTag_t;
struct ProcessData;

class Foreground_t
{
public:

    static
    MsgQueue*
    MessageQueue(
        void
        );


    static
    CWindow*
    Window(
        void
        );


    static
    CWindow*
    PreviousWindow(
        void
        );



    static
    bool
    WantsAllKeys(
        void
        );


    static
    void
    MessageQueueTerminated(
        MsgQueue*
        );


    static
    BOOL
    WINAPI
    GetForegroundInfo_I(
        GET_FOREGROUND_INFO *pgfi
        );

};



class AnimatedCursorInfo_t
{
public:
    HINSTANCE m_hinst;
    HBITMAP *m_phbmFrames;
    DWORD m_BaseResourceId;
    int m_cFrames;
    int m_FrameTimeInterval;
    HANDLE m_hProcCaller;
    bool m_bUseAnimatedCursor;

    ~AnimatedCursorInfo_t();

    void *
    operator new(
        size_t s
        );

    void
    operator delete(
        void *p
        );

    void
    CleanupFrameBitmaps(
        void
        );

    BOOL
    LoadBitmapFrames(
        HINSTANCE hinst,
        DWORD ResourceId,
        int cFrames
        );

};





//    Flags for setting foreground window, active window and focus.
typedef UINT32    SW_FLAGS;
#define swfNull                    0x0
#define swfMouseActivate        0x1
#define swfDoNotTop                0x2
#define swfSkipFocus            0x4
#define swfSkipActive            0x8
#define swfCheckEnabled            0x10
#define swfSetForeground        0x20
#define swfRestore                0x40
#define swfPickedBySystem        0x80
//#define swfSaveCursor            0x100
#define swfRestoringDesktop        0x200

// Window Position Argument (WPA) and Window Positon (WP) used for DeferWindowPos functions
class WindowPositionArgument_t
{
public:
    HWND hWnd;                // handle to window to position
    HWND hWndInsertAfter;    // placement-order handle
    int x;                    // horizontal position
    int y;                    // vertical position
    int cx;                    // width
    int cy;                    // height
    unsigned int uFlags;                // window-positioning flags

};

class DeferWindowPosHeader_t
{
public:
    DeferWindowPosHeader_t *pNextHeader;
    int iNumElems;        // number of elements in WP array
    int iFreeCount;        // Count of free Window Position Argument (WPA) slots
    WindowPositionArgument_t *pwpaArray;         // Pointer to beginning of WPA array
};

struct TimerEntry_t
{
private:

    TimerEntry_t();                                //    Default constructor.  Not implemented.
    TimerEntry_t(const TimerEntry_t&);                //    Default copy constructor.  Not implemented.
    TimerEntry_t& operator=(const TimerEntry_t&);    //    Default assignment operator.  Not implemented.

    static const unsigned int s_SigValid;
    unsigned int m_SigObject;

    static
    unsigned int
    SetTimerCommon(
        HWND hwnd,
        unsigned int idTimer,
        unsigned int uTimeout,
        TIMERPROC tmprc,
        HPROCESS hProcCallingContext,
        bool bGenerateUniqueScrollId
        );

public:
    TimerEntry_t *m_pNextInMsgQueue;
    TimerEntry_t *m_pNextInTimerQueue;
    HWND m_hwnd;
    HPROCESS m_hProcCallingContext;

private:
    unsigned int m_idTimer;
    unsigned int m_uTimeout;
    unsigned int m_uRemainingTime;
    TIMERPROC m_tmprc;
    LPARAM m_MousePosAtPost;
    MsgQueue *m_pmsgqOwner;
    bool m_Fired;
    bool m_Callback;    // A call back timer entry. This WM_TIMER message won't go through
                        // message queue, it will call into passed in TIMERPROC instead.

public:

    TimerEntry_t(
        MsgQueue *pmsgq,
        HWND hwnd,
        unsigned int idTimer,
        unsigned int Timeout,
        TIMERPROC tmprc,
        HPROCESS hProcCallingContext,
        bool bGenerateUniqueScrollId
        );

    ~TimerEntry_t();    //    Default destructor.

    bool
    IsValidTimerEntry(
        void
        );

    static
    TimerEntry_t*
    TimerQueuesRemoveSingleEvent(
        HWND hwnd,
        unsigned int idTimer,
        MsgQueue *pmsgq
        );

    static
    void
    TimerQueuesRemoveAllMsgQueueOrHwnd(
        MsgQueue *pmsgq,
        HWND hwnd
        );

    static
    void
    TimerServiceLoop(
        void
        );


    static
    void
    TimerQueueWindowDestroyedNotification(
        MsgQueue * pmsgq,
        HWND hwnd
        );

    static
    BOOL
    WINAPI
    KillTimer_I(
        HWND hwnd,
        unsigned int uIDEvent
        );

    static
    unsigned int
    WINAPI
    SetTimer_I(
        HWND hwnd,
        unsigned int idTimer,
        unsigned int uTimeout,
        TIMERPROC tmprc,
        HPROCESS hProcCallingContext
        );

    static
    unsigned int
    SetScrollTimer(
        HWND hwnd,
        unsigned int uTimeout,
        TIMERPROC tmprc
        );

    static
    unsigned int
    SetCallbackTimer(
        unsigned int uTimerout,
        TIMERPROC tmprc
        );

    bool
    IsFired(
        void
        );

    bool
    TestAndReset(
        bool Peeking,
        HWND *pHwnd,
        WPARAM *wp,
        TIMERPROC *pTimerProc,
        LPARAM *pMousePosition
        );


};



#define MSGSRC_HARDWARE_MOUSE        3


class    PostedMsgQueueEntry_t
{
public:
    PostedMsgQueueEntry_t *pNext;
    HWND Hwnd;
    unsigned int Message;
    WPARAM wParam;
    LPARAM lParam;
    DWORD time;
    LPARAM MousePosAtPost;
    unsigned int Source;
};


/*    Structure for keyboard entries. */
class PostedMsgQueueEntryKeyboard_t : public PostedMsgQueueEntry_t
    {
public:
    KEY_STATE_FLAGS ShiftStateFlags;
    unsigned int cCharacters;
    unsigned int Chars;
    };

/* Structure for extended mouse entries. */
class PostedMsgQueueEntryMouse_t : public PostedMsgQueueEntry_t
    {
public:
    KEY_STATE_FLAGS ShiftStateFlags;
    unsigned int UIQStartIndex;
    unsigned int UIQCount;
    };

/* Structure for extended gesture entries */
class PostedMsgQueueEntryGesture_t : public PostedMsgQueueEntry_t
    {
public:
    GESTUREINFODATA gestureData;    // WARNING: variable sized struct
    };

// Macro to pack the Touch Gesture parameters.
//   cmd should be the 1-byte command ID (GCI_COMMAND_xx)
//   arg should be the 32-bit argument
//
// The packing arrangement is undocumented and subject to change in future versions
// of CE. Applications should use GetGestureCommand to unpack wParam and lParam safely.
// Currently the arrangement is as follows:
//
// WPARAM
//
//  31                                  0
//  +--------+--------+--------+--------+
//  |         arg 0..24        |   cmd  |
//  +--------+--------+--------+--------+
//
// LPARAM
//
//  31                                  0
//  +--------+--------+--------+--------+
//  | arg24  |    pos Y   |    pos X    |
//  +--------+--------+--------+--------+
//
// The 32-bit argument is split across wParam and lParam. The low 24-bits are stored
// in the top 24-bits of wParam and the remaining 8-bits is stored in the top 8-bits
// of lParam. The X/Y coordinates are constrained to 12-bits each and occupy the bottom
// 24-bits of lParam.
//
// The below macros faciliate packing and unpacking wParam and lParam. They should be
// used rather than attempt to interpret the parameter contents directly.
//
#define PACK_LOC(x,y)           ((((y) & 0x0FFF) << 12) | ((x) & 0x0FFF))
#define PACK_WPARAM(cmd, arg)   (((cmd) & 0x00FF) | ((arg) & 0x00FFFFFF) << 8)
#define PACK_LPARAM(x, y, arg)  (((arg) & 0xFF000000) | PACK_LOC(x,y))

#define UNPACK_CMD(wp)          LOBYTE(LOWORD((wp)))
#define UNPACK_ARG(wp, lp)      ((((wp) >> 8) & 0x00FFFFFF) | ((lp) & 0xFF000000))
#define UNPACK_LOC_X(lp)        ((lp) & 0x0FFF)
#define UNPACK_LOC_Y(lp)        (((lp) >> 12) & 0x0FFF)


// Used by SendMessageTimeout API
struct MessageTimeout
    {
    DWORD TimeoutValue;
    DWORD TimeStamp;
    int cReference;    // reference count used to prevent MessageTimeout from being deallocated in a nested SendMessageTimeout call 
    };


/*

smfSenderNoWait means that the sender will not wait for a reply from the
receiver.

smfSenderNoWaitIfDifferentThread means that the sender will not wait for a
reply from the receiver if the receiver is in a different thread.  The
send will be synchronous if the receiver is in the same thread, i.e., the
window proc will be invoked directly.  This flag is used by the system to
avoid hanging when sending notifications to windows in different threads
while still allowing synchronous behavior for windows in the same thread.

smfResultReady is used to signal that the receiver has called the
appropriate window proc and the result is now ready.  In other words, this
flag essentially signals a reply event.

*/
typedef UINT32    SEND_MSG_FLAGS;

#define smfNull                                0x00000000    // Uses defaults, i.e., synchronous send.
#define smfSenderNoWait                        0x00000001    // Asynchronous send.
#define smfSenderNoWaitIfDifferentThread    0x00000002    // Asynchronous if receiver is a different thread.
#define smfHwndIsThreadId                    0x00000004    // The hwnd parameter is actually a thread id.
#define smfResultReady                        0x80000000    // The result of a send message is ready.
#define smfSenderTerminated                    0x40000000    // The sender thread has terminated.
#define smfReceiverTerminated                0x20000000    // The receiver thread has terminated.
#define smfTimeout                            0x10000000    // Use time out
#define smfNotifyMessage                    0x08000000    // Used by SendNotifyMessage to set the correct return value

/*

The data used to send messages between threads.

For a synchronous send, the entry may be on the received message queue of
the receiver and the sent message stack of the sender simultaneously.

pmsgqReply is the message queue to be signalled when the reply is ready.

Messages which require extra data have extra room allocated at the end but
don't blindly access the Extra field since it is not always there.

*/

class SendMsgEntry_t
    {
public:
    SendMsgEntry_t *pReceivedNext;        //    Next entry on the received message queue.
    SendMsgEntry_t *pSentNext;            //    Next entry on the sent message stack.
    class MsgQueue *pmsgqReply;            //    The message queue (if any) waiting for the reply.
    SEND_MSG_FLAGS smFlags;            //    The send options.
    HWND Hwnd;
    unsigned int Message;
    WPARAM wParam;
    LPARAM lParam;
    LONG WndProcResult;        //    The result of the receiver window proc call.
    };


struct SendMsgEntryExtra
    {
    SendMsgEntry_t *pReceivedNext;        //    Next entry on the received message queue.
    SendMsgEntry_t *pSentNext;            //    Next entry on the sent message stack.
    class MsgQueue *pmsgqReply;            //    The message queue (if any) waiting for the reply.
    SEND_MSG_FLAGS smFlags;            //    The send options.
    HWND Hwnd;
    unsigned int Message;
    WPARAM wParam;
    LPARAM lParam;
    LONG WndProcResult;        //    The result of the receiver window proc call.
    unsigned int Extra;                //    Extra data if needed.
    };




/*

Entry on the paint request queue.

Remember that paint requests are different from other messages.  When the
window system requests a paint, an entry is put on the paint request list.
A later call to GetMessage looks for an entry but does not remove it from
the list.  In the course of processing the WM_PAINT message, the window
system calls back in to cancel the request which removes it from the list.

The PaintRequestXXX routines are the worker routines.  The window system
should use only RequestPaint.

*/
class PaintRequestEntry_t
    {
public:
    PaintRequestEntry_t *pNext;        //    Next request in the list.
    HWND Hwnd;        //    Hwnd to send the WM_PAINT to.
    LPARAM MousePosAtPost;
    };

//    Various flags in the message queue.
typedef UINT32    MSG_QUEUE_FLAGS;
#define msgqfGotWMQuitMessage        0x01    //    The queue has "received" a WM_QUIT message.
#define msgqfUntangling                0x02    //    The queue is untangling from other queues (prior to being freed).
#define msgqfFocusSetToNull            0x04    //    Whether the focus was set to NULL while activating.
#define msgqfCaretBlinkOn            0x08    //    Caret is "on" on the screen.
#define msgqfNCDeactivate            0x20    //    WM_NCACTIVATE NULL "message".
#define msgqfActivateForeground        0x40    //    SetActiveWindow will behave like SetForegroundWindow.
#define msgqfIsSip                    0x80    //    Thread is running an SI panel.
#define msgqfLastKeyWasAltDown        0x100    //    The last vkey pulled from the queue was Alt down.
#define msgqfWantsAllKeys            0x200    //    The queue will receive all HotKeys
#define msgqfDestroyingImeWindows    0x400    //    When set, the Ime windows are being destroyed.

enum MsgqGetEventFlag   //    MsgqGetEvent option flags
    {
    mgefNull = 0x00,            //    No flags set.
    mgefReply = 0x01,            //    Get reply messages.
    mgefTimerMsg = 0x02,            //    Get timer messages.
    mgefPostedMsg  = 0x04,            //    Get posted messages.
    mgefQuitMsg = 0x08,            //    Get quit messsage.
    mgefPaintMsg = 0x10,            //    Get paint messages.
    mgefNCMsg = 0x20,            //    Get (don't short circuit) NC messages.
    mgefDead = 0x40,            //    This msg queue is dead.
    mgefWait = 0x80000000,    //    Wait if no messages of the requested type(s).
    mgefPeekNoRemove = 0x40000000,    //    Don't remove if peeking.
    mgefTimeout = 0x20000000,     //  GetEvent with a time out
    };

enum OwnedWindowCount
{
    owcTopLevel,
    owcIMEClassStyle,
};

/*

The message queue structure.

The message queue is actually made up of a number of components which correspond to "events"
which are used in the course of message passing:

    1. Posted Message Queue
    2. Received Message Queue
    3. Sent Message Stack
    4. Paint Request List
    5. Quit Message


The posted message queue holds messages which have been posted to a window
owned by the message queue's thread.

When a cross thread send is done, a SendMsgEntry is created and placed on
the receiving message queue's received message queue.  The sender also
places the entry on its own sent message stack.

Paint requests are kept on their own paint request list using
PaintRequestEntry structures.  There is no specified ordering of this
list.

The quit message information is just kept using a flag and an exit code.


In addition, the most recent keyboard message is used to hold on to a
keyboard message from the user input system.  This message contains the
characters which correspond to a key and is kept so that a later call to
TranslateMessage can just look up the appropriate characters.  This
component does not correspond to any "event" but rather is just storage.

*/
class MsgQueue
    {
private:

    friend class TouchCursor_t;
    friend class MouseCursor_t;

    MsgQueue *pNext;                // All queues are linked into a list for cleanup.
    UINT32 m_sig;                // Signature

    HPROCESS m_hprcOwner;        // The process of the thread that created the queue.
    HTHREAD m_hthdOwner;        // The thread which created the queue.
    crtGlobTag_t *m_pCrtGlob;

    HKL m_hkl;               // Input Locale associated with the current thread.

    // Previous input locale associated with the current thread. This is used only when one 
    // uses a hotkey to toggle between an IME & a non-IME
    HKL m_hklPrev;          

    // Handle to Default IME window for the current thread.
    // 
    // Default IME Wnd handle value is stored in the CRT storage & is duplicated 
    // in the thread's message queue - this is done as IMM uses (read operations 
    // predominantly) it extensively as well as GWE also needs it some times 
    // - if we move the value from CRT to the Msg queue there will be a perf hit 
    // - so we just duplicate the value & make sure that the value is updated 
    // promptly
    HWND m_hwndDefaultIme;   

    HANDLE m_hNewEvents;        // Signal for new events.

    TimerEntry_t *m_pTimerHead;
    TimerEntry_t *m_pNextTimerToService;


    PostedMsgQueueEntry_t *m_pInputMsgQueueHead;    // Head of the input message queue.
    PostedMsgQueueEntry_t *m_pInputMsgQueueTail;    // Tail of the input message queue.

    NCDATA *m_pNCData;

    HWND m_hwndActiveWindow;
    HWND m_hwndFocus;
    HWND m_hwndKeyboardTarget;

    HWND m_hwndCapture;
    HWND m_hwndGestureTarget; // Window where gesture began
    HWND m_hwndNextGestureTarget;   // which window the gesture will be targeted at (set at button down time)

    class CWindow *m_pcwndCaret;
    class CWindow *m_pcwndCaretDisabled;
    unsigned int m_uCaretBlinkTime;
    RECT m_CaretRect;
    int m_CaretShowCount;        //    ShowCount > 0 turns on caret
    BOOL m_fCaretOnScreen;        //    Is caret actually drawn on the screen?

    HCURSOR m_hCursor;
    HCURSOR m_hCursorWait;
    int m_ShowCursorCount;            //    cShowCursor >= 0 shows cursor
    AnimatedCursorInfo_t *m_pAnimatedCursorInfo;            //  used for customizable animated cursor

    MsgQueue *m_pmsgquic;        // The message queue that owns the current UI context.

    DeferWindowPosHeader_t *m_pHeaderWPA;
    // TODO: Need m_ullSystemGestureFlags here?
    ProcessData* m_pProcessData;

    BOOL
    Init(
        HTHREAD hthd
        );


    void
    UnInit(
        void
        );


    void
    Untangle(
        void
        );


    BOOL
    InputQueuePost(
        PostedMsgQueueEntry_t *pNewEntry
        );


    void
    KillCancelableActivationMessages(
        void
        );


    BOOL
    GetPostedMsg(
        HWND hwndIn,                        //    The hwnd filter.
        UINT32 MsgMin,                        //    The min WM_* message filter.
        UINT32 MsgMax,                        //    The max WM_* message filter.
        MsgqGetEventFlag mgeFlags,                    //    Options.
        MSG *pmsg,                        //    Location to put retrieved info.
        HWND *phwndHitTopLevel,
        LRESULT *pHitCode,
        LPARAM *pMRMousePos,
        PostedMsgQueueEntry_t **ppMsgQueueHead,
        PostedMsgQueueEntry_t **ppMsgQueueTail
        );

    void
    DispatchReceivedMsg(
        void
        );

    MsgqGetEventFlag
    GetEvent(
        MsgqGetEventFlag mgeFlags,
        HWND hwndFilter,
        UINT32 MsgFilterMin,
        UINT32 MsgFilterMax,
        MSG *pmsg
        );


    void
    CaretRectInvert(
        class CWindow    *pcwnd
        );


    void
    CaretRectOff(
        void
        );


public:

    void* operator new(size_t);
    void operator delete(void *p);

    MsgQueue();
    ~MsgQueue();


    //    *****    MISC

    BOOL
    IsMsgQueue(
        void
        ) const;


    BOOL
    OkToContinue(
        void
        ) const;


    void
    Signal(
        void
        );


    void
    WindowDestroyed(
        HWND hwnd
        );


    void
    SetCursorImage(
        void
        );

    static
    BOOL
    SendImeNotificationMsgToThreadDefaultImeWnd(
        UINT Flags
        );

    static
    BOOL
    SendActivateLayoutMsgToThreadDefaultImeWnd(
        HKL hklNew,
        UINT Flags
        );

    static
    BOOL 
    SetHKL(
        HKL hkl,
        UINT Flags
        );

    static
    BOOL
    SetPrevHKL(
        HKL hkl
        );

    static
    BOOL 
    GetHKL(
        DWORD idThread,    
        HKL *,
        UINT
        );

    static
    BOOL
    GetDefIMEWindow(
        HTHREAD hThread,
        HWND *phwndDefaultIme
        );

    static
    BOOL
    SetDefIMEWindow(
        HWND hwndDefaultIme
        );

    BOOL 
    IsMsgQThreadHandle(
        HTHREAD hThread
        );

    static
    void
    TouchProcessThreadNotify(
        DWORD cause,
        HPROCESS hproc,
        HTHREAD hthread
        );

    static
    DWORD
    WINAPI
    MsgQueue::
    MsgWaitForMultipleObjectsEx_E(
        DWORD nCount,
        HANDLE *pHandles,
        DWORD dwMilliseconds,
        DWORD dwWakeMask,
        DWORD dwFlags
        );

    static
    DWORD
    WINAPI
    MsgQueue::
    MsgWaitForMultipleObjectsEx_IWrapper(
        DWORD nCount,
        HANDLE *pHandles,
        DWORD dwMilliseconds,
        DWORD dwWakeMask,
        DWORD dwFlags
        );

    static
    DWORD
    MsgWaitForMultipleObjectsEx_I(
        DWORD nCount,
        __out_ecount_opt(nCount) HANDLE *pHandles,
        HPROCESS HandlesOwnerProcess, // handle to the process which owns the list of handles being sent in        
        DWORD dwMilliseconds,
        DWORD dwWakeMask,
        DWORD dwFlags
        );


    static
    BOOL
    AllKeys_I(
        BOOL bAllKeys
        );
    
    void
    IncrementWindowReferenceCount(
        bool bIMEClassWindow
        );
    
    void
    DecrementWindowReferenceCount(
        bool bIMEClassWindow
        );

    MsgQueue*
    UserInterfaceContextOwner(
        void
        )
    {
        return m_pmsgquic;
    }

    void CleanupUserInputContextWindows(
        HWND hwnd
        );


    //    *****    MESSAGES



    BOOL
    InputQueuePostMessage(
        HWND hwnd,
        unsigned int Msg,
        WPARAM wParam,
        LPARAM lParam
        );


    BOOL
    InputQueuePostMouseMessage(
        unsigned int Msg,
        WPARAM wParam,
        LPARAM lParam,
        unsigned int Source,
        KEY_STATE_FLAGS ShiftStateFlags,
        int UserInputQueueIndex
        );


    BOOL
    InputQueuePostTouchGestureMessage(
        PostedMsgQueueEntry_t* pEntry
        );


    BOOL PostedQueuePost(
        PostedMsgQueueEntry_t* pNewEntry
        );

    static
    LPARAM
    SendMessageWithOptions(
        HWND hWnd,
        unsigned int uMsg,
        WPARAM wParam,
        LPARAM lParam,
        SEND_MSG_FLAGS smFlags
        );


    static
    BOOL
    PostKeybdMessageInternal(
        class MsgQueue *pMsgQueue,
        HWND hwnd,
        UINT32 VKey,
        UINT32 ScanCode,
        KEY_STATE_FLAGS ShiftStateFlags,
        unsigned int Source,
        UINT32 cCharacters,
        UINT32 *pShiftStateBuffer,
        UINT32 *pCharacterBuffer
        );

    static
    BOOL
    PeekMessageWInternal(
        MSG *pMsg,
        HWND hWnd,
        unsigned int wMsgFilterMin,
        unsigned int wMsgFilterMax,
        MsgqGetEventFlag mgeFlags
        );



    static
    BOOL
    PostMessageW_I(
        HWND hWnd,
        unsigned int Msg,
        WPARAM wParam,
        LPARAM lParam
        );

    static
    void
    PostQuitMessage_I(
        int nExitCode
        );

    static
    BOOL
    WINAPI
    PostKeybdMessage_E(
        HWND            hwnd,                    //    hwnd to send to.
        unsigned int    VKey,                    //    VKey to send.
        KEY_STATE_FLAGS    KeyStateFlags,            //    Flags to determine event and other key states.
        unsigned int    CharacterCount,            //    Count of characters.
        unsigned int  *pShiftStateBuffer,     //    Shift state buffer.
        unsigned int   ShiftStateBufferLen,
        unsigned int  *pCharacterBuffer,       //    Character buffer.
        unsigned int   CharacterBufferLen
        );

    static
    BOOL
    PostKeybdMessage_I(
        HWND            hwnd,                    //    hwnd to send to.
        unsigned int    VKey,                    //    VKey to send.
        KEY_STATE_FLAGS    KeyStateFlags,            //    Flags to determine event and other key states.
        unsigned int    CharacterCount,            //    Count of characters.
        unsigned int  *pShiftStateBuffer,     //    Shift state buffer.
        unsigned int  *pCharacterBuffer       //    Character buffer.
        );

    static
    LRESULT
    WINAPI    
    SendMessageW_E(
        HWND hWnd,
        unsigned int uMsg,
        WPARAM wParam,
        LPARAM lParam
        );

    static
    LRESULT
    WINAPI    
    SendMessageW_IW(
        HWND hWnd,
        unsigned int uMsg,
        WPARAM wParam,
        LPARAM lParam
        );

    static
    LRESULT
    SendMessageW_I(
        HWND hWnd,
        unsigned int uMsg,
        WPARAM wParam,
        LPARAM lParam
        );

    static
    LRESULT
    WINAPI
    SendMessageTimeout_E(
        HWND hWnd,
        unsigned int uMsg,
        WPARAM wParam,
        LPARAM lParam,
        unsigned int fuFlags,
        unsigned int uTimeout,
        DWORD_PTR *lpdwResult
        );

    static
    LRESULT
    SendMessageTimeout_IW(
        HWND hWnd,
        unsigned int uMsg,
        WPARAM wParam,
        LPARAM lParam,
        unsigned int fuFlags,
        unsigned int uTimeout,
        DWORD_PTR *lpdwResult
        );
    
    static
    LRESULT
    SendMessageTimeout_I(
        HWND hWnd,
        unsigned int uMsg,
        WPARAM wParam,
        LPARAM lParam,
        unsigned int fuFlags,
        unsigned int uTimeout,
        DWORD_PTR *lpdwResult
        );

    static
    BOOL
    WINAPI
    SendNotifyMessageW_E(
        HWND hWnd,
        unsigned int uMsg,
        WPARAM wParam,
        LPARAM lParam
        );

    static
    BOOL
    WINAPI
    SendNotifyMessageW_IW(
        HWND hWnd,
        unsigned int uMsg,
        WPARAM wParam,
        LPARAM lParam
        );

    static
    BOOL
    SendNotifyMessageW_I(
        HWND hWnd,
        unsigned int uMsg,
        WPARAM wParam,
        LPARAM lParam
        );

    static
    BOOL
    GetMessageW_I(
        MSG *pMsg,
        HWND hWnd,
        unsigned int wMsgFilterMin,
        unsigned int wMsgFilterMax
        );


    static
    BOOL
    GetMessageWNoWait_I(
        MSG *pMsg,
        HWND hWnd,
        unsigned int wMsgFilterMin,
        unsigned int wMsgFilterMax
        );


    static
    BOOL
    PeekMessageW_I(
        MSG *pMsg,
        HWND hWnd,
        unsigned int wMsgFilterMin,
        unsigned int wMsgFilterMax,
        unsigned int wRemoveMsg
        );


    static
    BOOL
    TranslateMessage_I(
        const MSG *pMsg
        );


    static
    LRESULT
    DispatchMessageW_I(
        const MSG *pMsg
        );


    static
    BOOL
    InSendMessage_I(
        void
        );


    static
    DWORD
    GetQueueStatus_I(
        unsigned int flags
        );


    static
    DWORD
    GetMessageQueueReadyTimeStamp_I(
        HWND hWnd
        );

    //    *****    UI CONTEXTS

    bool
    UicEqual(
        class MsgQueue *pmsgq
        ) const
    {
    return m_pmsgquic == pmsgq -> m_pmsgquic;
    }


    HKL
    KeyboardLayoutHandle(
        void
        ) const;


    //    *****    FOCUS AND ACTIVATION

    HWND
    FocusHwnd(
        void
        ) const;

    HWND
    ActiveHwnd(
        void
        ) const;

    HWND
    KeyboardTargetHwnd(
        void
        ) const;


    void
    PreprocessReceivedMsg(
        HWND hwnd,
        unsigned int message,
        WPARAM wp,
        LPARAM lp,
        bool *bToss
        );

    BOOL
    CanSendGesture(
        HWND hwnd,
        WPARAM wp
        );

    void
    PostprocessReceivedMsg(
        HWND hwnd,
        unsigned int message,
        WPARAM wp,
        LPARAM lp);

    void
    SetUicActiveWindow(
        class CWindow *pcwndNewActive,
        SW_FLAGS SWFlags
        );


    HWND
    SetUicFocus(
        class CWindow *pcwndNewFocus,
        SW_FLAGS SWFlags
        );


    HWND
    SetUicKeyboardTarget(
        class CWindow    *pcwndNewTarget
        );


    static
    HWND
    GetForegroundWindow_I(
        void
        );

    friend class CWindow;

    friend
        BOOL
        MsgQueue_SetActiveWindowInternal(
            HWND hwndNewActive,
            SW_FLAGS SWFlags
            );


    friend
        HWND
        SetFocus_I(
            HWND hWnd
            );


    friend
        HWND
        GetFocus_I(
            void
            );


    friend
        HWND
        GetKeyboardTarget_I(
            void
            );


    friend
        HWND
        GetActiveWindow_I(
            void
            );


    friend
        HWND
        SetActiveWindow_I(
            HWND hwndNewActive
            );



    //    *****    TIMERS

    void
    AddTimerEntry(
        TimerEntry_t *pNewEntry
        );


    void
    RemoveTimerEntryOrHwnd(
        TimerEntry_t *pTimerEntry,
        HWND hwnd
        );


    //    *****    CAPTURE

    HWND
    hwndUICCapture(
        void
        )
    {
    return m_pmsgquic -> m_hwndCapture;
    }


    HWND
    SetUicCapture(
        HWND hwnd
        );


    friend
        HWND
        SetCapture_I(
            HWND hwnd
            );


    friend
        BOOL
        ReleaseCapture_I(
            void
            );


    friend
        HWND
        GetCapture_I(
            void
            );


    //    *****    CARET

    class CWindow *
    pcwndUICCaret(
        void
        )
    {
    return m_pmsgquic -> m_pcwndCaret;
    }

    HWND
    hwndUICFocus(
        void
        )
    {
    return m_pmsgquic -> m_hwndFocus;
    }




    BOOL
    DestroyCaret(
        void
        );

    BOOL
    CreateCaret(
        CWindow *pcwnd,
        HBITMAP hBitmap,
        int nWidth,
        int nHeight
        );

    BOOL
    HideCaret(
        CWindow *pcwnd
        );

    BOOL
    ShowCaret(
        CWindow *pcwnd
        );

    BOOL
    SetCaretPos(
        int X,
        int Y
        );

    BOOL
    GetCaretPos(
        LPPOINT lpPoint
        );

    BOOL
    SetCaretBlinkTime(
        unsigned int    uMSeconds
        );

    unsigned int
    GetCaretBlinkTime(
        void
        );

    BOOL
    DisableCaretMaybe(
        class CWindow *pcwnd
        );

    void
    EnableCaretMaybe(
        class CWindow *pcwnd
        );


    void
    CaretBlink(
        class CWindow *pcwndCaret
        );

    void
    LinkUIContext(
        class MsgQueue *pmsgqTopLevel
        );


    //    *****    CURSOR

    static
    HCURSOR
    WINAPI    
    LoadCursorW_I(
        HINSTANCE hInstance,
        DWORD CursorNameResourceID,
        const WCHAR *pCursorName
        );

    static
    HCURSOR
    LoadAnimatedCursor_I(
        HINSTANCE hinst,
        DWORD ResourceId,
        int cFrames,
        int FrameTimeInterval
        );


    AnimatedCursorInfo_t*
    AnimatedCursorInfo(
        void
        ) const;


    void
    AnimatedCursorInfo(
        AnimatedCursorInfo_t*    pAnimatedCursorInfo
        );

    int
    ShowCursorCount(
        void
        ) const;
    

    static
    HCURSOR
    SetCursor_I(
        HCURSOR hCursor
        );


    static
    HCURSOR
    GetCursor_I(
        void
        );


    static
    int
    ShowCursor_I(
        BOOL bShow
        );


    static
    HCURSOR
    WaitCursor(
        void
        );


    //    *****    NON-CLIENT DATA

    friend
        void
        MsgQueue_SetNCData(
            NCDATA *pData
            );

    friend
        NCDATA *
        MsgQueue_GetNCData(
            void
            );

    //    *****    XXXDeferWindowPos handle to header of window position argument array

    friend
    BOOL
    MsgQueue_InsertDeferWindowPos_pHeaderWPA(
        DeferWindowPosHeader_t *pHeaderWPA
        );

    friend
    BOOL
    MsgQueue_RemoveDeferWindowPos_pHeaderWPA(
        DeferWindowPosHeader_t *pHeaderWPA
        );
    
    friend
    BOOL
    MsgQueue_FindDeferWindowPos_pHeaderWPA(
        DeferWindowPosHeader_t *pHeaderWPA
    );
    
    //    *****    IMM
    void
    ImmSetActiveContextCallback(
        CWindow    *pcwnd,
        BOOL    bActivate
        );

    //    *****    HOOKS

    LRESULT
    WM_HOOK_DO_CALLBACK_Handler(
        int idHook
        );


    static
    bool
    NeedsPixelDoubling(
        void
        );

    static
    bool
    SetPixelDoublingForCurrentThread(
        bool bThunk
        );

    BOOL
    GetQueueGestures(
        HWND hwnd,
        ULONGLONG * pullFlags,
        UINT uScope
        );

    BOOL
    SetQueueGestures(
        HWND hwnd,
        ULONGLONG ullFlags,
        UINT uScope
        );

    DWORD m_MRFirstDownTime;
    HWND m_MRFirstDownHwnd;
    int m_MRFirstDownX;
    int m_MRFirstDownY;

    UINT m_MRFirstDownMessage;
    WORD m_MRFirstXbutton; 

    LPARAM m_MRMousePosPulled;    // Most Recent mouse position pulled from the queue.
    LPARAM m_MRMousePosPut;    // Most Recent mouse position put on the queue.
    LPARAM m_MRMousePosOfGesture;    // Most Recent mouse position used for gesturing, this is either mouse down or first pan
    BOOL   m_fPanStarted; // we need this boolean to know if the PAN started or not


    PostedMsgQueueEntry_t *m_pPostedMsgQueueHead;            //    Head of the posted message queue.
    PostedMsgQueueEntry_t *m_pPostedMsgQueueTail;         //    Tail of the posted message queue.

    SendMsgEntry_t *m_pSentMsgStackHead;            //    Head of the stack of sent messages.

    SendMsgEntry_t *m_pReceivedMsgQueueHead;        //    Head of the received message queue.
    SendMsgEntry_t *m_pReceivedMsgQueueTail;        //    Tail of the received message queue.

    SendMsgEntry_t *m_pReceivedInProgressHead;        //    Dispatches in progress.

    PaintRequestEntry_t *m_pPaintRequests;

    KEY_STATE_FLAGS m_MRShiftStateFlagsPulled;
    KEY_STATE_FLAGS m_MRShiftStateFlagsPut;

    PostedMsgQueueEntryKeyboard_t
                        *m_pMRKeybdMsgPulled;            //    The most recent keyboard message pulled from the queue.
    PostedMsgQueueEntryKeyboard_t
                        *m_pMRKeybdMsgPut;                //    The most recent keyboard message put on the queue.

    LPARAM m_MousePosAtQuit;
    UINT32 m_nExitCode;                    //    The exit code from PostQuitMessage.

    int m_MMStartIndex;                    //    Start of mouse move info in user input queue.
    int m_MMCount;                        //    Number of mouse move in user input queue.

    int m_index;                        //    Used when partitioning message queues.
    int m_group;                        //    Used when partitioning message queues.


    MSG_QUEUE_FLAGS m_msgqFlags;                    //    Flags.

    UINT32 m_iQSBitsNew;                    // Queue status bits

    MessageTimeout *m_pMessageTimeout;                // Used to store SendMessageTimeout time-out and system time
    DWORD m_ReadyTimeStamp;                // time when message queue was last ready to process a message
    WORD m_cntTopLevelWindows;            // number of top level windows owned by this message queue
    WORD m_cntIMEClassStyleWindows;        // number of windows owned by this message queue that have the CS_IME
                                                        // window class style

    PostedMsgQueueEntry_t* m_pTouchGestureMsgQueueEntry;   // The most recent touch gesture message pulled from the queue.

    enum NeedPixelDoubling_t
    {
        DoNotKnow = 0,
        Yes,
        No
    };
    NeedPixelDoubling_t m_NeedsPixelDoubling;

    ULONGLONG m_ullGestureTargetWindowFlags; // Gesture flags for gesture target window

    friend class MsgQueueMgr;
    friend class CWindowManager;



    };

DWORD 
ImmAssociateValueWithGwesMessageQueue_I(
    DWORD,
    UINT
    );

MsgQueue*
CurrentThreadGetMsgQueue(
    void
    );


void
RequestPaint(
    HWND hwnd,
    BOOL fPaint
    );



UINT32
GetKeyShiftStateFlags(
    void
    );

SHORT
WINAPI
GetKeyState_I(
    int nVirtKey
    );



//    wParam is needed for WM_DEVICECHANGE messages.
#define IsLParamPtr(msg, wParam, lParam)                    \
    ( (msg==WM_SETTEXT) ||                                    \
      (msg==WM_GETTEXT) ||                                    \
      (msg==WM_WINDOWPOSCHANGED) ||                            \
      (msg==WM_STYLECHANGED) ||                                \
      (msg==WM_MEASUREITEM) ||                                \
      (msg==WM_GESTURE) ||                                    \
      ( (msg==WM_COPYDATA) && (lParam!=NULL) ) ||             \
      ( (msg==WM_SYSCOPYDATA) && (lParam!=NULL) ) ||        \
      ( (msg==WM_DEVICECHANGE) && (wParam & 0x8000) ) ||    \
      ( (msg==WM_NETCONNECT) && (lParam!=NULL) ) )


BOOL
WINAPI
CreateCaret_I(
    HWND hWnd,
    HBITMAP hBitmap,
    int nWidth,
    int nHeight
    );

BOOL
WINAPI
DestroyCaret_I(
    void
    );

BOOL
WINAPI
HideCaret_I(
    HWND hWnd
    );

BOOL
WINAPI
ShowCaret_I(
    HWND hWnd
    );

BOOL
WINAPI
SetCaretPos_I(
    int X,
    int Y
    );


BOOL
WINAPI
GetCaretPos_I(
    LPPOINT lpPoint
    );


BOOL
WINAPI
SetCaretBlinkTime_I(
    unsigned int    uMSeconds
    );


unsigned int
WINAPI
GetCaretBlinkTime_I(
    void
    );


int
WINAPI
DisableCaretSystemWide_I(
    void
    );

int
WINAPI
EnableCaretSystemWide_I(
    void
    );



DWORD
WINAPI
GetMessagePos_I(
    void
    );


void
DoubleClickReadRegistry(
    void
    );




class MsgQueueMgr
    {

public:

    static
    MsgQueue*
    MsgQueueMgr::
    GetCreateMQ(
        DWORD idThread
        );

    static
    void
    WINAPI
    CaretOffSystemWide(
        void
        );

    static
    void
    WINAPI
    ProcessThreadNotify(
        DWORD cause,
        HPROCESS proc,
        HTHREAD thread
        );

    static
    BOOL
    WINAPI
    PostThreadMessageW(
        DWORD idThread,
        unsigned int Msg,
        WPARAM wParam,
        LPARAM lParam
        );


    static
    void
    WINAPI
    PartitionMsgQueues(
        MsgQueue *pmsgqParent,
        MsgQueue *pmsgqChild,
        BOOL fAdopt
        );


    static
    unsigned int
    GetMessageSource(
        void
        );

    };

    



BOOL
WINAPI
SetForegroundWindow_I(
    HWND hwnd
    );


HWND
WINAPI
SetKeyboardTarget_I(
    HWND hwnd
    );




HHOOK
WINAPI
SetWindowsHookExW_I(
    int idHook,
    HOOKPROC lpfn,
    HINSTANCE hmod,
    DWORD dwThreadId);


BOOL
WINAPI
UnhookWindowsHookEx_I(
    HHOOK hhk);

#ifdef DEBUG
extern void DebugActive(TCHAR *lpszFmt, ...);
extern void DebugActiveEnter(TCHAR *lpszFmt, ...);
extern void DebugActiveLeave(TCHAR *lpszFmt, ...);


#define DEBUG_ACTIVE(PrintfExp)        \
    do                                \
        {                            \
        if ( DBGACTIVE )            \
            DebugActive PrintfExp;    \
        } while (0)

#define DEBUG_ACTIVE_ENTER(PrintfExp)        \
    do                                \
        {                            \
        if ( DBGACTIVE )            \
            DebugActiveEnter PrintfExp;    \
        } while (0)

#define DEBUG_ACTIVE_LEAVE(PrintfExp)        \
    do                                \
        {                            \
        if ( DBGACTIVE )            \
            DebugActiveLeave PrintfExp;    \
        } while (0)


#else
#define DEBUG_ACTIVE(PrintfExp)
#define DEBUG_ACTIVE_ENTER(PrintfExp)
#define DEBUG_ACTIVE_LEAVE(PrintfExp)
#endif



#endif


