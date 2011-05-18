/*
 * gagt3.h
 */
#ifndef GAGT3_H
#define GAGT3_H

#include "grads.h"

#if 1
/* for GrADS 2.0 or later */
#include "gatypes.h"
typedef gadouble FLOAT;
#else
typedef float FLOAT;
#endif

int gaggt3(struct gagrid *pgr, FLOAT *gr, char *mask, const int d[]);
int gagt3open(const char *arg, struct gacmn *pcm);
int gagt3vopen(const char *arg, struct gacmn *pcm);
int gagt3options(const char *arg, struct gacmn *pcm);
#endif
