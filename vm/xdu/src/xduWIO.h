#ifndef XDUWIO_INCLUDED
#define XDUWIO_INCLUDED

void w_copy_file(char* path, char* content, int eof, int ctime, int wtime);
void w_create_dir(char* path, int ctime, int wtime);
void w_read_file(char* path, char** data, int* len);

void initConsole();

#endif