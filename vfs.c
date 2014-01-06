/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall hello.c `pkg-config fuse --cflags --libs` -o hello
*/


// UNION_FIND (data structure) valgrind (profiler) gdb (debugger)

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <stdlib.h>

static const char *hello_str = "Hello World!\n";
static const char *hello_path = "/hello";

#define BLOCK_NUMBER 32
#define BLOCK_SIZE 32
#define FILENAME_LENGTH 8
#define FILE_NUMBER 32

#define IS_NOT_ENOUGH_SPACE 1000
#define NO_FREE_BLOCKS 1001
#define NO_FREE_FILE_DESCR_SPACE 1002
#define FILE_DOES_NOT_EXIST 1003

#define myfread(FILENAME, BUFFER) open_file(FILENAME, &BUFFER)
#define myfread1(FILENAME, PTR, FILEN, BUFFER) open_file1(FILENAME, PTR, FILEN, BUFFER)
#define readdata(FILENAME, BUFFER) get_file_data(FILENAME, &BUFFER)

typedef struct block {	
	char data[BLOCK_SIZE];
} block_t;

typedef struct file_descr {
	char isFree;
	char isFolder;
	int start;
	int size;//size in blocks = size/ BLOCK_SIZE
	char name[FILENAME_LENGTH];
} file_descr_t;

typedef struct filesystem {
	//{ block is free: nextBlock[i] == -2, block points to another block: nextBlock[i] == n, block contains end of file: nextBlock[i] == -1}
	int nextBlock[BLOCK_NUMBER];//adjacent blocks
	block_t blocks[BLOCK_NUMBER];//blocks 
	file_descr_t fd[FILE_NUMBER];//fat table
} filesystem_t;

filesystem_t fs;
char *currentDirectory;

int init_fs() {
	int i = 0;
	currentDirectory = "/";
	//initialize fat table
	for (i = 0; i < sizeof(fs.fd)/sizeof(fs.fd[0]); i++) {
		fs.fd[i].isFree = 1;
		fs.fd[i].isFolder = 0;
		memset(fs.fd[i].name, '\0', FILENAME_LENGTH);
		fs.fd[i].start = -1;
		fs.fd[i].size = 0;
	}
	//initialize next blocks
	for (i = 0; i < BLOCK_NUMBER; i++) {
		fs.nextBlock[i] = -2;
	}
	//initialize blocks
	for (i = 0; i < BLOCK_NUMBER; i++) {
		memset(fs.blocks[i].data, '\0', BLOCK_SIZE);
	}
}

int save_fs(char *fileName) {
	int i = 0;
	FILE *pfile = fopen(fileName,"w");
	//write fat table
	for (i = 0; i < sizeof(fs.fd)/sizeof(fs.fd[0]); i++) {
		fwrite(&fs.fd[i].isFree, sizeof(char), 1, pfile);
		fwrite(&fs.fd[i].isFolder, sizeof(char), 1, pfile);
		fwrite(fs.fd[i].name, sizeof(char), FILENAME_LENGTH, pfile);
		fwrite(&fs.fd[i].start, sizeof(int), 1, pfile);
		fwrite(&fs.fd[i].size, sizeof(int), 1, pfile);
	}
	//write next blocks
	for (i = 0; i < BLOCK_NUMBER; i++) {
		fwrite(&fs.nextBlock[i], sizeof(int), 1, pfile);
	}
	//write blocks
	for (i = 0; i < BLOCK_NUMBER; i++) {
		fwrite(fs.blocks[i].data, sizeof(char), BLOCK_SIZE, pfile);
	}
	fclose(pfile);
	return 0;
}

int load_fs(char *fileName) {
	int i = 0;
	FILE *pfile = fopen(fileName,"r");
	//read fat table
	for (i = 0; i < FILE_NUMBER; i++) {
		fread(&fs.fd[i].isFree, sizeof(char), 1, pfile);
		fread(&fs.fd[i].isFolder, sizeof(char), 1, pfile);
		fread(fs.fd[i].name, sizeof(char), FILENAME_LENGTH, pfile);
		fread(&fs.fd[i].start, sizeof(int), 1, pfile);
		fread(&fs.fd[i].size, sizeof(int), 1, pfile);
	}
	//read next blocks
	for (i = 0; i < BLOCK_NUMBER; i++) {
		fread(&fs.nextBlock[i], sizeof(int), 1, pfile);
	}
	//read blocks
	for (i = 0; i < BLOCK_NUMBER; i++) {
		fread(fs.blocks[i].data, sizeof(char), BLOCK_SIZE, pfile);
	}
	fclose(pfile);
	return 0;
}

int find_free_file_descr_place() {
	int i = 0;
	for (i = 0; i < FILE_NUMBER; i++) {
		if (fs.fd[i].isFree)
			return i;
	}
	return NO_FREE_FILE_DESCR_SPACE;
}

int find_free_block() {
	int i = 0;
	for (i = 0; i < BLOCK_NUMBER; i++) {
		if (fs.nextBlock[i] == -2)
			return i;
	}
	return NO_FREE_BLOCKS;
}

int delete_file(char *name) {
	return 0;
}

file_descr_t *get_file_meta(const char *name) {
	int i = 0;
	for (i = 0; i < FILE_NUMBER; i++) {
		if (strcmp(name, fs.fd[i].name) == 0) {
			printf("get file meta succeed \n");
			return &fs.fd[i];
		}
	}
	printf("There's no such a file\n");
	return NULL;
}

int get_file_data(const char *name, char **data1) {
	char *data = NULL;
	int currentBlock = 0;
	file_descr_t *fd;
	int gotBlocks = 0;
	int i = 0;
	int bytes = 0;
	fd = get_file_meta(name);
	if (fd == NULL)
		return -1;
	data = (char*)malloc(sizeof(char) * fd->size);
	currentBlock = fd->start;
	while (currentBlock != -1) {
		bytes = (fd->size - gotBlocks * BLOCK_SIZE) > BLOCK_SIZE ? BLOCK_SIZE : (fd->size - gotBlocks * BLOCK_SIZE);
		memcpy(data + gotBlocks * BLOCK_SIZE, fs.blocks[currentBlock].data, bytes);			
		gotBlocks++;
		currentBlock = fs.nextBlock[currentBlock];
	}
	*data1 = data;
	return fd->size;
}

int add_file(char *name, char *data, int size) {
	int previousBlock = -1;
	int writtenBlocks = 0;
	int currentBlock = 0;
	int startBlock = 0;
	int blocks = 0;
	int lastBlockBytes = 0;
	int file_descr_place = find_free_file_descr_place();
	if (file_descr_place == NO_FREE_FILE_DESCR_SPACE)
		return NO_FREE_FILE_DESCR_SPACE;
	fs.fd[file_descr_place].isFree = 0;
	fs.fd[file_descr_place].isFolder = 0;
	fs.fd[file_descr_place].size = size;
	strcpy(fs.fd[file_descr_place].name, name);
	startBlock = find_free_block();
	if (startBlock == NO_FREE_BLOCKS)
		return NO_FREE_BLOCKS;
	//write to blocks
	fs.fd[file_descr_place].start = startBlock;
	blocks = size / BLOCK_SIZE;
	lastBlockBytes = size % BLOCK_SIZE;
	currentBlock = startBlock;
	while (writtenBlocks != blocks) {
		//copy data to block
		memcpy(fs.blocks[currentBlock].data, data + writtenBlocks * BLOCK_SIZE, BLOCK_SIZE);
		writtenBlocks++;
		previousBlock = currentBlock;
		fs.nextBlock[previousBlock] = -1;
		//find next block
		currentBlock = find_free_block();
		if (currentBlock == NO_FREE_BLOCKS) {
			delete_file(name);
			return NO_FREE_BLOCKS;
		}
		if (writtenBlocks == blocks) {
			fs.nextBlock[previousBlock] = -1;
			break;
		}
		//set precedence for blocks
		fs.nextBlock[previousBlock] = currentBlock;
	}
	if (lastBlockBytes != 0) {
		memcpy(fs.blocks[currentBlock].data, data + writtenBlocks * BLOCK_SIZE, lastBlockBytes);
		if (blocks != 0)
			fs.nextBlock[previousBlock] = currentBlock;
		previousBlock = currentBlock;
		fs.nextBlock[previousBlock] = -1;
	}
	return 0;
}

// /aaa/bbb/ccc/file || aaa/bbb/ccc/file
// aaa/bbb/ccc/file
// bbb/ccc/file
// ccc/file
// file
int open_file1(const char *path, char *ptr, char *file, char **buf) {
	char *data;
	file_descr_t *fd;
	int i = 0, fdescr = 0, data_size = 0;
	char *start;
	printf("%s \n", file);
	data_size = readdata(file, data);
	printf("data %s \n", data);
	//set all the bytes to '\0' in file, that means we'll be capable to read filename with strcpy
	memset(file, '\0', FILENAME_LENGTH);
	//if we've reached end of path we've got file data
	if ((ptr - path) == strlen(path)) {
		*buf = data;
		return data_size;
	}
	start = ptr;
	while (*ptr++ != '/' && strlen(path) != (ptr - path));
	(ptr - path) != strlen(path) ? strncpy(file, start, ptr - start - 1) : strncpy(file, start, ptr - start);
	printf("%s \n", file);
	fd = get_file_meta(file);
	//printf("%d \n", 
	for (i = 0; i < data_size/sizeof(int); i++) {
		fdescr = ((int*)data)[i];
		printf("%d %d \n", i, fdescr);
		//checking whether "file" is described by fd[fdescr]
		if (strcmp(fs.fd[fdescr].name, file) == 0) {
			printf("file has been found in folder\n");
			free(data);
			return myfread1(path, ptr, file, buf);
			//printf("buf %d \n", buf);
		}
	}
	//if we haven't found file it's not right path, there's no such a file
	return -1;
}

//path - full path to the file
//buf - buffer to write the data
//size - file size
//return - number of read bytes, < 0 on failure

int open_file(const char *path, char **buf) {
	int size = 0;
	int name_length = 0;
	//ptr points at current symbol in path
	char *ptr = path;
	//file contains file name in fat
	char file[FILENAME_LENGTH];
	memset(file, '\0', FILENAME_LENGTH);
	//look for the first '/' or the end of the path and remember its position
	while (*ptr++ != '/' && (strlen(path) != (ptr - path)));
	//save the string from the beginning to ptr
	name_length = ptr - path;
	name_length == 1 ? strncpy(file, path, name_length) : strncpy(file, path, name_length - 1);

	if ((size = myfread1(path, ptr, file, buf)) < 0)
		printf("open_file: There's no such a file \n");
	memset(file, '\0', FILENAME_LENGTH);
	return size;
}



static int hello_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;

	char *resData;
	int len =  myfread(path, resData);

	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} /*else if (strcmp(path, hello_path) == 0) {
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size = strlen(hello_str);
	} else
		res = -ENOENT;

	return res;*/
	else if (resData != NULL) {
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size = strlen(resData);
	} else
		res = -ENOENT;
	return res;
}

static int hello_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	int i = 0;
	(void) offset;
	(void) fi;

	if (strcmp(path, "/") != 0)
		return -ENOENT;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	//filler(buf, hello_path + 1, NULL, 0);
	for (i = 0; i < FILE_NUMBER; i++) {
		if (!fs.fd[i].isFree) {
			filler(buf, fs.fd[i].name, NULL, 0);
		}
	}

	return 0;
}

static int hello_open(const char *path, struct fuse_file_info *fi)
{
	/*if (strcmp(path, hello_path) != 0)
		return -ENOENT;

	if ((fi->flags & 3) != O_RDONLY)
		return -EACCES;

	return 0;*/
	char *resData;
	int len = myfread(path, resData);
	if (resData == NULL)
		return -ENOENT;
	if ((fi->flags & 3) != O_RDONLY)
		return -EACCES;
	return 0;
}

static int hello_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	size_t len;
	char *resData;
	len = myfread(path, resData);
	if (resData == NULL) {
		return -ENOENT;
	}
	//len = strlen(resData);
	if (offset < len) {
		if (offset + size > len)
			size = len - offset;
		memcpy(buf, resData + offset, size);
	}
	else
		size = 0;
	return size;
	/*size_t len;
	(void) fi;
	if(strcmp(path, hello_path) != 0)
		return -ENOENT;

	len = strlen(hello_str);
	if (offset < len) {
		if (offset + size > len)
			size = len - offset;
		memcpy(buf, hello_str + offset, size);
	} else
		size = 0;

	return size;*/
}

static struct fuse_operations hello_oper = {
	.getattr	= hello_getattr,
	.readdir	= hello_readdir,
	.open		= hello_open,
	.read		= hello_read,
};

int main(int argc, char *argv[])
{
	int i = 0;
	int size = 0;
	char *fileName1 = "file";
	char *data1 = "trololo";
	char *fileName2 = "bigFile";
	char *data2 = "trolologknekneuwinbonboinemlisnkjvbnblmb;wnboiwrnbkmlbknlkwnbl;wmbln";
	char *fileName3 = "jusFile";
	char *data3 = "I'm file!";
	char *fileName4 = "file1";
	char *data4 = "klnblwbnowobnoiwnbonw";
	char *root = "/";
	char *folder = "folder";
	char *buf;
	char *fileName5 = "ui";
	char *data32 = "trampapapaprara";
	int fd[5];
	fd[0] = 1;
	fd[1] = 2;
	fd[2] = 3;
	fd[3] = 4;
	fd[4] = 5;
	int fd1[2];
	fd1[0] = 6;
	fd1[1] = 7;

	init_fs();
	save_fs("fs");
	load_fs("fs");
	add_file(root, fd, sizeof(int) * 5);
	add_file(fileName1, data1, strlen(data1));
	add_file(fileName2, data2, strlen(data2));
	add_file(fileName3, data3, strlen(data3));
	add_file(fileName4, data4, strlen(data4));
	add_file(folder, fd1, sizeof(int) * 2);
	add_file(fileName5, data32, strlen(data32));
	add_file(fileName2, data1, strlen(data1));
	for (i = 0; i < FILE_NUMBER; i++) {
		printf("%d %s \n", i, fs.fd[i].name);
	}
	size = myfread("/", buf);
	for (i = 0; i < 4; i++)
		printf("%d %d \n", i, ((int*)buf)[i]);
	free(buf);
	size = myfread("/file", buf);
	printf("%s \n", buf);
	free(buf);
	size = myfread("/bigFile", buf);
	printf("%s \n", buf);
	free(buf);
	size = myfread("/jusFile", buf);
	printf("%s \n", buf);
	free(buf);
	size = myfread("/file1", buf);
	printf("%s \n", buf);
	free(buf);
	size = myfread("/folder/ui", buf);
	printf("%s \n", buf);
	free(buf);
	save_fs("fs");
	//return fuse_main(argc, argv, &hello_oper, NULL);
	return 0;
}
