#define _GNU_SOURCE
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <ftw.h>
#include <libgen.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utime.h>
#include <curl/curl.h>

#include "include/version.h"
#include "include/strings.c"

#include "common.h"

#define LINEBUFFER 256

/*===========================================================================*/
static bool downloadoui(const char *ouiname)
{
CURLcode ret;
CURL *hnd;

FILE* fhoui;

printf("start downloading oui from http://standards-oui.ieee.org to: ~/%s\n", ouiname);

if((fhoui = fopen(ouiname, "w")) == NULL)
	{
	fprintf(stderr, "error creating file %s", ouiname);
	exit(EXIT_FAILURE);
	}

hnd = curl_easy_init ();
curl_easy_setopt(hnd, CURLOPT_URL, "http://standards-oui.ieee.org/oui.txt");
curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 5L);
curl_easy_setopt(hnd, CURLOPT_WRITEDATA, fhoui) ;

ret = curl_easy_perform(hnd);
curl_easy_cleanup(hnd);
fclose(fhoui);
if(ret != 0)
	{
	fprintf(stderr, "download not successful");
	exit(EXIT_FAILURE);
	}

printf("download finished\n");
return true;
}
/*===========================================================================*/
static size_t chop(char *buffer,  size_t len)
{
char *ptr = buffer +len -1;

while (len) {
	if (*ptr != '\n') break;
	*ptr-- = 0;
	len--;
	}

while (len) {
	if (*ptr != '\r') break;
	*ptr-- = 0;
	len--;
	}
return len;
}
/*---------------------------------------------------------------------------*/
static int fgetline(FILE *inputstream, size_t size, char *buffer)
{
if (feof(inputstream)) return -1;
		char *buffptr = fgets (buffer, size, inputstream);

	if (buffptr == NULL) return -1;

	size_t len = strlen(buffptr);
	len = chop(buffptr, len);

return len;
}
/*===========================================================================*/
static void getessidinfo(char *essidname)
{
static int l, p;
uint8_t essidbuffer[66];

l = strlen(essidname);
if((l < 2) || (l > 64))
	{
	fprintf(stderr, "not a valid ESSID hex string\n");
	return;
	}
if((l %2) != 0)
	{
	fprintf(stderr, "not a valid hex string\n");
	return;
	}
for(p = 0; p < l; p++)
	{
	if(!isxdigit(essidname[p]))
		{
		fprintf(stderr, "not a valid hex string\n");
		return;
		}
	}

memset(&essidbuffer, 0, 66);
if(hex2bin(essidname, essidbuffer, l /2) == false)
	{
	fprintf(stderr, "not a valid ESSID hex string\n");
	return;
	}
fprintf(stdout, "%s\n", essidbuffer);
return;
}
/*===========================================================================*/
static void gethexessidinfo(char *hexessidname)
{
static int l, p;
l = strlen(hexessidname);
for(p = 0; p < l; p++)
	{
	fprintf(stdout, "%02x", hexessidname[p]);
	}
fprintf(stdout, "\n");
return;
}
/*===========================================================================*/
static void get16800info(const char *ouiname, char *hash16800line)
{
int len;
int l, l1;
FILE* fhoui;
char *vendorptr;
char *essidptr;
char *passwdptr;

unsigned long long int macap;
unsigned long long int macsta;
unsigned long long int ouiap;
unsigned long long int ouista;
unsigned long long int vendoroui;

char linein[LINEBUFFER];
uint8_t essidbuffer[66];
char vendorapname[256];
char vendorstaname[256];

sscanf(&hash16800line[33], "%12llx", &macap);
ouiap = macap >> 24;
sscanf(&hash16800line[46], "%12llx", &macsta);
ouista = macsta >> 24;

essidptr = hash16800line +59;
l = strlen(essidptr);

passwdptr = strrchr(hash16800line, ':');
if(passwdptr != NULL)
	{
	l1 = strlen(passwdptr);
	if(l1 > 1)
		{
		l -= l1;
		}
	}
if((l%2 != 0) || (l > 64))
	{
	fprintf(stderr, "wrong ESSID length %s\n", essidptr);
	return;
	}
memset(&essidbuffer, 0, 66);
if(hex2bin(essidptr, essidbuffer, l /2) == false)
	{
	fprintf(stderr, "wrong ESSID %s\n", essidptr);
	return;
	}

if ((fhoui = fopen(ouiname, "r")) == NULL)
	{
	fprintf(stderr, "unable to open database %s\n", ouiname);
	exit (EXIT_FAILURE);
	}

strncpy(vendorapname, "unknown", 8);
strncpy(vendorstaname, "unknown", 8);

while((len = fgetline(fhoui, LINEBUFFER, linein)) != -1)
	{
	if (len < 10)
		continue;
	if(strstr(linein, "(base 16)") != NULL)
		{
		sscanf(linein, "%06llx", &vendoroui);
		if(ouiap == vendoroui)
			{
			vendorptr = strrchr(linein, '\t');
			if(vendorptr != NULL)
				{
				strncpy(vendorapname, vendorptr +1,255);
				}
			}
		if(ouista == vendoroui)
			{
			vendorptr = strrchr(linein, '\t');
			if(vendorptr != NULL)
				{
				strncpy(vendorstaname, vendorptr +1,255);
				}
			}
		}
	}
if(isasciistring(l /2, essidbuffer) == true)
	{
	fprintf(stdout, "\nESSID..: %s\n", essidbuffer);
	}
else
	{
	fprintf(stdout, "\nESSID..: $HEX[%s]\n", essidbuffer);
	}

fprintf(stdout, "MAC_AP.: %012llx\n"
		"VENDOR.: %s\n"
		"MAC_STA: %012llx\n"
		"VENDOR.: %s\n\n"
		, macap, vendorapname, macsta, vendorstaname);

fclose(fhoui);
return;
}
/*===========================================================================*/
static void get2500info(const char *ouiname, char *hash2500line)
{
int len;
int l, l1;
FILE* fhoui;
char *vendorptr;
char *essidptr;
char *passwdptr;

unsigned long long int macap;
unsigned long long int macsta;
unsigned long long int ouiap;
unsigned long long int ouista;
unsigned long long int vendoroui;

char linein[LINEBUFFER];
uint8_t essidbuffer[72];
char vendorapname[256];
char vendorstaname[256];

sscanf(&hash2500line[33], "%12llx", &macap);
ouiap = macap >> 24;
sscanf(&hash2500line[46], "%12llx", &macsta);
ouista = macsta >> 24;

essidptr = hash2500line +59;
l = strlen(essidptr);
passwdptr = strrchr(hash2500line, ':');
if((passwdptr -hash2500line) > 59)
	{
	if(passwdptr != NULL)
		{
		l1 = strlen(passwdptr);
		if(l1 > 1)
			{
			l -= l1;
			}
		}
	}
if(l > 70)
	{
	fprintf(stderr, "wrong ESSID length %s %d\n", essidptr, l);
	return;
	}
memset(&essidbuffer, 0, 72);
memcpy(&essidbuffer, essidptr, l);

if ((fhoui = fopen(ouiname, "r")) == NULL)
	{
	fprintf(stderr, "unable to open database %s\n", ouiname);
	exit (EXIT_FAILURE);
	}

strncpy(vendorapname, "unknown", 8);
strncpy(vendorstaname, "unknown", 8);

while((len = fgetline(fhoui, LINEBUFFER, linein)) != -1)
	{
	if (len < 10)
		continue;
	if(strstr(linein, "(base 16)") != NULL)
		{
		sscanf(linein, "%06llx", &vendoroui);
		if(ouiap == vendoroui)
			{
			vendorptr = strrchr(linein, '\t');
			if(vendorptr != NULL)
				{
				strncpy(vendorapname, vendorptr +1,255);
				}
			}
		if(ouista == vendoroui)
			{
			vendorptr = strrchr(linein, '\t');
			if(vendorptr != NULL)
				{
				strncpy(vendorstaname, vendorptr +1,255);
				}
			}
		}
	}
fprintf(stdout, "\nESSID..: %s\n", essidbuffer);
fprintf(stdout, "MAC_AP.: %012llx\n"
		"VENDOR.: %s\n"
		"MAC_STA: %012llx\n"
		"VENDOR.: %s\n\n"
		, macap, vendorapname, macsta, vendorstaname);

fclose(fhoui);
return;
}
/*===========================================================================*/
static void getoui(const char *ouiname, unsigned long long int oui)
{
int len;
FILE* fhoui;
char *vendorptr;
unsigned long long int vendoroui;
char linein[LINEBUFFER];
char vendorapname[256];

if ((fhoui = fopen(ouiname, "r")) == NULL)
	{
	fprintf(stderr, "unable to open database %s\n", ouiname);
	exit (EXIT_FAILURE);
	}

strncpy(vendorapname, "unknown", 8);
while((len = fgetline(fhoui, LINEBUFFER, linein)) != -1)
	{
	if (len < 10)
		continue;

	if(strstr(linein, "(base 16)") != NULL)
		{
		sscanf(linein, "%06llx", &vendoroui);
		if(oui == vendoroui)
			{
			vendorptr = strrchr(linein, '\t');
			if(vendorptr != NULL)
				{
				strncpy(vendorapname, vendorptr +1,255);
				}
			}
		}
	}

fprintf(stdout, "\nVENDOR: %s\n\n", vendorapname);

fclose(fhoui);
return;
}
/*===========================================================================*/
static void getvendor(const char *ouiname, char *vendorstring)
{
int len;

FILE* fhoui;
char *vendorptr;
unsigned long long int vendoroui;

char linein[LINEBUFFER];


if ((fhoui = fopen(ouiname, "r")) == NULL)
	{
	fprintf(stderr, "unable to open database %s\n", ouiname);
	exit (EXIT_FAILURE);
	}

while((len = fgetline(fhoui, LINEBUFFER, linein)) != -1)
	{
	if (len < 10)
		continue;

	if(strstr(linein, "(base 16)") != NULL)
		{
		if(strstr(linein, vendorstring) != NULL)
			{
			sscanf(linein, "%06llx", &vendoroui);
			vendorptr = strrchr(linein, '\t');
			fprintf(stdout, "%06llx%s\n", vendoroui, vendorptr);
			}
		}
	}
fclose(fhoui);
return;
}
/*===========================================================================*/
__attribute__ ((noreturn))
static void usage(char *eigenname)
{
printf("%s %s (C) %s ZeroBeat\n"
	"usage: %s <options>\n"
	"\n"
	"options:\n"
	"-d            : download http://standards-oui.ieee.org/oui.txt\n"
	"              : and save to ~/.hcxtools/oui.txt\n"
	"              : internet connection required\n"
	"-m <mac>      : mac (six bytes of mac addr) or \n"
	"              : oui (fist three bytes of mac addr)\n"
	"-p <hashline> : input PMKID hashline\n"
	"-P <hashline> : input EAPOL hashline from potfile\n"
	"-e <ESSID>    : input ESSID\n"
	"-x <xdigit>   : input ESSID in hex\n"
	"-e <ESSID>    : input ESSID\n"
	"-v <vendor>   : vendor name\n"
	"-h            : this help screen\n"
	"\n", eigenname, VERSION, VERSION_JAHR, eigenname);
exit(EXIT_SUCCESS);
}
/*===========================================================================*/
int main(int argc, char *argv[])
{
int auswahl;
int mode = 0;
int ret;
int l;
unsigned long long int oui = 0;

uid_t uid;
struct passwd *pwd;
struct stat statinfo;
char *vendorname = NULL;
char *hexessidname = NULL;
char *essidname = NULL;
char *hash16800line = NULL;
char *hash2500line = NULL;
const char confdirname[] = ".hcxtools";
const char ouiname[] = ".hcxtools/oui.txt";

while ((auswahl = getopt(argc, argv, "m:v:p:P:e:x:dh")) != -1)
	{
	switch (auswahl)
		{
		case 'd':
		mode = 'd';
		break;

		case 'm':
		if(strlen(optarg) == 6)
			{
			oui = strtoull(optarg, NULL, 16);
			mode = 'm';
			}

		else if(strlen(optarg) == 12)
			{
			oui = (strtoull(optarg, NULL, 16) >> 24);
			mode = 'm';
			}
		else
			{
			fprintf(stderr, "error wrong oui size %s (need 1122334455aa or 1122aa)\n", optarg);
			exit(EXIT_FAILURE);
			}
		break;

		case 'p':
		hash16800line = optarg;
		l = strlen(hash16800line);
		if(l < 61)
			{
			fprintf(stderr, "error hashline too short %s\n", optarg);
			exit(EXIT_FAILURE);
			}
		if((hash16800line[32] != '*') && (hash16800line[45] != '*') && (hash16800line[58] != '*'))
			{
			fprintf(stderr, "error hashline wrong format %s\n", optarg);
			exit(EXIT_FAILURE);
			}
		mode = 'p';
		break;

		case 'P':
		hash2500line = optarg;
		l = strlen(hash2500line);
		if(l < 61)
			{
			fprintf(stderr, "error hashline too short %s\n", optarg);
			exit(EXIT_FAILURE);
			}
		if((hash2500line[32] != ':') && (hash2500line[45] != ':') && (hash2500line[58] != ':'))
			{
			fprintf(stderr, "error hashline wrong format %s\n", optarg);
			exit(EXIT_FAILURE);
			}
		mode = 'P';
		break;

		case 'v':
		vendorname = optarg;;
		mode = 'v';
		break;

		case 'e':
		hexessidname = optarg;
		mode = 'e';
		break;

		case 'x':
		essidname = optarg;
		mode = 'x';
		break;

		default:
		usage(basename(argv[0]));
		}
	}

uid = getuid();
pwd = getpwuid(uid);
if (pwd == NULL)
	{
	fprintf(stdout, "failed to get home dir\n");
	exit(EXIT_FAILURE);
	}
ret = chdir(pwd->pw_dir);
if( ret == -1)
	fprintf(stdout, "failed to change dir\n");

if(stat(confdirname, &statinfo) == -1)
	{
	if (mkdir(confdirname,0755) == -1)
		{
		fprintf(stdout, "failed to create conf dir\n");
		exit(EXIT_FAILURE);
		}
	}


if(mode == 'd')
	downloadoui(ouiname);

if(stat(ouiname, &statinfo) != 0)
	{
	fprintf(stderr, "can't stat %s\n"
			"use download option -d to download it\n"
			"or download file http://standards-oui.ieee.org/oui.txt\n"
			"and save it to ~./hcxtools/oui.txt\n", ouiname);
	exit(EXIT_FAILURE);
	}

if(mode == 'm')
	{
	getoui(ouiname, oui);
	}
else if(mode == 'p')
	{
	get16800info(ouiname, hash16800line);
	}
else if(mode == 'P')
	{
	get2500info(ouiname, hash2500line);
	}

else if(mode == 'v')
	{
	getvendor(ouiname, vendorname);
	}

else if(mode == 'e')
	{
	gethexessidinfo(hexessidname);
	}

else if(mode == 'x')
	{
	getessidinfo(essidname);
	}

return EXIT_SUCCESS;
}
