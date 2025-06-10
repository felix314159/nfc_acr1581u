#include "em-4423.h"
#include "logging.c"
#include "main.h"
// 7 byte UID

// there are two different versions of the em-4423:
//  - EM4423V121 (small EPC version, extra 20 bytes of user memory), so total user memory is 240+20 = 260 bytes (2080-bit)
//  - EM4423V221 (large EPC version, extra 8 bytes of user memory), so total user memory is 240+8 = 248 bytes (1984-bit)
// in this code i will only use the 240 bytes that are present in both versions

// each page holds 4 bytes, but 60*4=240 bytes
const BYTE EM_4423_USER_MEMORY_PAGES[60] = {                           0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
                                               0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
                                               0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
                                               0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F };

const BYTE EM_4423_EXISTING_PAGES[99] =    {   0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
                                               0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
                                               0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
                                               0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
                                               0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
                                               0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
                                               0x60, 0x61, 0x62 };

// ---------------- write / read tag --------------------------------------------------

// em_4423_write_page writes 4 bytes to a page
BOOL em_4423_write_page(BYTE* data, BYTE page, SCARDHANDLE hCard, BYTE *pbRecvBuffer, DWORD *pbRecvBufferSize) {
    LOG_DEBUG("Trying to write to page 0x%02x.", page);
    // sanity check
    if (!is_byte_in_array(page, EM_4423_USER_MEMORY_PAGES, 60)) {
        LOG_WARN("Page 0x%02x is not a user memory page. Refusing to write there.", page);
        return FALSE;
    }

    BYTE APDU_Write[5 + 4] = { 0xff, 0xd6, 0x00, page, 0x04 };
    memcpy(APDU_Write + 5, data, 4);

    // write to page
    ApduResponse response = executeApdu(hCard, APDU_Write, sizeof(APDU_Write), pbRecvBuffer, pbRecvBufferSize);
    if (response.status != 0 || !(pbRecvBuffer[response.amount_response_bytes-2] == 0x90 && pbRecvBuffer[response.amount_response_bytes-1] == 0x00)) {
        LOG_ERROR("Failed to write to page 0x%02x. Aborting..", page);
        return FALSE;
    }
    LOG_INFO("Wrote data to page 0x%02x with success.", page);

    return TRUE;
}

// em_4423_read_page reads a 4 byte page
BOOL em_4423_read_page(BYTE page, SCARDHANDLE hCard, BYTE *pbRecvBuffer, DWORD *pbRecvBufferSize) {
    LOG_DEBUG("Trying to read page 0x%02x.", page);
    // sanity check
    if (!is_byte_in_array(page, EM_4423_EXISTING_PAGES, 99)) {
        LOG_WARN("Page 0x%02x is not a valid page. 0x62 is the last valid page for EM4423.", page);
        return FALSE;
    }

    BYTE APDU_Read[5] = { 0xff, 0xb0, 0x00, page, 0x04 };
    ApduResponse response = executeApdu(hCard, APDU_Read, sizeof(APDU_Read), pbRecvBuffer, pbRecvBufferSize);
    if (response.status != 0 || !(pbRecvBuffer[response.amount_response_bytes-2] == 0x90 && pbRecvBuffer[response.amount_response_bytes-1] == 0x00)) {
        LOG_ERROR("Failed to read from page 0x%02x. Aborting..", page);
        return FALSE;
    }
    LOG_INFO("Read data from page 0x%02x with success.", page);

    return TRUE;
}

// em_4423_fastread reads the entire tag memory at once
BOOL em_4423_fastread(SCARDHANDLE hCard, BYTE *pbRecvBuffer, DWORD *pbRecvBufferSize) {
    LOG_DEBUG("Trying to fastread the entire tag.");

    // store page data
    EM_4423_Pages tag_content = {0};

    // if you can't use extended apdu then u can read only 0xff = 255 bytes at once
    // BYTE APDU_Read[5] = { 0xff, 0xb0, 0x00, 0x00, 0xff };
    
    // page 93 of ref-acr1581u: extended apdu - you can put 2 bytes for amount of bytes to read
    //                     class,   INS,     P1,      P2,       LE (number of bytes to read) [index 0: announces extended apdu, index 1 and 2: 2-byte value of how many bytes to read (4*99 = 396 = 0x018C)]
    BYTE APDU_Read[7] = {  0xff,    0xb0,   0x00,    0x00,      0x00, 0x01, 0x8c }; // larger value than 8c wraps around (continues reading from start again)
    ApduResponse response = executeApdu(hCard, APDU_Read, sizeof(APDU_Read), pbRecvBuffer, pbRecvBufferSize);
    if (response.status != 0 || !(pbRecvBuffer[response.amount_response_bytes-2] == 0x90 && pbRecvBuffer[response.amount_response_bytes-1] == 0x00)) {
        LOG_ERROR("Failed to fastread entire tag. Aborting..");
        return FALSE;
    }

    // copy into object tag_content (99 pages a 4 bytes are copied at once)
    memcpy(tag_content.Pages, pbRecvBuffer, 99 * 4);

    em_4423_pages_object_print_all(&tag_content);

    LOG_INFO("Fastread entire tag with success.");
    return TRUE;
}

void em_4423_pages_object_print_all(EM_4423_Pages *tag_content) {
    for (BYTE i = 0x00; i < 0x99; ++i) {
        printf("[Page 0x%02X]\t0x%02X  0x%02X  0x%02X  0x%02X\n",
           i,
           tag_content->Pages[i][0],
           tag_content->Pages[i][1],
           tag_content->Pages[i][2],
           tag_content->Pages[i][3]);
    }
}
