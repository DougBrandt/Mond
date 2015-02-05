
#ifndef __MOND_H_
#define __MOND_H_

#include <stdio.h>

#define MAX_FILE_LEN 256
#define MAX_TIME_LEN 100

typedef struct {
   int sysFlag;
   char *execFile;
   int interval;
   char *logFileName;
} Args;

void usage(void);
int parseArgs(int argc, char *argv[], Args *args);

void timer_handler(int signum);
void setupTimer(int interval);

void openSysFiles(int *fdStat, int *fdMem, int *fdLoad, int *fdDisk);
void openProcessFiles(int pid, int *fdStatProc, int *fdStatm);

char *queryFileByLoc(int fd, int row, int col);
void printQuery(FILE *fLogFile, int fdSrc, char *queryField, int row, int col);

char *generateLogTime(char *timeStr);
void printSysLogs(FILE *fLogFile, int fdStat, int fdMem, int fdLoad, int fdDisk);
void printProcessLogs(FILE *fLogFile, int pid, int fdStatProc, int fdStatm);

void closeSysFiles(int fdStat, int fdMem, int fdLoad, int fdDisk);
void closeProcessFiles(int fdStatProc, int fdStatm);

#endif //__MOND_H_

