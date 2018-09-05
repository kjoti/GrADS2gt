/*
 * gagt3.c -- add-on code for GrADS to handle the GTOOL3 format.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gtool3.h"
#include "gagt3.h"
#include "caltime.h"
#include "debug.h"

/* XXX: use galloc. */
#define malloc(siz) galloc(siz, "gagt3")

#define XINDEX 0
#define YINDEX 1
#define ZINDEX 2
#define TINDEX 3
#define EINDEX 4

#define PATH_DELIM '/'

#define NAME_SIZE  4096         /* size of 'name' in struct gafile */
#define TMPL_SIZE  NAME_SIZE
#define ABBRV_SIZE 16           /* size of 'abbrv' in struct gavar */

/* GTOPTION: calendar */
#define CAL_UNDEF -1
extern struct gamfcmn mfcmn;
static int gt_caltype = CAL_UNDEF;

/* GTOPTION: tdef */
struct opt_tdef {
    int num, yr, mo, dy, hr, mn, modur, mndur;
};
static int opt_tdef_flag;
static struct opt_tdef opt_tdef;

/* GTOPTION: edef */
static int opt_edef_num = 1;    /* the number of ensembles */
static char ename_format[16] = "%d"; /* printf-format for ensembles */

/*
 * Associated actions table.
 */
static int opt_calendar(const char *);
static int opt_tdefine(const char *);
static int opt_edefine(const char *);
struct gtoptions {
    char *name;
    int (*action)(const char *args);
};
static struct gtoptions opttab[] = {
    { "calendar", opt_calendar },
    { "tdef", opt_tdefine },
    { "edef", opt_edefine }
};


#ifndef HAVE_STRLCPY
/* strlcpy() first appeared in OpenBSD 2.4 */
size_t
strlcpy(char *dest, const char *src, size_t size)
{
    const char *s = src;

    while (size > 1 && (*dest++ = *s++))
        --size;

    if (size <= 1) {
        if (size == 1)
            *dest = '\0';

        while (*s++)
            ;
    }
    return (s - src - 1);
}
#endif /* !HAVE_STRLCPY */


static char *
strtr(char *str, const char *s1, const char *s2)
{
    char *head = str;
    char *pos;

    while (*str) {
        if ((pos = strchr(s1, *str)))
            *str = *(s2 + (pos - s1));
        str++;
    }
    return head;
}


static char *
filename(const char *str)
{
    const char *p = strrchr(str, PATH_DELIM);

    return p ? (char *)p + 1 : (char *)str;
}


/*
 * another gaprnt(@gauser.c) using stdarg.
 */
static void
my_gaprnt(int level, const char *fmt, ...)
{
    va_list ap;
    char buf[1024];

    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    gaprnt(level, buf);
    va_end(ap);
}


static char *
gt3strerror(void)
{
    static char msgbuf[256];

    if (GT3_copyLastErrorMessage(msgbuf, sizeof msgbuf) < 0)
        strlcpy(msgbuf, "No error in libgtool3", sizeof msgbuf);

    GT3_clearLastError();
    return msgbuf;
}


static void
reverse_levs(FLOAT vals[], int num)
{
    int i, j;
    FLOAT temp;

    for (i = 0, j = num - 1; i < num / 2; i++, j--) {
        temp = vals[i];
        vals[i] = vals[j];
        vals[j] = temp;
    }
}


static const char *
calendar_name(int calendar)
{
    const char *name;

    name = ct_calendar_name(calendar);
    return name ? name : "unknown";
}


/*
 * set GrADS global calendar if it is undefined.
 */
static int
set_global_calendar(int calendar)
{
    if (mfcmn.cal365 >= 0 && mfcmn.cal365 != calendar)
        return -1;

    mfcmn.cal365 = calendar;
    return 0;
}


/*
 * "calendar" option via gtoptions.
 */
static int
opt_calendar(const char *args)
{
    struct idict { const char *name; int val; };
    static struct idict ctab[] = {
        { "gregorian",      CALTIME_GREGORIAN },
        { "noleap",         CALTIME_NOLEAP    },
        { "365_day",        CALTIME_NOLEAP    },
        { "360_day",        CALTIME_360_DAY   },
        { "all_leap",       CALTIME_ALLLEAP   },
        { "366_day",        CALTIME_ALLLEAP   },
        { "julian",         CALTIME_JULIAN    },
        { "undef",          CAL_UNDEF }
    };

    int i;
    char *ch;

    ch = nxtwrd((char *)args);
    if (ch == NULL) {
        my_gaprnt(0, "current calendar type: %s\n",
                  gt_caltype == CAL_UNDEF
                  ? "(undefined)" : calendar_name(gt_caltype));
        return 0;
    }

    for (i = 0; i < sizeof ctab / sizeof(struct idict); i++)
        if (strcmp(ctab[i].name, ch) == 0)
            break;

    if (i == sizeof ctab / sizeof(struct idict)) {
        my_gaprnt(0, "Error: %s: Unknown calendar name\n", ch);
        return 1;
    }
    gt_caltype = ctab[i].val;
    return 0;
}


/*
 * "tdef" option via gtoptions.
 */
static int
opt_tdefine(const char *args)
{
    const int MISSING = -1000;
    char *ch, *pos;
    int num;
    struct dt dt1, dt2, tdef;
    int months, minutes;

    tdef.yr = tdef.mo = tdef.dy = MISSING;
    ch = (char *)args;

    ch = nxtwrd(ch);
    if (ch && strcmp(ch, "undef") == 0) {  /* clear tdef */
        opt_tdef_flag = 0;
        return 0;
    }

    if (ch == NULL
        || (pos = intprs(ch, &num)) == NULL
        || *pos != ' '
        || (ch = nxtwrd(ch)) == NULL
        || cmpwrd("linear", ch) == 0
        || (ch = nxtwrd(ch)) == NULL
        || (pos = adtprs(ch, &tdef, &dt1)) == NULL
        || *pos != ' '
        || dt1.yr == MISSING || dt1.mo == MISSING || dt1.dy == MISSING
        || (ch = nxtwrd(ch)) == NULL
        || (pos = rdtprs(ch, &dt2)) == NULL) {

        my_gaprnt(0, "Error: %s: Invalid arguments\n", args);
        return 1;
    }

    months  = (dt2.yr * 12) + dt2.mo;
    minutes = (dt2.dy * 1440) + (dt2.hr * 60) + dt2.mn;

    if (months == 0 && minutes == 0) {
        gaprnt(0, "Error: 0 time increment in tdef option");
        return 1;
    }
    opt_tdef.num = num;
    opt_tdef.yr  = dt1.yr;
    opt_tdef.mo  = dt1.mo;
    opt_tdef.dy  = dt1.dy;
    opt_tdef.hr  = dt1.hr;
    opt_tdef.mn  = dt1.mn;
    opt_tdef.modur = months;
    opt_tdef.mndur = minutes;

    opt_tdef_flag = 1;
    return 0;
}


/*
 * check edef-option (printf-format).
 *
 * ex.)
 *  assert(check_ename_format("%d") == 0);
 *  assert(check_ename_format("%02d") == 0);
 *  assert(check_ename_format("run%02d") == 0);
 *  assert(check_ename_format("%%run%02d") == 0);
 *  assert(check_ename_format("%%run%02d%%") == 0);
 *  assert(check_ename_format("%s") < 0);
 *  assert(check_ename_format("%d %s") < 0);
 *  assert(check_ename_format("%s %d") < 0);
 *  assert(check_ename_format("% d") < 0);
 */
static int
check_ename_format(const char *fmt)
{
    char *p = (char *)fmt;
    int count = 0;
    char *endptr;

    while (*p) {
        if (*p == '\\') {
            p += 2;
            continue;
        }

        if (*p != '%') {
            p++;
            continue;
        }

        /* skip '%' */
        ++p;

        /* skip width if any */
        strtol(p, &endptr, 0);
        p = endptr;

        switch (*p) {
        case 'd':
        case 'i':
        case 'o':
        case 'u':
        case 'x':
        case 'X':
            count++;
            break;
        case '%':
            break;
        default:
            return -1;
        }
        p++;
    }
    return count == 1 ? 0 : -1;
}


/*
 * "edef" option via gtoptions.
 */
static int
opt_edefine(const char *args)
{
    char *ch;
    int value;

    ch = nxtwrd((char *)args);
    ch = intprs(ch, &value);
    if (value < 1)
        value = 1;
    opt_edef_num = value;

    /* set a printf-format. */
    if ((ch = nxtwrd(ch))) {
        char fmt[16];

        assert(sizeof fmt == sizeof ename_format);
        getwrd(fmt, ch, sizeof fmt - 1);

        if (check_ename_format(fmt) < 0) {
            my_gaprnt(0, "%s: invalid edef argument.\n", fmt);
            return 1;
        }
        strcpy(ename_format, fmt);
    } else
        strcpy(ename_format, "%d");

    return 0;
}


/*
 * another version of deflin() @ gaddes.c
 */
static int
my_deflin(struct gafile *pfi, int dim, double v1, double v2)
{
#if 1
    FLOAT *vals;

    assert(v2 != 0.0);

    if ((vals = (FLOAT *)malloc(sizeof(FLOAT) * 6)) == NULL)
        return -1;

    *(vals+1) = v1 - v2;
    *(vals) = v2;
    *(vals+2) = -999.9;
    pfi->grvals[dim] = vals;
    *(vals+4) = -1.0 * ((v1 - v2) / v2);
    *(vals+3) = 1.0 / v2;
    *(vals+5) = -999.9;
    pfi->abvals[dim] = vals + 3;
    pfi->ab2gr[dim] = liconv;
    pfi->gr2ab[dim] = liconv;
    pfi->linear[dim] = 1;
    return 0;
#else
    /* test code */
    char buf[64];

    snprintf(buf, sizeof buf, "LINEAR %g %g", v1, v2);
    return deflin(buf, pfi, dim, 0);
#endif
}


/* 1.0, 2.0, 3.0,...  */
static int
dummy_level(struct gafile *pfi, int id)
{
    return my_deflin(pfi, id, 1.0, 1.0);
}


/*
 * another version of deflev() @ gaddes.c
 */
static int
my_deflev(struct gafile *pfi, int id, const double *values, int size)
{
    FLOAT *vals, *p;
    int i;

    assert(size > 1 && pfi->dnum[id] == size);

    if ((vals = (FLOAT *)malloc(sizeof(FLOAT) * (size + 5))) == NULL)
        return -1;

    p = vals;
    *p++ = (FLOAT)size;
    for (i = 0; i < size; i++)
        *p++ = (FLOAT)values[i];
    *p++ = -999.9;

    pfi->abvals[id] = vals;
    pfi->grvals[id] = vals;
    pfi->ab2gr[id]  = lev2gr;
    pfi->gr2ab[id]  = gr2lev;
    pfi->linear[id] = 0;

    return 0;
}


static int
is_shifted_axis(const GT3_Varbuf *var, int id, int gtaxid)
{
    const char *axis[] = { "AITM1", "AITM2", "AITM3" };
    char name[17];

    name[0] = '\0';
    (void)GT3_getVarAttrStr(name, sizeof name, var, axis[gtaxid]);
    if (id == XINDEX) {
        if (var->dimlen[2] > 1
            || strncmp(name, "GLON", 4) == 0
            || strncmp(name, "OCLON", 4) == 0)
            return 0;

        lowcas(name);
        return strstr(name, "lon") == NULL;
    } else {
        if (var->dimlen[2] > 1
            || strncmp(name, "GGLA", 4) == 0
            || strncmp(name, "GLAT", 4) == 0
            || strncmp(name, "OCLAT", 5) == 0)
            return 0;

        lowcas(name);
        return strstr(name, "lat") == NULL;
    }
    return 0;
}


/*
 * id: id of index used in GrADS.
 * gtaxid: id of index used in GTOOL3.
 *
 * NOTE: gtaxid is not always identical to id.
 */
static int
setup_dim(struct gafile *pfi, int id, int gtaxid)
{
    char axisname[17]; /* e.g., "GLON360"  */
    char itemname[8];  /* AITM[1-3], ASTR[1-3], AEND[1-3] */
    int rval;
    GT3_Dim *dim;
    int len, str, end;
    int uniform, reversed;


    assert(id >= 0 && id < 3);
    assert(gtaxid >= 0 && gtaxid < 3);

    /* get axis name */
    snprintf(itemname, sizeof itemname, "AITM%d", gtaxid + 1);
    (void)GT3_getVarAttrStr(axisname, sizeof axisname, pfi->gtvar, itemname);

    if (axisname[0] == '\0' || strcmp(axisname, "SFC1") == 0)
        return dummy_level(pfi, id);

    if ((dim = GT3_getDim(axisname)) == NULL) {
        my_gaprnt(0, "%s: GTOOL3 AXIS file not found\n", axisname);
        return 1;
    }

    /* get STR */
    snprintf(itemname, sizeof itemname, "ASTR%d", gtaxid + 1);
    rval = GT3_getVarAttrInt(&str, pfi->gtvar, itemname);
    assert(rval == 0);

    /* get END */
    snprintf(itemname, sizeof itemname, "AEND%d", gtaxid + 1);
    rval = GT3_getVarAttrInt(&end, pfi->gtvar, itemname);
    assert(rval == 0);

    len = end - str + 1;
    assert(len == pfi->gtvar->dimlen[gtaxid]);

    reversed = len > 1 && dim->values[0] > dim->values[1];
    uniform =  strncmp(axisname, "GLON", 4) == 0
        || strncmp(axisname, "OCLON", 5) == 0
        || strncmp(axisname, "GLAT", 4) == 0
        || strncmp(axisname, "NUMBER", 6) == 0;

    /*
     * setup dim
     */
    if (len <= 1 || (uniform && !reversed)) {
        double intval = (len <= 1)
            ? 1.0
            : dim->values[str] - dim->values[str - 1];

        rval = my_deflin(pfi, id, dim->values[str - 1], intval);
    } else
        rval = my_deflev(pfi, id, dim->values + str - 1, len);

    if (rval == 0) {
        if (id == XINDEX && dim->cyclic && dim->len == len + 1)
            pfi->wrap = 1;

        if (id == YINDEX && reversed)
            pfi->yrflg = 1;
    }

    GT3_freeDim(dim);
    return rval;
}


#if 0
static int
clipint(int val, int low, int up, int def, const char *msg)
{
    if (val < low || val > up) {
        my_gaprnt(0, "%s clipped: %d -> %d\n", msg, val, def);
        return def;
    }
    return val;
}
#endif


static int
my_tdef(struct gafile *pfi,
        int yr, int mo, int dy, int hh, int mn,
        int dmon, int tdur)
{
    FLOAT *vals;

    if ((vals = (FLOAT *)malloc(sizeof(FLOAT) * 8)) == NULL)
        return 1;

    vals[0] = yr;
    vals[1] = mo;
    vals[2] = dy;
    vals[3] = hh;
    vals[4] = mn;
    vals[5] = dmon;             /* duration (months) */
    vals[6] = tdur;             /* duration (minutes) */
    vals[7] = -999.9;

    free(pfi->grvals[TINDEX]);

    pfi->grvals[TINDEX] = vals;
    pfi->abvals[TINDEX] = vals;
    pfi->linear[TINDEX] = 1;
    return 0;
}


/*
 * Correct wrong duration set by "gtavr".
 * XXX: The command "gtavr" sets wrong DATE1 and DATE2.
 *
 * return value:
 *     0: not corrected.
 *     1: corrected.
 */
static int
correct_wrong_duration(GT3_Duration *dur, const GT3_Date *date1)
{
    if (dur->unit == GT3_UNIT_MON
        && (dur->value + 1) % 12 == 0
        && !(date1->day == 1 && date1->hour == 0
             && date1->min == 0 && date1->sec == 0)) {
        dur->unit = GT3_UNIT_YEAR;
        dur->value = (dur->value + 1) / 12;
        return 1;
    }

    if (dur->unit == GT3_UNIT_DAY
        && dur->value >= 359) {
        double f;
        int year;

        f = dur->value / 365.25;
        year = (int)(f + 0.5);

        if (fabs(1. - year / f) < 0.02) {
            dur->value = year;
            dur->unit = GT3_UNIT_YEAR;
            return 1;
        }
    }

#if 0
    if (dur->unit == GT3_UNIT_DAY
        && dur->value >= 27) {
        double f;
        int mon;

        f = dur->value / 30.;
        mon = (int)(f + 0.5);

        if (fabs(1. - mon / f) < 0.12) {
            dur->value = mon;
            dur->unit = GT3_UNIT_MON;
            return 1;
        }
    }
#endif

    if (dur->unit == GT3_UNIT_HOUR && dur->value >= (360 * 24 - 3)) {
        double f;
        int year;

        f = dur->value / (24. * 365.25);
        year = (int)(f + 0.5);

        if (fabs(1. - year / f) < 0.02) {
            dur->value = year;
            dur->unit = GT3_UNIT_YEAR;
            return 1;
        }
    }
    return 0;
}


static int
setup_tdef(struct gafile *pfi)
{
    GT3_HEADER head;
    GT3_Duration dur;
    int dmon, tdur;
    GT3_Date date1, date2;
    int date_missing = 0;


    if (GT3_readHeader(&head, pfi->gthist) < 0) {
        my_gaprnt(0, "%s\n", gt3strerror());
        return -1;
    }

    /*
     *  DATE1: Use "DATE1" instead of "DATE".
     */
    if (GT3_decodeHeaderDate(&date1, &head, "DATE1") < 0) {
        my_gaprnt(0, "%s\n", gt3strerror());

        gaprnt(0, "Using DATE instead...\n");

        if (GT3_decodeHeaderDate(&date1, &head, "DATE") < 0) {
            my_gaprnt(0, "%s\n", gt3strerror());
            return -1;
        }
        date_missing = 1;
    }

    if (!date_missing && GT3_decodeHeaderDate(&date2, &head, "DATE2") < 0) {
        my_gaprnt(0, "%s\n", gt3strerror());
        date_missing = 1;
    }

    dur.value = 0;

    if (!date_missing
        && GT3_calcDuration(&dur, &date1, &date2, pfi->calendar) < 0) {
        dur.value = 1;
        dur.unit = GT3_UNIT_HOUR;
    }

    if (dur.value == 0 && GT3_getNumChunk(pfi->gthist) > 1) {
        /*
         * SNAPSHOT DATA or missing DATE[12].
         * Check next chunk to determine the duration.
         */
        GT3_copyDate(&date2, &date1);

        GT3_next(pfi->gthist);
        GT3_readHeader(&head, pfi->gthist);
        GT3_decodeHeaderDate(&date2, &head, "DATE");
        GT3_rewind(pfi->gthist);

        if (GT3_calcDuration(&dur, &date1, &date2, pfi->calendar) < 0) {
            my_gaprnt(0, "%s\n", gt3strerror());

            dur.value = 1;
            dur.unit = GT3_UNIT_HOUR;
        }

        if (dur.value == 0) {
            /*
             *  Restart file??
             */
            gaprnt(0, "Time-duration is zero. Restart file?\n");
        }
    }

    if (correct_wrong_duration(&dur, &date1) == 1)
        gaprnt(0, "Warning: Time-duration corrected.\n");

    if (dur.value <= 0) {
        dur.value = 1;
        dur.unit = GT3_UNIT_HOUR;
    }

    /*
     * convert time-duration from 'GT3_Duration' to 'GrADS style'.
     */
    dmon = 0;
    tdur = 0;
    switch (dur.unit) {
    case GT3_UNIT_YEAR:
        dmon = 12 * dur.value;
        break;
    case GT3_UNIT_MON:
        dmon = dur.value;
        break;
    case GT3_UNIT_DAY:
        tdur = 24 * 60 * dur.value;
        break;
    case GT3_UNIT_HOUR:
        tdur = 60 * dur.value;
        break;
    case GT3_UNIT_MIN:
        tdur = dur.value;
        break;
    case GT3_UNIT_SEC:
        tdur = 1;               /* unsupported in GrADS */
        break;
    }

    return my_tdef(pfi,
                   date1.year,
                   date1.mon,
                   date1.day,
                   date1.hour,
                   date1.min,
                   dmon,
                   tdur);
}


static void
set_missing_value(struct gafile *pfi, double miss)
{
    pfi->undef = miss;

    pfi->ulow = fabs(pfi->undef / EPSILON);
    pfi->uhi = pfi->undef + pfi->ulow;
    pfi->ulow = pfi->undef - pfi->ulow;
}


static void
set_ensemble(struct gaens *ens, int num,
             int time_len, const gadouble *vals)
{
    int i;

    snprintf(ens->name, 15, ename_format, num);
    ens->length = time_len;
    ens->gt = 1;
    gr2t(vals, 1, &ens->tinit);

    for (i = 0; i < 4; i++)
        ens->grbcode[i] = -999;
}


static void
reset_ensemble(void)
{
    opt_edef_num = 1;
    strcpy(ename_format, "%d");
}


static void
set_var_default(struct gavar *pvar)
{
    int i;

    pvar->varnm[0] = '\0';
    pvar->abbrv[0] = '\0';
    pvar->longnm[0] = '\0';
    for (i = 0; i < 16; i++)
        pvar->units[i] = -999;
    pvar->offset   = 0;         /* unused in gtool3 */
    pvar->recoff   = 0;         /* unused in gtool3 */
    pvar->ncvid    = -999;
    pvar->sdvid    = -999;
    pvar->h5vid    = -999;
    pvar->levels   = 0;
    pvar->dfrm     = 0;
    pvar->var_t    = 0;
    pvar->scale    = 1.;
    pvar->add      = 0.;
    pvar->vecpair  = -999;      /* version 1.9 */
    pvar->isu      = 0;         /* version 1.9 */
    pvar->isdvar   = 0;         /* version 2.0 */
    pvar->nvardims = 0;         /* version 2.0 */
#if USEHDF5==1
    pvar->h5varflg = -999;
    pvar->dataspace = -999;
#endif
    /* pvar->var_z    = 1; */   /* removed in 2.0 */
    /* pvar->y_x      = 0; */   /* removed in 2.0 */
}


/*
 * allocate a "struct gavar" and fill it with GT3_Varbuf *.
 */
static struct gavar *
get_pvar(const GT3_Varbuf *var, const char *alias)
{
    struct gavar *pvar;
    char title[33], unit[17];
    int gtaxis;

    if ((pvar = (struct gavar *)malloc(sizeof(struct gavar))) == NULL)
        return NULL;

    /*
     * default settings
     */
    set_var_default(pvar);

    /*
     * variable description
     */
    GT3_getVarAttrStr(title, sizeof title, var, "TITLE");
    GT3_getVarAttrStr(unit,  sizeof unit,  var, "UNIT");
    snprintf(pvar->varnm, sizeof pvar->varnm, "%s [%s]", title, unit);

    /* set abbrv. name */
    strlcpy(pvar->abbrv,
            (alias && alias[0] != '\0')
            ? alias             /* with alias */
            : filename(var->fp->path),
            sizeof pvar->abbrv);

    strtr(pvar->abbrv, ".+-", "___");
    lowcas(pvar->abbrv);

    /*
     * XXX pvar->levels is not always identical to var->dimlen[2].
     */
    gtaxis = 0;
    if (!is_shifted_axis(var, XINDEX, gtaxis))
        gtaxis++;
    if (!is_shifted_axis(var, YINDEX, gtaxis))
        gtaxis++;

    if (var->dimlen[gtaxis] > 1)
        pvar->levels = var->dimlen[gtaxis];

    return pvar;
}


static void
set_mask(char *mask, const FLOAT *data, FLOAT miss, size_t size)
{
    int i;

    for (i = 0; i < size; i++)
        mask[i] = (data[i] == miss) ? 0 : 1;
}


static int
gaggt3_xy(struct gagrid *pgr, FLOAT *gr, char *mask, int zpos)
{
    struct gafile *pfi = pgr->pfile;
    GT3_Varbuf *var = pfi->gtvar;
    int i, j, ii, jj;
    int cyc_off = 0;
    float  *fptr = (float *)var->data;
    double *dptr = (double *)var->data;
    FLOAT *start;

    if (GT3_readVarZ(var, zpos) < 0) {
        my_gaprnt(0, "%s\n", gt3strerror());
        return 1;
    }

    /* most simple case */
    if (pfi->yrflg == 0
        && pgr->dimmin[0] == 1 && pgr->dimmin[1] == 1
        && pgr->isiz == var->dimlen[0]
        && pgr->jsiz == var->dimlen[1]) {

        GT3_copyVarDouble(gr, pgr->isiz * pgr->jsiz, var, 0, 1);
        set_mask(mask, gr, var->miss, pgr->isiz * pgr->jsiz);
        return 0;
    }

    if (pgr->dimmin[0] < 1 && pfi->wrap) {
        int width = var->dimlen[0];
        cyc_off = (-pgr->dimmin[0] / width  + 1) * width;
    }

    /*
     * for general case (i-dim can be wrapped).
     */
    start = gr;
    for (jj = 0; jj < pgr->jsiz; jj++) {
        j = pgr->dimmin[1] + jj - 1;

        if (j < 0 || j >= var->dimlen[1]) {
            for (i = 0; i < pgr->isiz; i++)
                *gr++ = pgr->undef;
            continue;
        }

        if (pfi->yrflg == 1)
            j = pfi->dnum[1] - j - 1; /* reverse */

        if (pfi->wrap == 0)
            /* not cyclic */
            for (ii = 0; ii < pgr->isiz; ii++) {
                i = pgr->dimmin[0] + ii - 1;

                *gr++ = (i < 0 || i >= var->dimlen[0])
                    ? pgr->undef
                    : ((var->type == GT3_TYPE_DOUBLE)
                       ? dptr[i + var->dimlen[0] * j]
                       : fptr[i + var->dimlen[0] * j]);
            }
        else
            for (ii = 0; ii < pgr->isiz; ii++) {
                i = (cyc_off + ii + pgr->dimmin[0] - 1) % var->dimlen[0];
                *gr++ = (var->type == GT3_TYPE_DOUBLE)
                    ? dptr[i + var->dimlen[0] * j]
                    : fptr[i + var->dimlen[0] * j];
            }
    }
    set_mask(mask, start, var->miss, gr - start);
    return 0;
}


static FLOAT
gaggt3_value(struct gagrid *pgr, const int d[])
{
    double rval;
    int stat, p[] = {0, 0, 0};

    switch (pgr->pfile->gtaxis_degen) {
    case 0:
        p[0] = d[0];
        p[1] = pgr->pfile->yrflg ? pgr->pfile->dnum[1] - d[1] - 1: d[1];
        p[2] = d[2];
        break;
    case 1:
        p[0] = pgr->pfile->yrflg ? pgr->pfile->dnum[1] - d[1] - 1: d[1];
        p[1] = d[2];
        p[2] = 0;
        break;
    case 2:
        p[0] = d[0];
        p[1] = d[2];
        p[2] = 0;
        break;
    case 3:
        p[0] = d[2];
        p[1] = 0;
        p[2] = 0;
        break;
    }

    stat = GT3_readVar(&rval, pgr->pfile->gtvar, p[0], p[1], p[2]);
    if (stat < 0) {
        my_gaprnt(0, "%s\n", gt3strerror());
        return pgr->undef;
    }
    return (FLOAT)rval;
}


static void
expand_tmplat(char *path, const struct gafile *pfi, int tidx, int eidx)
{
    struct dt dtim, dtimi;
    gaint flag;

    assert(pfi->tmplat);

    gr2t(pfi->grvals[TINDEX], tidx + 1.0, &dtim);
    gr2t(pfi->grvals[TINDEX], 1.0, &dtimi);

    gafndt_impl(path, TMPL_SIZE,
                pfi->gtvlist[pfi->gtcurr],
                &dtim, &dtimi, pfi->abvals[3], NULL,
                pfi->ens1, 1, eidx + 1, &flag);
}


static int
cmp_vardim(const GT3_Varbuf *var1, const GT3_Varbuf *var2)
{
    return var1->dimlen[0] != var2->dimlen[0]
        || var1->dimlen[1] != var2->dimlen[1]
        || var1->dimlen[2] != var2->dimlen[2];
}


/*
 * XXX: 'tindx' and 'eidx' are starting with zero.
 *
 */
static int
seek_time(struct gafile *pfi, int tidx, int eidx)
{
    if (tidx < 0 || tidx >= pfi->dnum[TINDEX]) {
        debug1("t=%d: out of range", tidx);
        return -1;
    }
    if (eidx < 0 || eidx >= pfi->dnum[EINDEX]) {
        debug1("e=%d: out of range", eidx);
        return -1;
    }

    /*
     * template support.
     */
    if (pfi->tmplat
        && (pfi->fnumc != pfi->fnums[tidx] || pfi->fnume != eidx)) {
        char newpath[NAME_SIZE];
        GT3_File *newhist;
        GT3_Varbuf *newvar;

        expand_tmplat(newpath, pfi, tidx, eidx);

        if ((newhist = GT3_openHistFile(newpath)) == NULL
            || (newvar = GT3_getVarbuf(newhist)) == NULL) {
            my_gaprnt(0, "%s\n", gt3strerror());
            return -1;
        }

        if (pfi->gtvar && cmp_vardim(pfi->gtvar, newvar)) {
            /* uum... */
            my_gaprnt(0, "%s: dimension size has changed\n", newpath);
            my_gaprnt(0, "  (%d %d %d) -> (%d,%d,%d)\n",
                      pfi->gtvar->dimlen[0],
                      pfi->gtvar->dimlen[1],
                      pfi->gtvar->dimlen[2],
                      newvar->dimlen[0],
                      newvar->dimlen[1],
                      newvar->dimlen[2]);

            GT3_freeVarbuf(newvar);
            GT3_close(newhist);
            return -1;
        }

        /*
         * Ok. replace a gtool3 file.
         */
        debug1("%s: new gtool3 file", newpath);
        GT3_freeVarbuf(pfi->gtvar);
        GT3_close(pfi->gthist);
        pfi->gthist = newhist;
        pfi->gtvar  = newvar;

        /* */
        set_missing_value(pfi, pfi->gtvar->miss);

        pfi->fnumc = pfi->fnums[tidx];
        pfi->fnume = eidx;
    }

    if (pfi->tmplat)
        tidx -= pfi->fnumc;

    return GT3_seek(pfi->gthist, tidx, SEEK_SET);
}


static int
switch_active_var(struct gafile *pfi, const struct gavar *pvar)
{
    int pos;

    pos = pvar - pfi->pvar1;
    assert(pos >= 0 && pos < pfi->vnum);
    if (pos == pfi->gtcurr)
        return 0;               /* nothing to do */

    if (!pfi->tmplat) {
        char *newpath = pfi->gtvlist[pos];
        GT3_File *newhist;
        GT3_Varbuf *newvar;

        if ((newhist = GT3_openHistFile(newpath)) == NULL
            || (newvar = GT3_getVarbuf(newhist)) == NULL) {
            my_gaprnt(0, "%s\n", gt3strerror());
            return -1;
        }

        /*
         * Ok. replace a gtool3 file.
         */
        debug1("%s: new gtool3 file", newpath);
        GT3_freeVarbuf(pfi->gtvar);
        GT3_close(pfi->gthist);
        pfi->gthist = newhist;
        pfi->gtvar  = newvar;

        set_missing_value(pfi, pfi->gtvar->miss);
    } else {
        /*
         * invalidate. 'gthist' and 'gtvar' are set later.
         */
        GT3_freeVarbuf(pfi->gtvar);
        GT3_close(pfi->gthist);
        pfi->gthist = NULL;
        pfi->gtvar  = NULL;
        pfi->fnumc = -1;
        pfi->fnume = -1;
    }

    /* all passed */
    pfi->gtcurr = pos;
    return 0;
}


/*
 * gaggt3() is invorked from gaggrd().
 *
 * d[0..4]: X, Y, Z, T, E
 */
int
gaggt3(struct gagrid *pgr, FLOAT *gr, char *mask, const int d[])
{
    struct gafile *pfi = pgr->pfile;
    int i, ii, jj, pos[5];
    int width, wrapping, cyc_off;

    if (switch_active_var(pfi, pgr->pvar) != 0)
        return 1;
    if (seek_time(pfi, d[TINDEX] - 1, d[EINDEX] - 1) != 0)
        return 1;

    /*
     * XY plane
     */
    if (pgr->idim == XINDEX && pgr->jdim == YINDEX)
        return gaggt3_xy(pgr, gr, mask, d[ZINDEX] - 1);

    for (i = 0; i < 5; i++)
        pos[i] = d[i] - 1;

    /*
     * 0-Dim
     */
    if (pgr->idim == -1) {
        *gr = gaggt3_value(pgr, pos);
        *mask = (*gr  == pfi->undef) ? 0 : 1;
        return 0;
    }

    /*
     * generic case
     */
    wrapping = pgr->idim == XINDEX && pfi->wrap;
    width = pfi->dnum[0];
    if (wrapping && pos[XINDEX] < 0)
        cyc_off = (-pos[XINDEX] / width + 1) * width;
    else
        cyc_off = 0;

    assert(cyc_off >= 0);

    for (jj = 0; jj < pgr->jsiz; jj++) {
        if (pgr->jdim == TINDEX || pgr->jdim == EINDEX)
            if (seek_time(pfi, pos[TINDEX], pos[EINDEX]) != 0)
                return 1;

        for (ii = 0; ii < pgr->isiz; ii++) {
            pos[pgr->idim] = wrapping
                ? (cyc_off + d[0] - 1 + ii) % width
                : d[pgr->idim] - 1 + ii;

            if (pgr->idim == TINDEX || pgr->idim == EINDEX)
                if (seek_time(pfi, pos[TINDEX], pos[EINDEX]) != 0)
                    return 1;

            *gr = gaggt3_value(pgr, pos);
            *mask++ = (*gr  == pfi->undef) ? 0 : 1;
            gr++;
        }
        if (pgr->jdim >= 0)
            pos[pgr->jdim]++;
    }
    return 0;
}


/*
 * gt3ddes() fills a "struct gafile" with information
 * as well as "data description file".
 */
static int
gt3ddes(struct gafile *pfi, const char *alias)
{
    struct gavar *pvar;
    int i, rc, gtaxis;

    if ((pfi->gthist = GT3_openHistFile(pfi->name)) == NULL
        || (pfi->gtvar = GT3_getVarbuf(pfi->gthist)) == NULL) {

        my_gaprnt(0, "gtopen: %s\n", gt3strerror());
        return 1;
    }

    if (pfi->gthist->fmt == GT3_FMT_URC1)
        my_gaprnt(0, "Warning: %s:  URC version 1 is obsoleted.\n",
                  pfi->name);

    /*
     * [VARS ... ENDVARS]
     */
    if ((pvar = get_pvar(pfi->gtvar, alias)) == NULL)
        return 1;

    pfi->vnum = 1;
    pfi->pvar1 = pvar;

    pfi->gtrsv = 1;
    pfi->gtcurr = 0;
    pfi->gtvlist[0] = pfi->name;  /* XXX */

    /*
     * [OPTIONS]
     */
    if (gt_caltype != CAL_UNDEF) {
        pfi->calendar = gt_caltype;
        gt_caltype = CAL_UNDEF; /* reset */
    } else {
        int caltype;

        caltype = GT3_guessCalendarFile(pfi->name);
        if (caltype >= CALTIME_DUMMY || caltype < 0) {
            my_gaprnt(0, "Warning: %s: cannot guess Calendar.\n", pfi->name);
            caltype = CALTIME_GREGORIAN;
        }
        pfi->calendar = caltype;
    }
    if (set_global_calendar(pfi->calendar) == 0)
        my_gaprnt(0, "The global calendar is set to %s.\n",
                  calendar_name(pfi->calendar));
    else {
        my_gaprnt(0, "Warning: The calendar in this file is %s,\n"
                  "    but the global calendar (%s) remains unchanged.\n",
                  calendar_name(pfi->calendar),
                  calendar_name(mfcmn.cal365));
        my_gaprnt(0, "Use \"set dfile\" to change the global calendar.\n");
    }

    /*
     * [UNDEF]
     */
    set_missing_value(pfi, pfi->gtvar->miss);

    /*
     * [TITLE]  XXX name of dataset(not a variable).
     */
    GT3_getVarAttrStr(pfi->title, sizeof pfi->title, pfi->gtvar, "DSET");

    /*
     * [X, Y, and Z]
     */
    gtaxis = 0;
    pfi->gtaxis_degen = 0;

    /*
     * [XDEF]
     */
    if (!is_shifted_axis(pfi->gtvar, XINDEX, gtaxis)) {
        /* ok. AXIS1(in GTOOL) is longitude. */

        pfi->dnum[XINDEX] = pfi->gtvar->dimlen[gtaxis];
        if (setup_dim(pfi, XINDEX, gtaxis) != 0)
            my_deflin(pfi, XINDEX, .0, 360.0 / pfi->dnum[XINDEX]);

        gtaxis++;
    } else {
        pfi->dnum[XINDEX] = 1;
        dummy_level(pfi, XINDEX);
        pfi->gtaxis_degen |= 1 << XINDEX;
    }

    /*
     * [YDEF]
     */
    if (!is_shifted_axis(pfi->gtvar, YINDEX, gtaxis)) {
        /* ok. AXIS2(in GTOOL) is latitude. */

        pfi->dnum[YINDEX] = pfi->gtvar->dimlen[gtaxis];

        if ((rc = setup_dim(pfi, YINDEX, gtaxis)) != 0)
            my_deflin(pfi, YINDEX, -90.0, 180.0 / pfi->dnum[YINDEX]);

        /* XXX for north-to-south */
        if (rc == 0 && pfi->yrflg == 1) {
            /* GrADS assumes that YDEF is northwards */
            reverse_levs(pfi->abvals[YINDEX] + 1, pfi->dnum[YINDEX]);
        }
        gtaxis++;
    } else {
        pfi->dnum[YINDEX] = 1;
        dummy_level(pfi, YINDEX);
        pfi->gtaxis_degen |= 1 << YINDEX;
    }

    /*
     * [ZDEF]
     */
    pfi->dnum[ZINDEX] = pfi->gtvar->dimlen[gtaxis];
    if (setup_dim(pfi, ZINDEX, gtaxis) != 0)
        dummy_level(pfi, ZINDEX);  /* XXX */

    /*
     * [TDEF]
     */
    if (opt_tdef_flag) {
        pfi->dnum[TINDEX] = opt_tdef.num;
        if (my_tdef(pfi,
                    opt_tdef.yr, opt_tdef.mo, opt_tdef.dy,
                    opt_tdef.hr, opt_tdef.mn,
                    opt_tdef.modur, opt_tdef.mndur) < 0) {

            gaprnt(0, "TDEF setting failed\n");
            return 1;
        }
        opt_tdef_flag = 0; /* reset */
    } else {
        pfi->dnum[TINDEX] = GT3_getNumChunk(pfi->gthist);
        if (setup_tdef(pfi) != 0) {
            gaprnt(0, "TDEF setting failed\n");
            return 1;
        }
    }

    /*
     * [EDEF] ...ensembles.
     */
    pfi->dnum[EINDEX] = opt_edef_num;
    my_deflin(pfi, EINDEX, 1., 1.);
    if ((pfi->ens1 = malloc(sizeof(struct gaens)
                            * pfi->dnum[EINDEX])) == NULL)
        return 1;
    for (i = 0; i < pfi->dnum[EINDEX]; i++)
        set_ensemble(pfi->ens1 + i,
                     i + 1,
                     pfi->dnum[TINDEX],
                     pfi->grvals[TINDEX]);

    /* XXX: reset the number of ensembles. */
    reset_ensemble();

    /*
     * etc...
     */
    pfi->gsiz = pfi->dnum[0] * pfi->dnum[1];
    strcpy(pfi->dnam, pvar->abbrv); /* dnam: unused in gtool3 */
    pfi->rbuf = NULL;           /* unused in gtool3 */

    return 0;
}


static int
gt3ddes_tmpl(struct gafile *pfi, const char *tmpl, int ntime)
{
    char expnd[2][TMPL_SIZE];           /* expanded pathname */
    struct dt tdefi, tdef;
    int fnum, i, cr, flag;

    assert(tmpl[0] != '\0');

    pfi->tmplat = 1;
    pfi->dnum[3] = ntime;
    strlcpy(pfi->name, tmpl, sizeof pfi->name);

    /*
     * create mapping table (fnums) of the tamplated files.
     */
    if ((pfi->fnums = (int *)malloc(sizeof(int) * ntime)) == NULL) {
        gaprnt(0, "Memory allocation Error\n");
        return 1;
    }
    expnd[0][0] = '\0';
    expnd[1][0] = '\0';
    gr2t(pfi->grvals[TINDEX], 1.0, &tdefi);

    fnum = 0;
    for (i = 0, cr = 0; i < ntime; i++, cr ^= 1) {
        gr2t(pfi->grvals[TINDEX], i + 1.0, &tdef);

        gafndt_impl(expnd[cr], TMPL_SIZE, tmpl,
                    &tdef, &tdefi, pfi->abvals[3], NULL,
                    pfi->ens1, 1, 1, &flag);

        if (strcmp(expnd[cr], expnd[cr ^ 1]) != 0)
            fnum = i;  /* filename has changed */

        /* XXX Meanings of "fnums" slightly differs from the orignal. */
        pfi->fnums[i] = fnum;
        debug2("template map: t=%d, fnum=%d", i + 1, fnum);
    }
    assert(pfi->fnums[0] == 0);

    pfi->fnumc = 0;             /* already "0" has opened */
    pfi->fnume = 0;
    return 0;
}


/*
 * parse arguments in a command "gtopen".
 */
static int
parse_openarg(const char *arg, char *path, char *alias,
              char *tmpl, int *ntmpl)
{
    char *ch = (char *)arg;

    /*
     * path (gtool3 filename)
     */
    getwrd(path, ch, NAME_SIZE - 1);
    if (path[0] == '\0') {
        /* no argument */
        gaprnt(0, "No argument for gt3open\n");
        return 1;
    }

    while ((ch = nxtwrd(ch)) != NULL) {
        debug1("parsing arguments: (%s)", ch);

        if (ch[0] == 'a' && ch[1] == 's' && isspace(ch[2])) {
            /* alias name */
            if ((ch = nxtwrd(ch)) == NULL) {
                gaprnt(0, "No alias name\n");
                return 1;
            }
            getwrd(alias, ch, ABBRV_SIZE - 1);
            continue;
        }

        /* template */
        if (tmpl[0] == '\0') {
            getwrd(tmpl, ch, TMPL_SIZE - 1);

            if ((ch = nxtwrd(ch)) == NULL || !isdigit(*ch)) {
                gaprnt(0, "# of timesteps should be specified\n");
                return 1;
            }
            ch = intprs(ch, ntmpl);
            if (*ntmpl < 2)
                my_gaprnt(0, "# of timesteps(%d) is less than two.\n", *ntmpl);
        }
    }
    return 0;
}


static void
default_world(struct gafile *pfi, struct gacmn *pcm)
{
    char pout[1024];

    if (pfi->type == 2 || pfi->wrap)
        gacmd("set lon 0 360", pcm, 0);
    else {
        sprintf(pout, "set x 1 %i", pfi->dnum[0]);
        gacmd(pout, pcm, 0);
    }

    if (pfi->type == 2) {
        gacmd("set lat -90 90", pcm, 0);
        gacmd("set lev 500", pcm, 0);
    } else {
        sprintf(pout, "set y 1 %i", pfi->dnum[1]);
        gacmd(pout, pcm, 0);

        /*mf --- set z to max if x or y = 1 970729 mf*/

        if ((pfi->type == 1 && pfi->dnum[2] >= 1)
            && ((pfi->dnum[0] == 1) || (pfi->dnum[1] == 1))) {

            sprintf(pout, "set z 1 %i", pfi->dnum[2]);
            gacmd(pout, pcm, 0);
        } else {
            gacmd("set z 1", pcm, 0);
        }
    }
    gacmd("set t 1", pcm, 0);
    gacmd("set e 1", pcm, 0);
}


/*
 * 'gtopen' command
 *
 * Invorked from gacmd() to open GTOOL3 files.
 */
int
gagt3open(const char *arg, struct gacmn *pcm)
{
    char alias[ABBRV_SIZE];
    char tmpl[TMPL_SIZE];       /* tmplate string */
    int ntime = 1;
    struct gafile *pfi, *pfio;

    assert(pcm != NULL);

    /* allocate struct gafile and fill with default setting */
    if ((pfi = getpfi()) == NULL
        || (pfi->gtvlist = (char **)malloc(sizeof(char *))) == NULL) {
        gaprnt(0, "Memory Allocation Error: On File Open\n");
        return 1;
    }

    /*
     * parse arguments
     */
    alias[0] = tmpl[0] = '\0';
    if (parse_openarg(arg, pfi->name, alias, tmpl, &ntime) != 0) {
        gaprnt(0, "Invalid arguments for gtopen\n");
        return 1;
    }

    /*
     * fill "strcut gafile" with the information in the GTOOL3 header.
     */
    if (gt3ddes(pfi, alias) != 0) {
        frepfi(pfi, 0);
        return 1;
    }
    if (tmpl[0] != '\0' && gt3ddes_tmpl(pfi, tmpl, ntime) != 0) {
        frepfi(pfi, 0);
        return 1;
    }

    pcm->fseq++;
    pfi->fseq = pcm->fseq;

    /*
     * copied from gaopen().
     */
    if (pcm->pfi1 == NULL) {
        pcm->pfi1 = pfi;
    } else {
        pfio = pcm->pfi1;
        while (pfio->pforw != NULL)
            pfio = pfio->pforw;
        pfio->pforw = pfi;
    }
    pfi->pforw = NULL;
    pcm->fnum++;

    if (pcm->fnum == 1) {
        pcm->pfid = pcm->pfi1;
        pcm->dfnum = 1;
    }

    my_gaprnt(2, "Data file %s is open as file %i\n",
              pfi->name, pcm->fnum);

    /* If first file open, set up some default dimension
       ranges for the user */
    if (pcm->fnum == 1)
        default_world(pfi, pcm);

    return 0;
}


static int
attach_variable(struct gafile *pfi, const struct gavar *pvar,
                const char *path)
{
    assert(pfi->vnum >= 1);

    if (pfi->vnum >= pfi->gtrsv) {
        struct gavar *temp;
        int newsize = 2 * pfi->gtrsv;
        size_t siz = sizeof(struct gavar) * newsize;
        char **temp2;
        size_t siz2 = sizeof(char *) * newsize;

        if ((temp = (struct gavar *)realloc(pfi->pvar1, siz)) == NULL
            || (temp2 = (char **)realloc(pfi->gtvlist, siz2)) == NULL) {

            gaprnt(0, "memory allocation error in attach_variable()\n");
            return 1;
        }

        pfi->gtrsv = newsize;
        assert(pfi->gtrsv > pfi->vnum);

        pfi->pvar1 = temp;
        pfi->gtvlist = temp2;
    }

    pfi->pvar1[pfi->vnum]   = *pvar;  /* XXX */
    pfi->gtvlist[pfi->vnum] = strdup(path);
    pfi->vnum++;

    return 0;
}


static int
attachable(GT3_Varbuf *var1, GT3_Varbuf *var2, const char *msg)
{
    enum { NO, YES };

    if (var1->dimlen[0] != var2->dimlen[0]
        || var1->dimlen[1] != var2->dimlen[1])
        return NO;

    if (var2->dimlen[2] > 1) {
        char name1[17], name2[17];

        GT3_getVarAttrStr(name1, sizeof name1, var1, "AITM3");
        GT3_getVarAttrStr(name2, sizeof name2, var2, "AITM3");

        if (var2->dimlen[2] > var1->dimlen[2])
            my_gaprnt(0, "%s: %s: the number of levels exceeds %d (%s)\n",
                      msg, name2, var1->dimlen[2], name1);

        if (name2[0] == '\0' || strcmp(name2, "NUMBER1000") == 0)
            return YES;

        if (var2->dimlen[2] < var1->dimlen[2])
            my_gaprnt(0, "%s: %s: subset of %s? ok?\n", msg, name2, name1);
    }

    return YES;
}


static struct gafile *
lookup_gtopened_last(struct gacmn *pcm)
{
    struct gafile *pfi = NULL;
    struct gafile *temp;

    for (temp = pcm->pfi1; temp; temp = temp->pforw)
        if (temp->gthist != NULL)
            pfi = temp;

    return pfi;
}


/*
 * 'vgtopen' commnad.
 *
 * Invorked from gacmd() to open GTOOL3 files.
 */
int
gagt3vopen(const char *arg, struct gacmn *pcm)
{
    char path[NAME_SIZE], alias[ABBRV_SIZE], tmpl[TMPL_SIZE];
    int ntime;
    GT3_File *hist;
    GT3_Varbuf *gtvar;
    struct gafile *pfi;
    int rval = 1;

    assert(pcm != NULL);

    /*
     * parse arguments
     */
    path[0] = alias[0] = tmpl[0] = '\0';
    if (parse_openarg(arg, path, alias, tmpl, &ntime) != 0) {
        gaprnt(0, "Invalid arguments for gtopen\n");
        return 1;
    }

    /*
     * temporary open
     */
    if ((hist = GT3_openHistFile(path)) == NULL
        || (gtvar = GT3_getVarbuf(hist)) == NULL) {

        my_gaprnt(0, "vgtopen: %s\n", gt3strerror());
        return 1;
    }

    if ((pfi = lookup_gtopened_last(pcm)) != NULL) {
        if (attachable(pfi->gtvar, gtvar, filename(path))) {
            struct gavar *pvar;

            /* ok attachable */
            pvar = get_pvar(gtvar, alias);
            rval = attach_variable(pfi, pvar, tmpl[0] != '\0' ? tmpl : path);
            free(pvar);
        } else
            my_gaprnt(0, "%s: not attachable\n", path);
    } else
        gaprnt(0, "No gtopened file\n");

    GT3_freeVarbuf(gtvar);
    GT3_close(hist);

    return rval;
}


/*
 * 'gtoptions' commnad.
 *
 * Invorked from gacmd() to set options.
 */
/* ARGSUSED */
int
gagt3options(const char *arg, struct gacmn *pcm)
{
    int i;
    char name[32];
    int (*action)(const char *) = NULL;

    if (arg == NULL)
        return 0;

    getwrd(name, (char *)arg, sizeof name - 1);
    for (i = 0; i < sizeof opttab / sizeof(struct gtoptions); i++) {
        if (strcmp(name, opttab[i].name) == 0) {
            action = opttab[i].action;
            break;
        }
    }
    if (!action) {
        my_gaprnt(0, "Error: gtoptions: %s: unkonwn option\n", name);
        return 1;
    }

    return action(arg);
}
