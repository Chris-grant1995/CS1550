/*
Chris Grant CS 1550 Project 4
	FUSE: Filesystem in Userspace
	Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

	This program can be distributed under the terms of the GNU GPL.
	See the file COPYING.

	gcc -Wall `pkg-config fuse --cflags --libs` cs1550.c -o cs1550
*/

#define	FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

//size of a disk block
#define	BLOCK_SIZE 512

#define BITMAPSIZE 1280 //Bitmap size in bytes, gets converted down into bits when dealing with bitmap
//5MB = 2^22 + 2^20 bits
//(2^22 + 2^20)/512 bit blocks = 10,240
//10240/8 bits in a byte = 1280


//we'll use 8.3 filenames
#define	MAX_FILENAME 8
#define	MAX_EXTENSION 3

//How many files can there be in one directory?
#define	MAX_FILES_IN_DIR (BLOCK_SIZE - (MAX_FILENAME + 1) - sizeof(int)) / \
	((MAX_FILENAME + 1) + (MAX_EXTENSION + 1) + sizeof(size_t) + sizeof(long))

//How much data can one block hold?
#define	MAX_DATA_IN_BLOCK (BLOCK_SIZE - sizeof(unsigned long))

struct cs1550_directory_entry
{
	int nFiles;	//How many files are in this directory.
				//Needs to be less than MAX_FILES_IN_DIR

	struct cs1550_file_directory
	{
		char fname[MAX_FILENAME + 1];	//filename (plus space for nul)
		char fext[MAX_EXTENSION + 1];	//extension (plus space for nul)
		size_t fsize;					//file size
		long nStartBlock;				//where the first block is on disk
	} __attribute__((packed)) files[MAX_FILES_IN_DIR];	//There is an array of these

	//This is some space to get this to be exactly the size of the disk block.
	//Don't use it for anything.
	char padding[BLOCK_SIZE - MAX_FILES_IN_DIR * sizeof(struct cs1550_file_directory) - sizeof(int)];
} ;

typedef struct cs1550_directory_entry cs1550_directory_entry;

#define MAX_DIRS_IN_ROOT (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + sizeof(long))

struct cs1550_root_directory
{
	int nDirectories;	//How many subdirectories are in the root
						//Needs to be less than MAX_DIRS_IN_ROOT
	struct cs1550_directory
	{
		char dname[MAX_FILENAME + 1];	//directory name (plus space for nul)
		long nStartBlock;				//where the directory block is on disk
	} __attribute__((packed)) directories[MAX_DIRS_IN_ROOT];	//There is an array of these

	//This is some space to get this to be exactly the size of the disk block.
	//Don't use it for anything.
	char padding[BLOCK_SIZE - MAX_DIRS_IN_ROOT * sizeof(struct cs1550_directory) - sizeof(int)];
} ;

typedef struct cs1550_root_directory cs1550_root_directory;

struct cs1550_disk_block
{
	//The first 4 bytes will be the value 0xF113DA7A
	unsigned long magic_number;
	//And all the rest of the space in the block can be used for actual data
	//storage.
	char data[MAX_DATA_IN_BLOCK];
};

typedef struct cs1550_disk_block cs1550_disk_block;

//How many pointers in an inode?
#define NUM_POINTERS_IN_INODE (BLOCK_SIZE - sizeof(unsigned int) - sizeof(unsigned long))/sizeof(unsigned long)

struct cs1550_inode
{
	//The first 4 bytes will be the value 0xFFFFFFFF
	unsigned long magic_number;
	//The number of children this node has (either other inodes or data blocks)
	unsigned int children;
	//An array of disk pointers to child nodes (either other inodes or data)
	unsigned long pointers[NUM_POINTERS_IN_INODE];
};

typedef struct cs1550_inode cs1550_inode;

//Helper Functions

//Gets and parses path from path string, and loads into 3 distinct variables, used as timesaver.
static void getPath(const char *path, char *dir, char *fileName, char *ext) {
	dir[0] = '\0';
	fileName[0] = '\0';
	ext[0] = '\0';
	sscanf(path, "/%[^/]/%[^.].%s", dir, fileName, ext);
	dir[MAX_FILENAME] = '\0';
	fileName[MAX_FILENAME] = '\0';
	ext[MAX_EXTENSION] = '\0';
}

//Used to determine what type a path the user typed in, 0 for root, 1 for directory, 2&3 for file
static int getPathType(const char *path, char *dir, char *fileName, char *ext) {
	int ret = -1;
	if (strcmp(path, "/") == 0) { ret = 0; }
	if (strcmp(dir, "\0") != 0)      { ret = 1; }
	if (strcmp(fileName, "\0") != 0)       { ret = 2; }
	if (strcmp(ext, "\0") != 0)      { ret = 3; }
	return ret;
}

//Gets the root from .disk and loads into memory
static void getRoot(cs1550_root_directory *root) {
	FILE *f = fopen(".disk", "rb");
	if (f != NULL) {
		fread(root, sizeof(cs1550_root_directory), 1, f);
		fclose(f);
	}
}

//Updates the root on .disk
static void updateRootOnDisk(cs1550_root_directory *root) {
	FILE *f = fopen(".disk", "rb+");
	if (f != NULL) {
		fseek(f, 0, SEEK_END);
		int size = ftell(f);
		rewind(f);
		char *buffer = (char *)malloc(size);
		fread(buffer, size, 1, f);
		rewind(f);
		memmove(buffer, root, BLOCK_SIZE);
		fwrite(buffer, size, 1, f);
		fclose(f);
		free(buffer);
	}
}

//Gets a directory from .disk and loads into memory
static void getDir(cs1550_directory_entry *fill, char *directory) {
	long startBlock = 0;
	cs1550_root_directory r;
	getRoot(&r);
	int i;
	for (i = 0; i < r.nDirectories; i++) {
		if (strcmp(directory, r.directories[i].dname) == 0) {
			startBlock = r.directories[i].nStartBlock;
		}
	}

	FILE *f = fopen(".disk", "rb");
	if (f != NULL) {
		fseek(f, startBlock, SEEK_SET);
		fread(fill, BLOCK_SIZE, 1, f);
		fclose(f);
	}
}

//Updates directory entry on .disk
void updateDir(cs1550_directory_entry *newDir, char *dir) {
	cs1550_root_directory r;
	getRoot(&r);
	int i=0;
	for (i = 0; i < r.nDirectories; i++) {
		if (strcmp(dir, r.directories[i].dname) == 0) {
			long startBlock = r.directories[i].nStartBlock;
			FILE *f = fopen(".disk", "rb+");
			if (f != NULL) {
				fseek(f, 0, SEEK_END);
				int size = ftell(f);
				rewind(f);
				char *buffer = (char *)malloc(size);
				fread(buffer, size, 1, f);
				rewind(f);
				memmove(buffer+(int)startBlock, newDir, BLOCK_SIZE);
				fwrite(buffer, size, 1, f);
				free(buffer);
				fclose(f);
			}
			break;
		}
	}
}

//Checks if directory exists, returns 0 if doesn't, returns 1 if does
static int dirExists(char *dir) {
	cs1550_root_directory r;
	getRoot(&r);
	int i=0;
	for (i = 0; i < r.nDirectories; i++) {
		if (strcmp(dir, r.directories[i].dname) == 0) {
			return 1;
		}
	}
	return 0;
}

//gets the size of the file specificed, if it exists, if not returns -1
static int getFileSize(char *dir, char *fileName, char *ext, int path_type) {
	int ret = -1;

	if (dirExists(dir) == 0) {
		return -1;
	}
	else {
		cs1550_directory_entry parent_dir;
		getDir(&parent_dir, dir);


		int i;
		for (i = 0; i < parent_dir.nFiles; i++) {
			if (path_type == 2 && strcmp(fileName, parent_dir.files[i].fname) == 0) {
				ret = (int)parent_dir.files[i].fsize;
			} else if (path_type == 3 && strcmp(fileName, parent_dir.files[i].fname) == 0 && strcmp(ext, parent_dir.files[i].fext) == 0 ) {
					ret = (int)parent_dir.files[i].fsize;
			}
		}
	}
	return ret;
}
//Returns the next free block from the bitmap
static int getNextBlock() {
		int ret = -1;
		FILE *f = fopen(".disk", "rb");
		int offset = 0 - BITMAPSIZE;
		fseek(f,offset,SEEK_END);
		int i =0;
		for(i =0; i<BITMAPSIZE; i++){
			unsigned char block = fgetc(f);
			int x = 7;
			//Bitshifting Magic
			for(x=7; x>=0; x--){
				if(i!=0 && ((block >> x) & 0x01) == 0){
					ret = i*8 + x;
					break;
				}
			}

			offset++;
			fseek(f,offset,SEEK_END);
		}
		fclose(f);
		return ret;

}
//Updates the bitmap value at position index to value stored in val, works for both allocating and freeing
 static void updateBitmap(int index, int val){
 	FILE *f = fopen(".disk","rb+");
 	fseek(f,0,SEEK_END);
 	int size = ftell(f);
 	int offset = size - BITMAPSIZE;
 	rewind(f);
 	char *buffer = (char *)malloc(size);
 	fread(buffer,size,1,f);
 	rewind(f);
 	//Bit shifting magic here

 	char pos = buffer[offset + (index/8)];

 	pos |= val << (index % 8);
 	buffer[offset+(index/8)] = pos;
 	fwrite(buffer,size,1,f);
 	fclose(f);
 	free(buffer);
 }

//Creates a new empty directory on .disk
static void createDir(char *dir) {
	int block_number = getNextBlock();
	if (block_number != -1) {
			cs1550_root_directory r;
			getRoot(&r);
			strcpy(r.directories[r.nDirectories].dname, dir);
			r.directories[r.nDirectories].nStartBlock = (long)(BLOCK_SIZE * block_number);
			r.nDirectories = r.nDirectories + 1;
			updateRootOnDisk(&r);
			updateBitmap(block_number, 1);
	}
}





//Writes given block to given position
void writeBlock(cs1550_disk_block *fileBlock, long seek) {
	FILE *f = fopen(".disk", "rb+");
	if (f != NULL) {
		fseek(f, 0, SEEK_END);
		int size = ftell(f);
		rewind(f);
		char *buffer = (char *)malloc(size);
		fread(buffer, size, 1, f);
		rewind(f);
		memmove(buffer+seek, fileBlock, BLOCK_SIZE);
		fwrite(buffer, size, 1, f);
		fclose(f);
		free(buffer);
	}
}


/*
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not.
 *
 * man -s 2 stat will show the fields of a stat structure
 */
static int cs1550_getattr(const char *path, struct stat *stbuf)
{

	memset(stbuf, 0, sizeof(struct stat));

	char dir[MAX_FILENAME + 1];
	char fileName[MAX_FILENAME + 1];
	char ext[MAX_EXTENSION + 1];

	getPath(path, dir, fileName, ext);

	int pathType = getPathType(path, dir, fileName, ext);

	if (pathType == 0) {
		//Root
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	}
	else if (pathType == 1) {
		//Directory
		if (dirExists(dir) == 1) {

			 stbuf->st_mode = S_IFDIR | 0755;
			 stbuf->st_nlink = 2;
		}
		else {

			return -ENOENT;
		}

	}
	else if (pathType == 2 || pathType == 3) {
		//File
		int size = getFileSize(dir, fileName, ext, pathType);
		if (size != -1) {
			stbuf->st_mode = S_IFREG | 0666;
			stbuf->st_nlink = 1; //file links
			stbuf->st_size = (size_t)size;
		}
		else {
			return -ENOENT;
		}
	}
	else {
		return -ENOENT;
	}

	return 0;
}

/*
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int cs1550_mkdir(const char *path, mode_t mode)
{
	(void) mode;

	char dir[MAX_FILENAME + 1];
	char fileName[MAX_FILENAME + 1];
	char ext[MAX_EXTENSION + 1];

	getPath(path, dir, fileName, ext);
	int pathType = getPathType(path, dir, fileName, ext);

	if (strlen(dir) >= MAX_FILENAME) {
		return -ENAMETOOLONG;
	}
	else if (dirExists(dir) == 1){
			return -EEXIST;
	}
	else if (pathType != 1){
		return -EPERM;
	}
	else {
				cs1550_root_directory r;
				getRoot(&r);
				if (r.nDirectories < MAX_DIRS_IN_ROOT) {
					createDir(dir);
				}
			}
	return 0;
}

/*
 * Called whenever the contents of a directory are desired. Could be from an 'ls'
 * or could even be when a user hits TAB to do autocompletion
 */
static int cs1550_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	//Since we're building with -Wall (all warnings reported) we need
	//to "use" every parameter, so let's just cast them to void to
	//satisfy the compiler
	(void) offset;
	(void) fi;

	char dir[MAX_FILENAME + 1];
	char fileName[MAX_FILENAME + 1];
	char ext[MAX_EXTENSION + 1];
	getPath(path, dir, fileName, ext);
	int pathType = getPathType(path, dir, fileName, ext);

	if (pathType == 0) {
		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);
		cs1550_root_directory r;
		getRoot(&r);
		int i = 0;
		for(i = 0; i < r.nDirectories; i++) {
			filler(buf, r.directories[i].dname, NULL, 0);
		}
	}
	else if (pathType == 1) {
		int dir_exists = dirExists(dir);
		if ( dir_exists == 1) {
			filler(buf, ".", NULL,0);
			filler(buf, "..", NULL, 0);
			cs1550_directory_entry curDir;
			getDir(&curDir, dir);
			int i =0;
			for (i = 0; i < curDir.nFiles; i++) {
				if ((strcmp(curDir.files[i].fext, "\0") == 0)) {
					filler(buf, curDir.files[i].fname, NULL, 0);
				} else {
					char *fileNameExt = (char *) malloc(2 + MAX_FILENAME + MAX_EXTENSION);
					strcpy(fileNameExt, curDir.files[i].fname);
					strcat(fileNameExt, ".");
					strcat(fileNameExt, curDir.files[i].fext);
					filler(buf, fileNameExt, NULL, 0);
				}
			}
		}
		else {
			return -ENOENT;
		}
	}
	else {
		return -ENOENT;
	}
	return 0;
}

/*
 * Removes a directory.
 */
static int cs1550_rmdir(const char *path)
{
	(void) path;
    return 0;
}

/*
 * Read size bytes from file into buf starting from offset
 *
 */
static int cs1550_read(const char *path, char *buf, size_t size, off_t offset,
			  struct fuse_file_info *fi)
{
	(void) fi;
	int ret =0;
	char dir[MAX_FILENAME + 1];
	char fileName[MAX_FILENAME + 1];
	char ext[MAX_EXTENSION + 1];
	getPath(path, dir, fileName, ext);
	int pathType = getPathType(path, dir, fileName, ext);
	int fileSize = getFileSize(dir, fileName, ext, pathType);

	if (pathType < 2) {
		return -EISDIR;
	}
	else if (dirExists(dir) == 1 && fileSize != -1 && size > 0){
		cs1550_directory_entry parentDir;
		getDir(&parentDir, dir);
		int i =0;
		for (i = 0; i < parentDir.nFiles; i++) {
			if ((pathType==2 && strcmp(parentDir.files[i].fname, fileName)==0) || (pathType==3 && strcmp(parentDir.files[i].fname, fileName) == 0 && strcmp(parentDir.files[i].fext, ext) == 0)) {
				if (offset <= parentDir.files[i].fsize) {
					long startBlock = parentDir.files[i].nStartBlock;
					int blockNum = offset / BLOCK_SIZE;

					long seek = startBlock;
					long bStart = 0;
					FILE *f = fopen(".disk", "rb+");
					int j =0;
					for (j = 0; j <= blockNum; j++) {
						bStart = seek;
						fseek(f, seek, SEEK_SET);
						cs1550_disk_block fileBlock;
						fread(&fileBlock, BLOCK_SIZE, 1, f);
						seek = fileBlock.magic_number;
					}
					rewind(f);
					int off = (int)offset - (blockNum * BLOCK_SIZE);
					int count = off;
					int index = 0;
					seek = bStart;
					while(seek != 0) {
						fseek(f, seek, SEEK_SET);
						cs1550_disk_block curBlock;
						fread(&curBlock, BLOCK_SIZE, 1, f);
						if (count < MAX_DATA_IN_BLOCK) {
							buf[index] = (char)curBlock.data[count];
							count++;
							index++;
						}
						else {
							seek = curBlock.magic_number;
							count = 0;
						}
					}
					fclose(f);
					ret = size;
				}
			}
		}
	}
	return ret;
}

/*
 * Write size bytes from buf into file starting from offset
 *
 */
static int cs1550_write(const char *path, const char *buf, size_t size,
			  off_t offset, struct fuse_file_info *fi)
{
	int ret = 0;
	(void) fi;

	char dir[MAX_FILENAME + 1];
	char fileName[MAX_FILENAME + 1];
	char ext[MAX_EXTENSION + 1];
	getPath(path, dir, fileName, ext);
	int pathType = getPathType(path, dir, fileName, ext);
	int fileSize = getFileSize(dir, fileName, ext, pathType);

	if (dirExists(dir) == 1 && pathType >= 2 && fileSize != -1 && size > 0) {
		cs1550_directory_entry parentDir;
		getDir(&parentDir, dir);
		int i =0;
		for (i = 0; i < parentDir.nFiles; i++) {
			if ((pathType==2 && strcmp(parentDir.files[i].fname, fileName)==0) || (pathType==3 && strcmp(parentDir.files[i].fname, fileName) == 0 && strcmp(parentDir.files[i].fext, ext) == 0)) {
				if (offset > parentDir.files[i].fsize) {
					return -EFBIG;
				}
				else {
					long startBlock = parentDir.files[i].nStartBlock;
					int blockNum = offset / BLOCK_SIZE;

					long seek = startBlock;
					long bStart = 0;
					FILE *f = fopen(".disk", "rb+");
					int j = 0;
					for (j = 0; j <= blockNum; j++) {
						bStart = seek;
						fseek(f, seek, SEEK_SET);
						cs1550_disk_block fileBlock;
						fread(&fileBlock, BLOCK_SIZE, 1, f);
						seek = fileBlock.magic_number;
					}
					rewind(f);
					int off = (int)offset - (blockNum * BLOCK_SIZE);
					int index;
					int count = off;
					seek = bStart;
					fseek(f, seek, SEEK_SET);
					cs1550_disk_block curBlock;
					fread(&curBlock, BLOCK_SIZE, 1, f);
					for (index = 0; index < strlen(buf); index++) {
						if (count < MAX_DATA_IN_BLOCK) {
							curBlock.data[count] = (char)buf[index];
							count++;
						}
						else {
							count = 0;
							if (curBlock.magic_number != 0) {
								writeBlock(&curBlock, seek);
								seek = curBlock.magic_number;
								fseek(f, seek, SEEK_SET);
								fread(&curBlock, BLOCK_SIZE, 1, f);
							}
							else {
								long cSeek = seek;
								int nextBlock = getNextBlock();
								seek = nextBlock * BLOCK_SIZE;
								curBlock.magic_number = seek;
								writeBlock(&curBlock, cSeek);
								fseek(f, seek, SEEK_SET);
								fread(&curBlock, BLOCK_SIZE, 1, f);
								updateBitmap(nextBlock, 1);
							}
						}
						if (index == strlen(buf)-1) {
							writeBlock(&curBlock, seek);
							count = 0;
						}
					}
					fclose(f);
					int old = parentDir.files[i].fsize;
					int newSize =  (size - (old - offset)) + (old - (old - offset));
					parentDir.files[i].fsize = newSize;
					updateDir(&parentDir, dir);
					ret = newSize;
				}
			}
		}
	}
	return ret;
}


/*
 * Does the actual creation of a file. Mode and dev can be ignored.
 *
 */
static int cs1550_mknod(const char *path, mode_t mode, dev_t dev)
{
	(void) mode;
	(void) dev;

	char dir[MAX_FILENAME + 1];
	char fileName[MAX_FILENAME + 1];
	char ext[MAX_EXTENSION + 1];
	getPath(path, dir, fileName, ext);
	int pathType = getPathType(path, dir, fileName, ext);
	int fileSize = getFileSize(dir, fileName, ext, pathType);

	if (pathType < 2) {
		return -EPERM;
	}
	else if (fileSize != -1){
			return -EEXIST;
	}
	else if (strlen(fileName) > MAX_FILENAME || strlen(ext) > MAX_EXTENSION) {
			return -ENAMETOOLONG;
	}
	else {
			cs1550_root_directory r;
			getRoot(&r);
			int i =0;
			for(i = 0; i < r.nDirectories; i++) {
				if (strcmp(r.directories[i].dname, dir) == 0) {
					int blockNum = getNextBlock();
					long startBlock = (long)(BLOCK_SIZE * blockNum);
					updateBitmap(blockNum, 1);
					cs1550_directory_entry parentDir;
					getDir(&parentDir, dir);
					strcpy(parentDir.files[parentDir.nFiles].fname, fileName);
					strcpy(parentDir.files[parentDir.nFiles].fext, ext);
					parentDir.files[parentDir.nFiles].fsize = 0;
					parentDir.files[parentDir.nFiles].nStartBlock = startBlock;
					parentDir.nFiles++;
					int parentDirMagicNum = r.directories[i].nStartBlock;
					FILE *f = fopen(".disk", "rb+");
					if (f != NULL) {
						fseek(f, 0, SEEK_END);
						int diskSize = ftell(f);
						rewind(f);
						char *buffer = (char *)malloc(diskSize);
						fread(buffer, diskSize, 1, f);
						rewind(f);
						memmove(buffer+parentDirMagicNum, &parentDir, BLOCK_SIZE);
						fwrite(buffer, diskSize, 1, f);
						fclose(f);
						free(buffer);
					}
				}
			}
		}
	return 0;
}




/*
 * Deletes a file
 */
static int cs1550_unlink(const char *path)
{

	char dir[MAX_FILENAME + 1];
	char fileName[MAX_FILENAME + 1];
	char ext[MAX_EXTENSION + 1];
	getPath(path, dir, fileName, ext);
	int pathType = getPathType(path, dir, fileName, ext);
	int fileSize = getFileSize(dir, fileName, ext, pathType);

	if (pathType < 2) {
		return -EISDIR;
	}
	else if (fileSize == -1){
		return -ENOENT;
	}
	else {
		cs1550_directory_entry parentDir;
		getDir(&parentDir, dir);
		int i =0;
		for (i = 0; i < parentDir.nFiles; i++) {
			if ((pathType==2 && strcmp(parentDir.files[i].fname, fileName)==0) || (pathType==3 && strcmp(parentDir.files[i].fname, fileName) == 0 && strcmp(parentDir.files[i].fext, ext) == 0)) {
				long startBlock = parentDir.files[i].nStartBlock;
				long seek = startBlock;
				FILE *f = fopen(".disk", "rb");
				while(seek != 0) {
					fseek(f, seek, SEEK_SET);
					cs1550_disk_block curBlock;
					fread(&curBlock, BLOCK_SIZE, 1, f);
					cs1550_disk_block empty;
					memset(&empty, 0, BLOCK_SIZE);
					writeBlock(&empty, seek);
					int index = seek / BLOCK_SIZE;
					updateBitmap(index, 0);
					if (curBlock.magic_number != 0) {
						seek = curBlock.magic_number;
					}
					else {
						seek = 0;
					}
				}
				fclose(f);
				int x =0;
				for (x = 0; x < parentDir.nFiles; x++) {
					if (x >= i && x != parentDir.nFiles-1) {
						strcpy(parentDir.files[x].fname, parentDir.files[x+1].fname);
						strcpy(parentDir.files[x].fext, parentDir.files[x+1].fext);
						parentDir.files[x].fsize = parentDir.files[x+1].fsize;
						parentDir.files[x].nStartBlock = parentDir.files[x+1].nStartBlock;
					}
				}
				parentDir.nFiles--;
				updateDir(&parentDir, dir);
			}
		}
	}
  return 0;
}

/******************************************************************************
 *
 *  DO NOT MODIFY ANYTHING BELOW THIS LINE
 *
 *****************************************************************************/

/*
 * truncate is called when a new file is created (with a 0 size) or when an
 * existing file is made shorter. We're not handling deleting files or
 * truncating existing ones, so all we need to do here is to initialize
 * the appropriate directory entry.
 *
 */
static int cs1550_truncate(const char *path, off_t size)
{
	(void) path;
	(void) size;

    return 0;
}


/*
 * Called when we open a file
 *
 */
static int cs1550_open(const char *path, struct fuse_file_info *fi)
{
	(void) path;
	(void) fi;
    /*
        //if we can't find the desired file, return an error
        return -ENOENT;
    */

    //It's not really necessary for this project to anything in open

    /* We're not going to worry about permissions for this project, but
	   if we were and we don't have them to the file we should return an error

        return -EACCES;
    */

    return 0; //success!
}

/*
 * Called when close is called on a file descriptor, but because it might
 * have been dup'ed, this isn't a guarantee we won't ever need the file
 * again. For us, return success simply to avoid the unimplemented error
 * in the debug log.
 */
static int cs1550_flush (const char *path , struct fuse_file_info *fi)
{
	(void) path;
	(void) fi;

	return 0; //success!
}

/**************************************************************************************************************************************************/
/*************************************************Helper functions: used by the previous functions*************************************************/
/**************************************************************************************************************************************************/
/*
 * Parse a given path name into directory, filename, and extension.
 * From a user interface perspective, our file system will be a two level
 * directory system, with the following restrictions/simplifications:
 * 1. The root directory “/” will only contain other subdirectories, and no regular files
 * 2. The subdirectories will only contain regular files, and no subdirectories of their own
 */


static struct fuse_operations hello_oper = {
    .getattr	= cs1550_getattr,
    .readdir	= cs1550_readdir,
    .mkdir	= cs1550_mkdir,
	.rmdir = cs1550_rmdir,
    .read	= cs1550_read,
    .write	= cs1550_write,
	.mknod	= cs1550_mknod,
	.unlink = cs1550_unlink,
	.truncate = cs1550_truncate,
	.flush = cs1550_flush,
	.open	= cs1550_open,
};

//Don't change this.
int main(int argc, char *argv[])
{
	return fuse_main(argc, argv, &hello_oper, NULL);
}
