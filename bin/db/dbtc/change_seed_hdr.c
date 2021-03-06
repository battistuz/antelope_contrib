/*
 *
 *************************************************************************/
#include "dbtc.h"
 
#define TIM_OFF  20
#define DATA_OFF 44


 
 
       if( (dbgetv( dba, 0, "time", &stime, 0 )) == dbINVALID )  
           die( 0, "dbgetv faild for record #%d\n", dba.record );  
       
       for( dbc.record = 0; dbc.record < dbrec; dbc.record++) {
	            
	    if( (dbgetv( db, 0,
	         "time", &ctime,
	         "tcorr", &tcorr,
	         "bchan", chan,
	         "bnet", net,
	         "bsta", sta, 0 )) == dbINVALID )
	          die( 0, "dbgetv faild for record #%d\n", dbc.record );
	        
	    sprintf( key, "%s_%s_%s\0", net, sta, chan );

	    if( !regexec( &srcmatch, key, (size_t) 0, NULL, 0 ) )  {
	          if( prev_time == 0.0 ) prev_time = ctime;
	          if( stime < ctime )  {
			stime = prev_time + prev_tcor;
			break;
		  }
	    }
       }
    }
  
    dbclose(dbout);
  
  return 1;
}

int change_arr( int dbrec, int wfrec )
{
    double stime, ctime, tcorr;
    char newname[512], key[32];
    char sta[8], chan[12], net[12];
    Dbptr dbout;
    double samprate, crnt_time, rectime;
    ushort_t doff;
    int dcode, foff, wfdoff; 
    int dbrec, seed=0;
    char datatype[4], sta[8], ssta[8], chan[12], schan[4];
    char  dfname[512]; 
    char tmpname[512], str[512];
    char Dfile[64], dbname[132];
    char newnm[132];
    int done=0, nread,nleft, i;
    char *seed_buf=0, *rec=0;
    FILE *infp=0, *outfp=0;
 
 
     
    if (dbopen_database ( NEW, "r+", &dbout ) == dbINVALID )
         die (0, "Can't open database %s\n",  NEW );
 
    dbout = dblookup (dbout, 0, "wfdisc", 0, 0);
    if (dbout.table < 0)  {
        die (0, "Can't open wfdisc table '%s'\n", dbname );
    }
    
    
    sprintf(Dfile , "%s.w\0", dbname);
 
    wfdoff = 0;
    for( dbw.record = 0, dbout.record=0; dbw.record < wfrec; dbw.record++, dbout.record++)   {
 
       if( (dbgetv( dbin, 0,
          "time", &stime,
          "endtime", &endtime,
          "foff", &foff,
          "samprate", &samprate,
          "sta", sta,
          "chan", chan,
          "datatype", datatype,
           0 )) == dbINVALID )
           die( 0, "dbgetv faild for record #%d\n", dbin.record );

       dcode = trdatacode ( datatype);
       if( Log )  {
           fprintf( stdout, "fixing db records for %s_%s (%lf %lf) ", 
                                sta, chan, stime, endtime );
           if( dcode == trSEED) 
              fprintf( stdout, "and time in a SEED header.\n");

           fflush(stdout);
       }
       if( dcode == trSEED )  {
          
          if( rec == 0 ) allot( char *, rec, 64 );
          if( seed_buf == 0 ) allot( char *, seed_buf, 4096 );
 
          if(!seed )  {
             sprintf( tmpname, "tmp.w\0" );
             outfp = fopen (tmpname, "w");
             if (outfp == 0) die ( 1, "Unable to open '%s'\n", tmpname);
             seed = 1;
          }
          if (dbfilename (dbin, dfname) < 1) 
                  die (1, "Unable to find input file '%s'\n", dfname);
          if( strncmp( old_fname, dfname, sizeof(dfname) ) ) {
             if(infp != 0 )  fclose(infp);
             infp = zopen (dfname);
             if (infp == 0) 
                  die ( 1, "Unable to open '%s'\n", dfname);
          } 
          strcpy( old_fname, dfname);
          if (fseek(infp, foff, 0) < 0) {
                fclose (infp);
                die ( 0, "fseek() error on '%s'\n", dfname);
          }  
 
      
          while( !done )  {
 
              if ( (nread=fread ( seed_buf, 1, 48, infp)) != 48 ) {
                    if( nread == 0 ) break;
                    else  die( 1, "Can't read first 48 bytes of a SEED record.\n" );
              }
              memcpy(ssta,  seed_buf+8, 5 ) ;
              TRIM(ssta,6);
     
              memcpy(schan,  seed_buf + 15, 3 ) ;
              TRIM(schan,4);
                    
              crnt_time = seedtime( seed_buf );
              rectime = crnt_time + timerr ;
    
              if( strncmp(sta, ssta, sizeof(sta) ) || 
                    strncmp(chan, schan, sizeof(chan) ) ||  
                    ( crnt_time - endtime ) > 1.0/samprate ) {
                    break;        
              } else { 
                    memcpy( (char *) &doff, seed_buf+DATA_OFF, 2);
                    change_time( seed_buf, rectime, nread, &rec );
  
                    if( !wrt_data( outfp, rec, 48) )  
                        die( 1, "write error for %s_%s SEEDHDR. \n", sta, chan );  
                
                    nleft = doff - 48;
                    if( doff <= 0 ) break;   
                    if ( (nread=fread ( seed_buf, 1, nleft, infp)) != nleft ) 
                          die( 1, "Can't read SEED headers.\n" );
                    if( !wrt_data( outfp, seed_buf, nread ) )  
                          die( 1, "write error for %s_%s SEEDHDR.\n", sta, chan);  
               
                    nleft = 4096 - doff; 
                    if ( (nread=fread ( seed_buf, 1, nleft, infp)) != nleft ) 
                          die( 1, "Can't read SEED headers.\n" );
                    if( !wrt_data( outfp, seed_buf, nread) )  
                          die( 1, "write error for %s_%s at %lf.\n", sta, chan, rectime );  
             
              }

              if( Log )  {
                  fprintf( stderr, "sta=%s chan=%s time=%lf databytes:%d total_bytes:%ld\n", 
                                    ssta, schan, rectime, nread, Foff );
                  fflush(stdout);
              } 
          }
 
     } else wfdoff = foff; 
         
     dbputv( dbout, 0,       
             "dfile", Dfile,
             "dir", ".",
             "foff", wfdoff,
             "time", stime + timerr,
             "endtime", endtime + timerr, 0);
     wfdoff = Foff;
  }
  
  fclose( outfp );
  fclose(infp);
 
  dbclose( dbin); dbclose(dbout);
  
  if(seed )  {
 
    sprintf( newnm, "%s.w\0", dbname );
    if( stat( newnm, &stat_buf) != 0 ) {
        if(!ENOENT) die(1, "stat error\n");
    }  else {
        sprintf( str, "mv %s.w old_%s.w\0", dbname, dbname );
        if( system(str) == -1 ) 
          die( 1, "can't save an original data file - %s.w\n", set->dbname );
    
    } 
 
    sprintf( str, "mv %s %s\0", tmpname, Dfile );
    if( system(str) == -1 ) 
       die( 1, "can't rename new data file - %s\n", Dfile);
    
  }
  return 1;
}

