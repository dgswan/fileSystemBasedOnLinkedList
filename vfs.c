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

#define E_NES 1000
#define E_NFB 1001
#define E_NFD 1002
#define E_FNE 1003

#define FS_FILE "/home/dmitry/VSU/OS/task3/fs"

typedef struct file_descr {
	char not_free;
	char is_folder;
	int start;
	int size;
	char name[FILENAME_LENGTH];
} fd_t;

//block is free: next[i] == -2
//block points to another block: next[i] == n
//block contains end of file: next[i] == -1

typedef struct fs {
	int next[BLOCK_NUMBER];
	fd_t fd[FILE_NUMBER];
} fs_t;

fs_t fs;


int init_fs() {
	int i = 0;
	for (i = 0; i < sizeof(fs.fd)/sizeof(fs.fd[0]); i++) {
		fs.fd[i].not_free = 0;
		fs.fd[i].is_folder = 0;
		memset(fs.fd[i].name, '\0', FILENAME_LENGTH);
		fs.fd[i].start = -1;
		fs.fd[i].size = 0;
	}
	for (i = 0; i < BLOCK_NUMBER; i++) {
		fs.next[i] = -2;
	}
	return 0;
}

int load_fs() {
	int i = 0;
	init_fs();
	FILE *pfile = fopen(FS_FILE,"r");
	fread(fs.fd, sizeof(fd_t), FILE_NUMBER, pfile);
	fread(fs.next, sizeof(int), BLOCK_NUMBER, pfile);
	for (i = 0; i < BLOCK_NUMBER; i++)
		if (fs.next[i] == 0)
			fs.next[i] = -2;
	fclose(pfile);
	return 0;
}

int find_free_fd_id() {
	int i = 0;
	for (i = 0; i < FILE_NUMBER; i++) {
		if (!fs.fd[i].not_free)
			return i;
	}
	return E_NFD;
}

int find_free_block() {
	int i = 0;
	for (i = 0; i < BLOCK_NUMBER; i++) {
		if (fs.next[i] == -2)
			return i;
	}
	return E_NFB;
}

fd_t *get_fd_by_id(int id) {
	return &fs.fd[id];
}

int delete_file(char *name) {
	return 0;
}

int write_fd(fd_t *fd, int id) {
	FILE *pfile = fopen(FS_FILE, "r+");
	fseek(pfile, id * sizeof(fd_t), SEEK_SET);
	fwrite(fd, 1, sizeof(fd_t), pfile);
	fclose(pfile);
	return 0;
}

int write_data(fd_t *fd, void *data, int size) {
	if (size == 0)
		return 0;
	int i = fd->start, j = 0, b = BLOCK_SIZE;
	FILE *pfile = fopen(FS_FILE, "r+");
	while (i != -1) {
		b = (j * BLOCK_SIZE + BLOCK_SIZE) > size ? size % BLOCK_SIZE : b;
		fseek(pfile, sizeof(fd_t) * FILE_NUMBER + sizeof(int) * BLOCK_NUMBER + i * BLOCK_SIZE, SEEK_SET);
		fwrite(data + j * BLOCK_SIZE, 1, 1 * b, pfile);
		i = fs.next[i];
		j++;
	}
	fd->size = size;
	fclose(pfile);
	return 0;
}

int write_precedence_vector(fd_t *fd) {
	FILE *pfile = fopen(FS_FILE, "r+");
	int i = fd->start;
	fseek(pfile, sizeof(fd_t) * FILE_NUMBER + i * sizeof(int), SEEK_SET);
	fwrite(&fs.next[i], sizeof(int), 1, pfile);
	while (i != -1) {
		i = fs.next[i];
		fseek(pfile, sizeof(fd_t) * FILE_NUMBER + i * sizeof(int), SEEK_SET);
		fwrite(&fs.next[i], sizeof(int), 1, pfile);
	}
	fclose(pfile);
	return 0;
}

int add_file(char *name, char *data, int size, char is_folder) {
	fd_t *fd = NULL;
	int start = 0, curr = 0, prev = -1, blocks = 0, written = 0, bytes_in_last_block = 0, i = 0;
	int fd_id = find_free_fd_id();
	if (fd_id == E_NFD)
		return E_NFD;
	fd = get_fd_by_id(fd_id);
	fd->not_free = 1;
	fd->is_folder = is_folder;
	fd->size = size;
	strcpy(fd->name, name);
	start = find_free_block();
	if (start == E_NFB)
		return E_NFB;
	//write to blocks
	fd->start = start;
	blocks = size / BLOCK_SIZE;
	bytes_in_last_block = size % BLOCK_SIZE;
	prev = curr = start;
	while (written != blocks) {
		written++;
		prev = curr;
		fs.next[prev] = -1;
		//find next block
		curr = find_free_block();
		if (curr == E_NFB) {
			delete_file(name);
			return E_NFB;
		}
		if (written == blocks)
			break;
		//set precedence for blocks
		fs.next[prev] = curr;
	}
	if (bytes_in_last_block != 0) {
		if (blocks != 0)
			fs.next[prev] = curr;
		prev = curr;
	}
	fs.next[prev] = -1;
	write_data(fd, data, size);
	write_fd(fd, fd_id);	
	write_precedence_vector(fd);
	return fd_id;
}


//--------------------------------------------------------

int get_data(fd_t *fd, char **buf) {
	FILE *pfile = fopen(FS_FILE, "r");
	char *data = NULL;
	int curr = 0;
	int gotBlocks = 0;
	int i = 0;
	int bytes = 0;
	if (fd == NULL)
		return -1;
	data = (char*)malloc(sizeof(char) * fd->size);
	curr = fd->start;
	while (curr != -1) {
		bytes = (fd->size - gotBlocks * BLOCK_SIZE) > BLOCK_SIZE ? BLOCK_SIZE : (fd->size - gotBlocks * BLOCK_SIZE);
		fseek(pfile, sizeof(fd_t) * FILE_NUMBER + BLOCK_NUMBER * sizeof(int) + curr * BLOCK_SIZE, SEEK_SET); 
		fread(data + gotBlocks * BLOCK_SIZE, 1, bytes, pfile);
		gotBlocks++;
		curr = fs.next[curr];
	}
	fclose(pfile);
	*buf = data;
	return fd->size;
}


int get_id(char *data, int size, char *filename) {
	int i = 0;
	for (i = 0; i < size / sizeof(int); i++)
		if (strcmp(fs.fd[((int*)data)[i]].name, filename) == 0)
			return ((int*)data)[i];
	return -1;
}

int get_meta(const char *path, fd_t **fd) {
	int size = 0;
	fd_t *meta = NULL;
	int meta_id = -1;
	int id = -1;
	char filename[FILENAME_LENGTH], *data, *start, *ptr = path;
	memset(filename, '\0', FILENAME_LENGTH);
	if (strcmp(path, "/") == 0) {
		*fd = get_fd_by_id(0);
		return 0;
	}
	if (*ptr++ == '/')
		meta = get_fd_by_id(0);
	else 
		return NULL;
	while ((ptr - path) != strlen(path)) {
		if (meta->size == 0)
			return -1;
		size = get_data(meta, &data);
		meta = NULL;
		start = ptr;
		while ((*ptr++ != '/') && ((ptr - path) < strlen(path)));
		(ptr - path) < strlen(path) ? strncpy(filename, start, ptr - start - 1) : strncpy(filename, start, ptr - start);
		id = get_id(data, size, filename);
		if (id == -1)
			return -1;
		meta = get_fd_by_id(id);
		memset(filename, '\0', FILENAME_LENGTH);
		free(data);
	}
	*fd = meta;
	return id;
}

int mkdir1(const char *path, int type) {
	int i = 0;
	int fd = 0, size = 0, id = -1;
	char *p = NULL, *ptr = path;
	char *directory;
	char *filename;
	int *data, *mdata;
	fd_t *meta;
	while (p = *ptr == '/' ? ptr : p, *ptr++ != '\0');
	if ((p - path) != 0) {
		directory = (char*)malloc(sizeof(char) * (p - path));
		strncpy(directory, path, p - path);
		directory[p - path] = '\0';
	}
	else {
		directory = (char*)malloc(sizeof(char) * 2);
		strcpy(directory, "/\0");
	}
	filename = (char*)malloc(sizeof(char) * (ptr - p));
	strncpy(filename, p + 1, ptr - p);
	id = get_meta(directory, &meta);
	size = get_data(meta, &data);
	mdata = (char*)malloc(size + sizeof(int));
	memcpy(mdata, data, size);
	fd = add_file(filename, NULL, 0, type);
	mdata[size/sizeof(int)] = fd;
	write_data(meta, mdata, size + sizeof(int));
	meta->size = size + sizeof(int);
	write_fd(meta, id);
	free(data);
	free(filename);
	free(directory);
	return 0;
}

int remove_dir(const char *path) {
	int i = 0, j = 0;
	int fd = 0, size = 0, directory_id = -1, file_id = -1;
	char *p = NULL, *ptr = path;
	char *directory;
	int *data, *mdata;
	fd_t *directory_fd, *file_fd;
	while (p = *ptr == '/' ? ptr : p, *ptr++ != '\0');
	if ((p - path) != 0) {
		directory = (char*)malloc(sizeof(char) * (p - path));
		strncpy(directory, path, p - path);
		directory[p - path] = '\0';
	}
	else {
		directory = (char*)malloc(sizeof(char) * 2);
		strcpy(directory, "/\0");
	}
	directory_id = get_meta(directory, &directory_fd);
	file_id = get_meta(path, &file_fd);
	size = get_data(directory_fd, &data);

	mdata = (char*)malloc(size - sizeof(int));
	for (i = 0; i < size/sizeof(int); i++)
		if (data[i] != file_id)
			mdata[j++] = data[i];
	write_data(directory_fd, mdata, size - sizeof(int));
	directory_fd->size = size - sizeof(int);
	write_fd(directory_fd, directory_id);
	free(data);
	free(directory);
	return 0;
}



int create_clear_fs() {
	int i = 0;
	char *buf;
	FILE *pfile;
	pfile = fopen(FS_FILE, "w+");
	//allocate place for file descriptors
	buf = (char*)malloc(sizeof(fd_t));
	memset(buf, '\0', sizeof(fd_t));
	for (i = 0; i < FILE_NUMBER; i++) {
		fwrite(buf, sizeof(fd_t), 1, pfile);
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
	buf = (char*)malloc(BLOCK_SIZE);
	memset(buf, '\0', BLOCK_SIZE);
	for (i = 0; i < BLOCK_NUMBER; i++) {
		fwrite(buf, BLOCK_SIZE, 1, pfile);
	}
	free(buf);
	fclose(pfile);
	init_fs();
	add_file("/", buf, 0, 1);
	return 0;
}


//---------------------------------------------------------------------------

static int my_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;

	char *data;

	fd_t *meta;
	int id = get_meta(path, &meta);

	if (id == -1)
		return -ENOENT;

	memset(stbuf, 0, sizeof(struct stat));
	if (meta->is_folder == 1) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} 
	else {
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size = meta->size;
	};
	stbuf->st_mode = stbuf->st_mode | 0777;
	return res;
}

static int my_create (const char *path, mode_t mode, struct fuse_file_info *fi) {
	int res;
	res = mkdir1(path, 0);
	if (res != 0)
		return -1;
	return 0;
}

static int my_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
	int i = 0;
	(void) offset;
	(void) fi;
	char *data;
	int size;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	fd_t *meta;
	int id = get_meta(path, &meta);

	if (id == -1)
		return -ENOENT;

	size = get_data(meta, &data);

	if (meta->size == 0)
		return 0;

	for (i = 0; i < size / sizeof(int); i++)
		filler(buf, fs.fd[((int*)data)[i]].name, NULL, 0);
	return 0;
}

static int my_open(const char *path, struct fuse_file_info *fi)
{
	fd_t *meta;
	int fd_id = 0;
	fd_id = get_meta(path, &meta);
	if (fd_id == -1)
		return -ENOENT;
	return 0;
}

static int my_opendir(const char *path, struct fuse_file_info *fi) {
	fd_t *meta;
	int fd_id = 0;
	fd_id = get_meta(path, &meta);
	if (fd_id == -1)
		return -ENOENT;
	return 0;
}

static int my_mkdir(const char *path, mode_t mode) {
	int res;
	res = mkdir1(path, 1);
	if (res != 0)
		return -1;
	return 0;
}

static int my_unlink(const char *path) {
	int res;
	res = remove_dir(path);
	if (res != 0)
		return -1;
	return 0;
}

static int my_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
	fd_t *fd;
	int res;
	int fd_id = get_meta(path, &fd);
	if (fd_id == -1)
		return -ENOENT;
	res = write_data(fd, buf, size);
	write_fd(fd, fd_id);
	if (res != 0)
		return -1;
	return 0;
}

static int my_rmdir(const char *path) {
	int res;
	res = remove_dir(path);
	if (res != 0)
		return -1;
	return 0;
}

static int my_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	size_t len;
	char *data;
	fd_t *meta;
	get_meta(path, &meta);
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
	.rmdir		= my_rmdir,
	.create		= my_create,
	.unlink		= my_unlink,
	.write		= my_write,
	.opendir	= my_opendir,
};

int main(int argc, char *argv[])
{
	int i = 0;
	if (argc > 1 && strcmp(argv[1],"n") == 0)
		create_clear_fs();
	load_fs();
	printf("--------------------\n");
	for (i = 0; i < FILE_NUMBER; i++)
		printf("%s %d \n", fs.fd[i].name, fs.fd[i].start);
	for (i = 0; i < BLOCK_NUMBER; i++)
		printf("%d %d \n", i, fs.next[i]);
	printf("--------------------\n");
	return fuse_main(argc, argv, &hello_oper, NULL);
}
