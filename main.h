#ifndef MAIN_H
#define MAIN_H

#ifndef COMMON_H
#include "common.h"
#endif

// ApduResponse is a struct that holds both the status (e.g. success or failure) and the amount of bytes that the response consists of (e.g. 16)
typedef struct {
    LONG status;
    LONG amount_response_bytes;
} ApduResponse;

// general functions
LONG getAvailableReaders(SCARDCONTEXT hContext, char *mszReaders, DWORD *dwReaders);
LONG connectToReader(SCARDCONTEXT hContext, const char *reader, SCARDHANDLE *hCard, DWORD *dwActiveProtocol, BOOL directConnect);
ApduResponse executeApdu(SCARDHANDLE hCard, BYTE *pbSendBuffer, DWORD dwSendLength, BYTE *pbRecvBuffer, DWORD *pbRecvBufferSize);
LONG disableBuzzer(SCARDCONTEXT hContext, const char *reader, SCARDHANDLE *hCard, DWORD *dwActiveProtocol, BYTE *pbRecvBuffer, DWORD *pbRecvBufferSize);
void disconnectReader(SCARDHANDLE hCard, SCARDCONTEXT hContext);

// general interactions with tags
LONG getUID(SCARDHANDLE hCard, BYTE *pbRecvBuffer, DWORD *pbRecvBufferSize, BOOL printResult);
ApduResponse getATS_14443A(SCARDHANDLE hCard, BYTE *pbRecvBuffer, DWORD *pbRecvBufferSize, char *tagName);
LONG getStatus(SCARDHANDLE *hCard, char *mszReaders, DWORD dwState, DWORD dwReaders, DWORD *dwActiveProtocol, BYTE *pbRecvBuffer, DWORD *pbRecvBufferSize, BOOL printResult, char *tagName);

// helper functions
BOOL containsSubstring(const char *string, const char *substring);
void printHex(LPCBYTE pbData, DWORD cbData);
void dump_response_buffer_16(BYTE *pbRecvBuffer);
void dump_response_buffer_256(BYTE *pbRecvBuffer);
void resetBuffer2048(BYTE *buffer);
BOOL is_byte_in_array(BYTE value, const BYTE *array, size_t size);

BOOL test(SCARDHANDLE hCard, BYTE *pbRecvBuffer, DWORD *pbRecvBufferSize);

#endif