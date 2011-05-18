/*
 *  caltime.h
 */
#ifndef CALTIME__H
#define CALTIME__H

#ifdef __cplusplus
extern "C" {
#endif

struct caltime {
    int caltype;

    int year;
    int month, day;  /* starting with 0 */
    int sec;         /* seconds since midnight [0-86399] */
};

typedef struct caltime caltime;

/*
 *  calendar type
 */
enum {
    CALTIME_GREGORIAN,  /* proleptic_gregorian */
    CALTIME_NOLEAP,
    CALTIME_ALLLEAP,
    CALTIME_360_DAY,
    CALTIME_JULIAN,
    CALTIME_DUMMY
};

/*
 *  init & set
 */
int ct_init_caltime(caltime *date, int type, int yr, int mo, int dy);
int ct_set_date(caltime *date, int yr, int mo, int dy);
int ct_set_time(caltime *date, int hour, int min, int sec);

/*
 *  operators (+)
 */
caltime* ct_add_years(caltime *date, int year);
caltime* ct_add_months(caltime *date, int month);
caltime* ct_add_days(caltime *date, int day);
caltime* ct_add_hours(caltime *date, int hour);
caltime* ct_add_minutes(caltime *date, int min);
caltime* ct_add_seconds(caltime *date, int sec);

/*
 *  operators (==, <, >)
 */
int ct_cmp(const caltime *date1, const caltime *date2);
int ct_cmpto(const caltime *date, int, int, int, int, int, int);
int ct_eqdate(const caltime *date, int yr, int mo, int day);
int ct_equal(const caltime *date1, const caltime *date2);

/*
 *  difference
 */
int ct_diff_days(const caltime *date2, const caltime *date1);
double ct_diff_daysd(const caltime *date2, const caltime *date1);
double ct_diff_seconds(const caltime *date2, const caltime *date1);

/*
 *  utils
 */
int ct_verify_date(int type, int yr, int mo, int dy);
int ct_day_of_year(const caltime *date);
int ct_num_days_in_year(const caltime *date);
int ct_num_days_in_month(const caltime *date);
int ct_snprint(char *buf, size_t num, const caltime *date);

#ifdef __cplusplus
}
#endif
#endif /* !CALTIME__H */