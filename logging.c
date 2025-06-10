#ifndef LOGGING_C
#define LOGGING_C

#include <stdio.h>
#include <time.h>

#define LOG_FMT(level, fmt, ...) \
    do { \
        time_t t = time(NULL); \
        struct tm *lt = localtime(&t); \
        char timestr[20]; \
        strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", lt); \
        fprintf(stderr, "[%s] [%s] [%s:%d] " fmt "\n", timestr, level, __FILE__, __LINE__, ##__VA_ARGS__); \
    } while (0)

#define LOG_DEBUG(fmt, ...)   	LOG_FMT("DEBUG", fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)    	LOG_FMT("INFO", fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)    	LOG_FMT("WARN", fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...)   	LOG_FMT("ERROR", fmt, ##__VA_ARGS__)
#define LOG_CRITICAL(fmt, ...)	LOG_FMT("CRITICAL", fmt, ##__VA_ARGS__)

#endif