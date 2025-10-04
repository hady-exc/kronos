#include "xduWIO.h"
#include <windows.h>
#include <winbase.h>
#include "stdio.h"
#include "xduTime.h"
#include <assert.h>
  

void set_time_attrs(HANDLE file, int created, int modified)
{
	ULARGE_INTEGER wtime;
	FILETIME ct, wt;
	wtime.QuadPart = ktime2wtime(created);
	ct.dwHighDateTime = wtime.HighPart;
	ct.dwLowDateTime = wtime.LowPart;
	wtime.QuadPart = ktime2wtime(modified);
	wt.dwHighDateTime = wtime.HighPart;
	wt.dwLowDateTime = wtime.LowPart;
	if (!SetFileTime(file, &ct, NULL, &wt)) {
		printf("\nERROR: can't set time");
	}
}

int isText(char* fname)
{
	int len = strlen(fname);
	char* c = fname + len;
	do { len--; c--; } while (len && *c != '.');
	//return 0;
	return ((len != 0) && ((strcmp(c, ".m") == 0) || (strcmp(c, ".d") == 0)));
}

#define RS (0x1e)
#define CR (0x0d)
#define LF (0x0a)

char* toUTF8(char *src, int *src_len)
{
	int i = *src_len;
	int lines = 0;
	char* c = src;
	while (i) {
		if (*(c++) == RS) lines++;
		i--;
	}
	char* str = malloc(*src_len + lines);
	assert(str != NULL);
	c = src;
	char* d = str;
	i = *src_len;
	while (i) {
		if (*c == RS) { *(d++) = CR; *(d++) = LF; }
		else *(d++) = *c;
		c++; i--;
	}

	int wchar_len = MultiByteToWideChar(20866, 0, str, (*src_len) + lines, NULL, 0);
	wchar_t* wideString = malloc(sizeof(wchar_t) * wchar_len);
	MultiByteToWideChar(20866, 0, str, (*src_len) + lines, wideString, wchar_len);

	int utf_len = WideCharToMultiByte(CP_UTF8, 0, wideString, wchar_len, NULL, 0, NULL, NULL);
	char* utf = malloc(utf_len + 1);
	WideCharToMultiByte(CP_UTF8, 0, wideString, wchar_len, utf, utf_len, NULL, NULL);
	utf[utf_len] = 0;

	free(wideString);
	free(str);
	*src_len = utf_len;
	return utf;
}

void w_copy_file(char* path, char* content, int eof, int ctime, int wtime)
{
	char* converted = NULL;

	HANDLE file = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
	if (file == INVALID_HANDLE_VALUE) {
		printf("ERROR creating file %s: %d", path, GetLastError());
		exit(1);
	}
	char* src = content;
	int src_len = eof;
	if (isText(path)) { 
		converted = toUTF8(content, &src_len); 
		src = converted;
	}

	int written = 0;
	if (!WriteFile(file, src, src_len, &written, NULL)) {
		printf("ERROR writing to file %s: %d\n", path, GetLastError());
		exit(1); 
	}
	if (converted) { free(converted); converted = NULL; }
	set_time_attrs(file, ctime, wtime);
	if (!CloseHandle(file)) {
		printf("ERROR closing % s: % d\n", path, GetLastError());
		exit(1);
	}
}

void w_create_dir(char* path, int ctime, int wtime)
{
	if (!CreateDirectory(path, NULL)) {
		int error = GetLastError();
		if (error != ERROR_ALREADY_EXISTS) {
			printf("Can not create directory \"%s\"  error %d\n", path, error);
			exit(1);
		}
	}
/*  Commented out because did not work and is not much necessary for directories
	HANDLE file = CreateFile(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL );
	if (file == INVALID_HANDLE_VALUE) {
		printf("\nERROR opening directory %s for setting creation time\n", path);
		return;
	}
	set_time_attrs(file, ctime, wtime);
	if (!CloseHandle(file)) {
		printf("ERROR closing % s: % d\n", path, GetLastError());
		exit(1);
	}
*/
}

void init_console()
{
	if (!SetConsoleOutputCP(20866))
		printf("Set Console CP error code %d\n", GetLastError());
}