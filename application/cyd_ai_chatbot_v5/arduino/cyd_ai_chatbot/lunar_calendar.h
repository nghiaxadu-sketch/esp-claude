// Vietnamese lunar calendar (am lich), computed on the board: lunar date with leap months, can chi of the
// year/month/day/hour, the 24 solar terms (tiet khi), auspicious hours (gio hoang dao) and the main
// solar/lunar holidays. Astronomical algorithm by Ho Ngoc Duc (new moons and sun longitude, UTC+7).
// Plain C++ (no Arduino headers) so it is unit-tested on a PC. Names are ASCII Vietnamese for the 5x7 font.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace lunar {

constexpr double kVietnamTimeZone = 7.0;

struct LunarDate {
    int day;
    int month;
    int year;
    bool leap;  // "thang nhuan"
};

// Julian day number of a Gregorian date, and back.
int32_t jdFromDate(int day, int month, int year);
void jdToDate(int32_t jd, int &day, int &month, int &year);

LunarDate fromSolar(int day, int month, int year, double timeZone = kVietnamTimeZone);
// False when the lunar date does not exist (e.g. a leap month that year does not have).
bool toSolar(const LunarDate &lunar, int &day, int &month, int &year, double timeZone = kVietnamTimeZone);

// Number of days in the lunar month that contains the given solar date (29 or 30).
int lunarMonthLength(int day, int month, int year, double timeZone = kVietnamTimeZone);

// 0 = Sunday ... 6 = Saturday.
int weekday(int32_t jd);
// "Chu nhat", "Thu hai", ...
const char *weekdayName(int weekday);
int daysInSolarMonth(int month, int year);

// Heavenly stems / earthly branches: "Giap".."Quy" (10) and "Ty".."Hoi" (12).
const char *canName(int index);
const char *chiName(int index);

// "Can Chi" text such as "Binh Ngo" written into buf (at least 16 bytes is enough).
void yearCanChi(int lunarYear, char *buf, size_t size);
void monthCanChi(int lunarMonth, int lunarYear, char *buf, size_t size);
void dayCanChi(int32_t jd, char *buf, size_t size);
// Can chi of the two-hour period that contains `hour` (0..23) on day jd ("Giap Ty" for 23:00-01:00...).
void hourCanChi(int32_t jd, int hour, char *buf, size_t size);

// Solar term (0..23, 0 = "Xuan phan") in effect at the start of day jd.
int solarTerm(int32_t jd, double timeZone = kVietnamTimeZone);
const char *solarTermName(int term);

// Auspicious hours of day jd as up to 6 branch indexes (0 = Ty); returns how many were written.
size_t hoangDaoHours(int32_t jd, int (&chiIndexes)[6]);
// Clock range of a branch's two hours, e.g. chi 0 -> "23-1", chi 6 -> "11-13".
void chiHourRange(int chiIndex, char *buf, size_t size);

// Holiday name for a date, or nullptr. Lunar holidays need the lunar date and its month length
// (for "Giao thua" on the last day of the 12th month).
const char *holiday(int solarDay, int solarMonth, const LunarDate &lunar, int lunarMonthDays);

}  // namespace lunar
