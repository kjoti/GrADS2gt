/*
 * galudf.c -- GrADS Legacy User Defined Function
 *
 * Copyed from src/gafunc.c in GrADS 1.9b4:
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


static struct gaufb *ufba;  /* Anchor for user function defs */
char *gxgnam(char *);       /* This is also in gx.h */


/* Handle user specified function call.  The args are written
   out, the user's process is invoked, and the result is read
   back in.  */

int ffuser (struct gaufb *ufb, struct gafunc *pfc, struct gastat *pst) {
FILE *ifile,*ofile;
struct gagrid *pgr;
struct dt dtim;
float (*conv) (float *, float);
float rvals[20],*v;
int rc,iarg,siz,i;
char *ch,rec[80];
int rdw;

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
  if (ufb->sflg) {
    rdw = sizeof(float)*20;
    fwrite (&rdw,sizeof(int),1,ofile);
  }
  rc = fwrite (rvals,sizeof(float),20,ofile);
  if (rc<20) goto werr;
  if (ufb->sflg) fwrite (&rdw,sizeof(int),1,ofile);

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
      siz = pgr->isiz*pgr->jsiz;                 /* Write header */
      if (ufb->sflg) {
        rdw = sizeof(float)*20;
        fwrite (&rdw,sizeof(int),1,ofile);
      }
      rc = fwrite(rvals,sizeof(float),20,ofile);
      if (rc<20) {
        gafree(pst);
        goto werr;
      }                                          /* Write grid   */
      if (ufb->sflg) fwrite (&rdw,sizeof(int),1,ofile);
      if (ufb->sflg) {
        rdw = sizeof(float)*siz;
        fwrite (&rdw,sizeof(int),1,ofile);
      }
      rc = fwrite(pgr->grid,sizeof(float),siz,ofile);
      if (rc<siz) {
        gafree(pst);
        goto werr;
      }
      if (ufb->sflg) fwrite (&rdw,sizeof(int),1,ofile);
      if (pgr->idim>-1) {                  /* write i dim scaling */
        v = pgr->grid;
        if (pgr->idim<3) {
          conv = pgr->igrab;
          for (i=pgr->dimmin[pgr->idim];i<=pgr->dimmax[pgr->idim];i++) {
            *v = conv(pgr->ivals,(float)i);
            v++;
          }
        } else {
          for (i=pgr->dimmin[pgr->idim];i<=pgr->dimmax[pgr->idim];i++) {
            *v = (float)i;
            v++;
          }
        }
        if (ufb->sflg) {
          rdw = sizeof(float)*pgr->isiz;
          fwrite (&rdw,sizeof(int),1,ofile);
        }
        rc = fwrite(pgr->grid,sizeof(float),pgr->isiz,ofile);
        if (rc<pgr->isiz) {
          gafree(pst);
          goto werr;
        }
        if (ufb->sflg) fwrite (&rdw,sizeof(int),1,ofile);
      }
      if (pgr->jdim>-1) {                /* write j dim scaling */
        v = pgr->grid;
        if (pgr->jdim<3) {
          conv = pgr->jgrab;
          for (i=pgr->dimmin[pgr->jdim];i<=pgr->dimmax[pgr->jdim];i++) {
            *v = conv(pgr->jvals,(float)i);
            v++;
          }
        } else {
          for (i=pgr->dimmin[pgr->jdim];i<=pgr->dimmax[pgr->jdim];i++) {
            *v = (float)i;
            v++;
          }
        }
        if (ufb->sflg) {
          rdw = sizeof(float)*pgr->jsiz;
          fwrite (&rdw,sizeof(int),1,ofile);
        }
        rc = fwrite(pgr->grid,sizeof(float),pgr->jsiz,ofile);
        if (rc<pgr->jsiz) {
          gafree(pst);
          goto werr;
        }
        if (ufb->sflg) fwrite (&rdw,sizeof(int),1,ofile);
      }
      gafree(pst);     /* Done with expr */
    }

    /* Handle Value */

    else if (ufb->atype[iarg]==2) {
      if (valprs(pfc->argpnt[iarg],rvals)==NULL) {
        sprintf (pout,"Error from %s: Invalid Argument\n",ufb->name);
        gaprnt (0,pout);
        sprintf (pout,"  Expecting arg %i to be a constant\n",iarg);
        gaprnt (0,pout);
      }
      if (ufb->sflg) {
        rdw = sizeof(float);
        fwrite (&rdw,sizeof(int),1,ofile);
      }
      rc = fwrite(rvals,sizeof(float),1,ofile);
      if (rc<1) goto werr;
      if (ufb->sflg) fwrite (&rdw,sizeof(int),1,ofile);
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
      if (ufb->sflg) {
        rdw = 80;
        fwrite (&rdw,sizeof(int),1,ofile);
      }
      rc = fwrite(rec,1,80,ofile);
      if (rc<80) goto werr;
      if (ufb->sflg) fwrite (&rdw,sizeof(int),1,ofile);
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

  if (ufb->sflg) {
    fread(&rdw,sizeof(int),1,ifile);
    if (rdw!=sizeof(float)*20) goto ferr;
  }
  rc = fread(rvals,sizeof(float),20,ifile);
  if (rc<20) goto rerr;
  if (ufb->sflg) fread(&rdw,sizeof(int),1,ifile);
  rc = (int)(*rvals+0.1);
  if (rc!=0) {
    fclose(ifile);   /*mf ---- add fclose of the udf input file ---- mf*/
    return (1);
  }

  /* If all is ok, read the grid header */

  if (ufb->sflg) {
    fread(&rdw,sizeof(int),1,ifile);
    if (rdw!=sizeof(float)*20) goto ferr;
  }
  rc = fread(rvals,sizeof(float),20,ifile);
  if (rc<20) goto rerr;
  if (ufb->sflg) fread(&rdw,sizeof(int),1,ifile);

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
      v = (float *)malloc(sizeof(float)*8);
      if (v==NULL) goto merr;
      *v = rvals[11];
      *(v+1) = rvals[12];
      *(v+2) = rvals[13];
      *(v+3) = rvals[14];
      *(v+4) = rvals[15];
      *(v+6) = rvals[16];
      *(v+5) = rvals[17];
      *(v+7) = -999.9;
      pgr->ivals = v;
      pgr->iavals = v;
    } else {
      v = (float *)malloc(sizeof(float)*6);
      if (v==NULL) goto merr;
      *v = rvals[8];
      *(v+1) = rvals[7]-rvals[8];
      *(v+2) = -999.9;
      pgr->ivals = v;
      *(v+3) = 1.0 / rvals[8];
      *(v+4) = -1.0 * (rvals[7]-rvals[8]) / rvals[8];
      *(v+5) = -999.9;
      pgr->iavals = v+3;
      pgr->iabgr = liconv;
      pgr->igrab = liconv;
    }
  }
  if (pgr->jdim>-1 && pgr->jlinr==1) {     /* Linear scaling info */
    if (pgr->jdim==3) {
      v = (float *)malloc(sizeof(float)*8);
      if (v==NULL) goto merr;
      *v = rvals[11];
      *(v+1) = rvals[12];
      *(v+2) = rvals[13];
      *(v+3) = rvals[14];
      *(v+4) = rvals[15];
      *(v+6) = rvals[16];
      *(v+5) = rvals[17];
      *(v+7) = -999.9;
      pgr->jvals = v;
      pgr->javals = v;
    } else {
      v = (float *)malloc(sizeof(float)*6);
      if (v==NULL) goto merr;
      *v = rvals[10];
      *(v+1) = rvals[9]-rvals[10];
      *(v+2) = -999.9;
      pgr->jvals = v;
      *(v+3) = 1.0 / rvals[10];
      *(v+4) = -1.0 * (rvals[9]-rvals[10]) / rvals[10];
      *(v+5) = -999.9;
      pgr->javals = v+3;
      pgr->jabgr = liconv;
      pgr->jgrab = liconv;
    }
  }

  /* Read in the data */

  siz = pgr->isiz * pgr->jsiz;
  v = (float *)malloc(sizeof(float)*siz);
  if (v==NULL) {
    free(pgr);
    goto merr;
  }
  if (ufb->sflg) {
    fread(&rdw,sizeof(int),1,ifile);
    if (rdw!=sizeof(float)*siz) goto ferr;
  }
  rc = fread(v,sizeof(float),siz,ifile);
  if (rc<siz) goto rerr;
  if (ufb->sflg) fread(&rdw,sizeof(int),1,ifile);
  pgr->grid = v;

  /* Read in non-linear scaling info, if any */

  if (pgr->idim>-1 && pgr->ilinr==0) {
    v = (float *)malloc(sizeof(float)*(pgr->isiz+2));
    if (v==NULL) {
      free(pgr->grid);
      free(pgr);
      goto merr;
    }
    *v = pgr->isiz;
    if (ufb->sflg) {
      fread(&rdw,sizeof(int),1,ifile);
      if (rdw!=sizeof(float)*pgr->isiz) goto ferr;
    }
    rc = fread(v+1,sizeof(float),pgr->isiz,ifile);
    if (ufb->sflg) fread(&rdw,sizeof(int),1,ifile);
    if (rc<pgr->isiz) goto rerr;
    *(v+pgr->isiz+1) = -999.9;
    pgr->ivals = v;
    pgr->iavals = v;
    pgr->iabgr = lev2gr;
    pgr->igrab = gr2lev;
  }
  if (pgr->jdim>-1 && pgr->jlinr==0) {
    v = (float *)malloc(sizeof(float)*(pgr->jsiz+2));
    if (v==NULL) {
      free(pgr->grid);
      free(pgr);
      goto merr;
    }
    *v = pgr->jsiz;
    if (ufb->sflg) {
      fread(&rdw,sizeof(int),1,ifile);
      if (rdw!=sizeof(float)*pgr->jsiz) goto ferr;
    }
    rc = fread(v+1,sizeof(float),pgr->jsiz,ifile);
    if (rc<pgr->jsiz) goto rerr;
    if (ufb->sflg) fread(&rdw,sizeof(int),1,ifile);
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

void gafdef (void) {
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
