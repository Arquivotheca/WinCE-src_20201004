#ifndef __STATE_INFO_H__
#define __STATE_INFO_H__

#define EVENTLOG_TEST

HRESULT getChannelInfo(WCHAR * szChannelName, EventLogChannel *pChannel, CircularEventBuffer *pEventBuffer);
HRESULT PrintEventInformation(CircularEventBuffer *pEventBuffer, BYTE *pBuffer);
HRESULT PrintChannelInformation(EventLogChannel *pChannel);
HRESULT PrintCircularBufferInformation(CircularEventBuffer *pEventBuffer);

BOOL g_bTestHooksEnabled;
#endif //__STATE_INFO_H__

