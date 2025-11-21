/* vim: set tabstop=3 expandtab:
**
** This file is in the public domain.
**
** osd.c
**
** $Id: osd.c,v 1.2 2001/04/27 14:37:11 neil Exp $
**
*/

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
       
#include <noftypes.h>
#include <nofconfig.h>
#include <log.h>
#include <osd.h>
#include <nofrendo.h>

#include <version.h>

#include "rtthread.h"

char configfilename[]="na";

/* This is os-specific part of main() */
int osd_main(int argc, char *argv[])
{
   config.filename = configfilename;

   return main_loop(argv[0], system_nes);
}

/* File system interface */
void osd_fullname(char *fullname, const char *shortname)
{
   strncpy(fullname, shortname, PATH_MAX);
}

/* This gives filenames for storage of saves */
char *osd_newextension(char *string, char *ext)
{
   size_t len;
   char *p;

   if (string == NULL || ext == NULL)
      return string;

   len = strlen(string);

   /* Strip common compression extensions */
   if (len > 3 && strcmp(string + len - 3, ".gz") == 0)
   {
      string[len - 3] = '\0';
      len -= 3;
   }
   else if (len > 4 && strcmp(string + len - 4, ".bz2") == 0)
   {
      string[len - 4] = '\0';
      len -= 4;
   }

   /* Find last dot in the filename (extension separator) */
   p = strrchr(string, '.');

   if (p != NULL)
   {
      /* Replace existing extension with new one, if it fits */
      if ((size_t)(p - string) + strlen(ext) < PATH_MAX)
      {
         strcpy(p, ext);
      }
      else
      {
         /* Truncate defensively */
         strncpy(p, ext, PATH_MAX - (p - string) - 1);
         p[PATH_MAX - (p - string) - 1] = '\0';
      }
   }
   else
   {
      /* No extension present: append the new extension if space allows */
      if (len + strlen(ext) < PATH_MAX)
         strcat(string, ext);
      else
      {
         /* Truncate defensively */
         size_t copy = PATH_MAX - strlen(ext) - 1;
         if (copy > 0)
         {
            char tmp[PATH_MAX];
            strncpy(tmp, string, copy);
            tmp[copy] = '\0';
            strcpy(string, tmp);
            strcat(string, ext);
         }
      }
   }

   return string;
}

/* This gives filenames for storage of PCX snapshots */
int osd_makesnapname(char *filename, int len)
{
   return -1;
}

// void osd_getmouse(int *x, int *y, int *button)
// {
//    return;
// }

// void osd_shutdown(void)
// {
//    return;
// }

// int osd_init(void)
// {
//    return 0;
// }

// int osd_installtimer(int frequency, void *func, int funcsize,
//                             void *counter, int countersize)
// {
//    return 0;
// }

// void osd_getinput(void)
// {
//     return;
// }

// void osd_setsound(void (*playfunc)(void *buffer, int size))
// {
//    return;
// }

// void osd_getsoundinfo(sndinfo_t *info)
// {
//    return;
// }

// void osd_getvideoinfo(vidinfo_t *info)
// {
//    return;
// }