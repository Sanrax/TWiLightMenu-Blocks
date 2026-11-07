#include "date.h"

#include "common/clockText.h"
#include "common/twlmenusettings.h"
#include "language.h"

namespace {
twlm::ClockTextCache clockText;
}

const std::string &getDate(void)
{
    return clockText.date(std::time(nullptr), STR_DATE_FORMAT);
}

const std::string &retTime(bool colonVisible)
{
    return clockText.time(std::time(nullptr), ms().show12hrClock ? STR_TIME_FORMAT_12 : STR_TIME_FORMAT_24, colonVisible);
}
