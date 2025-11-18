/*
* XDU - XD Utility (c) KRONOS 
* 
* Purpose: Basic access to Kronos virtual XD volume from Windows host
* 
*/

#include <stdio.h>
#include "xduDisk.h"
#include "xduWIO.h"
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include "xduTime.h"

extern void fatal(char* fmt, ...);

void pindent(int level)   
{ 
	while (level-- > 0) printf("  "); 
}

void pfilename(char* fn, int dir)
{
	char fname[40];
	strncpy(fname, fn, 33);
	if (dir) strcat(fname, "/");
	printf("%-16s", fname);
}

void copy_file(int ino, char* fname, char* path)
{
	struct x_file file;
	xfile_open(ino, &file);
	assert((file.inode->mode & i_dir) == 0);

	char fullname[250];
	strncpy(fullname, path, 248);
	if (strlen(fullname) + strlen(fname) >= 248) {
		fatal("ERROR: too long filename \"%s/%s\"\n", path, fname);
	}
	strcat(fullname, "/");
	strcat(fullname, fname);
	printf("%s\r", fullname);
	char* content = xfile_read(&file);
	w_copy_file(fullname, content, file.inode->eof, file.inode->cTime, file.inode->wTime);
	xfile_close(&file);
	printf("%s -- DONE\n", fullname);
}

void copy_dir(int ino, char* fname, char* path)
{
	struct x_dir dir;
	xdir_open(ino, &dir);
	assert(dir.file.inode->mode & i_dir);

	char fullname[250];
	strncpy(fullname, path, 248);
	if (strlen(fullname) + strlen(fname) >= 248) {
		fatal("ERROR: too long filename \"%s/%s\"\n", path, fname);
	}
	if (fullname[0] != 0) {
		strcat(fullname, "/");
	}
	strcat(fullname, fname);

	w_create_dir(fullname, dir.file.inode->cTime, dir.file.inode->wTime);

	printf("DIR %s\n", fullname);
	dNode dnode = dir.dnodes;
	int i = dir.dnodes_no;
	while (i--) {
		if ((dnode->kind & d_del) == 0) {
			char fname[40];
			strncpy(fname, dnode->name, 33);
			if ((dnode->kind & d_dir) && (strcmp(fname,"..") != 0)) {
				copy_dir(dnode->inode, fname, fullname);
			}
			else if (dnode->kind & d_file) {
				copy_file(dnode->inode, fname, fullname);
			}
		}
		dnode++;
	}
	xdir_close(&dir);
	printf("DIR %s -- DONE\n", fullname);
}

void listDir(int ino, int level)
{
	struct x_dir dir;
	xdir_open(ino, &dir);
	/* print subdirectories */
	int i = dir.dnodes_no;
	dNode dnode = dir.dnodes;
	while (i--) {
		if ((dnode->kind & d_del) == 0) {
			if (dnode->kind & d_dir) {
				iNode inode = get_inode(dnode->inode);
				pindent(level);
				pfilename(dnode->name,1);
				printf(" CR:");
				pKronosTime(inode->cTime);
				printf("  MD:");
				pKronosTime(inode->wTime);
				printf("\n");
			}
		}
		dnode++;
	}
	/* print files */
	i = dir.dnodes_no;
	dnode = dir.dnodes;
	while (i--) {
		if ((dnode->kind & d_del) == 0) {
			if (dnode->kind & d_file) {
				iNode inode = get_inode(dnode->inode);
				pindent(level);
				pfilename(dnode->name,0);
				printf(" %7d B  ", inode->eof);
				printf("CR:");
				pKronosTime(inode->cTime);
				printf("  MD:");
				pKronosTime(inode->wTime);
				printf("\n");
			}
		}
		dnode++;
	}
	/* iterate subdirectories */
	i = dir.dnodes_no;
	dnode = dir.dnodes;
	while (i--) {
		if ((dnode->kind & d_del) == 0) {
			if ((dnode->kind & d_dir) && (strncmp("..", dnode->name, 2) != 0)) {
				pindent(level);
				pfilename(dnode->name,1);
				printf("\n");
				listDir(dnode->inode, level + 1);
			}
		}
		dnode++;
	}
}

void list()
{
	printf("/\n");
	listDir(0, 1);
}

void download()
{
	copy_dir(0, "TMP", "");
}


char* get_fname(char* path)
{
	int len = strlen(path);
	char* s = path + len;
	while (len) {
		char c = *(--s);
		if (c == '\\' || c == '/') return s+1;
		len--;
	}
	return path;
}

void upload(char* path)
{
	char* fname = get_fname(path);
	if (*fname == 0) {
		fatal("invalid file name %s\n", path);
	}
	printf("Uploading %s to /host-exchange/%s\n", path, fname);
	struct x_dir root, upload;
	xdir_open(0, &root);
	xdir_create(&root, "host-exchange", &upload);
	xdir_close(&root);

	struct x_file file;
	int len = 0;
	char* data = null;
	w_read_file(path, &data, &len); // w_read_file converts text file to DKOI
	xfile_create(&file, len);
	xfile_write(&file, data, len);
	xfile_link(&upload, file.inode_no, fname, d_file);
	xfile_close(&file);
	free(data);
	xdir_close(&upload);
}

void help()
{
	fprintf(stderr, "xdu -- XD virtual volume utility (c) 2025 Kronos\n");
	fprintf(stderr, "usage:\n");
	fprintf(stderr, "  xdu XDFile\n");
	fprintf(stderr, "     Prnts Kronos volume file tree, like as \"ls //*\"\n");
	fprintf(stderr, "  xdu XDFile get\n");
	fprintf(stderr, "    Copy all files and directories from Kronos volume to ./TMP/ directory\n");
	fprintf(stderr, "    NOTE: *.d and *.m files are converted to UTF-8\n");
	fprintf(stderr, "  xdu XDFile put fileName\n");
	fprintf(stderr, "    Copies given file to /host-exchange folder of the XD volume.\n");
	fprintf(stderr, "    NOTE: *.d and *.m files are converted from UTF-8 to KOI8-R\n");
	fprintf(stderr, "  xdu XDFile zfb\n");
	fprintf(stderr, "    Fills all free blocks on XD volume by zeroes\n");
	fprintf(stderr, "    to make its ZIP archive more compact\n\n");
	exit(0);
}

int main(int argc, char** argv)
{
	if (argc < 2) {
		help(); 
	}
	mount(argv[1]);
	if (argc > 2) {
		if (strcmp(argv[2], "get") == 0) {
			download();
		} else if (strcmp(argv[2], "zfb") == 0) {
				printf("%d blocks cleaned\n", zero_free_blocks());
		} else if (strcmp(argv[2], "put") == 0) {
			if (argc <= 3) {
				help(); 
			} else {
				upload(argv[3]);
			}
		} else {
			help(); 
		}
	} else list();
	unmount();
	return 0;
}
