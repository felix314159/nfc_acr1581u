#include "logging.c"
#include "main.h"
#include "ndef.h"


BYTE* NewNDEF_SR_Text(const BYTE* text, BYTE text_len, size_t* out_total_size) {
    if (text_len > 252) { // payload_len is assumed to be one byte, i think this limitation is necessary to keep this function simple
        LOG_CRITICAL("Max text length supported by this function is 252 but you passed text_len: 0x%02x", text_len);
        return NULL;
    }

    const BYTE lang_code[2] = LANG_CODE_EN;
    const BYTE lang_len = 2;

    BYTE payload_len = 1 + lang_len + text_len; // 1 (STATUS byte) + 2 ('en') + len(payload)
    BYTE ndef_len = 7 + text_len;               // 1 (RECORD_HEADER) + 1 (TYPE_LENGTH) + 1 (payload_len) + 1 (RECORD_TYPE) + 1 (STATUS) + 2 (lang_len) + text_len = 7 + text_len

    // total size = struct + payload + 1 terminator byte
    size_t total_size = sizeof(NDEF_SR_Text) + text_len + 1;

    // round up to multiple of 4 bytes (should be easy to write to any tag if message is multiples of 4 bytes)
    size_t padded_size = (total_size + 3) & ~0x03; // TODO: what is this

    // allocate buffer
    BYTE* buffer = calloc(1, padded_size);
    if (buffer == NULL) {
        LOG_CRITICAL("Failed to allocate required amount of bytes 'padded_size'");
        return NULL;
    }

    NDEF_SR_Text* record = (NDEF_SR_Text*)buffer;
    record->tlv_header     = TLV_HEADER;
    record->ndef_length    = ndef_len;
    record->record_header  = RECORD_HEADER;
    record->type_length    = TYPE_LENGTH;
    record->payload_length = payload_len;
    record->record_type    = RECORD_TYPE;
    record->status         = STATUS;
    memcpy(record->lang_code, lang_code, lang_len);
    memcpy(record->payload, text, text_len);

    // add TLV Terminator (can't be part of struct unless i switch order of fields due to C limitation)
    buffer[sizeof(NDEF_SR_Text) + text_len] = TLV_TERMINATOR;

    if (out_total_size == NULL) {
        LOG_CRITICAL("Failed to set out_total_size");
        return NULL;
    }
    *out_total_size = padded_size; // set how many bytes (multiple of 4) this encoded ndef message consists of

    return buffer;
}
