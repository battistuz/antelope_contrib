#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include "stock.h"
#include "coords.h"
#include "arrays.h"
#include "db.h"
#include "dbmwspec.h"
/*
This function writes a spectral result stored in array spec to an output file 
defined with name formed as dir/dfile.  spec is assumed to hold nfreq 
floats.  

Normal return is foff returned by ftell (0 or position appended from)
Any negative return indicates an error:

	-1 - cannot open file
	-2 - write error


Author:  Gary Pavlis
Written:  August 1994
Modified:  July 1995 for revised spectral program now called mwspec.  
Changed behaviour such that when a file doesn't exist it is created.
and the function returns 0.  If the file name exists, the function seeks
to the end of the file and appends the given data onto the end of the 
given file.  In this condition it returns foff (end of previous file)
This was done to reduce the number of files generated by this program.
(see also save_spectrum below)
*/
  
long write_spectrum (spec,nfreq,dir,dfile)
float *spec;
int nfreq;
char *dir,*dfile;
{
	char *fname;
	FILE *fp;
	int size;
	long foff;

	size = strlen(dir) + strlen(dfile) + 4;
	if( (fname = (char *) malloc(size)) == NULL) return(1);
	strcpy(fname,dir);
	strcat(fname,"/");
	strcat(fname,dfile);
	if( (fp = fopen(fname,"a")) == NULL) return(-1);
	fseek(fp,0L,2);
	foff = ftell(fp);
	if( (fwrite(spec,sizeof(float),nfreq,fp)) != nfreq) return (-2);
	free(fname);
	fclose(fp);
	return(foff);
}
/*
This function writes spectral estimates to output files and updates a
database table defining each spectral estimate.  The logical variable "jack"
controls what exactly is written.  If jack is 0, only the contents of the
array "s" are saved.  If jack is nonzero, upper and lower 95% 
confidence limits are also calculated. 
Args:
tr - db pointer to trace object from which this spectrum was derived
db - input database pointer
p - descriptor of this spectral window derived from recipe file 
s - float vector of length nfft/2+1 of spectral estimate
error-parallel array to s containing jackknife error estimates
nfft - size parameter for s
nsamples - length of original time series
dt - original times series sample interval
pick - base window epoch time (normally a measured arrival time )
dir - directory to write output files to
jack - logical switch (see above)\

Returns:
	0  - normal aok
	-1 - could not write spectral output files
	-2 - dvaddv failure in writing spectral file
	-3 - like -1 but for confidence limits file
	-4 - like -2 but for confidence limit db entry.
	-5 - error from dbgetv extracting parameters from database

Author:  Gary Pavlis
Written:  August 1994

*/
int save_spectrum(Dbptr db,
	Spectra_phase_specification *p,
	float *s,
	float *error,
	int nfft,
	int nsamples,
	float dt,
	double pick,
	char *dir,
	int correct,
	int jack)
{
	/* scratch variables */
	Dbptr dbsta,dbs;
	char string[128];
	int n;
	double today;
	char *today_time;
	char user[10];
	int i;

	/* These variables written to specdisc are assembled from
	variables passed to this routine */
	char sta[8],chan[10];
	int arid;
	char *phase;
	double tbp;


	/* These variables are set in this routine */
	double time,twin,endtime;
	int ondate,offdate;
	char auth[16];
	double freqmin=0.0,freqmax;
	int nfreq;
	double df;
	double rayleigh;
	double scalib;
	char spectype[10];
	char dfile[33];
	long foff;
	/* new attributes for specdisc that were not in older version */
	char net[10],rsptype[8];
	char units[15];
	int nwin;
	char rsprm[2];
	char *method="multitaper";
	char *specfmt="binary";

	/* First load the variables that come in pretty much 
	directly.  Some of this is slightly wasteful of 
	space, but it makes the code clearer regarding where
	items come from. */
	phase = strdup(p->phase_reference);
	tbp = p->tbwp;
	nwin = rint(tbp*2);

	/* We extract these items from the db.  This assumes that
	the db record pointer is set properly. Note we are getting the
	base dfile name from wfdisc dfile name.  Note also this
	assumes db points to the view that joins wfdisc and arrival 
	or we won't get an arid value.  Ditto for net */
	if((dbgetv(db,0,"sta",sta,"chan",chan,
		"dfile",dfile,"arid",&arid,"net",net,"segtype",rsptype,
		0)) == dbINVALID)
	{
		return(-5);
	}
	/* This is a crude way to do this, but the alternative requires 
	strong database constraints I don't think are wise.*/
	if(!strcmp(rsptype,"A"))
		strcpy(units,"nm/sec**2");
	else if(!strcmp(rsptype,"V"))
		strcpy(units,"nm/sec");
	else if(!strcmp(rsptype,"D"))
		strcpy(units,"nm");
	else
	{
		elog_notify(0,"save_spectrum:  %s:%s unknown segtype in wfdisc = %s\nAssuming units are velocity in nm/sec\n",
			sta,chan,rsptype);
		strcpy(units,"nm/sec");
		strcpy(rsptype,"V");
	}

	/* These quantities have to be calculated in various ways*/
	time = pick + (p->start);
	endtime = pick + (p->end);
	twin = (p->end) - (p->start);
	/* here we get the current time (GMT) using CSS time utiliites */
	today = now();
	today_time = epoch2str(today,"%y%j%k");
	cuserid(user);
	sprintf(auth,"dbmwspec:%7.7s:%7.7s",user,today_time);
	nfreq = nfft/2 + 1;
	df = 1.0/(dt*( (double) nfft));
	freqmax = freqmin + df*((double)(nfreq-1));
	rayleigh = 1.0/(dt*( (double) nsamples));
	scalib = 1.0;
	strcpy(spectype,"sp1c");
	if(correct)
		strcpy(rsprm,"y");
	else
		strcpy(rsprm,"n");

	/* dfile is produced by appending "_mwspec" to the dfile name used
	in wfdisc.  We do this here cautiously to avoid the special case
	when the dfile name is already 32 characters long (the size
	of the dfile file in css3.0. The magic number 
	25 = dimension of dfile (32) - strlen("_mwspec")*/
	if(strlen(dfile) > 25) dfile[25] = '\0';
	strcat(dfile,"_mwspec");

	/* Because we can append to dfile and get foff from write_spectrum
	we need to write the data first.  For this reason, however, if 
	dbaddv fails below it must be a fatal error to preventing endless
	quantities of garbage in the output files with no corresponding
	entries in the database. */

	if((foff=write_spectrum(s,nfreq,dir,dfile)) < 0)
	{
		/* this should also be a fatal error */
		return (-1);
	}
			

	db = dblookup(db,0,"specdisc",0,0);
	if(dbaddv(db,0,
			"net",net,
			"sta",sta,
			"chan",chan,
			"arid",arid,
			"rsptype",rsptype,
			"time",time,
			"endtime",endtime,
			"twin",twin,
			"nwin",nwin,
			"rsprm",rsprm,
			"phase",phase,
			"auth",auth,
			"freqmin",freqmin,
			"freqmax",freqmax,
			"nfreq",nfreq,
			"df",df,
			"rayleigh",rayleigh,
			"tbp",tbp,
			"scalib",scalib,
			"method",method,
			"spectype",spectype,
			"units",units,
			"specfmt",specfmt,
			"dir",dir,
			"foff", (int) foff,
				"dfile",dfile,0) < 0  )
	{
		return(-2);
	}

	/* now for error spectra.  Form low and highs */
	if(jack)
	{
		if((foff=write_spectrum(s,nfreq,dir,dfile)) < 0)
			return(-3);

		strcpy(spectype,"jker");
		if(dbaddv(db,0,
			"net",net,
			"sta",sta,
			"chan",chan,
			"time",time,
			"endtime",endtime,
			"phase",phase,
			"arid",arid,
			"rsptype",rsptype,
			"freqmin",freqmin,
			"freqmax",freqmax,
			"nfreq",nfreq,
			"df",df,
			"rayleigh",rayleigh,
			"tbp",tbp,
			"scalib",scalib,
			"twin",twin,
			"nwin",nwin,
			"rsprm",rsprm,
			"method",method,
			"spectype",spectype,
			"units",units,
			"specfmt",specfmt,
			"foff", (int) foff,
			"auth",auth,
			"dir",dir,
				"dfile",dfile,0) < 0  )
		{
			return(-4);
		}
	}
	free(phase);
	return(0);	
			
}
