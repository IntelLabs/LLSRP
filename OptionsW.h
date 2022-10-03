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
int32_t splitNamePort(const char *In, int32_t Len, char *Out, int32_t &Port) {
  int32_t i;
  char *PortChar;
  for (i = 0; i < Len; i++)
    Out[i] = In[i];
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

void printUsage() {
  printf("Usage: client [-g] [-l=INT] [-r=INT]-[-L=INT] [-R=INT] [-n=INT] [-d=INT]\n");
  printf("[-I=INT] [-h] <device names addresses>\n");
}
void printHelp() {
  printf("Usage: client [OPTIONS] <device names addresses>\n");
  printf("-l    Number of local devices\n");
  printf("-r    Number of remote devices\n");
  printf("-L    Local port (will be incremented by 1 for each device\n");
  printf("-R    Remore port (will be incremented by 1 for each device\n");
  printf("-n    Error correction informational bits nominator\n");
  printf("-d    Error correction redundancy bits denominator\n");
  printf("-g    Enable game mode (center window, keep mouse in window,\n"
         "        use relative mouse coordinates\n");
  printf("-I    Number of frames to receive, exits after\n");
}

char **parseClientOpts(int32_t *ArgSize, int argc, wchar_t *argW[],
                       int32_t *LNum, int32_t *RNum,
                       int32_t *LPort, int32_t *RPort,
                       int32_t *Nom, int32_t *Denom,
                       int32_t *GameMode, int32_t *Iterations) {

  char **ArgsC;

  if (argc < 2) {
    printUsage();
    exit(1);
  }
  *GameMode = 0;

  for (int i = 1; i < argc; i++) {
    int len = wcslen(argW[i]);
    char *argv = new char[len + 1];
    wcstombs(argv, argW[i], len);
    argv[len] = '\0';
    if (strcmp(argv, "-h") == 0 || strcmp(argv, "--help") == 0) {
      printHelp();
      delete [] argv;
      return 0;
    } else if (strcmp(argv, "-g") == 0) {
      *GameMode = 1;
    } else if (argv[0] == '-') {
      if (i == argc - 1) {
        printf("The option %s should have argument (see --help)\n", argv);
        delete [] argv;
        break;
      }
      int lenN = wcslen(argW[i + 1]);
      char *argvN = new char[lenN + 1];
      wcstombs(argvN, argW[i + 1], lenN);
      argvN[lenN] = '\0';
      if (strcmp(argv, "-l") == 0) {
        *LNum = atoi(argvN);
      } else if (strcmp(argv, "-r") == 0) {
        *RNum = atoi(argvN);
      } else if (strcmp(argv, "-L") == 0) {
        *LPort = atoi(argvN);
      } else if (strcmp(argv, "-R") == 0) {
        *RPort = atoi(argvN);
      } else if (strcmp(argv, "-n") == 0) {
        *Nom = atoi(argvN);
      } else if (strcmp(argv, "-d") == 0) {
        *Denom = atoi(argvN);
      } else if (strcmp(argv, "-I") == 0) {
        *Iterations = atoi(argvN);
      } else {
        printf("Unknown option %s\n", argv);
        delete [] argv;
        delete [] argvN;
        break;
      }
      i++;
      delete [] argvN;
    } else {
      (*ArgSize)++;
    }
    delete [] argv;
  }
  if(*ArgSize == 0) {
     printf("Specify local names remote addresses .e.g., lo lo 127.0.0.1\n"
            "or with port lo:4001 lo:4002 127.0.0.1:3001\n");
     return NULL;
  }
  ArgsC = new char *[*ArgSize];
  int j = 0;
  for (int i = 1; i < argc; i++) {
    int len = wcslen(argW[i]);
    char *argv = new char[len + 1];
    wcstombs(argv, argW[i], len);
    argv[len] = '\0';
    if (argv[0] == '-') {
      if (strlen(argv) > 1 && argv[1] != 'g')
        i++;
      delete [] argv;
      continue;
    }
    ArgsC[j] = new char[strlen(argv) + 1];
    strncpy(ArgsC[j], argv, strlen(argv));
    ArgsC[strlen(argv)] = '\0';
    j++;
    delete [] argv;
  }
  return ArgsC;
}
