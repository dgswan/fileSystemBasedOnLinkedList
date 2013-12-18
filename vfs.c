/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall hello.c `pkg-config fuse --cflags --libs` -o hello
*/

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <stdlib.h>

static const char *hello_str = "Hello World!\n";
static const char *hello_path = "/hello";


typedef struct block {	
	int next;
	char *data;
} block_t;

typedef struct file {
	char *name;
	int start;
	int end;
} file_t;

typedef struct directory {
	file_t *files;
	int size;
} directory_t;

typedef struct filesystem {
	directory_t directory;
	block_t *blocks;
	int blockSize;
	int blockNumber;
} filesystem_t;

int directory_init(directory_t *d, int size) {
	int i = 0;
	d->files = (file_t*)malloc(sizeof(file_t) * size);
	if (d->files == NULL) {
		perror("Can't allocate memory for file metainformation");
		return -2;
	}
	for (i = 0; i < size; i++) {
		d->files[i].name = (char*)malloc(sizeof(char) * 32);
		if (d->files[i].name == NULL) {
			perror("Can't allocate memory for file name");
			return -3;
		}
	}
	d->size = size;
	return 0;
}

int filesystem_init(filesystem_t *fs, int blockSize, int blockNumber) {
	int i = 0;
	directory_init(&fs->directory, blockNumber);
	fs->blocks = (block_t*)malloc(sizeof(block_t) * blockNumber);
	if (fs->blocks == NULL) {
		perror("Can't allocate memory for blocks");
		return -3;
	}
	for (i = 0; i < blockNumber; i++) {
		fs->blocks[i].data = (char*)malloc(sizeof(char) * blockSize);
		if (fs->blocks[i].data == NULL) {
			perror("Can't allocate memory for block data");
			return -4;
		}
		memset(fs->blocks[i].data, 0, blockSize);
		fs->blocks[i].next = -2;
	}
	fs->blockSize = blockSize;
	fs->blockNumber = blockNumber;
	return 0;
}

int fileinfo_add(char *name, int start, int end, directory_t *directory) {
	int i = 0;
	for (i = 0; i < directory->size; i++) {
		if (strcmp(directory->files[i].name,"") == 0) {
			strcpy(directory->files[i].name, name);
			directory->files[i].start = start;
			directory->files[i].end = end;
			return 0;
		}
	}
	return -1;
}

int file_add(char *name, char *data, int size, filesystem_t *fs) {
	block_t *previousBlock = NULL;
	int start = -1, end = -1;
	int i = 0, j = 0; 
	int n = size / fs->blockSize;
	for (i = 0; i < n; i++) {
		for (j = 0; j < fs->blockNumber; j++) {
			if (fs->blocks[j].next == -2) {
				if (previousBlock != NULL) {
					previousBlock->next = j;
				}
				previousBlock = &fs->blocks[j];
				fs->blocks[j].data = memcpy(fs->blocks[j].data, data + i * fs->blockSize, fs->blockSize);
				fs->blocks[j].next = -1;
				if (i == 0) {
					start = j;
				}
				break;
			}
		}
	}
	if (size % fs->blockSize != 0) {
		for (j = 0; j < fs->blockNumber; j++) {
			if (fs->blocks[j].next == -2) {	
				if (previousBlock != NULL) {
					previousBlock->next = j;
				}
				memcpy(fs->blocks[j].data, data + n * fs->blockSize, size - n * fs->blockSize);
				fs->blocks[j].next = -1;
				if (n == 0) {
					start = j;
				}
				end = j;
				break;
			}
		}
	}
	fileinfo_add(name, start, end, &fs->directory);
	return 0;
}

char *file_get(char *name, filesystem_t *fs) {
	char *file = (char*)malloc(sizeof(char) * 256);
	int i = 0, j = 0;
	for (i = 0; i < fs->blockNumber; i++) {
		if (strcmp(fs->directory.files[i].name, name) == 0) {
			for (j = fs->directory.files[i].start; j != fs->directory.files[i].end; j = fs->blocks[j].next) {
				strcat(file, fs->blocks[j].data);
			}
			return file;
		}
	}
	return NULL;
}

int filesystem_print(filesystem_t *fs) {
	int i = 0;
	printf("Directory: \n");
	for (i = 0; i < fs->directory.size; i++) {
		printf("Name: %s Start: %d End: %d \n", fs->directory.files[i].name, fs->directory.files[i].start, fs->directory.files[i].end);
	}
	printf("Blocks: \n");
	for (i = 0; i < fs->blockNumber; i++) {
		printf("Block number: %d Data: %s Next block: %d \n", i, fs->blocks[i].data, fs->blocks[i].next);
	}
	return 0;
}

int file_delete(char *name, filesystem_t *fs) {
	int next = 0;
	int i = 0, j = 0;
	for (i = 0; i < fs->blockNumber; i++) {
		if (strcmp(fs->directory.files[i].name, name) == 0) {
			for (j = fs->directory.files[i].start; j != fs->directory.files[i].end; j = next) {
				next = fs->blocks[j].next;
				fs->blocks[j].next = -2;
				memset(fs->blocks[j].data, 0, fs->blockSize);
			}
			memset(fs->blocks[j].data, 0, fs->blockSize);
			return 0;
		}
	}
	return -1;
}

int fs_save(filesystem_t *fs) {
	int i = 0;
	FILE *pfile = fopen("fs", "w");
	fwrite(&fs->directory.size, sizeof(int), 1, pfile);
	fwrite(&fs->blockNumber, sizeof(int), 1, pfile);
	fwrite(&fs->blockSize, sizeof(int), 1, pfile);
	for (i = 0; i < fs->directory.size; i++) {
		fwrite(fs->directory.files[i].name, sizeof(char), 32, pfile);
		fwrite(&fs->directory.files[i].start, sizeof(int), 1, pfile);
		fwrite(&fs->directory.files[i].end, sizeof(int), 1, pfile);
	}
	for (i = 0; i < fs->blockNumber; i++) {
		fwrite(fs->blocks[i].data, sizeof(char), fs->blockSize, pfile);
		fwrite(&fs->blocks[i].next, sizeof(int), 1, pfile);
	}
	fclose(pfile);
	return 0;
}

filesystem_t *fs_load() {
	filesystem_t *fs = (filesystem_t*)malloc(sizeof(filesystem_t));
	int i = 0;
	int blockNumber = 0, blockSize = 0, directorySize = 0;
	if (fs == NULL) {
		return NULL;
	}
	FILE *pfile = fopen("fs", "r");
	fread(&directorySize, sizeof(int), 1, pfile);
	printf("%d \n", directorySize);
	fread(&blockNumber, sizeof(int), 1, pfile);
	printf("%d \n", blockNumber);
	fread(&blockSize, sizeof(int), 1, pfile);
	printf("%d \n", blockSize);
	filesystem_init(fs, blockSize, blockNumber);
	for (i = 0; i < fs->directory.size; i++) {
		fread(fs->directory.files[i].name, sizeof(char), 32, pfile);
		fread(&fs->directory.files[i].start, sizeof(int), 1, pfile);
		fread(&fs->directory.files[i].end, sizeof(int), 1, pfile);
		//printf("%s %d %d \n", fs->
	}
	for (i = 0; i < fs->blockNumber; i++) {
		fread(fs->blocks[i].data, sizeof(char), fs->blockSize, pfile);
		fread(&fs->blocks[i].next, sizeof(int), 1, pfile);
	}
	fclose(pfile);
	filesystem_print(fs);
	return fs;
}

static int hello_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;

	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else if (strcmp(path, hello_path) == 0) {
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size = strlen(hello_str);
	} else
		res = -ENOENT;

	return res;
}

static int hello_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	(void) offset;
	(void) fi;

	if (strcmp(path, "/") != 0)
		return -ENOENT;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	filler(buf, hello_path + 1, NULL, 0);

	return 0;
}

static int hello_open(const char *path, struct fuse_file_info *fi)
{
	if (strcmp(path, hello_path) != 0)
		return -ENOENT;

	if ((fi->flags & 3) != O_RDONLY)
		return -EACCES;

	return 0;
}

static int hello_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	size_t len;
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

	return size;
}

static struct fuse_operations hello_oper = {
	.getattr	= hello_getattr,
	.readdir	= hello_readdir,
	.open		= hello_open,
	.read		= hello_read,
};

int main(int argc, char *argv[])
{
	//return fuse_main(argc, argv, &hello_oper, NULL);
	int error = 0;
	char *fileName = "file";
	char *fileData = "data";
	int dataSize = strlen(fileData);

	char *bigFileName = "bigFile";
	char *bigFileData = "mlwnbowhbiuwgybvwnlniuhcnpwomcpowouguiwoijvmwopouwgyvunwlmvpiwbiwbvggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggg";
	int bigDataSize = strlen(bigFileData);

	filesystem_t *fs;

	fs = fs_load();

	//printf("%d \n", &fs);

	filesystem_print(fs);

/*	filesystem_t fs;
	if ((error = filesystem_init(&fs, 50, 32)) != 0) {
		printf("fs init error %d", error);
		return -1;
	}
	file_add(fileName, fileData, dataSize, &fs);
	file_add(fileName, fileData, dataSize, &fs);
	file_add(fileName, fileData, dataSize, &fs);
	file_add(bigFileName, bigFileData, bigDataSize, &fs);
	filesystem_print(&fs);
	file_delete(bigFileName, &fs);
	file_add(fileName, fileData, dataSize, &fs);
	file_add(fileName, fileData, dataSize, &fs);
	filesystem_print(&fs);
	fs_save(&fs);*/
}

