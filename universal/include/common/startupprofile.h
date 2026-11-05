#ifndef TWLM_STARTUP_PROFILE_H
#define TWLM_STARTUP_PROFILE_H

#if defined(TWLM_PROFILE_STARTUP) && CURRENT_SCREEN_MODE == 3
void startupProfileBegin();
void startupProfileMark(const char *phase);
void startupProfileEnd();
#else
inline void startupProfileBegin() {}
inline void startupProfileMark(const char *) {}
inline void startupProfileEnd() {}
#endif

#endif
