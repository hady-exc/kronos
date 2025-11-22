#ifndef XDUTIME_INCLUDED
#define XDUTIME_INCLUDED

#include <windows.h>

//void xduTimeInit();

//void kt2wst(int ktime, SYSTEMTIME* res);
unsigned long long ktime2wtime(int ktime);
int wft2kt(FILETIME* wtime);
void pKronosTime(int ktime);


#endif