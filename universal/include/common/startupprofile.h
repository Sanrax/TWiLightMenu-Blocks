#ifndef TWLM_STARTUP_PROFILE_H
#define TWLM_STARTUP_PROFILE_H

#if defined(TWLM_PROFILE_STARTUP) && (CURRENT_SCREEN_MODE == 0 || CURRENT_SCREEN_MODE == 1 || CURRENT_SCREEN_MODE == 2 || CURRENT_SCREEN_MODE == 3)
void startupProfileBegin(const char *filename = nullptr);
void startupProfileMark(const char *phase);
void startupProfileEnd();
#elif defined(TWLM_STARTUP_TRACE)
#include "common/startupTrace.h"
inline void startupProfileBegin(const char * = nullptr) { startupTraceBegin(); }
inline void startupProfileMark(const char *phase) { startupTraceMark(phase); }
inline void startupProfileEnd() { startupTraceEnd(); }
#else
inline void startupProfileBegin(const char * = nullptr) {}
inline void startupProfileMark(const char *) {}
inline void startupProfileEnd() {}
#endif

#endif
