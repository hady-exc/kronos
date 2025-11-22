#include <Windows.h>
#include <stdio.h>
#include "xduTime.h"

const unsigned long long eraDiff = (unsigned long long)140618 * 24 * 3600; // seconds between 01.01.1600 and 01.01.1986

unsigned long long ktime2wtime(int ktime)
{
	return (eraDiff + (ULONGLONG)ktime) * 1000 * 10000;
}

void kt2wft(int ktime, FILETIME* res)
{
	ULARGE_INTEGER wtime;
	wtime.QuadPart = ktime2wtime(ktime);
	res->dwHighDateTime = wtime.HighPart;
	res->dwLowDateTime = wtime.LowPart;
}

void kt2wst(int ktime, SYSTEMTIME* res)
{
	FILETIME ftime;
	kt2wft(ktime, &ftime);
	FileTimeToSystemTime(&ftime, res);
}

static const int y4days = 1461;     // 365 * 4 + 1
static const int y100days = 36524;  // y4days * 25 - 1;
static const int y400days = 146097; // y100days * 4 + 1;
static const int firstDate = 1985 * 365 + (1985 / 4) - (1985 / 100) + (1985 / 400) + 1; 
static const int add[12] = { 0,1,-1,0,0,1,1,2,3,3,4,4 };

static int packTime(int year, int month, int day, int hour, int min, int sec)
{
	int is_leap = (year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0));
	year -= 1;
	int d = 0;
	d += year / 400 * y400days;
	year %= 400;
	d += year / 100 * y100days;
	year %= 100;
	d += year / 4 * y4days;
	year %= 4;
	d += year * 365 + (month -1)*30 + add[month-1];
	if (is_leap && month > 2) d++;
	d -= firstDate;
	if (d < 0) return 0;
	return d * 24 * 3600 + hour * 3600 + min * 60 + sec;
}

int wft2kt(FILETIME* wtime)
{
	SYSTEMTIME wt;
	if (!FileTimeToSystemTime(wtime, &wt)) {
		return -1;
	}
	return packTime(wt.wYear, wt.wMonth, wt.wDay, wt.wHour, wt.wMinute, wt.wSecond);
}


void pKronosTime(int ktime)
{
	SYSTEMTIME st;
	kt2wst(ktime, &st);
	printf("%02d.%02d.%d %02d:%02d:%02d", st.wDay, st.wMonth, st.wYear, st.wHour, st.wMinute, st.wSecond);
}

