/*

Binmay - command line binary search
Copyright (C) 2004 Sean Loaring

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.


email: sloaring at tec-man dot com

If you use this software then send me an email and tell me!

Changes:


050111: added patch from Bill Poser that allows input to be binary rather than hex

*/

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

//#define DEBUG(x) x;
#define DEBUG(x) ;

void use();

FILE *infile;
FILE *outfile;

#define BUF_LEN 1024
#define BLOCK_LEN 1024

char infilename[BUF_LEN];
char outfilename[BUF_LEN]; 

unsigned char replace_string[BUF_LEN];
int rlen=-1;
unsigned char search_string[BUF_LEN];
int slen=-1;

unsigned char splice_string[BUF_LEN];

unsigned char replace_mask[BUF_LEN];
int rmlen=0;
unsigned char search_mask[BUF_LEN];
int smlen=-1;


unsigned char pukebuf[BUF_LEN];
int plen=-1;

int verbose=0;
int UseBinaryP = 0;

void open_outfile( char *filename );
void open_infile( char *filename );

void flush_outbuf();
void flush_outbuf_block();

int b_getchar();
void do_search_replace();
void fill_ibuf();
void b_fwrite( unsigned char *buf, int len );
void process_string( unsigned char *str, int *len );

int matches=0;

int main( int argc, char **argv )
{
  char c;

  infile=stdin;
  outfile=stdout;

  char options[] = "bvp:S:R:i:o:s:r:";
  if( 1 == argc )
  {
	use();
	return 0;
  }

  while( (c=getopt( argc, argv, options )) >0 )
  {
    switch( c )
    {
      case 'b':
	UseBinaryP = 1;
	break;
      case 'i':
	strncpy( infilename, optarg, BUF_LEN);
	open_infile( infilename );
	break;
      case 'o':
	strncpy( outfilename, optarg, BUF_LEN );
	open_outfile( outfilename );
	break;
      case 'r':
	if( strlen( optarg ) > BUF_LEN )
	{
	  fprintf(stderr, "The replacement string is longer than the buffer :(\n");
	  fprintf(stderr, "You might have to modify BUF_LEN and recompile...sorry!\n");
	  exit(1);
	}
	strncpy( replace_string, optarg, BUF_LEN );
	process_string( replace_string, &rlen );
	break;
      case 's':

	if( strlen( optarg ) > BUF_LEN )
	{
	  fprintf(stderr, "The search string is longer than the buffer :(\n");
	  fprintf(stderr, "You might have to modify BUF_LEN and recompile...sorry!\n");
	  exit(1);
	}
	strncpy( search_string, optarg, BUF_LEN );
	process_string( search_string, &slen );
	break;

      case 'v':
	verbose++;
	break;

      case 'S':
	if( strlen( optarg ) > BUF_LEN )
	{
	  fprintf(stderr, "The search mask is longer than the buffer :(\n");
	  fprintf(stderr, "You might have to modify BUF_LEN and recompile...sorry!\n");
	  exit(1);
	}
	strncpy( search_mask, optarg, BUF_LEN );
	process_string( search_mask, &smlen );
	break;


      case 'R':
	if( strlen( optarg ) > BUF_LEN )
	{
	  fprintf(stderr, "The replace mask is longer than the buffer :(\n");
	  fprintf(stderr, "You might have to modify BUF_LEN and recompile...sorry!\n");
	  exit(1);
	}
	strncpy( replace_mask, optarg, BUF_LEN );
	process_string( replace_mask, &rmlen );
	break;

      case 'p':
	if( strlen( optarg ) > BUF_LEN )
	{
	  fprintf(stderr, "The replace mask is longer than the buffer :(\n");
	  fprintf(stderr, "You might have to modify BUF_LEN and recompile...sorry!\n");
	  exit(1);
	}
	strncpy( pukebuf, optarg, BUF_LEN );
	process_string( pukebuf, &plen );
	break;

      default:
      {
	fprintf(stderr, "Command line parser: I don't understand option '-%c'.\n", (unsigned int)c );
	fprintf(stderr, "Run with no options for help\n");
	exit(1);
      }
    }
  }


  //make a search mask if the user doesn't specify one
  if( slen > 0 && -1 == smlen )
  {
    smlen = slen;
    memset( search_mask, 0xff, smlen );
  }


  //some sanity checks

  if( slen != smlen )
  {
    fprintf(stderr, "Search mask length != search string length, I don't know what do do!\n");
    exit(1);
  }

  if( -1 != slen && -1 != rlen && ( rmlen > rlen || rmlen > slen ) )
  {
    fprintf(stderr,
      "Replace mask is longer than either either the search string or replacement string,\n"
      "ignoring extra mask.\n"
    );
    rmlen = (rlen>slen)?slen:rlen;
  }

  if( -1 != slen && -1 != plen )
  {
    perror( "I don't know how to puke and search at the same time.");
  }

  if( -1 != slen )
  {
    do_search_replace();

    if( verbose ) fprintf(stderr, " Done: %d matches.\n", matches );
  }

  if( -1 != plen )
  {
    if( verbose )fprintf( stderr, "Puking binary to output\n");
    fflush( stderr );
    if( 1 != fwrite( pukebuf, plen, 1, outfile ) )
    {
      fprintf(stderr, "Write error: \"%s\" :(\n", strerror( errno ));
      exit(1);
    }
  }


  return 0;
}


void open_infile( char *filename )
{

  fprintf( stderr, "Reading from  %s...\n", filename );
  if( NULL == ( infile = fopen( filename, "rb" ) ) )
  {
    fprintf(stderr, "Error opening input file %s\n", filename );
    exit(1);
  }
}


void open_outfile( char *filename )
{

  if( verbose ) fprintf( stderr, "Writing to %s...\n", filename );
  if( NULL == ( outfile = fopen( filename, "wb" ) ) )
  {
    fprintf(stderr, "Error opening output file %s\n", filename );
    exit(1);
  }
}

//how many times have I written this stupid function?
void hexdump( FILE *out, unsigned char *buf, int len )
{
  int i=0;

  for( i=0; i<len; i++ )
  {
    fprintf( out, "%02x ", (unsigned int)buf[i] );
  }

  fprintf( out, "\n" );
}

unsigned char ssbuf[BUF_LEN];

void do_search_replace()
{
  int ssoff=0;
  int chr;
  long unsigned int offset=0;
  for ( offset=0; -1 != ( chr = b_getchar( ) ); offset++ )
  {
    ssbuf[ssoff] = (unsigned char)chr;

    DEBUG(fprintf(stderr, "ssbuf[%d] == %x\n", ssoff, chr );)

    if( ( ssbuf[ssoff] & search_mask[ssoff] ) != ( search_string[ssoff] & search_mask[ssoff] ) )
    {
      DEBUG(fprintf( stderr, "   unmatch, dump %d\n", ssoff + 1 );)
      b_fwrite( ssbuf, ssoff + 1 );
      ssoff=0;
    }
    else
    {
      ssoff++;
      DEBUG(fprintf(stderr, "Matched %d of search string\n", ssoff );)

      if( ssoff == slen )
      {
	DEBUG(fprintf(stderr, "Matched entire search string.  Replacement time.\n");)
	matches++;

	if( verbose )
	{
	  fprintf( stderr, "match: 0x%08lx: ", offset );
	  hexdump( stderr, ssbuf, slen );
	}

	if( rlen >= 0 )
	{
	  int i=0;
	  if( 0 != rmlen )
	  {
	    //mask what we can
	    for( i=0; i< rmlen; i++ )
	      splice_string[i] = ((replace_string[i] & replace_mask[i]) | (ssbuf[i] & (~replace_mask[i])));

	    if( verbose > 3 )
	    {
	      fprintf(stderr, "source string:  ");
	      hexdump(stderr, ssbuf, ssoff );

	      fprintf(stderr, "replace string: ");
	      hexdump(stderr, replace_string, rlen );

	      fprintf(stderr, "replace mask:   ");
	      hexdump(stderr, replace_mask, rmlen );


	      fprintf(stderr, "result:         ");
	      hexdump(stderr, splice_string, rmlen );

	    }

	    b_fwrite( splice_string, rmlen );
	  }

	  DEBUG(fprintf( stderr, "** Writing %d\n", rlen - rmlen );)
	  b_fwrite( replace_string + rmlen, rlen - rmlen );

	  if( verbose > 2 )
	  {
	    fprintf( stderr, "replacement (mask): 0x%08lx: ", offset );
	    hexdump( stderr, splice_string, rmlen );

	    fprintf( stderr, "replacement (nonmask): 0x%08lx: ", offset );
	    hexdump( stderr, replace_string + rmlen, rlen - rmlen );
	  }
	}

	ssoff=0;
      }
    }

  }

  b_fwrite( ssbuf, ssoff );
  ssoff=0;
  flush_outbuf();
  fclose( outfile );
  fclose( infile );
  
}

#define OB_LENGTH BLOCK_LEN * 2

unsigned char outbuf[OB_LENGTH];
int obused=0;

unsigned char inbuf[ BLOCK_LEN ];
int iboff=0;
int ibavail=0;

int b_getchar()
{
  int result;
  if( iboff >= ibavail )
    fill_ibuf();

  if( iboff >= ibavail ) return -1;

  result=inbuf[iboff];

DEBUG(  fprintf(stderr, "b_getchar() returning %c\n", result ); )

  iboff++;

  return result;
}

void dump_buf( char*buf, int len)
{
  int i;
  fprintf(stderr, "DUMPING BUFFER------------------------------\n");
  fprintf( stderr, "%d >", len );
  for( i=0; i<len; i++)
  {
    fprintf( stderr, "%c", buf[i] );
  }


  fprintf( stderr, "<" );

  fprintf( stderr, "\n");

  fprintf(stderr, "--------------------------------------------------\n");
}

//WARNING: only call this if the buffer is ALL USED UP
void fill_ibuf()
{
  DEBUG(fprintf(stderr, "fill_ibuf() pre...%d %d\n", iboff, ibavail );)
  if( iboff != ibavail )
  {
    fprintf(stderr, "Odd, trying to fill_ibuf() before it needs it? %d\n", iboff);
    exit(1);
  }

  ibavail = fread( inbuf, 1, BLOCK_LEN, infile );

  iboff=0;

  DEBUG(fprintf(stderr, "fill_ibuf() post...%d %d\n", iboff, ibavail ););

  DEBUG( dump_buf( inbuf, ibavail ); )

}

void b_fwrite( unsigned char *buf, int len )
{

  DEBUG( fprintf(stderr, "b_fwrite called on: "); )
  DEBUG( dump_buf( buf, len ); )
  if( 0 == len ) return;
  if( len + obused > OB_LENGTH )
    flush_outbuf_block();

  memcpy( outbuf + obused, buf, len );
  obused += len;

}

void flush_outbuf_block()
{
  if( 1 != fwrite( outbuf, BLOCK_LEN, 1, outfile ) )
  {
    fprintf(stderr, "Nuts, write error (%s)!\n", strerror( errno ) );
    exit(1);
  }

  obused -= BLOCK_LEN;

  memcpy( outbuf, outbuf + BLOCK_LEN, obused );
}

void flush_outbuf()
{
  int result;
  if( 0 != obused && 1 != (result=fwrite( outbuf, obused, 1, outfile ) ) )
  {
    fprintf(stderr, "Nuts, write error (obused=%d, result=%d, err=%s)!\n", obused, result, strerror( errno ) );
  }
}


int hval( char c )
{
  switch( c )
  {
    case '0': return 0;
    case '1': return 1;
    case '2':return 2;
    case '3':return 3;
    case '4':return 4;
    case '5':return 5;
    case '6':return 6;
    case '7':return 7;
    case '8':return 8;
    case '9':return 9;
    case 'A':return 10;
    case 'B':return 11;
    case 'C':return 12;
    case 'D':return 13;
    case 'E':return 14;
    case 'F':return 15;
    default:
      fprintf(stderr, "INTERNAL ERROR: hval() got something weird: \"%c\" == %d \n", (unsigned int)c, (unsigned int)c);
      exit(1);
    
  }
}

int
isbdigit(char x)
{
  switch(x)
    {
    case '0':
    case '1':
      return (1);
    default:
      return(0);
  }
}

void
process_bin_string( unsigned char *str, int *len )
{
  int off;
  int noff;

  for( off=0; off<strlen( str ); off++ )
  {
    //clean out crap
    while( str[off] != 0 && ! isbdigit(str[off]) ) {
      for ( noff=off;str[noff] !=0; noff++ ){
	str[noff] = str[noff+1];
      }
    }
  }
  DEBUG( fprintf(stderr, "Cleanstr: %s\n", str ); )

  *len = strlen(str); 
  if( *len % 8 != 0 ){
    fprintf(stderr, "Error: invalid binary string: %s, length must be a multiple of 8.\n", str);
    exit(1);
  }
  *len = *len/8;
  DEBUG(fprintf(stderr, "len: %d\n", *len );)

  for( noff=0; noff < *len; noff++) {
    str[noff] =
      ((str[noff*2]  -0x30) << 7) +
      ((str[noff*2+1]-0x30) << 6) +
      ((str[noff*2+2]-0x30) << 5) +
      ((str[noff*2+3]-0x30) << 4) +
      ((str[noff*2+4]-0x30) << 3) +
      ((str[noff*2+5]-0x30) << 2) +
      ((str[noff*2+6]-0x30) << 1) +
        str[noff*2+7]-0x30;

    DEBUG(fprintf( stderr, "noff: %d\n", noff );)
    DEBUG(fprintf(stderr, "A val: %x\n", (unsigned int)str[noff]);)
  }
}

void
process_hex_string( unsigned char *str, int *len )
{
  int off;
  int noff;

  for( off=0; off<strlen( str ); off++ )
  {
    //clean out crap
    while( str[off] != 0 && ! isxdigit( str[off] ) )
    {
      for ( noff=off;str[noff] !=0; noff++ )
      {
	str[noff] = str[noff+1];
      }
    }

    //ucase the string
    str[off] = toupper(str[off]);
  }

  *len = strlen( str );

  DEBUG( fprintf(stderr, "Cleanstr: %s\n", str ); )

  //convert to values
  if( *len % 2 != 0 )
  {
    fprintf(stderr, "Error: invalid hex string: %s, length must be a multiple of two.\n", str );
    exit(1);
  }

  *len = *len/2;

  DEBUG(fprintf(stderr, "len: %d\n", *len );)

  for( noff=0; noff<*len; noff++)
  {
    str[noff] = (hval( str[noff*2] ) << 4) + hval( str[noff*2+1] );

    DEBUG(fprintf( stderr, "noff: %d\n", noff );)
    DEBUG(fprintf(stderr, "A val: %x\n", (unsigned int)str[noff]);)
  }

  DEBUG( fprintf(stderr, "process_string() Done\n"); );
}

void
process_string( unsigned char *str, int *len )
{
  if(UseBinaryP) process_bin_string(str,len);
  else process_hex_string(str,len);
}

void use()
{
	fprintf( stderr, 
	  "use: binmay [options] [-i infile] [-o outfile] -s search [-r replacement]\n"
	  "      search:        the string to search for\n"
	  "      replacement:   the string to replace \"search\" with \n"
	  "  options:\n"
	  "      -v             verbose\n"
	  "      -b             use binary rather than hex\n"
	  "      -i [infile]    specify input file (default: stdin)\n"
	  "      -p [hex]       puke raw binary\n"
	  "      -o [outfile]   specify output file (default: stdout)\n"
	  "      -s [hex]       string to search for (in hex, see below)\n"
	  "      -r [hex]       replacement string (in hex, see below)\n"
	  "      -S [hex]       search mask (see readme)\n"
	  "      -R [hex]       replace mask (see readme)\n"
	  "\n"
	  "  String format: hex, upper or lower case.  Non-hex is ignored\n"
	  "         This will work: \"021abf\"\n"
	  "         As will this: \"02 1a BF\"\n"
	  "         and this: \"02-1A-BF\"\n"
	  "         and even this: \"02----=+$#@1A-BF\"\n"
	  "            (though it's probably not a good idea)\n"
	);
}
