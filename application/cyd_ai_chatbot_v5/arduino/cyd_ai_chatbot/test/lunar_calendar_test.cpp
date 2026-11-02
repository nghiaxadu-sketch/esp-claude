// PC test for the lunar calendar: known Tet dates, leap months, can chi, solar terms and round trips.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lunar_calendar.h"

namespace {

int failures = 0;

void check(bool ok, const char *what)
{
    if (!ok) {
        printf("FAIL: %s\n", what);
        ++failures;
    }
}

void checkLunar(int d, int m, int y, int ld, int lm, int ly, bool leap)
{
    const lunar::LunarDate l = lunar::fromSolar(d, m, y);
    char what[96];
    snprintf(what, sizeof(what), "%02d/%02d/%d -> %d/%d%s/%d (got %d/%d%s/%d)", d, m, y, ld, lm, leap ? "n" : "", ly,
             l.day, l.month, l.leap ? "n" : "", l.year);
    check(l.day == ld && l.month == lm && l.year == ly && l.leap == leap, what);
}

void checkText(const char *got, const char *want, const char *what)
{
    if (strcmp(got, want) != 0) {
        printf("FAIL: %s: got \"%s\", want \"%s\"\n", what, got, want);
        ++failures;
    }
}

}  // namespace

int main()
{
    // Julian day round trip and weekday (1 Jan 2000 was a Saturday).
    const int32_t jd2000 = lunar::jdFromDate(1, 1, 2000);
    check(jd2000 == 2451545, "jd of 2000-01-01");
    check(lunar::weekday(jd2000) == 6, "2000-01-01 is Saturday");
    for (int32_t jd = 2440000; jd < 2470000; jd += 37) {
        int d, m, y;
        lunar::jdToDate(jd, d, m, y);
        check(lunar::jdFromDate(d, m, y) == jd, "jd round trip");
    }

    // Tet (1/1 lunar).
    checkLunar(10, 2, 2024, 1, 1, 2024, false);
    checkLunar(29, 1, 2025, 1, 1, 2025, false);
    checkLunar(17, 2, 2026, 1, 1, 2026, false);
    checkLunar(22, 1, 2023, 1, 1, 2023, false);
    // Leap month 2 of 2023 started on 22 Mar 2023; leap month 6 of 2025 on 25 Jul 2025.
    checkLunar(22, 3, 2023, 1, 2, 2023, true);
    checkLunar(25, 7, 2025, 1, 6, 2025, true);
    checkLunar(24, 7, 2025, 30, 6, 2025, false);
    // Mid-autumn 2026 (15/8 lunar) is 25 Sep 2026; the day before Tet 2026 is 29/12 of 2025.
    checkLunar(25, 9, 2026, 15, 8, 2026, false);
    checkLunar(16, 2, 2026, 29, 12, 2025, false);

    // Lunar -> solar, including the leap month.
    int d, m, y;
    check(lunar::toSolar({1, 1, 2026, false}, d, m, y) && d == 17 && m == 2 && y == 2026, "toSolar Tet 2026");
    check(lunar::toSolar({1, 6, 2025, true}, d, m, y) && d == 25 && m == 7 && y == 2025, "toSolar leap 6/2025");
    check(!lunar::toSolar({1, 3, 2025, true}, d, m, y), "2025 has no leap month 3");
    for (int32_t jd = lunar::jdFromDate(1, 1, 2020); jd < lunar::jdFromDate(1, 1, 2032); ++jd) {
        int sd, sm, sy;
        lunar::jdToDate(jd, sd, sm, sy);
        const lunar::LunarDate l = lunar::fromSolar(sd, sm, sy);
        int rd, rm, ry;
        if (!lunar::toSolar(l, rd, rm, ry) || rd != sd || rm != sm || ry != sy) {
            printf("FAIL: round trip %02d/%02d/%d\n", sd, sm, sy);
            ++failures;
            break;
        }
    }

    // Can chi.
    char buf[24];
    lunar::yearCanChi(2024, buf, sizeof(buf));
    checkText(buf, "Giap Thin", "year 2024");
    lunar::yearCanChi(2026, buf, sizeof(buf));
    checkText(buf, "Binh Ngo", "year 2026");
    lunar::dayCanChi(jd2000, buf, sizeof(buf));
    checkText(buf, "Mau Ngo", "day 2000-01-01");
    lunar::monthCanChi(1, 2026, buf, sizeof(buf));
    checkText(buf, "Canh Dan", "month 1 of 2026");
    lunar::hourCanChi(jd2000, 0, buf, sizeof(buf));
    checkText(buf, "Nham Ty", "hour Ty of a Mau day");

    // Solar terms: winter solstice 2025 is 21 Dec, spring equinox 2026 is 20 Mar.
    checkText(lunar::solarTermName(lunar::solarTerm(lunar::jdFromDate(22, 12, 2025))), "Dong chi", "term 22/12/2025");
    checkText(lunar::solarTermName(lunar::solarTerm(lunar::jdFromDate(21, 3, 2026))), "Xuan phan", "term 21/3/2026");

    // Auspicious hours: 6 per day.
    int hours[6];
    check(lunar::hoangDaoHours(jd2000, hours) == 6, "6 auspicious hours");
    lunar::chiHourRange(0, buf, sizeof(buf));
    checkText(buf, "23-1", "Ty hours");

    // Holidays.
    const lunar::LunarDate tet = lunar::fromSolar(17, 2, 2026);
    checkText(lunar::holiday(17, 2, tet, 30), "Tet Nguyen Dan", "Tet holiday");
    const lunar::LunarDate eve = lunar::fromSolar(16, 2, 2026);
    checkText(lunar::holiday(16, 2, eve, lunar::lunarMonthLength(16, 2, 2026)), "Giao thua", "New Year's eve");
    checkText(lunar::holiday(2, 9, lunar::fromSolar(2, 9, 2026), 30), "Quoc khanh", "National day");
    check(lunar::holiday(3, 9, lunar::fromSolar(3, 9, 2026), 30) == nullptr, "ordinary day");
    check(lunar::daysInSolarMonth(2, 2024) == 29 && lunar::daysInSolarMonth(2, 2100) == 28, "February length");

    if (failures == 0) {
        printf("lunar_calendar: all tests passed\n");
        return 0;
    }
    printf("lunar_calendar: %d failures\n", failures);
    return 1;
}
