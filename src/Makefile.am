## Makefile.am for GrADS
## Process this file with automake to produce Makefile.in


##---Targets---------------------------------------------------------------------


## Targets that will built by "make all"
##   The "bin_" and "lib_" prefixes indicate the installation locations
bin_PROGRAMS = grads gribmap stnmap gribscan grib2scan bufrscan
lib_LTLIBRARIES = libgxpGD.la libgxdX11.la libgxpCairo.la libgxdCairo.la \
                  libgxdummy.la
#libgradspy.la

## Convenience targets
utils: gribmap stnmap gribscan grib2scan bufrscan
libs: libgxpGD.la libgxdX11.la libgxpCairo.la libgxdCairo.la libgxdummy.la
#gradspy: libgradspy.la


##---Source files---------------------------------------------------------------------


## Configuration-dependent sources
if USEGADAP
src_gadap = dodstn.c
endif

if USEGUI
src_gui = gagui.c gsgui.c
endif

## Source files to be compiled
grads_SOURCES =	grads.c  gxsubs.c gxmeta.c  gxchpl.c gxcntr.c gxstrm.c gxdb.c \
		gxwmap.c gxshad.c gxshad2.c gaexpr.c gafunc.c gautil.c gagx.c \
		gscrpt.c gamach.c gatxt.c   galloc.c gaddes.c gacfg.c  gaio.c \
                gauser.c gasdf.c  bufrstn.c gabufr.c gabufrtbl.c \
                $(src_gui) $(src_gadap)

libgxpGD_la_SOURCES    = gxprintGD.c gxGD.c
libgxdX11_la_SOURCES   = gxX11.c gxdxwd.c
libgxpCairo_la_SOURCES = gxprint.c gxC.c
libgxdCairo_la_SOURCES = gxX.c gxC.c
libgxdummy_la_SOURCES  = gxdummy.c


## These may not always be compiled but must be listed in order to go in the distribution
EXTRA_grads_SOURCES = gagmap.c gagui.c gsgui.c dodstn.c \
                      gxprintGD.c gxGD.c gxdxwd.c gxX11.c \
                      gxprint.c gxC.c gxX.c gxdummy.c


##---Header files---------------------------------------------------------------------


## Headers must be listed here to be included in the distribution.
## The "noinst_" prefix prevents "make install" from trying to do anything with them
hdr_core  = grads.h  gatypes.h bitmaps.h bitmaps2.h gvt.h wx.h gs.h
hdr_ga    = gagmap.h gagui.h gabufr.h gasdf.h gasdf_std_time.h
hdr_gx    = gx.h gxGD.h gxC.h gxmap.h

noinst_HEADERS = $(hdr_core) $(hdr_ga) $(hdr_gx)

## This gets rid of buildinfo.h and config.h on "make distclean"
DISTCLEANFILES = buildinfo.h config.h


##---------------------------------------------------------------------
## Compiler and linker settings


## Paths to external headers and libraries
supp_include_dir	= $(SUPPLIBS)/include
supp_lib_dir		= $(SUPPLIBS)/lib
LDADD			= -L$(supp_lib_dir)

common_ldadd            = $(LDADD) $(host_ldadd)
X11_ldadd               = $(X_LIBS) $(X_PRE_LIBS) -lX11 -lXext $(X_EXTRA_LIBS)


AM_CPPFLAGS		= $(gui_inc) $(nc_inc) \
			  $(hdf_inc) $(hdf5_inc) $(geotiff_inc) $(shp_inc) $(gadap_inc) \
                          $(X_CFLAGS) $(XAW_CFLAGS) $(HDF4_CFLAGS)

## libraries needed
grads_LDADD	      = $(common_ldadd) $(readline_libs) $(grib2_libs) \
			$(hdf_libs) $(hdf5_libs) $(nc_libs) $(geotiff_libs) \
			$(shp_libs) $(gadap_libs) $(gui_libs)

# print with GD
libgxpGD_la_CPPFLAGS = -I. -I$(SUPPLIBS)/include
libgxpGD_la_LIBADD = $(LDADD) $(gd_dynlibs)

# display X11

libgxdX11_la_CPPFLAGS = -I. -I$(SUPPLIBS)/include
libgxdX11_la_LIBADD = $(X11_ldadd)

# print with Cairo
libgxpCairo_la_CPPFLAGS = -I. -I$(SUPPLIBS)/include -I$(SUPPLIBS)/include/cairo -I$(SUPPLIBS)/include/freetype2
libgxpCairo_la_LIBADD = $(LDADD) $(cairo_dynlibs)

## display X11 with Cairo
libgxdCairo_la_CPPFLAGS = -I. -I$(SUPPLIBS)/include -I$(SUPPLIBS)/include/cairo -I$(SUPPLIBS)/include/freetype2
libgxdCairo_la_LIBADD = $(X11_ldadd) $(LDADD) $(cairo_dynlibs)

## dummy (for display and print)
libgxdummy_la_CPPFLAGS = -I.


##---------------------------------------------------------------------
## GradsPy -- not ready for 2.2.0 release, commented out for now

#libgradspy_la_SOURCES  = grads.c gxsubs.c gxmeta.c  gxchpl.c gxcntr.c gxstrm.c gxdb.c \
		gxwmap.c gxshad.c gxshad2.c gaexpr.c gafunc.c gautil.c gagx.c \
		gscrpt.c gamach.c gatxt.c   galloc.c gaddes.c gacfg.c  gaio.c \
                gauser.c gasdf.c  bufrstn.c gabufr.c gabufrtbl.c \
                $(src_gui) $(src_gadap)

## this object file needs to be recompiled for gradspy with the SHRDOBJ flag set
#libgradspy_la-grads.o: $(srcdir)/grads.c
#	$(COMPILE) -DSHRDOBJ -o libgradspy_la-grads.lo -c $(srcdir)/grads.c

#libgradspy_la_CFLAGS   = -DSHRDOBJ
#libgradspy_la_CPPFLAGS = $(gui_inc) $(X_CFLAGS) $(XAW_CFLAGS)
#libgradspy_la_LIBADD   = $(common_ldadd) $(readline_dynlibs) $(grib2_dynlibs) \
			 $(hdf_dynlibs) $(hdf5_dynlibs) $(nc_dynlibs) $(geotiff_dynlibs) \
			 $(shp_dynlibs) $(gadap_dynlibs) $(gui_dynlibs)




##---------------------------------------------------------------------
## If gadap is enabled, use C++ linker instead of C linker
if USEGADAP
grads_LINK          = $(CXX) $(AM_CFLAGS) $(CFLAGS) $(AM_LDFLAGS) $(LDFLAGS) -o $@
else
grads_LINK          = $(CC) $(AM_CFLAGS) $(CFLAGS) $(AM_LDFLAGS) $(LDFLAGS) -o $@
endif


## Custom compilation for object files
COMPILE_C   = $(COMPILE)
COMPILE_DAP = $(COMPILE)


##---------------------------------------------------------------------
## Utilities


## Object files that need to be recompiled for standalone utilities
gaddes.sa.o: $(srcdir)/gaddes.c
	$(COMPILE) -DSTNDALN -o gaddes.sa.o -c $(srcdir)/gaddes.c

gautil.sa.o: $(srcdir)/gautil.c
	$(COMPILE) -DSTNDALN -o gautil.sa.o -c $(srcdir)/gautil.c

## stnmap
stnmap_SOURCES	       = stnmap.c gamach.c galloc.c
stnmap_LDADD	       = gaddes.sa.o gautil.sa.o gatxt.o $(LDADD) $(readline_libs)

## gribmap
gribmap_SOURCES	       = gribmap.c gagmap.c gamach.c galloc.c
gribmap_LDADD	       = gaddes.sa.o gautil.sa.o gatxt.o $(LDADD) $(readline_libs) $(grib2_libs)

## gribscan
gribscan_SOURCES       = gribscan.c gamach.c galloc.c
gribscan_LDADD	       = gautil.sa.o gatxt.o $(LDADD) $(readline_libs)

## grib2scan
grib2scan_SOURCES      = grib2scan.c gamach.c galloc.c
grib2scan_LDADD	       = gautil.sa.o gatxt.o $(LDADD) $(readline_libs)

## bufrscan
bufrscan_SOURCES       = bufrscan.c gabufr.c gabufrtbl.c gamach.c
