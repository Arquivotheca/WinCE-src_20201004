// common.h

#ifndef _COMMON_H_
#define _COMMON_H_

#ifndef _PREFAST_
#pragma warning(disable:4068)
#endif

// Server port that client will connect on to submit test requests
const WORD  SERVER_SERVICE_REQUEST_PORT = 4998;

// Length of time to wait before closing connection on no activity
const DWORD TEST_TIMEOUT_MS = 5000;

// Length of time to wait on test connection establishment
const DWORD TEST_CONNECTION_TIMEOUT_MS = 5000;

#endif // _COMMON_H_
