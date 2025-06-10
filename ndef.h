#ifndef NDEF_H
#define NDEF_H

#ifndef MAIN_H
#include "main.h"
#endif

#ifndef COMMON_H
#include "common.h"
#endif

#ifndef LOGGING_C
#include "logging.c"
#endif


// structs

// NDEF Container Structure:
//      03              START of container ("TLV Header")
//      09              NDEF_LEN: hex length of entire ndef from here on (not including FE)      
//      D1              RECORD_HEADER (fairly complex, it encodes: MB,ME,CF,SR,IL,TNF so you get 1101 0001)
//      01              TYPE_LENGTH (the type is e.g. T: text record)
//      05              PAYLOAD_LENGTH (STATUS + LANGUAGE CODE + PAYLOAD = 1 + 2 + len(payload)) [i think it's weird that TYPE does not count]
//      54              RECORD_TYPE T=0x54 (Text)
//      02              STATUS (UTF-8 with 2 char language code)
//      65 6E           LANGUAGE_CODE "en"
//      ...             payload
//      FE              END of container ("TLV Terminator")

//      Examples of NDEF container that holds english text record:
// "Hello, world!"
// 03 14 D1 01
// 10 54 02 65
// 6E 48 65 6C  (? H e l)
// 6C 6F 2C 20
// 77 6F 72 6C
// 64 21 FE     (d ! FE)

// "yo"
// 03 09 D1 01
// 05 54 02 65
// 6E 79 6F FE (? y o FE)

#define TLV_HEADER      0x03
#define RECORD_HEADER   0xD1
#define TYPE_LENGTH     0x01
#define RECORD_TYPE     0x54            // "T" (Text)
#define STATUS          0x02
#define LANG_CODE_EN    { 0x65, 0x6E }  // "en"
#define TLV_TERMINATOR  0xFE

typedef struct NDEF_SR_Text {
    BYTE tlv_header;        // constant
    BYTE ndef_length;
    BYTE record_header;     // here: constant
    BYTE type_length;       // here: constant
    BYTE payload_length;    
    BYTE record_type;       // here: constant T=0x54
    BYTE status;            // here: constant
    BYTE lang_code[2];      // here: constant as only "en" is used (4 char lang codes do exist, e.g. en-US)
    BYTE payload[];
    //BYTE tlv_terminator;    // constant, C is so annoying bruh why are u not allowed to put another byte
} NDEF_SR_Text;

// typedef struct NDEFShortRecord { // as described on page 7 of TS_RTD_Text_1.0.pdf
//     BYTE RECORD_INFO;               // i use short format, so 0x01
//     BYTE RECORD_NAME_LENGTH;
//     BYTE PAYLOAD_LENGTH;            // 1 byte (STATUS) + 2 byte (LANGUAGE_CODE) + PAYLOAD length
//     BYTE NDEF_TYPE;                 // i only use ShortRecord of type Text, so "T"=0x54
//     BYTE STATUS;                    // i use utf-8 with 2-digit language code, so 0x02
//     BYTE LANGUAGE_CODE[2];          // RFC5646 (https://datatracker.ietf.org/doc/html/rfc5646), ISO/IANA language code e.g. "en"=65 6E, "de"=64 65
//     BYTE PAYLOAD[];
// } NDEFShortRecord;


// methods
BYTE* NewNDEF_SR_Text(const BYTE* text, BYTE text_len, size_t* out_total_size);



#endif