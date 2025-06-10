#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h> // contains usleep

#ifndef ACR_90_00_FAILURE
#define ACR_90_00_FAILURE ((LONG)0x13371337) // made up to signal that expected 90 00 was not returned by the reader
#endif

#ifndef SCARD_CTL_CODE
#define SCARD_CTL_CODE(code) (0x42000000 + (code)) // https://web.archive.org/web/20171027125417/https://pcsclite.alioth.debian.org/api/reader_8h.html
#endif

#ifndef SCARD_E_NO_SMARTCARD
#define SCARD_E_NO_SMARTCARD ((LONG)0x8010000C) // https://pcsclite.apdu.fr/api/group__ErrorCodes.html#gaaf69330d6d119872ef76ae81c6b826db
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef BYTE
#define BYTE unsigned char
#endif


// Platform-specific includes and defines

// ------------------- WINDOWS ------------------------------
#ifdef _WIN32
#include <windows.h> // contains Sleep
#include <winscard.h>
#include <wtypes.h>
#define SLEEP_CUSTOM(milliseconds) Sleep(milliseconds)

// ------------------- APPLE ------------------------------
#elif __APPLE__
#include <PCSC/winscard.h>
#include <PCSC/wintypes.h>
#define SLEEP_CUSTOM(milliseconds) usleep((milliseconds) * 1000)

#ifndef DWORD
#define DWORD uint32_t
#endif

#ifndef LONG
#define LONG int32_t // changing this long silences warnings but leads to pscd error messages not matching stuff like SCARD_E_NO_SMARTCARD anymore, so just ignore warnings
#endif

// ------------------- LINUX ------------------------------
#elif __linux__
#include <winscard.h>
#include <pcsclite.h>  // PCSC Lite for Linux
extern int usleep(__useconds_t microseconds);
#define SLEEP_CUSTOM(milliseconds) usleep((milliseconds) * 1000)

#endif // end of platform-specific stuff

#endif // COMMON_H
