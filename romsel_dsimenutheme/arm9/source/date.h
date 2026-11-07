#ifndef DATE_H
#define DATE_H

#include <string>

/**
 * Get the current date formatted for the current language.
 * The returned text stays valid until the next date update.
 */
const std::string &getDate(void);

/**
 * Get the current time formatted for the top bar.
 * Includes the menu's leading zero and blinking colon. The returned text
 * stays valid until the next time update.
 */
const std::string &retTime(bool colonVisible);

#endif // DATE_H
