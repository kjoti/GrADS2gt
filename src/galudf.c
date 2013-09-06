/*
 * galudf.c -- GrADS Legacy User Defined Function
 *
 * Copy from src/gafunc.c in version GrADS 1.9b4:
 *   ffuser()
 *   gafdef()
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
/* If autoconfed, only include malloc.h when it's presen */
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#else /* undef HAVE_CONFIG_H */
#include <malloc.h>
#endif /* HAVE_CONFIG_H */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "grads.h"

char *gxgnam(char *);       /* This is also in gx.h */

static struct gaufb *ufba = NULL;  /* Anchor for user function defs */
static char pout[256];   /* Build error msgs here */


/*
 * Write a record into the UDF transfer file.
 * Return 0 if successful end.
 */
static int
write_record(void *ptr, size_t size, int nelems, int is_seq, FILE *fp)
{
    int rsize = size * nelems;

    if (is_seq && fwrite(&rsize, sizeof(int), 1, fp) != 1)
        return -1;

    if (fwrite(ptr, size, nelems, fp) != nelems)
        return -1;

    if (is_seq && fwrite(&rsize, sizeof(int), 1, fp) != 1)
        return -1;

    return 0;
}


/*
 * Write float data (gadouble -> float).
 *
 * Return value:
 *    0: successful end
 *   -1: error
 *
 * Compatible with version GrADS 1.9 UDF.
 */
static int
write_float(gadouble *ptr, int nelems, int is_seq, FILE *fp)
{
    float *buf;
    int i, rval;

    if ((buf = malloc(sizeof(float) * nelems)) == NULL)
        return -1;

    for (i = 0; i < nelems; i++)
        buf[i] = (float)ptr[i];

    rval = write_record(buf, sizeof(float), nelems, is_seq, fp);
    free(buf);
    return rval;
}


/*
 * Read a record from the UDF transfer file .
 * Return 0 if successful end.
 */
static int
read_record(void *ptr, size_t size, int nelems, int is_seq, FILE *fp)
{
    int rsize = 0;

    if (is_seq && fread(&rsize, sizeof(int), 1, fp) != 1
        && rsize != size * nelems)
        return -1;

    if (fread(ptr, size, nelems, fp) != nelems)
        return -1;

    if (is_seq && fread(&rsize, sizeof(int), 1, fp) != 1
        && rsize != size * nelems)
        return -1;

    return 0;
}


/*
 * Read float data.
 */
static int
read_float(gadouble *ptr, size_t nelems, int is_seq, FILE *fp)
{
    float *buf;
    int i, rval;

    if ((buf = malloc(sizeof(float) * nelems)) == NULL)
        return -1;

    rval = read_record(buf, sizeof(float), nelems, is_seq, fp);
    if (rval == 0)
        for (i = 0; i < nelems; i++)
            ptr[i] = (gadouble)buf[i];

    free(buf);
    return rval;
}


/* Handle user specified function call.  The args are written
   out, the user's process is invoked, and the result is read
   back in.  */

gaint ffuser (struct gaufb *ufb, struct gafunc *pfc, struct gastat *pst) {
FILE *ifile,*ofile;
struct gagrid *pgr;
struct dt dtim;
gadouble (*conv) (gadouble *, gadouble);
gadouble *v;
float rvals[20];
int rc,iarg,siz,i;
char *ch,rec[80];

  /* Check number of args */

  if (pfc->argnum<ufb->alo || pfc->argnum>ufb->ahi) {
    sprintf (pout,"Error from %s: Too many or too few args\n",ufb->name);
    gaprnt (0,pout);
    return (1);
  }

  /* Open the transfer file */

  ofile = fopen(ufb->oname,"wb");
  if (ofile==NULL) {
    sprintf (pout,"Error from %s: Error opening transfer file\n",ufb->name);
    gaprnt (0,pout);
    sprintf (pout,"  File name: %s\n",ufb->oname);
    gaprnt (0,pout);
    return (1);
  }

  /* Write hearder record to transfer file */

  rvals[0] = (float)pfc->argnum;
  if (write_record(rvals, sizeof(float), 20, ufb->sflg, ofile) < 0)
      goto werr;

  /* Write args to the transfer file */

  for (iarg=0; iarg<pfc->argnum; iarg++) {

    /* Handle expression */

    if (ufb->atype[iarg]==1) {
      rc = gaexpr(pfc->argpnt[iarg],pst);         /* Evaluate      */
      if (rc) {                  /*mf ---- add fclose of the udf output file ---- mf*/
        fclose(ofile);
        return (rc);
      }
      if (pst->type!=1) {
        fclose(ofile);           /*mf ---- add fclose of the udf output file ---- mf*/
        gafree (pst);
        return(-1);
      }
      pgr = pst->result.pgr;                   /* Fill in header */
      rvals[0] = pgr->undef;
      rvals[1] = pgr->idim;
      rvals[2] = pgr->jdim;
      rvals[3] = pgr->isiz;
      rvals[4] = pgr->jsiz;
      rvals[5] = pgr->ilinr;
      rvals[6] = pgr->jlinr;
      if (pgr->idim>-1 && pgr->ilinr==1) {     /* Linear scaling info */
        if (pgr->idim==3) {
          gr2t (pgr->ivals,pgr->dimmin[3],&dtim);
          rvals[11] = dtim.yr;
          rvals[12] = dtim.mo;
          rvals[13] = dtim.dy;
          rvals[14] = dtim.hr;
          rvals[15] = dtim.mn;
          rvals[16] = *(pgr->ivals+6);      /*mf - make udf return time values as advertized mf*/
          rvals[17] = *(pgr->ivals+5);
          rvals[9] = -999.0;
          rvals[10] = -999.0;               /*mf - make udf return time values as advertized mf*/
        } else {
          conv = pgr->igrab;
          rvals[7] = conv(pgr->ivals,pgr->dimmin[pgr->idim]);
          rvals[8] = *(pgr->ivals);
        }
      }
      if (pgr->jdim>-1 && pgr->jlinr==1) {
        if (pgr->jdim==3) {
          gr2t (pgr->jvals,pgr->dimmin[3],&dtim);     /*mf - bug change pgr->ivals ot pgr->jvals mf*/
          rvals[11] = dtim.yr;
          rvals[12] = dtim.mo;
          rvals[13] = dtim.dy;
          rvals[14] = dtim.hr;
          rvals[15] = dtim.mn;
          rvals[16] = *(pgr->jvals+6);
          rvals[17] = *(pgr->jvals+5);
          rvals[9] = -999.0;
          rvals[10] = -999.0;
        } else {
          conv = pgr->jgrab;
          rvals[9] = conv(pgr->jvals,pgr->dimmin[pgr->jdim]);
          rvals[10] = *(pgr->jvals);
        }
      }

      /* Write header */
      if (write_record(rvals, sizeof(float), 20, ufb->sflg, ofile) < 0)
        goto werr;

      /* Write grid */
      siz = pgr->isiz * pgr->jsiz;
      if (write_float(pgr->grid, siz, ufb->sflg, ofile) < 0)
        goto werr;

      if (pgr->idim>-1) {                  /* write i dim scaling */
        v = pgr->grid;
        if (pgr->idim<3) {
          conv = pgr->igrab;
          for (i=pgr->dimmin[pgr->idim];i<=pgr->dimmax[pgr->idim];i++) {
            *v = conv(pgr->ivals,(gadouble)i);
            v++;
          }
        } else {
          for (i=pgr->dimmin[pgr->idim];i<=pgr->dimmax[pgr->idim];i++) {
            *v = (gadouble)i;
            v++;
          }
        }
        if (write_float(pgr->grid, pgr->isiz, ufb->sflg, ofile) < 0)
          goto werr;
      }
      if (pgr->jdim>-1) {                /* write j dim scaling */
        v = pgr->grid;
        if (pgr->jdim<3) {
          conv = pgr->jgrab;
          for (i=pgr->dimmin[pgr->jdim];i<=pgr->dimmax[pgr->jdim];i++) {
            *v = conv(pgr->jvals,(gadouble)i);
            v++;
          }
        } else {
          for (i=pgr->dimmin[pgr->jdim];i<=pgr->dimmax[pgr->jdim];i++) {
            *v = (gadouble)i;
            v++;
          }
        }
        if (write_float(pgr->grid, pgr->jsiz, ufb->sflg, ofile) < 0)
          goto werr;
      }
      gafree(pst);     /* Done with expr */
    }

    /* Handle Value */

    else if (ufb->atype[iarg]==2) {
      if (getflt(pfc->argpnt[iarg],rvals)==NULL) {
        sprintf (pout,"Error from %s: Invalid Argument\n",ufb->name);
        gaprnt (0,pout);
        sprintf (pout,"  Expecting arg %i to be a constant\n",iarg);
        gaprnt (0,pout);
      }
      if (write_record(rvals, sizeof(float), 1, ufb->sflg, ofile) < 0)
        goto werr;
    }

    /* Handle Character */

    else if (ufb->atype[iarg]==3) {
      ch = pfc->argpnt[iarg];
      for (i=0; i<80; i++) rec[i] = ' ';
      i = 0;
      while (*ch && i<80) {
        rec[i] = *ch;
        i++; ch++;
      }
      if (write_record(rec, 1, 80, ufb->sflg, ofile) < 0)
        goto werr;
    }
  }

  /* All the args are written.  Close the transfer file */

  fclose (ofile);

  /* Invoke the user's routine.  */

#ifdef XLIBEMU
  System (ufb->fname);  /*ams need to do housekeeping before system() ams*/
#else
  system (ufb->fname);
#endif

  /* Read the user's output file -- our input file. */

  ifile = fopen(ufb->iname,"rb");
  if (ifile==NULL) {
    sprintf (pout,"Error from %s: Error opening transfer file\n",ufb->name);
    gaprnt (0,pout);
    sprintf (pout,"  File name: %s\n",ufb->iname);
    gaprnt (0,pout);
    return (1);
  }

  /* Read the header record, which contains the return code */

  if (read_record(rvals, sizeof(float), 20, ufb->sflg, ifile) < 0)
    goto rerr;
  rc = (int)(*rvals+0.1);
  if (rc!=0) {
    fclose(ifile);   /*mf ---- add fclose of the udf input file ---- mf*/
    return (1);
  }

  /* If all is ok, read the grid header */

  if (read_record(rvals, sizeof(float), 20, ufb->sflg, ifile) < 0)
    goto rerr;

  /* Start building the gagrid block */

  pgr = (struct gagrid *)malloc(sizeof(struct gagrid));
  if (pgr==NULL) goto merr;

  /* Fill in and check values */

  pgr->alocf = 0;
  pgr->undef = rvals[0];
  pgr->idim = (int)(floor(rvals[1]+0.1));
  pgr->jdim = (int)(floor(rvals[2]+0.1));
  pgr->iwrld = 0; pgr->jwrld = 0;
  pgr->isiz = (int)(rvals[3]+0.1);
  pgr->jsiz = (int)(rvals[4]+0.1);
  pgr->ilinr = (int)(rvals[5]+0.1);
  pgr->jlinr = (int)(rvals[6]+0.1);
  for (i=0; i<4; i++) pgr->dimmin[i] = 1;
  if ( pgr->idim<-1 || pgr->idim>3 ) goto ferr;
  if ( pgr->jdim<-1 || pgr->jdim>3 ) goto ferr;
  if ( pgr->ilinr<0 || pgr->ilinr>1) goto ferr;
  if ( pgr->jlinr<0 || pgr->jlinr>1) goto ferr;
  if ( pgr->jdim>-1 && pgr->idim>pgr->jdim) goto derr;
  if ( pgr->idim==-1 && pgr->isiz!=1) goto ferr;
  if ( pgr->jdim==-1 && pgr->jsiz!=1) goto ferr;
  if ( pgr->isiz<1) goto ferr;
  if ( pgr->jsiz<1) goto ferr;
  for (i=0; i<4; i++) pgr->dimmin[i] = 1;
  if (pgr->idim>-1) pgr->dimmax[pgr->idim] = pgr->isiz;
  if (pgr->jdim>-1) pgr->dimmax[pgr->jdim] = pgr->jsiz;

  if (pgr->idim>-1 && pgr->idim!=pst->idim && pgr->idim!=pst->jdim) goto derr;
  if (pgr->jdim>-1 && pgr->jdim!=pst->idim && pgr->jdim!=pst->jdim) goto derr;

  /* Set up linear scaling info */

  if (pgr->idim>-1 && pgr->ilinr==1) {     /* Linear scaling info */
    if (pgr->idim==3) {
      v = (gadouble *)malloc(sizeof(gadouble)*8);
      if (v==NULL) goto merr;
      *v     = (gadouble)rvals[11];
      *(v+1) = (gadouble)rvals[12];
      *(v+2) = (gadouble)rvals[13];
      *(v+3) = (gadouble)rvals[14];
      *(v+4) = (gadouble)rvals[15];
      *(v+6) = (gadouble)rvals[16];
      *(v+5) = (gadouble)rvals[17];
      *(v+7) = -999.9;
      pgr->ivals = v;
      pgr->iavals = v;
    } else {
      v = (gadouble *)malloc(sizeof(gadouble)*6);
      if (v==NULL) goto merr;
      *v     = (gadouble)rvals[8];
      *(v+1) = (gadouble)(rvals[7]-rvals[8]);
      *(v+2) = -999.9;
      pgr->ivals = v;
      *(v+3) = (gadouble)(1.0 / rvals[8]);
      *(v+4) = (gadouble)(-1.0 * (rvals[7]-rvals[8]) / rvals[8]);
      *(v+5) = -999.9;
      pgr->iavals = v+3;
      pgr->iabgr = liconv;
      pgr->igrab = liconv;
    }
  }
  if (pgr->jdim>-1 && pgr->jlinr==1) {     /* Linear scaling info */
    if (pgr->jdim==3) {
      v = (gadouble *)malloc(sizeof(gadouble)*8);
      if (v==NULL) goto merr;
      *v     = (gadouble)rvals[11];
      *(v+1) = (gadouble)rvals[12];
      *(v+2) = (gadouble)rvals[13];
      *(v+3) = (gadouble)rvals[14];
      *(v+4) = (gadouble)rvals[15];
      *(v+6) = (gadouble)rvals[16];
      *(v+5) = (gadouble)rvals[17];
      *(v+7) = -999.9;
      pgr->jvals = v;
      pgr->javals = v;
    } else {
      v = (gadouble *)malloc(sizeof(gadouble)*6);
      if (v==NULL) goto merr;
      *v     = (gadouble)rvals[10];
      *(v+1) = (gadouble)(rvals[9]-rvals[10]);
      *(v+2) = -999.9;
      pgr->jvals = v;
      *(v+3) = (gadouble)(1.0 / rvals[10]);
      *(v+4) = (gadouble)(-1.0 * (rvals[9]-rvals[10]) / rvals[10]);
      *(v+5) = -999.9;
      pgr->javals = v+3;
      pgr->jabgr = liconv;
      pgr->jgrab = liconv;
    }
  }

  /* Read in the data */

  siz = pgr->isiz * pgr->jsiz;
  v = (gadouble *)malloc(sizeof(gadouble)*siz);
  if (v==NULL) {
    free(pgr);
    goto merr;
  }
  if (read_float(v, siz, ufb->sflg, ifile) < 0)
    goto rerr;
  pgr->grid = v;

  /* set umask */
  if ((pgr->umask = malloc(siz)) == NULL) {
    free(v);
    free(pgr);
    goto merr;
  }
  for (i = 0; i < siz; i++)
    pgr->umask[i] = (v[i] == pgr->undef) ? 0 : 1;

  /* Read in non-linear scaling info, if any */

  if (pgr->idim>-1 && pgr->ilinr==0) {
    v = (gadouble *)malloc(sizeof(gadouble)*(pgr->isiz+2));
    if (v==NULL) {
      free(pgr->grid);
      free(pgr);
      goto merr;
    }
    *v = pgr->isiz;

    if (read_float(v + 1, pgr->isiz, ufb->sflg, ifile) < 0)
      goto rerr;

    *(v+pgr->isiz+1) = -999.9;
    pgr->ivals = v;
    pgr->iavals = v;
    pgr->iabgr = lev2gr;
    pgr->igrab = gr2lev;
  }
  if (pgr->jdim>-1 && pgr->jlinr==0) {
    v = (gadouble *)malloc(sizeof(gadouble)*(pgr->jsiz+2));
    if (v==NULL) {
      free(pgr->grid);
      free(pgr);
      goto merr;
    }
    *v = pgr->jsiz;

    if (read_float(v + 1, pgr->jsiz, ufb->sflg, ifile) < 0)
      goto rerr;

    *(v+pgr->jsiz+1) = -999.9;
    pgr->jvals = v;
    pgr->javals = v;
    pgr->jabgr = lev2gr;
    pgr->jgrab = gr2lev;
  }
  fclose(ifile);

#ifndef __CYGWIN32__
  sprintf(pout,"rm %s",ufb->oname);
  system (pout);
  sprintf(pout,"rm %s",ufb->iname);
  system (pout);
#endif

  /* We are done.  Return.  */

  pst->result.pgr = pgr;
  pst->type = 1;

  return (0);

werr:
  sprintf (pout,"Error from %s: Error writing to transfer file\n",ufb->name);
  gaprnt (0,pout);
  sprintf (pout,"  File name: %s\n",ufb->oname);
  gaprnt (0,pout);
  fclose(ofile);
  return (1);

rerr:
  sprintf (pout,"Error from %s: Error reading from transfer file\n",ufb->name);
  gaprnt (0,pout);
  sprintf (pout,"  File name: %s\n",ufb->oname);
  gaprnt (0,pout);
  fclose(ifile);
  return (1);

merr:
  sprintf (pout,"Error from %s: Memory Allocation Error\n",ufb->name);
  gaprnt (0,pout);
  fclose (ifile);
  return (1);

ferr:
  sprintf (pout,"Error from %s: Invalid transfer file format\n",ufb->name);
  gaprnt (0,pout);
  sprintf (pout,"  File name: %s\n",ufb->iname);
  gaprnt (0,pout);
  fclose (ifile);
  return (1);

derr:
  sprintf (pout,"Error from %s: Invalid dimension environment ",ufb->name);
  gaprnt (0,pout);
  gaprnt (0,"in result grid\n");
  fclose (ifile);
  return (1);
}


/* Routine to read the user function definition file, and build
   the appropriate link list of function definition blocks.
   The file name is pointed to by the GAFDEF environment variable;
   if unset then no user functions will be set up */

void gafdef2 (void) {
struct gaufb *ufb, *oufb;
char *cname;
FILE *cfile;
char rec[260],*ch;
int i,j,pass;

  ufba = NULL;

  /* Make two passes.  First read user function table, then read
     system function table */

  pass = 0;
  while (pass<2) {
    if (pass==0) {
      cname = getenv("GAUDFT");
      if (cname==NULL) {
        pass++;
        continue;
      }
      cfile = fopen(cname,"r");
      if (cfile==NULL) {
        gaprnt(0,"Error opening user function definition table\n");
        sprintf (pout,"  File name is: %s\n",cname);
        gaprnt (0,pout);
        pass++;
        continue;
      }
    } else {
      cname = gxgnam("udft");
      cfile = fopen(cname,"r");
      if (cfile==NULL) break;
    }

    /* Read the file. */

    while (1) {
      ufb = (struct gaufb *)malloc(sizeof(struct gaufb));
      if (ufb==NULL) goto memerr;

      /* Read First record (name and arg types) */

      ch = fgets(rec,256,cfile);
      if (ch==NULL) break;
      ch = rec;
      lowcas(ch);
      while (*ch==' ') ch++;
      i = 0;
      while (*ch!=' ' && *ch!='\0' && *ch!='\n') {
        if (i<15) {
          ufb->name[i] = *ch;
          i++;
        }
        ch++;
      }
      ufb->name[i] = '\0';
      if (*ch!=' ') goto fmterr;
      while (*ch==' ') ch++;
      if (intprs(ch,&(ufb->alo))==NULL) goto fmterr;
      if ( (ch = nxtwrd(ch))==NULL) goto fmterr;
      if (intprs(ch,&(ufb->ahi))==NULL) goto fmterr;
      i = 0;
      while (i<ufb->ahi) {
        if ( (ch = nxtwrd(ch))==NULL) goto fmterr;
        if (cmpwrd("expr",ch)) ufb->atype[i]=1;
        else if (cmpwrd("value",ch)) ufb->atype[i]=2;
        else if (cmpwrd("char",ch)) ufb->atype[i]=3;
        else goto fmterr;
        i++;
      }

      /* Read 2nd record -- options */

      ch = fgets(rec,256,cfile);
      if (ch==NULL) goto rderr;
      ch = rec;
      lowcas(ch);
      while (*ch==' ') ch++;
      if (*ch=='\n' || *ch=='\0') goto fmterr;
      while (1) {
        if (cmpwrd("direct",ch)) ufb->sflg=0;
        else if (cmpwrd("sequential",ch)) ufb->sflg=1;
        else goto fmterr;
        if ( (ch = nxtwrd(ch))==NULL) break;
      }

      /* Read 3rd record -- file name of executable */

      ch = fgets(rec,256,cfile);
      if (ch==NULL) goto rderr;
      i = 0;
      while (rec[i]!='\n' && rec[i]!='\0') i++;
      ufb->fname = (char *)malloc(i+1);
      if (ufb->fname==NULL) {
        free (ufb);
        goto memerr;
      }
      for (j=0; j<i; j++) *(ufb->fname+j) = rec[j];
      *(ufb->fname+i) = '\0';

      /* Read 4th record -- file name of data transfer to user */

      ch = fgets(rec,256,cfile);
      if (ch==NULL) goto rderr;
      i = 0;
      while (rec[i]!='\n' && rec[i]!='\0') i++;
      ufb->oname = (char *)malloc(i+1);
      if (ufb->oname==NULL) {
        free (ufb);
        goto memerr;
      }
      for (j=0; j<i; j++) *(ufb->oname+j) = rec[j];
      *(ufb->oname+i) = '\0';

      /* Read 5th record -- file name for data transfer from user */

      ch = fgets(rec,256,cfile);
      if (ch==NULL) goto rderr;
      i = 0;
      while (rec[i]!='\n' && rec[i]!='\0') i++;
      ufb->iname = (char *)malloc(i+1);
      if (ufb->iname==NULL) {
        free (ufb);
        goto memerr;
      }
      for (j=0; j<i; j++) *(ufb->iname+j) = rec[j];
      *(ufb->iname+i) = '\0';

      /* Chain this ufb */

      ufb->ufb = NULL;

      if (ufba==NULL) ufba = ufb;
      else oufb->ufb = ufb;
      oufb = ufb;
    }

    fclose (cfile);
    if (pass==1) free(cname);
    pass++;
  }
  return;

memerr:
  gaprnt(0,"Memory allocation error: user defined functions\n");
  return;

fmterr:
  gaprnt(0,"Format error in user defined function table:\n");
  sprintf (pout,"  Processing function name: %s\n",ufb->name);
  gaprnt (0,pout);
  free (ufb);
  goto wname;

rderr:
  gaprnt(0,"Read error on user defined function table:\n");
  free (ufb);
  goto wname;

wname:
  sprintf (pout,"  File name is: %s\n",cname);
  gaprnt (0,pout);
  return;
}


void gaqufb (void) {
struct gaufb *ufb;
char name[20];
int i;

  ufb = ufba;
  while (ufb) {
    for (i=0; i<8; i++) name[i] = ufb->name[i];
    name[8] = '\0';
    sprintf (pout,"%s  Args: %i %i  Exec: %s\n",name,ufb->alo,ufb->ahi,
              ufb->fname);
    gaprnt (2,pout);
    ufb = ufb->ufb;
  }
}


struct gaufb *get_ludf(void)
{
  return ufba;
}
