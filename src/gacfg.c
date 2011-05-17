/*  Copyright (C) 1988-2010 by Brian Doty and the
    Institute of Global Environment and Society (IGES).
    See file COPYRIGHT for more information.   */


/* file: gacfg.c
 *
 *   Prints the configuration options of this build of GrADS.
 *   This function is invoked at startup and with 'q config'.
 *
 *   REVISION HISTORY:
 *
 *   09sep97   da Silva   Initial code.
 *   12oct97   da Silva   Small revisions, use of gaprnt().
 *   15apr98   da Silva    Added BUILDINFO stuff, made gacfg() void.
 *   24jun02   K Komine   Added 64-bit mode .
 *
 *   --
 *   (c) 1997 by Arlindo da Silva
 *
 *   Permission is granted to any individual or institution to use,
 *   copy, or redistribute this software so long as it is not sold for
 *   profit, and provided this notice is retained.
 *
 */

/* Include ./configure's header file */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include "buildinfo.h"

#if GRIB2==1
#include "grib2.h"
#endif

#if USEHDF==1
#include "mfhdf.h"
#endif

#if USEHDF5==1
#include "hdf5.h"
#endif

#if USENETCDF==1
#include "netcdf.h"
const char *nc_inq_libvers(void);
#endif

#if USEGADAP==1
const char *libgadap_version(void);
#endif
/*
 * gacfg() - Prints several configuration parameters.
 *
 *           verbose = 0   only config string
 *                   = 1   config string + verbose description
 *                   > 1   no screen display.
 */
void gaprnt (int, char *);
void gacfg(int verbose) {
char cmd[256];
#if USEHDF==1
char hdfverstr[1024];
uint32 majorv=0,minorv=0,release=0;
#endif
#if (USEHDF5==1)
unsigned vmajor=0,vminor=0,vrelease=0;
#endif

snprintf(cmd,255,"Config: v%s",GRADS_VERSION);
#if BYTEORDER==1
 strcat(cmd," big-endian");
#else
 strcat(cmd," little-endian");
#endif
#if READLINE==1
 strcat(cmd," readline");
#endif
#if GXPNG==1
 strcat(cmd," printim");
#endif
#if GRIB2==1
 strcat(cmd," grib2");
#endif
#if USENETCDF==1
 strcat(cmd," netcdf");
#endif
#if USEHDF==1
 strcat(cmd," hdf4-sds");
#endif
#if USEHDF5==1
 strcat(cmd," hdf5");
#endif
#if USEDAP==1
 strcat(cmd," opendap-grids");
#endif
#if USEGADAP==1
 strcat(cmd,",stn");
#endif
#if USEGUI==1
 strcat(cmd," athena");
#endif
#if GEOTIFF==1
 strcat(cmd," geotiff");
#endif
#if USESHP==1
 strcat(cmd," shapefile");
#endif
 strcat(cmd,"\n");
 gaprnt(verbose,cmd);

 if (verbose==0) {
   gaprnt(verbose,"Issue 'q config' command for more detailed configuration information\n");
   return;
 }

 gaprnt (verbose, "Grid Analysis and Display System (GrADS) Version " GRADS_VERSION "\n");
 gaprnt (verbose, "Copyright (c) 1988-2010 by Brian Doty and the\n");
 gaprnt (verbose, "Institute for Global Environment and Society (IGES) \n");
 gaprnt (verbose, "This program is distributed WITHOUT ANY WARRANTY \n");
 gaprnt (verbose, "See file COPYRIGHT for more information. \n\n");

 gaprnt (verbose, buildinfo );

 gaprnt(verbose,"\n\nThis version of GrADS has been configured with the following options:\n");

#if BYTEORDER==1
   gaprnt(verbose,"  o Built on a BIG ENDIAN machine\n");
#else
   gaprnt(verbose,"  o Built on a LITTLE ENDIAN machine\n");
#endif

#if USEGUI==1
   gaprnt(verbose,"  o Athena Widget GUI ENABLED\n");
#else
   gaprnt(verbose,"  o Athena Widget GUI DISABLED\n");
#endif

#if READLINE==1
   gaprnt(verbose,"  o Command line editing ENABLED \n");
   gaprnt(verbose,"      http://tiswww.case.edu/php/chet/readline/rltop.html \n");
#else
   gaprnt(verbose,"  o Command line editing DISABLED\n");
#endif

#if GXPNG==1
   gaprnt(verbose,"  o printim command for image output ENABLED \n");
   gaprnt(verbose,"      http://www.zlib.net \n");
   gaprnt(verbose,"      http://www.libpng.org/pub/png/libpng.html \n");
   gaprnt(verbose,"      http://www.libgd.org/Main_Page \n");
#else
   gaprnt(verbose,"  o printim command DISABLED\n");
#endif

#if GRIB2==1
   gaprnt(verbose,"  o GRIB2 interface ENABLED \n");
   gaprnt(verbose,"      http://www.ijg.org \n");
   gaprnt(verbose,"      http://www.ece.uvic.ca/~mdadams/jasper \n");
   gaprnt(verbose,"      http://www.nco.ncep.noaa.gov/pmb/codes/GRIB2 \n");
   snprintf(cmd,255,   "      %s  \n",G2_VERSION);
   gaprnt(verbose,cmd);
#else
   gaprnt(verbose,"  o GRIB2 interface DISABLED\n");
#endif

#if USENETCDF==1
   gaprnt(verbose,"  o NetCDF interface ENABLED \n");
   gaprnt(verbose,"      http://www.unidata.ucar.edu/software/netcdf  \n");
   snprintf(cmd,255,   "      netcdf %s  \n",(char*)nc_inq_libvers());
   gaprnt(verbose,cmd);
#else
   gaprnt(verbose,"  o NetCDF interface DISABLED\n");
#endif

#if USEDAP==1
   gaprnt(verbose,"  o OPeNDAP gridded data interface ENABLED\n");
#else
   gaprnt(verbose,"  o OPeNDAP gridded data interface DISABLED\n");
#endif

#if USEGADAP==1
   gaprnt(verbose,"  o OPeNDAP station data interface ENABLED\n");
   gaprnt(verbose,"      http://iges.org/grads/gadoc/supplibs.html \n");
   snprintf(cmd,255,   "      %s  \n", libgadap_version());
   gaprnt(verbose,cmd);
#else
   gaprnt(verbose,"  o OPeNDAP station data interface DISABLED\n");
#endif

#if (USEHDF==1 || USEHDF5==1)
#if (USEHDF==1 && USEHDF5==1)
   /* we've got both */
   gaprnt(verbose,"  o HDF4 and HDF5 interfaces ENABLED \n");
   gaprnt(verbose,"      http://hdfgroup.org \n");
   Hgetlibversion(&majorv,&minorv,&release,hdfverstr);
   snprintf(cmd,255,   "      HDF %d.%dr%d \n",majorv,minorv,release);
   gaprnt(verbose,cmd);
   H5get_libversion(&vmajor,&vminor,&vrelease);
   snprintf(cmd,255,   "      HDF5 %d.%d.%d \n",vmajor,vminor,vrelease);
   gaprnt(verbose,cmd);
#else
#if USEHDF==1
   /* we've only got hdf4 */
   gaprnt(verbose,"  o HDF4 interface ENABLED \n");
   gaprnt(verbose,"      http://hdfgroup.org \n");
   Hgetlibversion(&majorv,&minorv,&release,hdfverstr);
   snprintf(cmd,255,   "      HDF %d.%dr%d \n",majorv,minorv,release);
   gaprnt(verbose,cmd);
#else
   /* we've only got hdf5 */
   gaprnt(verbose,"  o HDF5 interface ENABLED \n");
   gaprnt(verbose,"      http://hdfgroup.org \n");
   H5get_libversion(&vmajor,&vminor,&vrelease);
   snprintf(cmd,255,   "      HDF5 %d.%d.%d \n",vmajor,vminor,vrelease);
   gaprnt(verbose,cmd);
#endif
#endif
#else
   gaprnt(verbose,"  o HDF interface DISABLED\n");
#endif

#if GEOTIFF==1
   gaprnt(verbose,"  o GeoTIFF and KML/TIFF output ENABLED\n");
   gaprnt(verbose,"      http://www.libtiff.org \n");
   gaprnt(verbose,"      http://geotiff.osgeo.org \n");
#else
   gaprnt(verbose,"  o GeoTIFF and KML/TIFF output DISABLED\n");
#endif
   gaprnt(verbose,"  o KML contour output ENABLED\n");
#if USESHP==1
   gaprnt(verbose,"  o Shapefile interface ENABLED\n");
   gaprnt(verbose,"      http://shapelib.maptools.org \n");
#else
   gaprnt(verbose,"  o Shapefile interface DISABLED\n");
#endif


 gaprnt(verbose,"\nFor additional information please consult http://iges.org/grads\n\n");

}
