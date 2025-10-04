/*
* disk layout:
* block 0 -- cold booter
* block 1 -- superblock (label, blocks ans inodes free/busy maps)
* blocks 2..(inodes_no + 63) DIV 64 + 2 - inodes table
* . . . 
* . . .
* . . .
*/

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "xduDisk.h"
#include "xduTime.h"

typedef int  WBLOCK[1024];
typedef char CBLOCK[4096];
typedef iNodeRec IBLOCK[4096 / 64];

typedef struct {
	FILE* file;

	union {
		char* data;
		WBLOCK* iblocks; 
		CBLOCK* cblocks; 
	};

	iNodeRec* inodes;
	char label[12];
	int i_no;
	int b_no;
	int c_time;
	int* bset;
	int* iset;
} xDiskRec;

static xDiskRec *disk = NULL;

void fatal(char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
	exit(1);
}


#define ISSET(set,bit) (((set[bit/32] >> (bit % 32)) & 1))
#define ISBUSY(set,bit) (!ISSET(set,bit))
#define INCL(set,bit) set[bit/32] |= (1 << (bit % 32))
#define EXCL(set,bit) set[bit/32] &= ~(1 << (bit % 32))

int calcBusy(int* set, int sz)
{
	int total = 0;
	for (int i = 0; i < sz; i++) 
		if (ISBUSY(set, i)) total++;
	return total;
}

void unpackSuper(xDiskRec* disk) {
	char* super = disk->data + 4096;
	strncpy(disk->label, super, 8);
	int *label = (int*)super;
	disk->i_no = label[4];
	disk->b_no = label[5];
	disk->c_time = label[7];
	disk->inodes = (iNode)&disk->data[4096 + 4096];
	disk->bset = (int*)(super + 64);
	disk->iset = disk->bset + ((disk->b_no + 31) / 32);
}

void mount(char* fname) {
	static xDiskRec _disk;
	
	assert(disk == null);
	FILE *file = fopen(fname, "r+b");
	if (!file) {
		perror("file open");
		exit(1);
	}
	if (fseek(file, 0, SEEK_END) != 0) {
		perror("SEEK ERROR");
		exit(1);
	}
	long fsize = ftell(file);
	if (fsize < 0) {
		perror("FTELL ERROR");
		exit(1);
	}
	if (fsize % 4096 != 0) fatal("%s: invalid file size %d", fname, fsize);
	
	_disk.data = (char *)malloc(fsize);
	if (_disk.data == NULL) 
		fatal("not enough memory for %d bytes\n", fsize);

	rewind(file);
	size_t read = fread(_disk.data, 1, fsize, file);
	if (read != fsize) {
		perror("FILE READ ERROR"); 
		exit(1);
	}

	unpackSuper(&_disk);
	_disk.file = file;
	disk = &_disk;
	printf("XD volume \"%s\":\n", fname);
	printf("  label  : \"%s\"\n", disk->label);
	printf("  blocks : %d / %d\n", disk->b_no, calcBusy(disk->bset, disk->b_no));
	printf("  inodes : %d / %d\n", disk->i_no, calcBusy(disk->iset, disk->i_no));
	printf("  created : "); pKronosTime(disk->c_time);
	printf("\n");
}

void unmount()
{
	if (disk) {
		free(disk->data);
		fclose(disk->file);
		disk->file = null;
		disk = null;
	}
}

void xfile_open(int ino, xFile *file)
{
	assert((file != null));
	assert((disk != null));
	assert((ino >= 0 && ino < disk->i_no));

	file->inode = &disk->inodes[ino];
	file->blocks_no = (file->inode->eof + 4095) / 4096;
	if (file->inode->mode & i_long) {
		file->blocks = (int *) (disk->iblocks + file->inode->ref[0]);
		if (file->blocks_no > 1024) 
			fatal("files over 4Mb are not supported yet, sorry\n");
	}
	else {
		file->blocks = file->inode->ref;
		if (file->blocks_no > 8) 
			fatal("invalid file descriptor %d\n",ino);
	}
	file->data = null;
}

void xfile_close(xFile* file)
{
	assert((file != null && file->inode != null));

	if (file->data != null) free(file->data);
	file->data = null;
	file->blocks = null;
	file->blocks_no = 0;
	file->inode = null;
}

char* xfile_read(xFile* file) // allocate buffer, reads eof bytes and returns pointer to the buf
{
	assert((file != null && file->inode != null));

	if (file->data == null) {
		file->data = malloc(file->blocks_no * 4096);
		if (file->data == null) 
			fatal("not enough memory\n");
		CBLOCK* buf = (CBLOCK*)file->data;
		int i = 0;
		while (i < file->blocks_no) 
			memcpy(buf++, disk->cblocks + file->blocks[i++], sizeof(CBLOCK));
	}
	return file->data;
}

void xdir_open(int ino, xDir* dir)
{
	assert((dir != null));
	assert((disk != null));
	assert((ino >= 0 && ino < disk->i_no));

	iNode inode = &disk->inodes[ino];
	if ((inode->mode & i_dir) == 0) {
		printf("file %d is not directory", ino);
		exit(1);
	}
	if (inode->eof % sizeof(dNode) != 0) {
		printf("WARNING: directory %d has invalid file size %d", ino, inode->eof);
	}
	xfile_open(ino, &dir->file);
	dir->dnodes_no = inode->eof / sizeof(dNodeRec);
	dir->dnodes = (dNode)xfile_read(&dir->file);
}

void xdir_close(xDir* dir)
{
	if (dir == null || dir->file.data == null) return;
	xfile_close(&dir->file);
}

iNode get_inode(int no)
{
	assert(disk != null && no >= 0 && no < disk->i_no);
	return disk->inodes + no;
}


void flushBlock(int no)
{
	assert(disk != null && no >= 0 && no < disk->b_no);
	assert(fseek(disk->file, no * 4096, SEEK_SET) == 0);
	if ( fwrite(disk->data + 4096*no, 1, 4096, disk->file) != 4096 )
		fatal("Error writing block %d\n", no);
}

int isEmpty(WBLOCK blk)
{
	int i = 1024;
	while (i--)
		if (*(blk++)) 
			return 0;
	return 1;
}

int zeroFreeBlocks()
{
	assert(disk != null);
	int i = disk->b_no, total = 0;
	while (i--) {
		if (ISSET(disk->bset,i) && !isEmpty(disk->iblocks[i])) {
			total++;
			memset(disk->cblocks[i], 0, 4096);
			flushBlock(i);
		}
	}
	printf("%d\n", total);
	return total;
}

/*
template <T> DYNARR struct {
	int HIGH;
	T* ADR;

	DYNARR<T> new(int ammount)
	{
		assert(ammount > 0);
		DYNARR<T>* dynarr = malloc(sizeof(DYNARR + ammount * sizeof(T));
		if (dynarr !- NULL) {
			dynarr->HIGH = ammount - 1;
			dynarr->ADR = (T*)((char*)dynarr + sizeof(DYNARR));
		}	
		return dynarr;
	}

	inline T operator[int index] 
	{
		assert(index >= 0 && index <= it.HIGH);
		return *(it.ADR + index);
	}

	inline operator[int index] (T data)
	{
		assert(ndex >= 0 && index <= it.HIGH);
		*(it.ADR + index) = data;
	}
};
*/
