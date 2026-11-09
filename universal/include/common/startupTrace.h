// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#ifdef TWLM_STARTUP_TRACE
void startupTraceBegin(void);
void startupTraceMark(const char *phase);
void startupTraceEnd(void);
#else
static inline void startupTraceBegin(void) {}
static inline void startupTraceMark(const char *phase) { (void)phase; }
static inline void startupTraceEnd(void) {}
#endif
#ifdef __cplusplus
}
#endif
