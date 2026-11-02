#include "lunar_calendar.h"

#include <math.h>
#include <stdio.h>

namespace lunar {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDegToRad = kPi / 180.0;
constexpr double kSynodicMonth = 29.530588853;
constexpr double kNewMoonEpoch = 2415021.076998695;  // first new moon of 1900 (Julian day)
constexpr int32_t kGregorianStartJd = 2299161;       // 15 Oct 1582
constexpr int kMaxLeapSearch = 14;

const char *const kCan[10] = {"Giap", "At", "Binh", "Dinh", "Mau", "Ky", "Canh", "Tan", "Nham", "Quy"};
const char *const kChi[12] = {"Ty", "Suu", "Dan", "Mao", "Thin", "Ti", "Ngo", "Mui", "Than", "Dau", "Tuat", "Hoi"};
const char *const kWeekdays[7] = {"Chu nhat", "Thu hai", "Thu ba", "Thu tu", "Thu nam", "Thu sau", "Thu bay"};
// Solar terms every 15 degrees of sun longitude, starting at the March equinox.
const char *const kTerms[24] = {"Xuan phan",  "Thanh minh", "Coc vu",     "Lap ha",   "Tieu man",   "Mang chung",
                                "Ha chi",     "Tieu thu",   "Dai thu",    "Lap thu",  "Xu thu",     "Bach lo",
                                "Thu phan",   "Han lo",     "Suong giang", "Lap dong", "Tieu tuyet", "Dai tuyet",
                                "Dong chi",   "Tieu han",   "Dai han",    "Lap xuan", "Vu thuy",    "Kinh trap"};
// Auspicious hours by the day's branch (pattern repeats every 6 branches), one char per branch from Ty.
const char *const kHoangDao[6] = {"110100101100", "001101001011", "110011010010",
                                  "101100110100", "001011001101", "010010110011"};

struct DatedHoliday {
    uint8_t day;
    uint8_t month;
    const char *name;
};

const DatedHoliday kSolarHolidays[] = {
    {1, 1, "Tet Duong lich"},        {14, 2, "Le Tinh nhan"},       {8, 3, "Quoc te Phu nu"},
    {30, 4, "Giai phong mien Nam"},  {1, 5, "Quoc te Lao dong"},    {1, 6, "Quoc te Thieu nhi"},
    {2, 9, "Quoc khanh"},            {20, 10, "Phu nu Viet Nam"},   {20, 11, "Nha giao Viet Nam"},
    {25, 12, "Giang sinh"},
};

const DatedHoliday kLunarHolidays[] = {
    {1, 1, "Tet Nguyen Dan"}, {2, 1, "Tet Nguyen Dan"}, {3, 1, "Tet Nguyen Dan"},  {15, 1, "Ram thang Gieng"},
    {10, 3, "Gio To Hung Vuong"}, {15, 4, "Le Phat Dan"}, {5, 5, "Tet Doan Ngo"}, {15, 7, "Le Vu Lan"},
    {15, 8, "Tet Trung Thu"}, {23, 12, "Ong Cong Ong Tao"},
};

int32_t floorInt(double v)
{
    return (int32_t)floor(v);
}

// Julian day (with fraction) of the k-th new moon after the 1900 epoch.
double newMoon(int32_t k)
{
    const double t = k / 1236.85;
    const double t2 = t * t;
    const double t3 = t2 * t;
    double jd1 = 2415020.75933 + 29.53058868 * k + 0.0001178 * t2 - 0.000000155 * t3;
    jd1 += 0.00033 * sin((166.56 + 132.87 * t - 0.009173 * t2) * kDegToRad);
    const double m = 359.2242 + 29.10535608 * k - 0.0000333 * t2 - 0.00000347 * t3;
    const double mpr = 306.0253 + 385.81691806 * k + 0.0107306 * t2 + 0.00001236 * t3;
    const double f = 21.2964 + 390.67050646 * k - 0.0016528 * t2 - 0.00000239 * t3;
    double c1 = (0.1734 - 0.000393 * t) * sin(m * kDegToRad) + 0.0021 * sin(2 * kDegToRad * m);
    c1 = c1 - 0.4068 * sin(mpr * kDegToRad) + 0.0161 * sin(kDegToRad * 2 * mpr);
    c1 = c1 - 0.0004 * sin(kDegToRad * 3 * mpr);
    c1 = c1 + 0.0104 * sin(kDegToRad * 2 * f) - 0.0051 * sin(kDegToRad * (m + mpr));
    c1 = c1 - 0.0074 * sin(kDegToRad * (m - mpr)) + 0.0004 * sin(kDegToRad * (2 * f + m));
    c1 = c1 - 0.0004 * sin(kDegToRad * (2 * f - m)) - 0.0006 * sin(kDegToRad * (2 * f + mpr));
    c1 = c1 + 0.0010 * sin(kDegToRad * (2 * f - mpr)) + 0.0005 * sin(kDegToRad * (2 * mpr + m));
    double deltaT;
    if (t < -11) {
        deltaT = 0.001 + 0.000839 * t + 0.0002261 * t2 - 0.00000845 * t3 - 0.000000081 * t * t3;
    } else {
        deltaT = -0.000278 + 0.000265 * t + 0.000262 * t2;
    }
    return jd1 + c1 - deltaT;
}

// Sun longitude in radians [0, 2*pi) at Julian day jdn (UT).
double sunLongitude(double jdn)
{
    const double t = (jdn - 2451545.0) / 36525;
    const double t2 = t * t;
    const double m = 357.52910 + 35999.05030 * t - 0.0001559 * t2 - 0.00000048 * t * t2;
    const double l0 = 280.46645 + 36000.76983 * t + 0.0003032 * t2;
    double dl = (1.914600 - 0.004817 * t - 0.000014 * t2) * sin(kDegToRad * m);
    dl += (0.019993 - 0.000101 * t) * sin(kDegToRad * 2 * m) + 0.000290 * sin(kDegToRad * 3 * m);
    double l = (l0 + dl) * kDegToRad;
    l -= kPi * 2 * floor(l / (kPi * 2));
    return l;
}

int32_t newMoonDay(int32_t k, double tz)
{
    return floorInt(newMoon(k) + 0.5 + tz / 24);
}

// Sun longitude sector 0..11 (30 degrees each) at the start of the local day.
int sunSector(int32_t dayNumber, double tz)
{
    return (int)floorInt(sunLongitude(dayNumber - 0.5 - tz / 24) / kPi * 6);
}

// Start day of the lunar month that contains the winter solstice (month 11) of year yy.
int32_t lunarMonth11(int yy, double tz)
{
    const int32_t off = jdFromDate(31, 12, yy) - 2415021;
    const int32_t k = floorInt(off / kSynodicMonth);
    int32_t nm = newMoonDay(k, tz);
    if (sunSector(nm, tz) >= 9) {
        nm = newMoonDay(k - 1, tz);
    }
    return nm;
}

// Months after month 11 (start a11) until the first month without a major solar term (the leap month).
int leapMonthOffset(int32_t a11, double tz)
{
    const int32_t k = floorInt((a11 - kNewMoonEpoch) / kSynodicMonth + 0.5);
    int last;
    int i = 1;
    int arc = sunSector(newMoonDay(k + i, tz), tz);
    do {
        last = arc;
        ++i;
        arc = sunSector(newMoonDay(k + i, tz), tz);
    } while (arc != last && i < kMaxLeapSearch);
    return i - 1;
}

void join(char *buf, size_t size, const char *a, const char *b)
{
    snprintf(buf, size, "%s %s", a, b);
}

int mod(int32_t v, int m)
{
    const int r = (int)(v % m);
    return r < 0 ? r + m : r;
}

}  // namespace

int32_t jdFromDate(int day, int month, int year)
{
    const int32_t a = (14 - month) / 12;
    const int32_t y = year + 4800 - a;
    const int32_t m = month + 12 * a - 3;
    int32_t jd = day + (153 * m + 2) / 5 + 365 * y + y / 4 - y / 100 + y / 400 - 32045;
    if (jd < kGregorianStartJd) {
        jd = day + (153 * m + 2) / 5 + 365 * y + y / 4 - 32083;
    }
    return jd;
}

void jdToDate(int32_t jd, int &day, int &month, int &year)
{
    int32_t b, c;
    if (jd >= kGregorianStartJd) {
        const int32_t a = jd + 32044;
        b = (4 * a + 3) / 146097;
        c = a - (b * 146097) / 4;
    } else {
        b = 0;
        c = jd + 32082;
    }
    const int32_t d = (4 * c + 3) / 1461;
    const int32_t e = c - (1461 * d) / 4;
    const int32_t m = (5 * e + 2) / 153;
    day = (int)(e - (153 * m + 2) / 5 + 1);
    month = (int)(m + 3 - 12 * (m / 10));
    year = (int)(b * 100 + d - 4800 + m / 10);
}

LunarDate fromSolar(int day, int month, int year, double tz)
{
    const int32_t dayNumber = jdFromDate(day, month, year);
    const int32_t k = floorInt((dayNumber - kNewMoonEpoch) / kSynodicMonth);
    int32_t monthStart = newMoonDay(k + 1, tz);
    if (monthStart > dayNumber) {
        monthStart = newMoonDay(k, tz);
    }
    int32_t a11 = lunarMonth11(year, tz);
    int32_t b11 = a11;
    LunarDate out = {};
    if (a11 >= monthStart) {
        out.year = year;
        a11 = lunarMonth11(year - 1, tz);
    } else {
        out.year = year + 1;
        b11 = lunarMonth11(year + 1, tz);
    }
    out.day = (int)(dayNumber - monthStart + 1);
    const int diff = (int)floorInt((monthStart - a11) / 29.0);
    out.leap = false;
    out.month = diff + 11;
    if (b11 - a11 > 365) {
        const int leapDiff = leapMonthOffset(a11, tz);
        if (diff >= leapDiff) {
            out.month = diff + 10;
            out.leap = diff == leapDiff;
        }
    }
    if (out.month > 12) {
        out.month -= 12;
    }
    if (out.month >= 11 && diff < 4) {
        out.year -= 1;
    }
    return out;
}

bool toSolar(const LunarDate &lunar, int &day, int &month, int &year, double tz)
{
    int32_t a11, b11;
    if (lunar.month < 11) {
        a11 = lunarMonth11(lunar.year - 1, tz);
        b11 = lunarMonth11(lunar.year, tz);
    } else {
        a11 = lunarMonth11(lunar.year, tz);
        b11 = lunarMonth11(lunar.year + 1, tz);
    }
    const int32_t k = floorInt(0.5 + (a11 - kNewMoonEpoch) / kSynodicMonth);
    int off = lunar.month - 11;
    if (off < 0) {
        off += 12;
    }
    if (b11 - a11 > 365) {
        const int leapOff = leapMonthOffset(a11, tz);
        int leapMonth = leapOff - 2;
        if (leapMonth < 0) {
            leapMonth += 12;
        }
        if (lunar.leap && lunar.month != leapMonth) {
            return false;
        }
        if (lunar.leap || off >= leapOff) {
            off += 1;
        }
    } else if (lunar.leap) {
        return false;
    }
    const int32_t monthStart = newMoonDay(k + off, tz);
    jdToDate(monthStart + lunar.day - 1, day, month, year);
    return true;
}

int lunarMonthLength(int day, int month, int year, double tz)
{
    const int32_t dayNumber = jdFromDate(day, month, year);
    const int32_t k = floorInt((dayNumber - kNewMoonEpoch) / kSynodicMonth);
    int32_t start = newMoonDay(k + 1, tz);
    int32_t next = newMoonDay(k + 2, tz);
    if (start > dayNumber) {
        next = start;
        start = newMoonDay(k, tz);
    }
    return (int)(next - start);
}

int weekday(int32_t jd)
{
    return mod(jd + 1, 7);
}

const char *weekdayName(int wd)
{
    return kWeekdays[mod(wd, 7)];
}

int daysInSolarMonth(int month, int year)
{
    static const uint8_t kDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2) {
        const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
        return leap ? 29 : 28;
    }
    return kDays[mod(month - 1, 12)];
}

const char *canName(int index)
{
    return kCan[mod(index, 10)];
}

const char *chiName(int index)
{
    return kChi[mod(index, 12)];
}

void yearCanChi(int lunarYear, char *buf, size_t size)
{
    join(buf, size, canName(lunarYear + 6), chiName(lunarYear + 8));
}

void monthCanChi(int lunarMonth, int lunarYear, char *buf, size_t size)
{
    join(buf, size, canName(lunarYear * 12 + lunarMonth + 3), chiName(lunarMonth + 1));
}

void dayCanChi(int32_t jd, char *buf, size_t size)
{
    join(buf, size, canName((int)mod(jd + 9, 10)), chiName((int)mod(jd + 1, 12)));
}

void hourCanChi(int32_t jd, int hour, char *buf, size_t size)
{
    const int chi = ((hour + 1) / 2) % 12;
    // Hour 23 belongs to the next day's first period.
    const int32_t day = hour == 23 ? jd + 1 : jd;
    const int canTy = (int)mod((day - 1) * 2, 10);
    join(buf, size, canName(canTy + chi), chiName(chi));
}

int solarTerm(int32_t jd, double tz)
{
    return (int)floorInt(sunLongitude(jd - 0.5 - tz / 24) / kPi * 12) % 24;
}

const char *solarTermName(int term)
{
    return kTerms[mod(term, 24)];
}

size_t hoangDaoHours(int32_t jd, int (&chiIndexes)[6])
{
    const char *pattern = kHoangDao[mod(jd + 1, 12) % 6];
    size_t n = 0;
    for (int i = 0; i < 12 && n < 6; ++i) {
        if (pattern[i] == '1') {
            chiIndexes[n++] = i;
        }
    }
    return n;
}

void chiHourRange(int chiIndex, char *buf, size_t size)
{
    const int start = (chiIndex * 2 + 23) % 24;
    snprintf(buf, size, "%d-%d", start, (start + 2) % 24);
}

const char *holiday(int solarDay, int solarMonth, const LunarDate &lunar, int lunarMonthDays)
{
    if (!lunar.leap) {
        if (lunar.month == 12 && lunar.day == lunarMonthDays) {
            return "Giao thua";
        }
        for (const DatedHoliday &h : kLunarHolidays) {
            if (h.day == lunar.day && h.month == lunar.month) {
                return h.name;
            }
        }
    }
    for (const DatedHoliday &h : kSolarHolidays) {
        if (h.day == solarDay && h.month == solarMonth) {
            return h.name;
        }
    }
    return nullptr;
}

}  // namespace lunar
