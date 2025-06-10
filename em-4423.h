#ifndef EM_4423_H
#define EM_4423_H

#ifndef MAIN_H
#include "main.h"
#endif

#ifndef COMMON_H
#include "common.h"
#endif

#ifndef LOGGING_C
#include "logging.c"
#endif

extern const BYTE EM_4423_USER_MEMORY_PAGES[60];
extern const BYTE EM_4423_EXISTING_PAGES[99];

typedef struct EM_4423_Pages {
    BYTE Pages[99][4]; // 99 pages (0x00 - 0x62) with 4 bytes per page
} EM_4423_Pages;

void em_4423_pages_object_print_all(EM_4423_Pages *tag_content);

BOOL em_4423_write_page(BYTE* data, BYTE page, SCARDHANDLE hCard, BYTE *pbRecvBuffer, DWORD *pbRecvBufferSize);
BOOL em_4423_read_page(BYTE page, SCARDHANDLE hCard, BYTE *pbRecvBuffer, DWORD *pbRecvBufferSize);
BOOL em_4423_fastread(SCARDHANDLE hCard, BYTE *pbRecvBuffer, DWORD *pbRecvBufferSize);

#endif
