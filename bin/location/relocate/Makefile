MAN1=relocate.1
BIN=relocate
PF=relocate.pf
cflags = 
ldflags = 

ldlibs=-lgenloc $(DBLIBS) $(PERFLIBS) $(F77LIBS)

#
# Turn off the above, and use something like the following if the 
# Sun Performance Library (sunperf) is not installed on your system.
#  This example uses the C version of LAPACK that should appear in
# the contrib area of new releases.
#

#ldlibs= -lgenloc -ltt -L/opt/dsap/ext/CLAPACK/F2CLIBS  -llapack_alpha  -lblas_alpha -lI77f2c -lF77f2c -lcoords -lposix4 -lstock $(DBLIBS) $(F77LIBS) -ldl 
include $(ANTELOPEMAKE)
DIRS=

OBJS= relocate.o
CC=cc
$(BIN):$(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS) $(LDLIBS)

# $Id$ 
