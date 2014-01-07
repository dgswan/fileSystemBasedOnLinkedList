#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <stdlib.h>

#define BLOCK_NUMBER 32
#define BLOCK_SIZE 32
#define FILENAME_LENGTH 32
#define FILE_NUMBER 32

#define IS_NOT_ENOUGH_SPACE 1000
#define NO_FREE_BLOCKS 1001
#define NO_FREE_FILE_DESCR_SPACE 1002
#define FILE_DOES_NOT_EXIST 1003

#define FS_FILE "fs"

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


int init_fs() {
	int i = 0;
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
	return 0;
}

int save_fs(char *fileName) {
	int i = 0;
	/*FILE *pfile = fopen(fileName,"w");
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
	fclose(pfile);*/
	return 0;
}

int load_fs(char *fileName) {
	int i = 0;
	/*FILE *pfile = fopen(fileName,"r");
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
	fclose(pfile);*/
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
			return &fs.fd[i];
		}
	}
	return NULL;
}

int write_fd(file_descr_t *fd, int id) {
	FILE *pfile = fopen(FS_FILE, "r+");
	fseek(pfile, id * sizeof(file_descr_t), SEEK_SET);
	fwrite(fd, 1, sizeof(file_descr_t), pfile);
	fclose(pfile);
	return 0;
}

int _write(file_descr_t *fd, void *data, int size) {
	if (size == 0)
		return 0;
	int i = fd->start, j = 0, b = BLOCK_SIZE;
	FILE *pfile = fopen("fs", "r+");
	while (i != -1) {
		b = (j * BLOCK_SIZE + BLOCK_SIZE) > size ? size % BLOCK_SIZE : b;
		fseek(pfile, sizeof(file_descr_t) * FILE_NUMBER + sizeof(int) * BLOCK_NUMBER + i * BLOCK_SIZE, SEEK_SET);
		fwrite(data + j * BLOCK_SIZE, 1, 1 * b, pfile);
		i = fs.nextBlock[i];
		j++;
	}
	fclose(pfile);
	return 0;
}

int write_precedence_vector(file_descr_t *fd) {
	FILE *pfile = fopen(FS_FILE, "r+");
	int i = fd->start;
	while (i != -1) {
		fseek(pfile, sizeof(file_descr_t) * FILE_NUMBER + i * sizeof(int), SEEK_SET);
		fwrite(&i, sizeof(int), 1, pfile);
		i = fs.nextBlock[i];
	}
	fclose(pfile);
	return 0;
}

int add_file(char *name, char *data, int size, char isFolder) {
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
	fs.fd[file_descr_place].isFolder = isFolder;
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
		if (blocks != 0)
			fs.nextBlock[previousBlock] = currentBlock;
		previousBlock = currentBlock;
		fs.nextBlock[previousBlock] = -1;
	}
	_write(&fs.fd[file_descr_place], data, size);
	write_fd(&fs.fd[file_descr_place], file_descr_place);
	write_precedence_vector(&fs.fd[file_descr_place]);
	return file_descr_place;
}


//--------------------------------------------------------

int get_data(file_descr_t *fd, char **data1) {
	FILE *pfile = fopen(FS_FILE, "r");
	char *data = NULL;
	int currentBlock = 0;
	int gotBlocks = 0;
	int i = 0;
	int bytes = 0;
	if (fd == NULL)
		return -1;
	data = (char*)malloc(sizeof(char) * fd->size);
	currentBlock = fd->start;
	while (currentBlock != -1) {
		bytes = (fd->size - gotBlocks * BLOCK_SIZE) > BLOCK_SIZE ? BLOCK_SIZE : (fd->size - gotBlocks * BLOCK_SIZE);
		fseek(pfile, sizeof(file_descr_t) * FILE_NUMBER + BLOCK_NUMBER * sizeof(int) + currentBlock * BLOCK_SIZE, SEEK_SET); 
		fread(data + gotBlocks * BLOCK_SIZE, 1, bytes, pfile);
		gotBlocks++;
		currentBlock = fs.nextBlock[currentBlock];
	}
	fclose(pfile);
	*data1 = data;
	return fd->size;
}

file_descr_t *get_meta_by_id(int id) {
	return &fs.fd[id];
}

int get_id(char *data, int size, char *filename) {
	int i = 0;
	for (i = 0; i < size / sizeof(int); i++)
		if (strcmp(fs.fd[((int*)data)[i]].name, filename) == 0)
			return ((int*)data)[i];
	return -1;
}

file_descr_t *get_meta(const char *path) {
	int size = 0;
	file_descr_t *meta = NULL;
	int id = -1;
	char filename[FILENAME_LENGTH], *data, *start, *ptr = path;
	memset(filename, '\0', FILENAME_LENGTH);
	if (*ptr++ == '/') {
		meta = get_meta_by_id(0);
	}
	else 
		return NULL;
	while ((ptr - path) != strlen(path)) {
		size = get_data(meta, &data);
		meta = NULL;
		start = ptr;
		while ((*ptr++ != '/') && ((ptr - path) < strlen(path)));
		(ptr - path) < strlen(path) ? strncpy(filename, start, ptr - start - 1) : strncpy(filename, start, ptr - start);
		id = get_id(data, size, filename);
		if (id != -1) {
			meta = get_meta_by_id(id);
		}
		memset(filename, '\0', FILENAME_LENGTH);
		free(data);
	}
	return meta;
}

int mkdir1(const char *path) {
	int fd = 0, size = 0;
	char *p = NULL, *ptr = path;
	char *directory;
	char *filename;
	char *data, *mdata;
	file_descr_t *meta;
	while (*ptr++ != '\0') p = *ptr == '/' ? ptr : p;
	directory = (char*)malloc(sizeof(char) * (p - path));
	filename = (char*)malloc(sizeof(char) * (ptr - p));
	strncpy(directory, path, p - path);
	strncpy(filename, p + 1, ptr - p);
	meta = get_meta(directory);
	size = get_data(meta, &data);
	mdata = (char*)malloc(sizeof(int) * (size + 1));
	memcpy(mdata, data, size);
	fd = add_file(filename, NULL, 0, 1);
	mdata[size + 1] = fd;
	free(data);
	free(filename);
	free(directory);
	return 0;
}


int create_clear_fs() {
	printf("Hello");
	int i = 0;
	char *buf;
	FILE *pfile;
	pfile = fopen("fs", "w+");
	//allocate place for file descriptors
	buf = (char*)malloc(sizeof(file_descr_t));
	memset(buf, '\0', sizeof(file_descr_t));
	for (i = 0; i < FILE_NUMBER; i++) {
		fwrite(buf, sizeof(file_descr_t), 1, pfile);
	}
	free(buf);
	//allocate place for precedence vector
	buf = (char*)malloc(sizeof(int));
	memset(buf, '\0', sizeof(int));
	for (i = 0; i < BLOCK_NUMBER; i++) {
		fwrite(buf, sizeof(int), 1, pfile);
	}
	free(buf);
	//allocate place for data blocks
	buf = (char*)malloc(sizeof(block_t));
	memset(buf, '\0', sizeof(block_t));
	for (i = 0; i < BLOCK_NUMBER; i++) {
		fwrite(buf, sizeof(block_t), 1, pfile);
	}
	free(buf);
	fclose(pfile);
	add_file("/", buf, 0, 1);
	return 0;
}


//---------------------------------------------------------------------------

static int my_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;

	char *data;

	file_descr_t *meta = get_meta(path);

	if (meta == NULL)
		return -ENOENT;

	int len =  get_data(meta, &data);

	if (len < 0)
		return -ENOENT;

	memset(stbuf, 0, sizeof(struct stat));
	if (meta->isFolder == 1) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} 
	else {
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size = meta->size;
	};
	return res;
}

static int my_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	int i = 0;
	(void) offset;
	(void) fi;
	char *data;
	int size;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	file_descr_t *meta = get_meta(path);
	if (meta == NULL)
		return -ENOENT;

	size = get_data(meta, &data);

	if (size < 0)
		return -ENOENT;

	for (i = 0; i < size / sizeof(int); i++)
		filler(buf, fs.fd[((int*)data)[i]].name, NULL, 0);
	return 0;
}

static int my_open(const char *path, struct fuse_file_info *fi)
{
	char *data;
	file_descr_t *meta = get_meta(path);
	if (meta == NULL)
		return -ENOENT;
	int len = get_data(meta, &data);
	if (len < 0)
		return -ENOENT;
	if ((fi->flags & 3) != O_RDONLY)
		return -EACCES;
	return 0;
}

static int my_mkdir(const char *path, mode_t mode)
{
	int res;
	res = mkdir1(path);
	if (res != 0)
		return -1;
	return 0;
}

static int my_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	size_t len;
	char *data;
	file_descr_t *meta = get_meta(path);
	if (meta == NULL)
		return -ENOENT;

	len = get_data(meta, &data);

	if (len < 0)
		return -ENOENT;
	if (offset < len) {
		if (offset + size > len)
			size = len - offset;
		memcpy(buf, data + offset, size);
	}
	else
		size = 0;
	return size;
}

static struct fuse_operations hello_oper = {
	.getattr	= my_getattr,
	.readdir	= my_readdir,
	.open		= my_open,
	.read		= my_read,
	.mkdir		= my_mkdir,
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

	if (argc > 1 && strcmp(argv[1],"n") == 0) {
		//init_fs();
		create_clear_fs();
	}
	else {
		init_fs();
		add_file(root, fd, sizeof(int) * 5, 1);
		add_file(fileName1, data1, strlen(data1), 0);
		add_file(fileName2, data2, strlen(data2), 0);
		add_file(fileName3, data3, strlen(data3), 0);
		add_file(fileName4, data4, strlen(data4), 0);
		add_file(folder, fd1, sizeof(int) * 2, 1);
		add_file(fileName5, data32, strlen(data32), 0);
		add_file(fileName2, data1, strlen(data1), 0);
		
		file_descr_t *mfd = get_meta("/file");
		size = get_data(mfd, &buf);
		printf("%s \n", buf);
		free(buf);

		mfd = get_meta("/folder/ui");
		size = get_data(mfd, &buf);
		printf("%s \n", buf);
		free(buf);
	}
	//return fuse_main(argc, argv, &hello_oper, NULL);
	//if (mkdir1("/fol") != 0)
	//	printf("failure\n");
	//file_descr_t *mfd = get_meta("/fol");
	//printf("%s \n", mfd->name);
	return 0;
}
