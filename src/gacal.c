/*
 * gacal.c  -- calendar routines using caltime.c.
 *             (moved from gautil.c)
 */
#include "grads.h"
#include "gatypes.h"
#include "caltime.h"

extern struct gamfcmn mfcmn;


/* "struct dt" => "struct caltime". */
static void
set_cal(caltime *date, const struct dt *dt, int type)
{
    date->caltype = type < 0 ? 0 : type;

    date->year  = dt->yr;
    date->month = dt->mo - 1;
    date->day   = dt->dy - 1;
    date->sec   = 3600 * dt->hr + 60 * dt->mn;
}


void
timadd(const struct dt *dtim, struct dt *dto)
{
    caltime temp;

    set_cal(&temp, dto, mfcmn.cal365);

    temp.year += dtim->yr;
    ct_add_months(&temp, dtim->mo);
    ct_add_days(&temp, dtim->dy);
    ct_add_seconds(&temp, 3600 * dtim->hr + 60 * dtim->mn);

    dto->yr = temp.year;
    dto->mo = temp.month + 1;
    dto->dy = temp.day + 1;
    dto->hr = temp.sec / 3600;
    dto->mn = (temp.sec - 3600 * dto->hr) / 60;
}


void
timsub(const struct dt *dtim, struct dt *dto)
{
    caltime temp;

    set_cal(&temp, dtim, mfcmn.cal365);

    ct_add_seconds(&temp, -(3600 * dto->hr + 60 * dto->mn));
    ct_add_days(&temp, -dto->dy);
    ct_add_months(&temp, -dto->mo);
    temp.year -= dto->yr;

    dto->yr = temp.year;
    dto->mo = temp.month + 1;
    dto->dy = temp.day + 1;
    dto->hr = temp.sec / 3600;
    dto->mn = (temp.sec - 3600 * dto->hr) / 60;
}


/* returns in minutes */
int
timdif(const struct dt *dtim1, const struct dt *dtim2)
{
    caltime date1, date2;

    set_cal(&date1, dtim1, mfcmn.cal365);
    set_cal(&date2, dtim2, mfcmn.cal365);
    return 24 * 60 * ct_diff_days(&date2, &date1)
        +  (date2.sec - date1.sec) / 60;
}


/* returns in months */
int
timdif_in_month(const struct dt *dtim1, const struct dt *dtim2)
{
    return 12 * (dtim2->yr - dtim1->yr) + dtim2->mo - dtim1->mo;
}


gadouble
t2gr(const gadouble *vals, const struct dt *dtim)
{
    struct dt stim;
    gadouble mnincr, moincr, unit;
    gadouble rdiff;

    stim.yr = (int)(vals[0] + 0.1);
    stim.mo = (int)(vals[1] + 0.1);
    stim.dy = (int)(vals[2] + 0.1);
    stim.hr = (int)(vals[3] + 0.1);
    stim.mn = (int)(vals[4] + 0.1);
    moincr = vals[5];
    mnincr = vals[6];

    /*
     * If the increment for this conversion is days, hours, or minutes,
     * then we do our calculations in minutes.  If the increment is
     * months or years, we do our calculations in months.
     */
    if (mnincr > 0.1) {
        rdiff = timdif(&stim, dtim);
        unit = mnincr;
    } else {
        rdiff = 12.0 * (dtim->yr - stim.yr) + (dtim->mo - stim.mo);
        unit = moincr;

        if (dtim->dy != stim.dy
            || dtim->hr != stim.hr
            || dtim->mn != stim.mn) {
            caltime date;
            gadouble minutes;

            minutes = 24.0 * 60 * (dtim->dy - stim.dy)
                + 60 * (dtim->hr - stim.hr)
                + (dtim->mn - stim.mn);

            set_cal(&date, dtim, mfcmn.cal365);
            rdiff += minutes / (24.0 * 60 * ct_num_days_in_month(&date));
        }
    }

    return 1.0 + rdiff / unit;
}


void
gr2t(const gadouble *vals, gadouble gr, struct dt *dtim)
{
    struct dt stim;
    caltime date;
    gadouble mnincr, moincr, diff;

    stim.yr = (int)(vals[0] + 0.1);
    stim.mo = (int)(vals[1] + 0.1);
    stim.dy = (int)(vals[2] + 0.1);
    stim.hr = (int)(vals[3] + 0.1);
    stim.mn = (int)(vals[4] + 0.1);
    set_cal(&date, &stim, mfcmn.cal365);

    moincr = vals[5];
    mnincr = vals[6];

    /* Is it guaranteed always? */
    /* assert(moincr - (int)moincr == 0.0f); */
    /* assert(mnincr - (int)mnincr == 0.0f); */

    if (mnincr > 0.1) {
        diff = (gr - 1.0) * mnincr;
        diff += (diff > 0.0) ? 0.5 : -0.5;  /* for round */

        if (diff > 1e6 || diff < -1e6) {
            int days = (int)(diff / (24 * 60));

            ct_add_days(&date, days);
            diff -= 24 * 60 * days;
        }
        ct_add_seconds(&date, (int)diff * 60);
    } else {
        /* XXX mnincr is ignored... */
        int dmo;

        diff = (gr - 1.0) * moincr;

        /* round (sort of) */
        dmo = (diff < 0.0) ? diff - 0.9999 : diff + 0.0001;
        ct_add_months(&date, dmo);

        diff -= dmo;            /* get fractional month */
        if (diff >= 0.0001) {
            /* small fraction is ignored */

            diff *= ct_num_days_in_month(&date);  /* in days */
            diff *= 24 * 3600;  /* in seconds */
            ct_add_seconds(&date, (int)diff);
        }
    }

    dtim->yr = date.year;
    dtim->mo = date.month + 1;
    dtim->dy = date.day + 1;
    dtim->hr = date.sec / 3600;
    dtim->mn = (date.sec - 3600 * dtim->hr) / 60;
}


int
invalid_date(const struct dt *dtim)
{
    int type;

    type = mfcmn.cal365 < 0 ? 0 : mfcmn.cal365;

    return ct_verify_date(type, dtim->yr, dtim->mo, dtim->dy);
}


/*
 * return the day of year.
 */
int
get_day_of_year(const struct dt *dtim)
{
    caltime origin, curr;

    set_cal(&curr, dtim, mfcmn.cal365);

    origin = curr;
    origin.month = origin.day = origin.sec = 0;

    return ct_diff_days(&curr, &origin) + 1;
}
