// install drivers from https://www.acs.com.hk/en/products/583/acr1581u-dualboost-iii-usb-dual-interface-reader/
#include "main.h"
#include "ndef.h"
#include "em-4423.h"

#include "logging.c"


// -------------------- Functions that interact with reader -------------------------------

LONG getAvailableReaders(SCARDCONTEXT hContext, char *mszReaders, DWORD *dwReaders) {
    LONG lRet = SCardListReaders(hContext, NULL, mszReaders, dwReaders);
    if (lRet != SCARD_S_SUCCESS) {
        if (lRet == 0x8010002E) {
            LOG_CRITICAL("Failed to list readers: Are you sure your smart card reader is connected and turned on?\n");
        } else {
            LOG_CRITICAL("Failed to list readers: 0x%lX\n", lRet);
        }
    }

    return lRet;
}

LONG connectToReader(SCARDCONTEXT hContext, const char *reader, SCARDHANDLE *hCard, DWORD *dwActiveProtocol, BOOL directConnect) {
    LONG lRet;

    if (directConnect) {
        // direct communication with reader (no present tag required)
        lRet = SCardConnect(hContext, reader, SCARD_SHARE_DIRECT, SCARD_PROTOCOL_T1, hCard, dwActiveProtocol);
    } else {
        // T1 = block transmission (works), T0 = character transmission (did not work when testing), Tx = T0 | T1 (works)
        // https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-rdpesc/41673567-2710-4e86-be87-7b6f46fe10af
        lRet = SCardConnect(hContext, reader, SCARD_SHARE_SHARED, SCARD_PROTOCOL_T1, hCard, dwActiveProtocol);
    }

    return lRet;
}

// executes command and returns the amount of bytes that the response contains
ApduResponse executeApdu(SCARDHANDLE hCard, BYTE *pbSendBuffer, DWORD dwSendLength, BYTE *pbRecvBuffer, DWORD *pbRecvBufferSize) {
    // first reset first 2048 bytes of response buffer to all zeroes (to avoid that 90 00 from some previous command is later read and confused)
    resetBuffer2048(pbRecvBuffer);

    // this took me long to figure out (part 1): i want to always remember the size of the array that holds the response. but SCardTransmit modifies the value of pbRecvBufferSize to the amount of bytes of the response. thats why we can lose the information how big our buffer is. this can lead to nasty bugs (e.g. you just once forget to update pbRecvBufferSize to the amount of bytes of the expected response and then u get UB due to buffer overflow. so safer is to just always reset to actual buffer size)
    DWORD pbRecvBufferSizeBackup = *pbRecvBufferSize;

    LONG lRet = SCardTransmit(hCard, SCARD_PCI_T1, pbSendBuffer, dwSendLength, NULL, pbRecvBuffer, pbRecvBufferSize);
    // also print reply
    if (lRet == SCARD_S_SUCCESS) {
        // print which command you sent
        printf("> ");
        printHex(pbSendBuffer, dwSendLength);

        // print what you received
        printf("< ");
        printHex(pbRecvBuffer, *pbRecvBufferSize);

    } else {
        //printf("%08x\n", lRet);
        printf("%08lx\n", lRet);
    }

    LOG_DEBUG("Response status: %0lx (0 means success)", lRet);
    LOG_DEBUG("Response size: %lu bytes", *pbRecvBufferSize); // linux wants %lu, macos wants %u. just use either and ignore warning lul
    
    // return both status and length of response
    // ApduResponse response;
    // response.status = lRet;
    // response.amount_response_bytes = *pbRecvBufferSize;
    ApduResponse response = {
        .status = lRet,
        .amount_response_bytes = *pbRecvBufferSize
    };

    // this took me long to figure out (part 2): i want to always retain the constant buffer size in this variable
    *pbRecvBufferSize = pbRecvBufferSizeBackup;

    return response;
}

LONG disableBuzzer(SCARDCONTEXT hContext, const char *reader, SCARDHANDLE *hCard, DWORD *dwActiveProtocol, BYTE *pbRecvBuffer, DWORD *pbRecvBufferSize) {
    LONG lRet = connectToReader(hContext, reader, hCard, dwActiveProtocol, TRUE);
    if (lRet != SCARD_S_SUCCESS) {
        LOG_ERROR("Failed to connect: 0x%x\n", (unsigned int)lRet);
        return 1;
    }

    // APDU command to disable the buzzer sound of ACR1581U
    BYTE pbSendBuffer[6] = { 0xE0, 0x00, 0x00, 0x21, 0x01, 0x01 };

    LONG result = SCardControl(*hCard, SCARD_CTL_CODE(3500), pbSendBuffer, sizeof(pbSendBuffer), pbRecvBuffer, *pbRecvBufferSize, pbRecvBufferSize);

    return result;
}

void disconnectReader(SCARDHANDLE hCard, SCARDCONTEXT hContext) {
    SCardDisconnect(hCard, SCARD_LEAVE_CARD);
    SCardReleaseContext(hContext);
}

// -------------------- General Functions that interact with various tags -------------------------------

LONG getUID(SCARDHANDLE hCard, BYTE *pbRecvBuffer, DWORD *pbRecvBufferSize, BOOL printResult) {
    LOG_INFO("Will now try to determine UID");
    // Define the GET UID APDU command (PC/SC standard for many cards)
    BYTE pbSendBuffer[5] = { 0xFF, 0xCA, 0x00, 0x00, 0x00 }; // if u change last byte to e.g. 0x04 then u only get first 4 bytes of UID
    ApduResponse response = executeApdu(hCard, pbSendBuffer, sizeof(pbSendBuffer), pbRecvBuffer, pbRecvBufferSize);
    if (response.status != SCARD_S_SUCCESS) {
        return response.status;
    }

    // ensure 90 00 success is returned
    if ((!((pbRecvBuffer[4] == 0x90 && pbRecvBuffer[5] == 0x00) ||   // UID of length 4: single UID
         (pbRecvBuffer[7] == 0x90 && pbRecvBuffer[8] == 0x00)   ||   // UID of length 7: double UID
         (pbRecvBuffer[10] == 0x90 && pbRecvBuffer[11] == 0x00)     // UID of length 10: oh baby a triple oh yeah UID
         ))) {      
        
        //dump_response_buffer_256(pbRecvBuffer);
        return ACR_90_00_FAILURE;
    }

    if (printResult) {
        // success: now print UID
        printf("Detected UID: ");
        for (DWORD i = 0; i < *pbRecvBufferSize - 2; i++) { // -2 because no need to print success code 90 00
            // avoid printing entire buffer of zeroes, but check for 90 00 only if i+1 is in bounds
            if ((i + 1 < *pbRecvBufferSize) &&
                (pbRecvBuffer[i] == 0x90) &&
                (pbRecvBuffer[i + 1] == 0x00)) {
                printf("\n\n");
                return response.status;
            }
            printf("%02X ", pbRecvBuffer[i]);
        }
    }

    printf("\n");
    return response.status;
}

// getATS_14443A sends a RATS (Request for Answer To Select) to the tag (afaik only stuff like desfire, ntag 424 dna, smartMX and some java cards even support this)
ApduResponse getATS_14443A(SCARDHANDLE hCard, BYTE *pbRecvBuffer, DWORD *pbRecvBufferSize, char *tagName) {
    LOG_INFO("Will now try to determine ATS");
    BYTE pbSendBuffer[] = { 0xFF, 0xCA, 0x01, 0x00, 0x00 };
    ApduResponse response = executeApdu(hCard, pbSendBuffer, sizeof(pbSendBuffer), pbRecvBuffer, pbRecvBufferSize);

    if ((pbRecvBuffer[0] == 0x6A) && (pbRecvBuffer[1] == 0x81)) {
        LOG_WARN("Accessing ATS of this tag type is currently not supported by this program\n");
    }

    // identify Mifare Desfire EV3 8k
    if ((pbRecvBuffer[0] == 0x06) && (pbRecvBuffer[1] == 0x75) && (pbRecvBuffer[2] == 0x77) && (pbRecvBuffer[3] == 0x81) && (pbRecvBuffer[4] == 0x02) && (pbRecvBuffer[5] == 0x80)) {
        LOG_INFO("I now know for sure that your tag is: Desfire EV3");
        // update stored tag name
        strncpy(tagName, "Mifare Desfire EV3 8k", 99); // i dont own enough tags to tell whether that decides just desfire, or desfire ev3, or desfire ev3 8k
    } 
    // identify NTAG 424 DNA TT
    else if ((pbRecvBuffer[0] == 0x06) && (pbRecvBuffer[1] == 0x77) && (pbRecvBuffer[2] == 0x77) && (pbRecvBuffer[3] == 0x71) && (pbRecvBuffer[4] == 0x02) && (pbRecvBuffer[5] == 0x80)) {
        LOG_INFO("I now know for sure that your tag is: NTAG DNA 424 DNA");
        // update stored tag name
        strncpy(tagName, "NTAG 424 DNA TT", 99);
    }

    printf("\n");
    return response;
}

LONG getStatus(SCARDHANDLE *hCard, char *mszReaders, DWORD dwState, DWORD dwReaders, DWORD *dwActiveProtocol, BYTE *pbRecvBuffer, DWORD *pbRecvBufferSize, BOOL printResult, char *tagName) {
    LOG_INFO("Will now try to determine which model your tag is");
    LONG lRet = SCardStatus(*hCard, mszReaders, &dwReaders, &dwState, dwActiveProtocol, pbRecvBuffer, pbRecvBufferSize);
    if ((lRet == SCARD_S_SUCCESS) && printResult) {
        // success: now print status
        printf("Detected tag type: ");
        for (DWORD i = 0; i < *pbRecvBufferSize - 2; i++) { // -2 because no need to print success code 90 00
            printf("%02X ", pbRecvBuffer[i]);
        }
        printf("\n");

        if (*pbRecvBuffer < 8) {
            LOG_ERROR("Failed to identify the tag (reply too short)\n");
            strncpy(tagName, "UNIDENTIFIED TAG", 99);
            return 1;
        }

        // determine which kind of tag this is
        BYTE tagIdentifyingByte1 = pbRecvBuffer[13];
        BYTE tagIdentifyingByte2 = pbRecvBuffer[14];
        if ((tagIdentifyingByte1 == 0x00) &&  (tagIdentifyingByte2 == 0x01)) {
            printf("Identified tag as: Mifare Classic 1k\n");
            strncpy(tagName, "Mifare Classic 1k", 99); // copy less than 100 chars cuz that's how long the name buffer is
        } else if ((pbRecvBuffer[13] == 0x00) && (pbRecvBuffer[14] == 0x02)) {
            printf("Identified tag as: Mifare Classic 4k\n");
            strncpy(tagName, "Mifare Classic 4k", 99);
        } else if ((pbRecvBuffer[13] == 0x00) && (pbRecvBuffer[14] == 0x03)) {
            printf("Identified tag as: Mifare Ultralight or NTAG2xx\n");
            strncpy(tagName, "Mifare Ultralight or NTAG2xx", 99);
        } else if ((pbRecvBuffer[13] == 0x00) && (pbRecvBuffer[14] == 0x26)) {
            printf("Identified tag as: Mifare Mini\n");
            strncpy(tagName, "Mifare Mini", 99);
        } else if ((pbRecvBuffer[13] == 0xF0) && (pbRecvBuffer[14] == 0x04)) {
            printf("Identified tag as: Topaz/Jewel\n");
            strncpy(tagName, "Topaz/Jewel", 99);
        } else if ((pbRecvBuffer[13] == 0xF0) && (pbRecvBuffer[14] == 0x11)) {
            printf("Identified tag as: FeliCa 212K\n");
            strncpy(tagName, "FeliCa 212K", 99);
        } else if ((pbRecvBuffer[13] == 0xF0) && (pbRecvBuffer[14] == 0x12)) {
            printf("Identified tag as: FeliCa 424K\n");
            strncpy(tagName, "FeliCa 424K", 99);
        } else if ((pbRecvBuffer[0] == 0x3B) && (pbRecvBuffer[1] == 0x81) && (pbRecvBuffer[2] == 0x80) && (pbRecvBuffer[3] == 0x01) ) {
            printf("Identified tag as: Mifare Desfire EV3 8k or NTAG 424 DNA TT\n");
            strncpy(tagName, "Mifare Desfire EV3 8k or NTAG 424 DNA TT", 99);
        } else if (pbRecvBuffer[13] == 0xFF) {
            LOG_WARN("Identified tag as: UNKNOWN TAG\n");
            strncpy(tagName, "UNKNOWN TAG", 99);
        } 

        else {
            LOG_ERROR("Failed to identify the tag\n");
            strncpy(tagName, "UNIDENTIFIED TAG", 99);
        }
    }

    printf("\n");
    return lRet;
}

// -------------------- Helper functions -------------------------------------------------------

BOOL containsSubstring(const char *string, const char *substring) {
    // Edge case: if substring is empty, it's always considered a match
    if (*substring == '\0') {
        return TRUE;
    }

    // Iterate through the string
    for (const char *s = string; *s != '\0'; ++s) {
        // Check if the current character matches the first character of substring
        if (*s == *substring) {
            const char *str_ptr = s;
            const char *sub_ptr = substring;

            // Compare the substring starting from here
            while (*sub_ptr != '\0' && *str_ptr == *sub_ptr) {
                str_ptr++;
                sub_ptr++;
            }

            // If the entire substring matches, return 1 (true)
            if (*sub_ptr == '\0') {
                return TRUE;
            }
        }
    }

    // Return 0 (false) if the substring was not found
    return FALSE;
}

// printHex prints bytes as hex
void printHex(LPCBYTE pbData, DWORD cbData) {
    for (DWORD i = 0; i < cbData; i++) {
        printf("%02x ", pbData[i]);
    }
    printf("\n");
}

// dump_response_buffer_16 prints the first 16 values contained in the array (but there is more in the array, so maybe write a full dump function if necessary)
void dump_response_buffer_16(BYTE *pbRecvBuffer) {
    printf("\nDumping first 16 values in response buffer:\n");
    for (int i = 0; i < 16; i++) {
        printf("0x%02x", pbRecvBuffer[i]);
        if (i != 15) {
            printf("  ");
        }
    }
    printf("\n\n");
}

// dump_response_buffer_256 prints the first 256 values contained in the array. line-breaks after every 4 bytes.
void dump_response_buffer_256(BYTE *pbRecvBuffer) {
    printf("\nDumping first 256 values in response buffer:\n");
    for (int i = 0; i < 256; i++) {
        printf("0x%02x", pbRecvBuffer[i]);
        if ((i+1) % 4 == 0) { // +1 so that it doesnt line-break for i=0
            printf("\n");
        }
        else {
            printf("  ");
        }
    }
    printf("\n\n");
}

// resetBuffer2048 is used to reset the reply buffer to all zeroes, e.g. resetBuffer(pbRecvBuffer);
void resetBuffer2048(BYTE *buffer) {
    memset(buffer, 0, 2048);
    LOG_DEBUG("Response buffer has been reset to all zeroes");
}

// is_byte_in_array is a helper function to check whether passed byte value is an element in the array 'array' of size 'size'
// C feels really old here, not being able to get size of array that has static size because it decays to a pointer in this function is kinda cringe no cap
BOOL is_byte_in_array(BYTE value, const BYTE *array, size_t size) {
    for (size_t i = 0; i < size; i++) {
        if (array[i] == value) {
            return TRUE;
        }
    }
    return FALSE;
}


// -------------------------------------------------------

int main(void) {
    SCARDCONTEXT hContext;
    SCARDHANDLE hCard = 0;
    DWORD dwActiveProtocol;
    char mszReaders[1024];
    DWORD dwReaders = sizeof(mszReaders);

    BYTE pbRecvBuffer[2048] = {0};
    DWORD pbRecvBufferSize = sizeof(pbRecvBuffer);

    DWORD dwState = SCARD_POWERED; // TODO: can u dynamically request the actual state somehow?

    char connectedTag[100]; // will later hold e.g. "Mifare Classic 4k", just pre-alloc 100 bytes for the name (and 100% reason to remember the name)

    // Establish context
    LONG lRet = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
    if (lRet != SCARD_S_SUCCESS) {
        if (lRet == 0x8010001E) {
            LOG_CRITICAL("Error: 'SCARD_E_SERVICE_STOPPED: The Smart card resource manager has shut down.' Could be related to incompatible PCSCLite version. Are there error-hinting logs when you run: 'sudo systemctl status pcscd' ?");
        } else {
            LOG_CRITICAL("Failed to establish context: 0x%X\n", (unsigned int)lRet);
        }
        
        return 1;
    }

    // Get available readers (you might have multiple smart card readers connected)
    lRet = getAvailableReaders(hContext, mszReaders, &dwReaders);
    if (lRet != SCARD_S_SUCCESS) {
        SCardReleaseContext(hContext);
        return 1;
    }

    // Print connected readers and select the first one
    //      ACS ACR1581 1S Dual Reader [ACR1581 1S Dual Reader ICC] 00 00
    LOG_INFO("Available Smart Card Readers:");
    char *reader = NULL;
    char *p = mszReaders;

    while (*p) {
        printf("found reader: %s\n", p);
        // we want to connect to the CONTACTLESS interface (01 00 or 00 00?), so pick the one with 'PICC' in it
        // if you want to connect to the CONTACT interface (00 00 or 01 00?), then replace 'PICC' with 'ICC'
        // if you want to connect to the SAM interface (02 00), then replace 'PICC' with 'SAM'
        if (strstr(p, "PICC")) {
            reader = p;
        }
        p += strlen(p) + 1;   // jump to next entry
    }
    // case: PICC reader not found (critical)
    if (!reader) {
        reader = mszReaders;
        LOG_CRITICAL("No PICC reader found.\n");
        SCardReleaseContext(hContext);
        return 1;
    }

    // ensure you connected to ACR1581, this code is only tested with that reader
    const char *substring = "ACR1581";
    if (!(containsSubstring(reader, substring))) {
        LOG_CRITICAL("Your reader does not seem to be an ACR1581, cancelling program execution!");
        disconnectReader(hCard, hContext);
        return 1;
    }
    LOG_INFO("Connected to reader: %s\n", reader);

    // Turn off buzzer of ACR1581
    lRet = disableBuzzer(hContext, reader, &hCard, &dwActiveProtocol, pbRecvBuffer, &pbRecvBufferSize);
    if (lRet == 0) {
        LOG_INFO("Disabled buzzer of reader\n");
        SCardDisconnect(hCard, SCARD_LEAVE_CARD);
        // SCardControl sets buffer size to 0 (the command returns 0 byte and it says the response buffer is of size 0, but we want to keep the actual info how large our buffer is!)
        pbRecvBufferSize = sizeof(pbRecvBuffer);
    } else {
        if (lRet == (int32_t)0x80100016) { // https://pcsclite.apdu.fr/api/group__ErrorCodes.html
            LOG_ERROR("Failed to disable buzzer of ACR1581: SCARD_E_NOT_TRANSACTED - An attempt was made to end a nonexistent transaction.\n");
            return 1;
        } else {
            LOG_ERROR("Failed to disable buzzer of ACR1581: 0x%x\n", (unsigned int)lRet);
            return 1;
        } 
    }

    // Connect to the first reader
    lRet = connectToReader(hContext, reader, &hCard, &dwActiveProtocol, FALSE);
    BOOL didPrintWarningAlready = FALSE;
    while (lRet != SCARD_S_SUCCESS) {
        if ((lRet == SCARD_E_NO_SMARTCARD) || (lRet == SCARD_W_REMOVED_CARD) || (lRet == SCARD_E_TIMEOUT)) {
            if (!didPrintWarningAlready) {
                LOG_WARN("Did not detect a connected tag / NFC chip, please hold one near the reader."); //  Error Code: %0lx\n", lRet
                didPrintWarningAlready = TRUE;
            }
        }
        else {
            if (!didPrintWarningAlready) {
                LOG_ERROR("Google this pcsc-lite error code: 0x%x\n", (unsigned int)lRet);
                didPrintWarningAlready = TRUE;
            }
        }
        
        // wait a bit and retry (maybe user is not holding a tag near the reader yet)
        SLEEP_CUSTOM(50); // milliseconds (don't put this value too low, it never worked for me with 1 ms)
        lRet = connectToReader(hContext, reader, &hCard, &dwActiveProtocol, FALSE);
    }
    LOG_INFO("Detected an NFC tag");

    // -------------- Interact with tag ---------------------------

    // Get UID of detected tag
    lRet = getUID(hCard, pbRecvBuffer, &pbRecvBufferSize, TRUE);
    if (lRet != SCARD_S_SUCCESS) {
        if (lRet == ACR_90_00_FAILURE) {
            LOG_ERROR("Failed to get UID of tag: %0lx\n", lRet);
            return 1;
        } else if (lRet == SCARD_E_INSUFFICIENT_BUFFER) {
            LOG_ERROR("Failed to get UID of tag because the buffer is insufficient!");
            return 1;
        }
        
        LOG_ERROR("Failed to get UID with unknown error code: %0lx", lRet);
        disconnectReader(hCard, hContext);
        return 1;
    }

    lRet = getStatus(&hCard, mszReaders, dwState, dwReaders, &dwActiveProtocol, pbRecvBuffer, &pbRecvBufferSize, TRUE, connectedTag);
    if (lRet != SCARD_S_SUCCESS) {
        LOG_WARN("Failed to get status of tag: 0x%x\n", (unsigned int)lRet);
        disconnectReader(hCard, hContext);
        return 1;
    }
    // getStatus uses SCardStatus, so we must restore actual buffer size again
    pbRecvBufferSize = sizeof(pbRecvBuffer);

    // only try to get RATS if currently connected tag is DESFIRE 8k (only desfire i have) or NTAG 424 DNA TT
    //      strcmp only reads (and compares) up to the \n terminator, so it doesn't matter what is in rest of array
    BOOL try_to_get_rats = FALSE;
    if (strcmp(connectedTag, "Mifare Desfire EV3 8k or NTAG 424 DNA TT") == 0) {
        try_to_get_rats = TRUE;
    }

    if (try_to_get_rats) {
        ApduResponse response = getATS_14443A(hCard, pbRecvBuffer, &pbRecvBufferSize, connectedTag);
        if (response.status != SCARD_S_SUCCESS) {
            LOG_WARN("Failed to get ATS of tag: 0x%x\n", (unsigned int)response.status);
            disconnectReader(hCard, hContext);
            return 1;
        }

        // it should now be decided which tag we are working with
        //printf("%s", connectedTag);
    }
    
    // ------------------------------ USAGE EXAMPLES -----------------------------

    // ----------------------- EM4423 -------------------------------------------
    // WRITE:
    //      BYTE Msg[4] = { 0x05, 0x04, 0x03, 0x04 };
    //      em_4423_write_page(Msg, 0x04, hCard, pbRecvBuffer, &pbRecvBufferSize);
    // READ ONE PAGE:
    //      em_4423_read_page(0x04, hCard, pbRecvBuffer, &pbRecvBufferSize);
    // READ ALL PAGES AT ONCE:
    //      em_4423_fastread(hCard, pbRecvBuffer, &pbRecvBufferSize);

    // ---------------------------------------------------------------------------
    // TODO:
    //  - get firmware (update if possible)
    // how to adjust getStatus() to be able to distinguish Mifare Ultralight, NTAG2xx, em4423, ...?

    // Clean up
    disconnectReader(hCard, hContext);
    return 0;
}
