
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "mond.h"


void timer_handler(int signum) {
   return;
}

int main(int argc, char *argv[]) {
   Args args = {-1, NULL, -1, NULL};
   pid_t pid = -1;
   int status = -1;
   int fdStat = -1, fdMem = -1, fdLoad = -1, fdDisk = -1;
   int fdStatProc = -1, fdStatm = -1;
   FILE *fLogFile = NULL;

   if (parseArgs(argc, argv, &args) == -1) {
      usage();
   }

   openSysFiles(&fdStat, &fdMem, &fdLoad, &fdDisk);

   // open the output log file
   if ((fLogFile = fopen(args.logFileName, "w")) == NULL) {
      perror("fopen failed");
      exit(-1);
   }

   if ((pid = fork()) == -1) {
      perror("fork failed");
      exit(-1);
   }
   else if (pid == 0) { // child
      if (execlp(args.execFile, args.execFile, NULL) == -1) {
         perror("execv failed");
         exit(-1);
      }
   }
   else { // parent
      openProcessFiles(pid, &fdStatProc, &fdStatm);

      if (args.sysFlag == 1) {
         printSysLogs(fLogFile, fdStat, fdMem, fdLoad, fdDisk);
      }

      printProcessLogs(fLogFile, pid, fdStatProc, fdStatm);

      setupTimer(args.interval);

      while (1) {
         if (waitpid(pid, &status, 0) == -1) {
            if (errno != EINTR) {
               perror("waitpid failed");
               exit(-1);
            }
         }

         // child ended
         if (WIFEXITED(status)) {
            break;
         }

         if (args.sysFlag == 1) {
            printSysLogs(fLogFile, fdStat, fdMem, fdLoad, fdDisk);
         }

         printProcessLogs(fLogFile, pid, fdStatProc, fdStatm);
      }

   }

   fclose(fLogFile);
   closeSysFiles(fdStat, fdMem, fdLoad, fdDisk);
   closeProcessFiles(fdStatProc, fdStatm);

   return 0;
}

void usage(void) {
   printf("usage: mond [-s] <executable> <interval> <logFilename>\n");
   exit(-1);
}

int parseArgs(int argc, char *argv[], Args *args) {
   int offset = -1;

   if (argc == 4) {
      offset = 0;
   }
   else if (argc == 5) {
      offset = 1;
   }
   else {
      return -1;
   }

   if (offset == 1) {
      if (strncmp(argv[offset], "-s", sizeof("-s")) == 0) {
         args->sysFlag = 1;
      }
      else {
         return -1;
      }
   }

   args->execFile = argv[offset + 1];
   errno = 0;
   args->interval = strtol(argv[offset + 2], NULL, 10);
   if (errno == LONG_MIN || errno == LONG_MAX) {
      perror("Parsing interval value failed");
      exit(-1);
   }
   if (args->interval < 500) { // clamped
      args->interval = 500;
   }
   args->logFileName = argv[offset + 3];

   return 0;
}

void setupTimer(int interval) {
   struct sigaction sa;
   struct itimerval timer;

   memset(&sa, 0, sizeof(sa));
   sa.sa_handler = &timer_handler;

   if (sigaction(SIGALRM, &sa, NULL) == -1) {
      perror("sigaction failed");
      exit(-1);
   }

   timer.it_value.tv_sec = interval / 1000;
   timer.it_value.tv_usec = (interval % 1000) * 1000;  // millisec

   timer.it_interval.tv_sec = interval / 1000;
   timer.it_interval.tv_usec = (interval % 1000) * 1000;  // millisec

   if (setitimer(ITIMER_REAL, &timer, NULL) == -1) {
      perror("setitimer failed");
      exit(-1);
   }

   return;
}

void openSysFiles(int *fdStat, int *fdMem, int *fdLoad, int *fdDisk) {

   if ((*fdStat = open("/proc/stat", O_RDONLY)) == -1) {
      perror("open failed");
      exit(-1);
   }

   if ((*fdMem = open("/proc/meminfo", O_RDONLY)) == -1) {
      perror("open failed");
      exit(-1);
   }

   if ((*fdLoad = open("/proc/loadavg", O_RDONLY)) == -1) {
      perror("open failed");
      exit(-1);
   }

   if ((*fdDisk = open("/proc/diskstats", O_RDONLY)) == -1) {
      perror("open failed");
      exit(-1);
   }

   return;
}

void openProcessFiles(int pid, int *fdStatProc, int *fdStatm) {
   char file[MAX_FILE_LEN] = "";

   if (snprintf(file, MAX_FILE_LEN - 1, "/proc/%d/stat", pid) < 0) {
      perror("snprintf failed");
      exit(-1);
   }

   if ((*fdStatProc = open(file, O_RDONLY)) == -1) {
      perror("open failed");
      exit(-1);
   }

   if (snprintf(file, MAX_FILE_LEN - 1, "/proc/%d/statm", pid) < 0) {
      perror("snprintf failed");
      exit(-1);
   }

   if ((*fdStatm = open(file, O_RDONLY)) == -1) {
      perror("open failed");
      exit(-1);
   }

   return;
}

/*
 * Note: row and col are base 0 for the first line, first item
 *
 * Return: A (char *) is returned and the space needs to be freed
 */
char *queryFileByLoc(int fd, int row, int col) {
   int curRow = 0;
   int curCol = 0;
   char *linePtr = NULL, *tokenPtr = NULL, *retPtr = NULL;
   size_t nSize = -1;
   FILE *file = NULL;

   if (lseek(fd, SEEK_SET, 0) == -1) {
      perror("lseek failed");
      exit(-1);
   }

   if ((file = fdopen(fd, "r")) == NULL) {
      perror("fdopen failed");
      exit(-1);
   }

   // move to the correct row
   while (curRow <= row) {
      if (getline(&linePtr, &nSize, file) == -1) {
         if (feof(file)) {
            free(linePtr);
            return NULL;
         }
         perror("getline failed");
         exit(-1);
      }
      curRow++;
   }

   // move to the correct column
   while (curCol <= col) {
      if ((tokenPtr = strtok((curCol == 0) ? linePtr : NULL, " ")) == NULL) {
         free(linePtr);
         return NULL;
      }

      curCol++;
   }

   if ((retPtr = (char *)calloc(1, sizeof(char) * strlen(tokenPtr))) == NULL) {
      perror("calloc failed");
      exit(-1);
   }

   strcpy(retPtr, tokenPtr);

   free(linePtr);

   // remove ending '\n' for only end of line cases
   if (retPtr[strlen(retPtr) - 1] == '\n') {
      retPtr[strlen(retPtr) - 1] = '\0';
   }

   return retPtr;
}

void printQuery(FILE *fLogFile, int fdSrc, char *queryField, int row, int col) {
   char *retQuery = queryFileByLoc(fdSrc, row, col);
   fprintf(fLogFile, "%s %s", queryField, retQuery);
   free(retQuery);
   return;
}

char *generateLogTime(char *timeStr) {
   time_t timep;
   struct tm *tm;

   time(&timep);
   tm = localtime(&timep);
   strftime(timeStr, MAX_TIME_LEN - 1, "%a %b %e %T %Y", tm);

   return timeStr;
}

void printSysLogs(FILE *fLogFile, int fdStat, int fdMem, int fdLoad, int fdDisk) {
   char timeStr[MAX_TIME_LEN] = "";

   // log statistics
   fprintf(fLogFile, "[%s] System ", generateLogTime(timeStr));
   fprintf(fLogFile, " [PROCESS]");
   printQuery(fLogFile, fdStat, " cpuusermode", 0, 1);
   printQuery(fLogFile, fdStat, " cpusystemmode", 0, 3);
   printQuery(fLogFile, fdStat, " idletaskrunning", 0, 4);
   printQuery(fLogFile, fdStat, " iowaittime", 0, 5);
   printQuery(fLogFile, fdStat, " irqservicetime", 0, 6);
   printQuery(fLogFile, fdStat, " softirqservicetime", 0, 7);
   printQuery(fLogFile, fdStat, " intr", 2, 1);
   printQuery(fLogFile, fdStat, " ctxt", 3, 1);
   printQuery(fLogFile, fdStat, " forks", 5, 1);
   printQuery(fLogFile, fdStat, " runnable", 6, 1);
   printQuery(fLogFile, fdStat, " blocked", 7, 1);
   fprintf(fLogFile, " [MEMORY]");
   printQuery(fLogFile, fdMem, " memtotal", 0, 1);
   printQuery(fLogFile, fdMem, " memfree", 1, 1);
   printQuery(fLogFile, fdMem, " cached", 3, 1);
   printQuery(fLogFile, fdMem, " swapcached", 4, 1);
   printQuery(fLogFile, fdMem, " active", 5, 1);
   printQuery(fLogFile, fdMem, " inactive", 6, 1);
   fprintf(fLogFile, " [LOADAVG]");
   printQuery(fLogFile, fdLoad, " 1min", 0, 0);
   printQuery(fLogFile, fdLoad, " 5min", 0, 1);
   printQuery(fLogFile, fdLoad, " 15min", 0, 2);
   fprintf(fLogFile, " [DISKSTATS](sda)");
   printQuery(fLogFile, fdDisk, " totalnoreads", 16, 3);
   printQuery(fLogFile, fdDisk, " totalsectorsread", 16, 5);
   printQuery(fLogFile, fdDisk, " nomsread", 16, 6);
   printQuery(fLogFile, fdDisk, " totalnowrites", 16, 7);
   printQuery(fLogFile, fdDisk, " nosectorswritten", 16, 9);
   printQuery(fLogFile, fdDisk, " nomswritten", 16, 10);
   fprintf(fLogFile, "\n") ;

   return;
}

void printProcessLogs(FILE *fLogFile, int pid, int fdStatProc, int fdStatm) {
   char timeStr[MAX_TIME_LEN] = "";

   // log statistics
   fprintf(fLogFile, "[%s] Process(%d) ", generateLogTime(timeStr), pid);
   printQuery(fLogFile, fdStatProc, " [STAT] executable", 0, 1);
   printQuery(fLogFile, fdStatProc, " stat", 0, 2);
   printQuery(fLogFile, fdStatProc, " minorfaults", 0, 9);
   printQuery(fLogFile, fdStatProc, " majorfaults", 0, 11);
   printQuery(fLogFile, fdStatProc, " usermodetime", 0, 13);
   printQuery(fLogFile, fdStatProc, " kernelmodetime", 0, 14);
   printQuery(fLogFile, fdStatProc, " priority", 0, 17);
   printQuery(fLogFile, fdStatProc, " nice", 0, 18);
   printQuery(fLogFile, fdStatProc, " nothreads", 0, 19);
   printQuery(fLogFile, fdStatProc, " vsize", 0, 22);
   printQuery(fLogFile, fdStatProc, " rss", 0, 23);
   fprintf(fLogFile, " [STATM]");
   printQuery(fLogFile, fdStatm, " program", 0, 0);
   printQuery(fLogFile, fdStatm, " residentset", 0, 1);
   printQuery(fLogFile, fdStatm, " share", 0, 2);
   printQuery(fLogFile, fdStatm, " text", 0, 3);
   printQuery(fLogFile, fdStatm, " data", 0, 5);
   fprintf(fLogFile, "\n");

   return;
}

void closeSysFiles(int fdStat, int fdMem, int fdLoad, int fdDisk) {
   close(fdStat);
   close(fdMem);
   close(fdLoad);
   close(fdDisk);

   return;
}

void closeProcessFiles(int fdStatProc, int fdStatm) {
   close(fdStatProc);
   close(fdStatm);

   return;
}
