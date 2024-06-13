/*
 * Copyright 2022 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/*
 * Written by: Evgeny Stupachenko
 *             e-mail: evgeny.v.stupachenko@intel.com
 */

#include <popt.h>
#include <stdio.h>
#include <stdlib.h>


int32_t splitNamePort(const char *In, int32_t Len, char *Out, int32_t &Port) {
  int32_t i;
  char *PortChar;
  for (i = 0; i < Len; i++)
    Out[i] = In[i];
  Out[Len] = '\0';
  for (i = 0; i < Len; i++)
    if (In[i] == ':')
      break;
  if (i == Len)
    return 0;
  Out[i] = '\0';
  PortChar = &Out[i + 1];
  Port = atoi(PortChar);
  return 1;
}

void usage(poptContext optCon, int exitcode, char *error, char *addl) {
  poptPrintUsage(optCon, stderr, 0);
  if (error)
    fprintf(stderr, "%s: %s\n", error, addl);
  exit(exitcode);
}

const char **parseClientOpts(poptContext *optCon, int argc, char *argv[],
                             int32_t *LNum, int32_t *RNum,
                             int32_t *LPort, int32_t *RPort,
                             int32_t *Nom, int32_t *Denom,
                             int32_t *GameMode, int32_t *FlipWait,
                             int32_t *Iterations) {

  int c;
  int i = 0;
  char Buf[BUFSIZ+1];
  const char **Args;

  struct poptOption optionsTable[] = {
     { "lnum", 'l', POPT_ARG_INT, LNum, 0,
         "Number of local devices", NULL},
     { "rnum", 'r', POPT_ARG_INT, RNum, 0,
         "Number of remote devices", NULL},
     { "lport", 'L', POPT_ARG_INT, LPort, 0,
         "Local port (will be incremented by 1 for each device", NULL},
     { "rport", 'R', POPT_ARG_INT, RPort, 0,
         "Remore port (will be incremented by 1 for each device", NULL},
     { "i", 'n', POPT_ARG_INT, Nom, 0,
         "Info nominator", NULL},
     { "r", 'd', POPT_ARG_INT, Denom, 0,
         "Redundant denominator", NULL},
     { "game", 'g', 0, 0, 'g',
         "Enable game mode (center window, keep mouse in window,"
         " use relative mouse coordinates", NULL },
     { "flipwait", 'f', 0, 0, 'f',
         "Enable wait for flipped frame (when N+1 comes prior to N)."
         " When disabled (default) drops the frame.", NULL },
     { "I", 'I', POPT_ARG_INT, Iterations, 0,
         "Number of frames to receive", NULL},
     POPT_AUTOHELP
     { NULL, 0, 0, NULL, 0 }
   };

  *optCon = poptGetContext(NULL, argc, (const char **)argv, optionsTable, 0);
  poptSetOtherOptionHelp(*optCon, "[OPTIONS] <device names addresses>");

  if (argc < 2) {
    poptPrintUsage(*optCon, stderr, 0);
    exit(1);
  }
  *GameMode = 0;
  *FlipWait = 0;
  /* Now do options processing */
  while ((c = poptGetNextOpt(*optCon)) >= 0) {
    switch (c) {
      case 'l':
        Buf[i++] = 'l';
        break;
      case 'r':
        Buf[i++] = 'r';
        break;
      case 'L':
        Buf[i++] = 'L';
        break;
      case 'R':
        Buf[i++] = 'R';
        break;
      case 'n':
        Buf[i++] = 'n';
        break;
      case 'd':
        Buf[i++] = 'd';
        break;
      case 'I':
        Buf[i++] = 'I';
        break;
      case 'g':
        Buf[i++] = 'g';
        *GameMode = 1;
        break;
      case 'f':
        Buf[i++] = 'f';
        *FlipWait = 1;
        break;
    }
  }
  Args = poptGetArgs(*optCon);
  if((Args == NULL) || Args[0] == NULL) {
     usage(*optCon, 1, (char *)"Specify local names remote addresses",
           (char *)".e.g., lo lo 127.0.0.1\n"
           "or with port lo:4001 lo:4002 127.0.0.1:3001");
     return NULL;
  }
  printf("%s ", Args[0]);
  if (c < -1) {
     /* an error occurred during option processing */
     fprintf(stderr, "%s: %s\n",
     poptBadOption(*optCon, POPT_BADOPTION_NOALIAS),
     poptStrerror(c));
     return NULL;
  }

  return Args;
}

const char **parseServerOpts(poptContext *optCon, int argc, char *argv[],
                             int32_t *LNum, int32_t *RNum, int32_t *LPort,
                             int32_t *Nom, int32_t *Denom,
                             int32_t *CRate, int32_t *VSize, char **Disp,
                             int32_t *Iterations) {

  int c;
  int i = 0;
  char Buf[BUFSIZ + 1];
  const char **Args;

  struct poptOption optionsTable[] = {
     { "lnum", 'l', POPT_ARG_INT, LNum, 0,
         "Number of local devices", NULL},
     { "rnum", 'r', POPT_ARG_INT, RNum, 0,
         "Number of remote devices", NULL},
     { "lport", 'L', POPT_ARG_INT, LPort, 0,
         "Local port (will be incremented by 1 for each device", NULL},
     { "crate", 'c', POPT_ARG_INT, CRate, 0,
         "Constan rate (bytes per second)", NULL},
     { "vsize", 's', POPT_ARG_INT, VSize, 0,
         "Video size/FPS: 0 - FullHD/60, 1 - FullHD/25, 2 - VGA/60,"
         " 3 - VGA/25, 4 - 720P/60, 5 - 720P/25", NULL},
     { "disp", 'D', POPT_ARG_STRING, Disp, 0,
         "$DISPLAY", NULL},
     { "i", 'n', POPT_ARG_INT, Nom, 0,
         "Info nominator", NULL},
     { "r", 'd', POPT_ARG_INT, Denom, 0,
         "Redundant denominator", NULL},
     { "I", 'I', POPT_ARG_INT, Iterations, 0,
         "Number of frames to send", NULL},
     POPT_AUTOHELP
     { NULL, 0, 0, NULL, 0 }
   };

  *optCon = poptGetContext(NULL, argc, (const char **)argv, optionsTable, 0);
  poptSetOtherOptionHelp(*optCon, "[OPTIONS] <device names>");

  if (argc < 2) {
    poptPrintUsage(*optCon, stderr, 0);
    exit(1);
  }

  /* Now do options processing */
  while ((c = poptGetNextOpt(*optCon)) >= 0) {
    switch (c) {
      case 'l':
        Buf[i++] = 'l';
        break;
      case 'r':
        Buf[i++] = 'r';
        break;
      case 'L':
        Buf[i++] = 'L';
        break;
      case 'c':
        Buf[i++] = 'c';
        break;
      case 's':
        Buf[i++] = 's';
        break;
      case 'n':
        Buf[i++] = 'n';
        break;
      case 'd':
        Buf[i++] = 'd';
        break;
      case 'D':
        Buf[i++] = 'D';
        break;
      case 'I':
        Buf[i++] = 'I';
        break;
    }
    if (i >= BUFSIZ + 1) {
      printf("Warning: not all option are received correctly!\n");
      break;
    }
  }
  Args = poptGetArgs(*optCon);
  if((Args == NULL) || Args[0] == NULL) {
     usage(*optCon, 1, (char *)"Specify local names", (char *)".e.g., lo lo\n"
           "or with port lo:3001 lo:3002");
     return NULL;
  }

  if (c < -1) {
     /* an error occurred during option processing */
     fprintf(stderr, "%s: %s\n",
     poptBadOption(*optCon, POPT_BADOPTION_NOALIAS),
     poptStrerror(c));
     return NULL;
  }

  return Args;
}
