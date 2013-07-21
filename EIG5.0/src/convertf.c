#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <nicklib.h>
#include <getpars.h>

#include "admutils.h"
#include "mcio.h"  
#include "mcmcpars.h"  
#include "egsubs.h"  
#include "exclude.h" 

#define WVERSION   "3810" 
/** 
 reformats files.             
 pedfile junk (6, 7 cols, ACGT added)
 packped (.bed) added  input and output 
 pedfile can now be in random order
 tersemode supported 
 ped -> ancestrymap outputs alleles
 various fixups for ped files in computing variance, reference alleles 
 alleles can (say) be A X if second allele unknown
 allele flipped for bed format
 huge ascii file trapped now
 fastdup added
 r2thresh added
 killr2 added to convertf (default is NO)  
 bug in processing C0 SNPS in ped files fixed

 outputall added  (all indivs, snps output)
 genolistname added (list of packed ancestrymap files one per line)
 support for enhanced eigenstrat format and genolist can also be enhanced eigenstrat
 Chromosome # 1-89 now supported.  Improved treatment of funny chromosomes. 
 MT -> 90 
 XY -> 91

 Various bugfixes and checks for file size  
 inpack2 no longer needs big memory 
 hash table lookup for snpindex
 check size more carefully
 update to mcio.c  (snprawindex)
 maxmissfrac added

 poplistname added 
 remap (newsnpname) added
 flipsnpname, flipreference added  
 flipsnpname flips genotype values 
 if flipped we can flip reference allele too if we wish (default YES)
 badpedignore.  Crazy bases flagged as ignore
 remapind (newindivname) added
 flipstrandname moves allels to opposite strand
 maxmissing added (counts alleles like smartpca)
 minor bug fixed for plink homozygous files
 polarize added (force homozygotes to 2 if possible)
   if not possible SNP set -> ignore

 bigread now set 
 map files output 23, 24 for X, Y
 fastdupthresh, fastdupkill added
 remap now allows allele changes when genotypes are killed

 polarize added (homozygotes for ind -> 2) 
 zerodistance added 

 mcio now calls toupper on alleles
 bugfix for newsnpname + phasedmode

 downsample added (make pseudo homozygotes)
 chimpmode added support for chr2a etc

 support for snps out of order in packed format
 (pordercheck: NO) 
*/


#define MAXFL  50   
#define MAXSTR  512

char *trashdir = "/var/tmp" ;
int qtmode = NO ;
Indiv **indivmarkers, **indm2;
SNP **snpmarkers ;
SNP **snpm2 ;
int zerodistance = NO ; // YES => force gdis 0
int downsample = NO ;  // make pseudo homozygotes
int pordercheck = YES ;

int numsnps, numindivs, numind2 ; 
int nums2 ;

char  *genotypename = NULL ;
char  *genotypelist = NULL ;

char  *snpname = NULL ;
char *indoutfilename = NULL ;
char *snpoutfilename = NULL ;
char  *genooutfilename = NULL ;
char  *indivname = NULL ;
char  *newindivname = NULL ;
char *badsnpname = NULL ;
char *xregionname = NULL ;
char *deletesnpoutname = NULL ;
char *flipsnpname = NULL ;
char *flipstrandname = NULL ;
int flipreference = YES ;

char *poplistname = NULL ;

double r2thresh = -1.0 ;  
double r2genlim = 0.01 ; // Morgans 
double r2physlim = 5.0e6 ;     
double maxmissfrac = 1000.0 ; // no thresh
int maxmiss = -1  ; // no thresh
int killr2 = NO ;  
int mkdiploid = NO ;

int packout = -1 ;
int tersem  = YES ;
extern enum outputmodetype outputmode  ;
extern int checksizemode ;
extern int verbose;
extern int numchrom;
char *omode = "packedancestrymap" ;
extern int packmode ;
int ogmode = NO ;
int fastdup = NO ;
int fastdupnum = 10  ;
double fastdupthresh = .75  ;
double fastdupkill = .75  ;
char *polarid = NULL ;
int polarindex = -1 ;

int phasedmode = NO ;
int badpedignore = NO ;
int chimpmode = NO ;

int xchrom = -1 ;
int lopos = -999999999 ; 
int hipos = 999999999 ;
int minchrom = 1 ; 
int maxchrom = 97 ;

int deletedup = YES ; // only one marker at a position
char *newsnpname = NULL ;  // new map  


char  unknowngender = 'U' ;
double nhwfilter = -1 ;


void setomode(enum outputmodetype *outmode, char *omode)  ;
void readcommands(int argc, char **argv) ;
void outfiles(char *snpname, char *indname, char *gname, SNP **snpm, 
  Indiv **indiv, int numsnps, int numind, int packem, int ogmode) ;
void remap(SNP **s1, int nums1, SNP **s2, int nums2) ;
void remapind(SNP **snpmarkers, int numsnps, Indiv **indivmarkers, Indiv **indm2, int numindivs, int numind2) ;
void pickx(SNP *c1, SNP *c2, SNP **px1, SNP **px2) ;
void dedupit(SNP **snpmarkers, int numsnps) ;
void flipsnps(char *fsname, SNP **snpm, int numsnps, int phasedmode) ;
void flipstrand(char *fsname, SNP **snpm, int numsnps) ;
int  mkindh2d(Indiv **indivmarkers, Indiv ***pindm2, int numindivs) ;
void remaph2d(SNP **snpmarkers, int numsnps, Indiv **indivmarkers, Indiv **indm2, int numindivs, int numind2) ;
void flip1(SNP *cupt, int phasedmode, int flipreference) ;

void fixaa(SNP *cupt1, SNP *cupt2) ;
void fvalg(SNP *cupt, int val)   ;
char cxx(char *c1, char *c2)  ;
void downsamp(SNP *cupt) ;
 


int main(int argc, char **argv)
{

  char **eglist;
  int numeg;
  int i;
  SNP *cupt;
  Indiv *indx;
  int numvalidind;

  int nkill = 0 ;
  int t, k, g ;

  int nignore, numrisks = 1 ;

  char c1, c2 ;
  int t1, t2 ;

  malexhet = YES ;    // convertf default is don't change the data
  tersem = YES ;     // no snp counts

  readcommands(argc, argv) ;

  if (chimpmode) {  
   setchimpmode(YES) ;
   setchr(YES) ;
  }

  setomode(&outputmode, omode) ;
  packmode = YES ;
  settersemode(tersem) ;

  if (r2thresh > 0.0) killr2 = YES ;
  if (badpedignore) setbadpedignore() ;

  setpordercheck(pordercheck) ;

  numsnps = 
    getsnps(snpname, &snpmarkers, 0.0, badsnpname, &nignore, numrisks) ;

  for (i=0; i<numsnps; i++)  {  
   if (xchrom == -1) break ;  
   cupt = snpmarkers[i] ; 
   if (cupt -> chrom != xchrom) cupt -> ignore = YES ; 
   if (cupt -> ignore) continue ; 
   t = nnint(cupt -> physpos) ; 
   if ( (t< lopos) || (t >hipos)) cupt -> ignore = YES ;
  }

  nignore = 0 ;
  for (i=0; i<numsnps; i++)  {  
   cupt = snpmarkers[i] ; 
   if (cupt -> chrom > maxchrom) cupt -> ignore = YES ;  
   if (cupt -> chrom < minchrom) cupt -> ignore = YES ;  
   if (cupt -> ignore) ++nignore ;
  }

  if (numsnps == nignore) fatalx("no valid snps\n") ;


  numindivs = getindivs(indivname, &indivmarkers) ;
  if (polarid != NULL) {
   polarindex = indindex(indivmarkers, numindivs, polarid) ;
   if (polarindex<0) fatalx("polarid %s not found\n") ;
  }


  if (genotypelist!= NULL) {  
    getgenos_list(genotypelist, snpmarkers, indivmarkers, 
     numsnps, numindivs, nignore) ;
  }

  else {
   setgenotypename(&genotypename, indivname) ;
   getgenos(genotypename, snpmarkers, indivmarkers, 
     numsnps, numindivs, nignore) ;
  }

  if (newsnpname != NULL) { 
    nums2 = 
     getsnps(newsnpname, &snpm2, 0.0, NULL, &nignore, numrisks) ;
     remap(snpmarkers, numsnps, snpm2, nums2) ;
     snpmarkers = snpm2 ; 
     numsnps = nums2 ;
  }

  if (newindivname != NULL) { 
    numind2 = getindivs(newindivname, &indm2) ;
    remapind(snpmarkers, numsnps, indivmarkers, indm2, numindivs, numind2) ;
    indivmarkers = indm2 ;
    numindivs = numind2 ;
    if (polarid != NULL) {
     polarindex = indindex(indivmarkers, numindivs, polarid) ;
    }
  }

  if (mkdiploid) { 

    numindivs = rmindivs(snpmarkers, numsnps, indivmarkers, numindivs) ;
    numind2 = mkindh2d(indivmarkers, &indm2, numindivs) ;
    remaph2d(snpmarkers, numsnps, indivmarkers, indm2, numindivs, numind2) ;

    indivmarkers = indm2 ;
    numindivs = numind2 ;

  }


  if (deletedup) dedupit(snpmarkers, numsnps) ; // only one marker per position

  for (i=0; i<numsnps; i++)  {  
   cupt = snpmarkers[i] ;
   if (zerodistance) cupt -> genpos = 0.0 ;

   c1 = cupt -> alleles[0] ;  
   c2 = cupt -> alleles[1] ;  
   t1 = pedval(&c1) % 5 ;
   t2 = pedval(&c2) % 5 ;  // 0 and 5 are no good
   if ((t1==0) && (t2 >0)) flip1(cupt, phasedmode, YES) ;  
  }

  flipstrand(flipstrandname, snpmarkers, numsnps) ;
  flipsnps(flipsnpname, snpmarkers, numsnps, phasedmode) ;

  if (polarindex>=0) { 
    for (i=0; i<numsnps; i++)  {  
      cupt = snpmarkers[i] ;
      g = getgtypes(cupt, polarindex) ;
      if (g==0) { 
       printf("polarizing %s\n", cupt -> ID) ;
       flip1(cupt, NO, YES) ;
       g = getgtypes(cupt, polarindex) ;
       if (g!=2) fatalx("badbug\n") ;
      }
      if (g != 2) cupt -> ignore = YES ; 
    }
  }
  for (i=0; i<numsnps; i++)  {  
    cupt = snpmarkers[i] ;
    if (downsample) downsamp(cupt) ;
  }

  if (outputall) {
   outfiles(snpoutfilename, indoutfilename, genooutfilename, 
    snpmarkers, indivmarkers, numsnps, numindivs, packout, ogmode) ;

   printf("##end of convertf run (outputall mode)\n") ;
   return 0 ;
  }

  if (poplistname != NULL) 
  { 
    ZALLOC(eglist, numindivs, char *) ; 
    numeg = loadlist(eglist, poplistname) ;
    seteglist(indivmarkers, numindivs, poplistname);
    for (i=0; i<numindivs; ++i)  {     
     indx = indivmarkers[i] ; 
     if (indx -> affstatus == NO) indx -> ignore = YES ;
    }
  }
  else 
  setstatus(indivmarkers, numindivs, "Case") ;

  numsnps = rmsnps(snpmarkers, numsnps, deletesnpoutname) ;
  numindivs = rmindivs(snpmarkers, numsnps, indivmarkers, numindivs) ;

  if (killr2) {
   nkill = killhir2(snpmarkers, numsnps, numindivs, r2physlim, r2genlim, r2thresh) ;
   if (nkill>0) printf("killhir2.  number of snps killed: %d\n", nkill) ;
  }


  if ( nhwfilter > 0 )  {
    hwfilter(snpmarkers, numsnps, numindivs, nhwfilter, deletesnpoutname);
  }

  if ( xregionname )  {
    excluderegions(xregionname, snpmarkers, numsnps, deletesnpoutname);
  }


  numvalidind = 0 ;
  for (i=0; i<numindivs; ++i)  { 
   indx = indivmarkers[i] ;
   if (indx -> ignore) continue ; 
   if (numvalidgtind(snpmarkers, numsnps, i) ==0) { 
    indx -> ignore = YES ; 
    printf("no data for individual: %s\n", indx -> ID) ;
   }
   if (indx -> ignore == NO) ++numvalidind ;
  }

  if (maxmiss<0) maxmiss  = (int) (maxmissfrac * (double) numvalidind+1) ;
  printf("numvalidind:  %5d  maxmiss: %5d\n", numvalidind, maxmiss)  ;
  if (numvalidind  == 0) fatalx("no valid samples!\n") ;

  for (k=0; k<numsnps; ++k) {  
   if (maxmiss>numvalidind) break ;
   cupt = snpmarkers[k] ;
   t = numvalidind - numvalidgtypes(cupt) ;
// printf("zz %20s %4d %4d\n", cupt -> ID, t, numvalidind-t) ;
   if (maxmiss < t) { 
    cupt -> ignore = YES ;
   }
/**
   if (numvalidind ==  t) { 
    printf("no data for snp: %s\n", cupt -> ID) ;
    cupt -> ignore = YES ;
   }
*/

  }

  if (fastdup)  {  

   printf("fastdup set %d\n", fastdupnum) ;
   if (fastdupnum > 0) {
     setfastdupnum(fastdupnum) ;
     setfastdupthresh(fastdupthresh, fastdupkill) ;
     fastdupcheck(snpmarkers, indivmarkers, numsnps, numindivs) ;  
   }
  }

  if (decim>0) {  
   snpdecimate(snpmarkers, numsnps, decim, dmindis, dmaxdis) ;
  }

  outfiles(snpoutfilename, indoutfilename, genooutfilename, 
   snpmarkers, indivmarkers, numsnps, numindivs, packout, ogmode) ;

  printf("##end of convertf run\n") ;
  return 0 ;
}

void readcommands(int argc, char **argv) 

{
  int i;
  char *parname = NULL ;
  phandle *ph ;

  while ((i = getopt (argc, argv, "p:vV")) != -1) {

    switch (i)
      {

      case 'p':
	parname = strdup(optarg) ;
	break;

      case 'v':
	printf("version: %s\n", WVERSION) ; 
	break; 

      case 'V':
	verbose = YES ;
	break; 

      case '?':
	printf ("Usage: bad params.... \n") ;
	fatalx("bad params\n") ;
      }
  }

         
   pcheck(parname,'p') ;
   printf("parameter file: %s\n", parname) ;
   ph = openpars(parname) ;
   dostrsub(ph) ;

/**
DIR2:  /fg/nfiles/admixdata/ms2
SSSS:  DIR2/outfiles 
genotypename: DIR2/autos_ccshad_fakes
eglistname:    DIR2/eurlist  
output:        eurout
*/
   getstring(ph, "genotypename:", &genotypename) ;
   getstring(ph, "genotypelist:", &genotypelist) ;
   getstring(ph, "snpname:", &snpname) ;
   getstring(ph, "indivname:", &indivname) ;
   getstring(ph, "badsnpname:", &badsnpname) ;
   getstring(ph, "flipsnpname:", &flipsnpname) ;
   getstring(ph, "flipstrandname:", &flipstrandname) ;
   getstring(ph, "indoutfilename:", &indoutfilename) ;
   getstring(ph, "indivoutname:", &indoutfilename) ; /* changed 11/02/06 */
   getstring(ph, "snpoutfilename:", &snpoutfilename) ;
   getstring(ph, "snpoutname:", &snpoutfilename) ; /* changed 11/02/06 */
   getstring(ph, "genooutfilename:", &genooutfilename) ; 
   getstring(ph, "genotypeoutname:", &genooutfilename) ; /* changed 11/02/06 */
   getstring(ph, "outputformat:", &omode) ;
   getstring(ph, "outputmode:", &omode) ;
   getstring(ph, "polarize:", &polarid) ;
   getint(ph, "zerodistance:", &zerodistance) ;
   getint(ph, "checksizemode:", &checksizemode) ;
   getint(ph, "badpedignore:", &badpedignore) ;
   getint(ph, "downsample:", &downsample) ;
   getint(ph, "chimpmode:", &chimpmode) ;
   getint(ph, "pordercheck:", &pordercheck) ;

   getint(ph, "numchrom:", &numchrom) ;
   getstring(ph, "xregionname:", &xregionname) ;
   getdbl(ph, "hwfilter:", &nhwfilter) ;
   getstring(ph, "deletesnpoutname:", &deletesnpoutname);

   getint(ph, "outputgroup:", &ogmode) ;
   getint(ph, "malexhet:", &malexhet) ;
   getint(ph, "nomalexhet:", &malexhet) ; /* changed 11/02/06 */
   getint(ph, "tersemode:", &tersem) ;
   getint(ph, "familynames:", &familynames) ;
   getint(ph, "packout:", &packout) ; /* now obsolete 11/02/06 */
   getint(ph, "decimate:", &decim) ; 
   getint(ph, "dmindis:", &dmindis) ; 
   getint(ph, "dmaxdis:", &dmaxdis) ; 
   getint(ph, "fastdup:", &fastdup) ;
   getint(ph, "flipreference:", &flipreference) ;
   getint(ph, "fastdupnum:", &fastdupnum) ;
   getdbl(ph, "fastdupthresh:", &fastdupthresh) ;
   getdbl(ph, "fastdupkill:", &fastdupkill) ;
   getint(ph, "killr2:",  &killr2) ;
   getint(ph, "hashcheck:", &hashcheck) ;
   getint(ph, "outputall:", &outputall) ;
   getint(ph, "sevencolumnped:", &sevencolumnped) ;
   getint(ph, "phasedmode:", &phasedmode) ;

   getdbl(ph, "r2thresh:", &r2thresh) ;
   getdbl(ph, "r2genlim:", &r2genlim) ;
   getdbl(ph, "r2physlim:", &r2physlim) ;

   getint(ph, "chrom:", &xchrom) ;
   getint(ph, "lopos:", &lopos) ;
   getint(ph, "hipos:", &hipos) ;

   getint(ph, "minchrom:", &minchrom) ;
   getint(ph, "maxchrom:", &maxchrom) ;
   getdbl(ph, "maxmissfrac:", &maxmissfrac) ;
   getint(ph, "maxmissing:", &maxmiss) ;
   
   getstring(ph, "poplistname:", &poplistname) ;
   getstring(ph, "newsnpname:", &newsnpname) ;
   getstring(ph, "newindivname:", &newindivname) ;
   getint(ph, "deletedup:", &deletedup) ;
   getint(ph, "mkdiploid:", &mkdiploid) ;

   writepars(ph) ;
   closepars(ph) ;

}
void remap(SNP **s1, int nums1, SNP **s2, int nums2) 
{
  SNP *cupt1, *cupt2 ; 
  SNP tcupt, *cupt ;
  int i, k ;
  
  for (i=0; i<nums2; ++i)  { 
    cupt2 = s2[i] ; 
    k = snpindex(s1, nums1, cupt2 -> ID) ; 
    if (k<0) { 
      printf("%20s not found\n", cupt2->ID) ; 
      cupt2 -> ignore = YES ;
      continue ;
    }
    cupt1 = s1[k] ; 
    fixaa(cupt1, cupt2) ;
    if (cupt1 -> alleles[1] == 'X') {
      cupt1 -> alleles[1] = cxx(cupt1 -> alleles, cupt2 -> alleles) ;
    }
    tcupt = *cupt2 ;
    *cupt2 = *cupt1 ;
    cupt = &tcupt ;
    cupt2 -> chrom =   cupt -> chrom ; 
    cupt2 -> genpos =  cupt -> genpos ; 
    cupt2 -> physpos = cupt -> physpos ; 
    cupt2 -> alleles[0] = cupt -> alleles[0] ; 
    cupt2 -> alleles[1] = cupt -> alleles[1] ; 
  }
  freesnpindex() ;
}
char cxx(char *c1, char *c2) 
{
 if (c1[0] == c2[0]) return c2[1] ; 
 if (c1[0] == c2[1]) return c2[0] ;
 // okay, I assume this can never happen, but just to be safe...
 // (if this really can never happen, the if (c1[0] == c2[1]) check should be
 // removed)
 printf("Internal error in convertf.c cxx(): c1[0]=%c, c2[0]=%c, c2[1]=%c.\n", c1[0], c2[0], c2[1]);
 exit(1);
 return 0; // shut up the compiler
}
void fixaa(SNP *cupt1, SNP *cupt2) 
{
 char *c1, *c2 ; 
 int t ;
 
 c1 = cupt1 -> alleles ;
 c2 = cupt2 -> alleles ;

 if ((c1[1] == 'X') && (c1[0] == c2[0])) c1[1] = c2[1] ;
 if ((c1[1] == 'X') && (c1[0] == c2[1])) c1[1] = c2[0] ;
 if ((c2[1] == 'X') && (c1[0] == c2[0])) c2[1] = c1[1] ;
 if ((c2[1] == 'X') && (c1[1] == c2[0])) c2[1] = c1[0] ;
 if ((c1[0] == c2[0]) && (c1[1] == c2[1])) return ; 

 if ((c1[0] == c2[1]) && (c1[1] == c2[0])) { 
   flip1(cupt1, phasedmode, YES) ; 
   return ; 
 }
 t = 999 ;
 if ((c1[0] == c2[0]) && (c1[1] != c2[1])) { 
  t = 2 ;
  if (phasedmode) t=1 ;
 }
 if ((c1[1] == c2[1]) && (c1[0] != c2[0])) { 
  t = 0 ;
  fvalg(cupt1, 0) ; // only valid genotype ;
  return ;
 }
 fvalg(cupt1, t) ; // force all snps invalid
 if (t==999) {
  printf("forcing all genos invalid for %s %c %c     %c %c\n",cupt1 -> ID, c1[0], c1[1], c2[0], c2[1]) ;
 }
}

void downsamp(SNP *cupt) 
{
 int k, g, t;
 static int ncall = 0  ;

 ++ncall ;
 if (ncall == 1) {
   SRAND(77) ;
   printf("downsample set\n") ;
 }

 for (k=0; k<numindivs; ++k) { 
  g = getgtypes(cupt, k) ;
  if (g == 1 ) {
   t = ranmod(2) ;
   putgtypes(cupt, k, 2*t) ;
  }
 }
}

void fvalg(SNP *cupt, int val)  
{
 int k, g ;

 for (k=0; k<numindivs; ++k) { 
  g = getgtypes(cupt, k) ;
  if (g!= val) putgtypes(cupt, k, -1) ;
 }
}


void 
remapind(SNP **snpmarkers, int numsnps, Indiv **indivmarkers, Indiv **indm2, int numindivs, int numind2) 

{

  int *g1, *g2, *w1 ;
  int *tind, t, i, j, k ; 
  Indiv *indx ;
  SNP *cupt ;

  if (numindivs != numind2) fatalx("different remapind sizes %d %d\n", numindivs, numind2) ;
  ZALLOC(tind, numind2, int) ;
  ZALLOC(g2, numind2, int) ;
  ZALLOC(g1, numind2, int) ;
  ZALLOC(w1, numind2, int) ;

  for (k=0; k<numind2; ++k) { 
   indx = indm2[k] ;   
   t = tind[k] = indindex(indivmarkers, numindivs, indx -> ID) ;
   if (t<0) fatalx("bad newindiv: %s\n", indx -> ID) ;
  }

  for (i=0; i<numsnps; i++)  { 
   cupt = snpmarkers[i]  ;

   for (j=0; j<numind2; ++j) { 
    g1[j] = getgtypes(cupt, j) ;
   }
   copyiarr(g1, w1, numind2) ;

   for (k=0; k< numind2; ++k)  { 
     g2[k] = g1[tind[k]]  ;
   }

   ivclear(g1, -1, numind2) ;
   copyiarr(g2, g1, numind2) ;

   for (k=0; k<numind2; ++k) { 
    putgtypes(cupt, k, g1[k]) ;
   }

/**
   if (i<100) { 
    printf("zzz %s\n", cupt -> ID) ;
    for (j=0; j<numind2; ++j) { 
     g1[j] = getgtypes(cupt, j) ;
    }
    printimat(w1, 1, numind2) ;
    printimat(g1, 1, numind2) ;
   }
*/

  }

  free(w1) ; 
  free(g1) ; 
  free(g2) ;
  free(tind) ;

}



void  dedupit(SNP **snpmarkers, int numsnps) 
{
  SNP *cupt1, *cupt2 ;   
  SNP *x1, *x2 ;   
  int k ;

  cupt1 = NULL ;

  for (k=0; k<numsnps; ++k) { 
   cupt2 = snpmarkers[k] ;
   if (cupt2 -> ignore) continue ;
   if (cupt1 == NULL) { 
    cupt1 = cupt2 ;
    continue   ;
   }
   if (cupt1 -> chrom != cupt2 -> chrom) { 
    cupt1 = cupt2 ;
    continue   ;
   }
   if (cupt1 -> physpos != cupt2 -> physpos) { 
    cupt1 = cupt2 ;
    continue   ;
   }
   pickx(cupt1, cupt2, &x1, &x2) ;  // x2 bad
   x2 -> ignore = YES ;
   cupt1 = x1 ; 
  }
}
void pickx(SNP *c1, SNP *c2, SNP **px1, SNP **px2) 
{
// *px1 is retained *px2 dropped; Try and keep shorter rsnames (strip AFFX for instance)

  char *ch1, *ch2 ;
  int l1, l2, t ;
   
   ch1 = c1 -> ID ;
   ch2 = c2 -> ID ;
   l1 = strlen(ch1) ;
   l2 = strlen(ch2) ;

   if (l1<l2) { 
    *px2 = c2 ; 
    *px1 = c1 ; 
    return ;
   }
   if (l2<l1) { 
    *px2 = c1 ; 
    *px1 = c2 ; 
    return ;
   }
   t = strcmp(ch1, ch2) ;
   if (t>=0) { 
    *px2 = c2 ; 
    *px1 = c1 ; 
    return ;
   }
   *px2 = c1 ; 
   *px1 = c2 ; 
   return ;
}

void flipstrand(char *fsname, SNP **snpm, int numsnps) 
// move alleles to opposite strand
{
  FILE *fff ;
  char line[MAXSTR] ;
  char *spt[MAXFF] ;
  int nsplit, k ;
  SNP *cupt ; 

  if (fsname == NULL) return ;
  openit (fsname, &fff, "r") ;

  freesnpindex() ;

  while (fgets(line, MAXSTR, fff) != NULL)  {
    nsplit = splitup(line, spt, MAXFF) ; 
    if (nsplit==0) continue ;
    if (spt[0][0] == '#') { 
     freeup(spt, nsplit) ;
     continue ;
    }
    
     k = snpindex(snpm, numsnps, spt[0]) ;      
     if (k>=0) {
      cupt = snpm[k] ;
      cupt -> alleles[0] = compbase(cupt -> alleles[0]) ;
      cupt -> alleles[1] = compbase(cupt -> alleles[1]) ;
     }
    freeup(spt, nsplit) ;
  }
  fclose (fff) ;
}
void flipsnps(char *fsname, SNP **snpm, int numsnps, int phasedmode) 
{
  FILE *fff ;
  char line[MAXSTR] ;
  char *spt[MAXFF] ;
  int nsplit, k ;

  if (fsname == NULL) return ;
  openit (fsname, &fff, "r") ;

  freesnpindex() ;

  while (fgets(line, MAXSTR, fff) != NULL)  {
    nsplit = splitup(line, spt, MAXFF) ; 
    if (nsplit==0) continue ;
    if (spt[0][0] == '#') { 
     freeup(spt, nsplit) ;
     continue ;
    }
    
     k = snpindex(snpm, numsnps, spt[0]) ;      
     if (k>=0) {
      flip1(snpm[k], phasedmode, flipreference) ;
     }
    freeup(spt, nsplit) ;
  }
  fclose (fff) ;
}

void flip1(SNP *cupt, int phasedmode, int flipreference) 
{
      if (phasedmode == NO)  flipalleles(cupt) ;
      if (phasedmode == YES)  flipalleles_phased(cupt) ;
// just flips genotypes  
      if (flipreference) cswap(&cupt -> alleles[0], &cupt -> alleles[1]) ;
}
