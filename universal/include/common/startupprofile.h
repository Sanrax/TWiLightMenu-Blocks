#ifndef TWLM_STARTUP_PROFILE_H
#define TWLM_STARTUP_PROFILE_H

#if defined(TWLM_PROFILE_STARTUP) && (CURRENT_SCREEN_MODE == 0 || CURRENT_SCREEN_MODE == 1 || CURRENT_SCREEN_MODE == 2 || CURRENT_SCREEN_MODE == 3)
void startupProfileBegin(const char *filename = nullptr);
void startupProfileMark(const char *phase);
void startupProfileEnd();
#else
inline void startupProfileBegin(const char * = nullptr) {}
inline void startupProfileMark(const char *) {}
inline void startupProfileEnd() {}
#endif

#endif
