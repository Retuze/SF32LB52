/*
 * Logging via printf. Include <stdio.h> before this header.
 * on firmware that links picolibc (stdio).
 *
 * Optional per .c:
 *   #define LOG_TAG "WIFI"     // omit = no "[tag]" segment in the line
 *
 * Threshold (default: show everything):
 *   #define LOG_LEVEL LOG_LEVEL_INFO   // emit INFO, WARN, ERROR, FATAL (not TRACE/DEBUG)
 *
 * Severity order (each line must be >= LOG_LEVEL to print):
 *   TRACE < DEBUG < INFO < WARN < ERROR < FATAL
 *
 * Macros: LOGT LOGD LOGI LOGW LOGE LOGF  —  LOG(...) is the same as LOGI(...) (info, gated).
 *
 * Typical MCU .c:
 *   #define LOG_TAG "BLE"
 *   #define LOG_LEVEL LOG_LEVEL_WARN
 *   #include <stdio.h>
 *   #include "log.h"
 */
#pragma once
#ifndef LOG_H
#define LOG_H

#include <stdio.h>

/** Threshold: only lines with severity >= LOG_LEVEL are emitted (preprocessor constant). */
#define LOG_LEVEL_TRACE  0
#define LOG_LEVEL_DEBUG  1
#define LOG_LEVEL_INFO   2
#define LOG_LEVEL_WARN   3
#define LOG_LEVEL_ERROR  4
#define LOG_LEVEL_FATAL  5
#define LOG_LEVEL_NONE   6 /* no graded log output */

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_TRACE
#endif

#ifdef LOG_TAG
#define _LOG_PRI(levch, fmt, ...) printf("[%s][" levch "] " fmt "\n", LOG_TAG, ##__VA_ARGS__)
#else
#define _LOG_PRI(levch, fmt, ...) printf("[" levch "] " fmt "\n", ##__VA_ARGS__)
#endif

#if (LOG_LEVEL_TRACE >= LOG_LEVEL)
#define LOGT(fmt, ...) _LOG_PRI("T", fmt, ##__VA_ARGS__)
#else
#define LOGT(fmt, ...) ((void)0)
#endif

#if (LOG_LEVEL_DEBUG >= LOG_LEVEL)
#define LOGD(fmt, ...) _LOG_PRI("D", fmt, ##__VA_ARGS__)
#else
#define LOGD(fmt, ...) ((void)0)
#endif

#if (LOG_LEVEL_INFO >= LOG_LEVEL)
#define LOGI(fmt, ...) _LOG_PRI("I", fmt, ##__VA_ARGS__)
#else
#define LOGI(fmt, ...) ((void)0)
#endif

#if (LOG_LEVEL_WARN >= LOG_LEVEL)
#define LOGW(fmt, ...) _LOG_PRI("W", fmt, ##__VA_ARGS__)
#else
#define LOGW(fmt, ...) ((void)0)
#endif

#if (LOG_LEVEL_ERROR >= LOG_LEVEL)
#define LOGE(fmt, ...) _LOG_PRI("E", fmt, ##__VA_ARGS__)
#else
#define LOGE(fmt, ...) ((void)0)
#endif

#if (LOG_LEVEL_FATAL >= LOG_LEVEL)
#define LOGF(fmt, ...) _LOG_PRI("F", fmt, ##__VA_ARGS__)
#else
#define LOGF(fmt, ...) ((void)0)
#endif

#define LOG(fmt, ...) LOGI(fmt, ##__VA_ARGS__)

#endif /* LOG_H */
