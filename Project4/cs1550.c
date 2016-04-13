/*
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
#define NUM_POINTERS_IN_INODE (BLOCK_SIZE - sizeof(unsigned int) - sizeof(unsigned long))

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

/*
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not.
 *
 * man -s 2 stat will show the fields of a stat structure
 */
 //Helper Funcs
 int get_path_type(const char *path, char *dir, char *fileName, char *ext){
	int ret = -1;
	if (strcmp(path, "/") == 0) 					 { ret = 0; }
 	if (strcmp(directory, "\0") != 0)      { ret = 1; }
 	if (strcmp(filename, "\0") != 0)       { ret = 2; }
 	if (strcmp(extension, "\0") != 0)      { ret = 3; }
 	return ret;
 }
 static void get_root(cs1550_root_directory *r) {
 	FILE *f = fopen(".disk", "rb");
 	if (f != NULL) {
 		fread(r, sizeof(cs1550_root_directory), 1, f);
 		fclose(f);
 	}
 }
 static void getDir(cs1550_directory_entry *fill, char *dir) {
 	long startBlock = 0;
 	cs1550_root_directory r;
 	get_root(&r);
 	int i;
 	for (i = 0; i < r.nDirectories; i++) {
 		if (strcmp(dir, r.directories[i].dname) == 0) {
 			startBlock = r.directories[i].nStartBlock;
 		}
 	}

 	FILE *f = fopen(".disk", "rb");
 	if (f != NULL) {
 		// set the fill directory entry passed in to the directory we found
 		fseek(f, startBlock, SEEK_SET);
 		fread(fill, BLOCK_SIZE, 1, f);
 		fclose(f);
 	}
 }
 static int dirExists(char *dir) {
 	int ret = 0;
 	cs1550_root_directory r;
 	get_root(&r);
 	int i;
 	for (i = 0; i < r.nDirectories; i++) {
 		if (strcmp(dir, r.directories[i].dname) == 0) {
 			ret = 1;
 		}
 	}
 	return ret;
 }
 static int fileExists(char *dir, char *fileName, char *ext, int pathType) {
 	int ret = -1;

 	if (dirExists(dir) == 0) {
 		ret = -1;
 	} else {
 		cs1550_directory_entry parentDir;
 		getDir(&parentDir, directory);

 		int i;
 		for (i = 0; i < parentDir.nFiles; i++) {
 			if (pathType == 2 && strcmp(fileName, parentDir.files[i].fname) == 0) {
 				ret = (int)parentDir.files[i].fsize;
 			}
			else if (pathType == 3 && strcmp(fileName, parentDir.files[i].fname) == 0 && strcmp(ext, parentDir.files[i].fext) == 0 ) {
 					ret = (int)parentDir.files[i].fsize;
 			}
 		}
 	}
 	return ret;
 }
static int cs1550_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;

	memset(stbuf, 0, sizeof(struct stat));

	 char dir[MAX_FILENAME+1];
	 char fileName[MAX_FILENAME+1];
	 char ext = [MAX_EXTENSION+1];

	 dir[0] = '\0';
	 fileName[0] = '\0';
	 ext[0] = '\0';

	 sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

	 dir[MAX_FILENAME] = '\0';
	 fileName[MAX_FILENAME] = '\0';
	 ext[MAX_EXTENSION] = '\0';



	//is path the root dir?
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else {
		int pathType = get_path_type(path,dir,fileName, ext);
		if(pathType == 1){
			//subdirectory
			if(dirExists(dir) == 1){
				//subdirectory exists
				stbuf->st_mode = S_IFDIR | 0755;
				stbuf->st_nlink = 2;
			}
			else{
				res = -ENOENT;
			}
		}
	//Check if name is subdirectory
	/*
		//Might want to return a structure with these fields
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		res = 0; //no error
	*/
	else if(pathType == 2 || pathType == 3){
		//File
		int fSize = fileExists(dir,fileName,ext,pathType);
		if(fSize != -1){
			stbuf->st_mode = S_IFREG | 0666;
			stbuf->st_nlink = 1; //file links
			stbuf->st_size = (size_t) fSize); //file size - make sure you replace with real size!
		}
		else{
			res = -ENOENT;
		}
	}
	//Check if name is a regular file
	/*
		//regular file, probably want to be read and write
		stbuf->st_mode = S_IFREG | 0666;
		stbuf->st_nlink = 1; //file links
		stbuf->st_size = 0; //file size - make sure you replace with real size!
		res = 0; // no error
	*/

	else{
		res = -ENOENT;
	}
		//Else return that path doesn't exist
	}
	return res;
}

/*
 * Called whenever the contents of a directory are desired. Could be from an 'ls'
 * or could even be when a user hits TAB to do autocompletion
 */
static int cs1550_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	int res =0;
	//Since we're building with -Wall (all warnings reported) we need
	//to "use" every parameter, so let's just cast them to void to
	//satisfy the compiler
	(void) offset;
	(void) fi;
	char dir[MAX_FILENAME+1];
	char fileName[MAX_FILENAME+1];
	char ext = [MAX_EXTENSION+1];

	dir[0] = '\0';
	fileName[0] = '\0';
	ext[0] = '\0';

	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

	dir[MAX_FILENAME] = '\0';
	fileName[MAX_FILENAME] = '\0';
	ext[MAX_EXTENSION] = '\0';

	int pathType = get_path_type(path,dir,fileName,ext);

	if(pathType == 0){
		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);
		cs1550_root_directory r;
		get_root(&r);
		int i;
		for(i = 0; i < r.nDirectories; i++) {
			filler(buf, r.directories[i].dname, NULL, 0);
		}
	}
	else if(pathType == 1){
		int dirExists = dirExists(dir);
		if(dirExists == 1){
			filler(buf, ".", NULL,0);
			filler(buf, "..", NULL, 0);
			cs1550_directory_entry curDir;
			getDir(&curDir, dir);
			for(int i =0; i<curDir.nFiles; i++){
				if(strcmp(curDir.files[i].fext, "\0") == 0){
					filler(buf,curDir.files[i].fname, NULL, 0);
				}
				else{
					char * filenameExt = (char*) malloc(2+MAX_FILENAME + MAX_EXTENSION);// 2 for . and \0
					strcpy(filenameExt, curDir.files[i].fname);
					strcpy(filenameExt, ".");
					strcpy(filenameExt, curDir.files[i].fext);
					filler(buf,filenameExt, NULL, 0);
				}
			}
		}
		else{
			res = -ENOENT;
		}
	}
	else{
		res = -ENOENT;
	}
	return res;
	//This line assumes we have no subdirectories, need to change
	//if (strcmp(path, "/") != 0)
	//return -ENOENT;

	//the filler function allows us to add entries to the listing
	//read the fuse.h file for a description (in the ../include dir)
	//filler(buf, ".", NULL, 0);
	//filler(buf, "..", NULL, 0);

	/*
	//add the user stuff (subdirs or files)
	//the +1 skips the leading '/' on the filenames
	filler(buf, newpath + 1, NULL, 0);
	*/
}

/*
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int cs1550_mkdir(const char *path, mode_t mode)
{
	(void) path;
	(void) mode;

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
 * Does the actual creation of a file. Mode and dev can be ignored.
 *
 */
static int cs1550_mknod(const char *path, mode_t mode, dev_t dev)
{
	(void) mode;
	(void) dev;
	(void) path;
	return 0;
}

/*
 * Deletes a file
 */
static int cs1550_unlink(const char *path)
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
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

	//check to make sure path exists
	//check that size is > 0
	//check that offset is <= to the file size
	//read in data
	//set size and return, or error

	size = 0;

	return size;
}

/*
 * Write size bytes from buf into file starting from offset
 *
 */
static int cs1550_write(const char *path, const char *buf, size_t size,
			  off_t offset, struct fuse_file_info *fi)
{
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

	//check to make sure path exists
	//check that size is > 0
	//check that offset is <= to the file size
	//write data
	//set size (should be same as input) and return, or error

	return size;
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


//register our new functions as the implementations of the syscalls
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
