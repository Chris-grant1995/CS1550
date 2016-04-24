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

 //Helper Funcs
 int getPathType(const char *path, char *dir, char *fileName, char *ext){
	int ret = -1;
	if (strcmp(path, "/") == 0) 					 { ret = 0; }
 	if (strcmp(dir, "\0") != 0)      { ret = 1; }
 	if (strcmp(fileName, "\0") != 0)       { ret = 2; }
 	if (strcmp(ext, "\0") != 0)      { ret = 3; }
 	return ret;
 }
 static void getRoot(cs1550_root_directory *r) {
 	FILE *f = fopen(".disk", "rb");
 	if (f != NULL) {
 		fread(r, sizeof(cs1550_root_directory), 1, f);
 		fclose(f);
 	}
 }
 static void getDir(cs1550_directory_entry *fill, char *dir) {
 	long startBlock = 0;
 	cs1550_root_directory r;
 	getRoot(&r);
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
 	getRoot(&r);
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
 		getDir(&parentDir, dir);

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
static int getNextBlock(){
	int res = -1;
	FILE *f = fopen(".disk", "rb");
	int offset = 0 - BITMAPSIZE;
	fseek(f,offset,SEEK_END);
	int i =0;
	for(i =0; i<BITMAPSIZE; i++){
		unsigned char block = fgetc(f);
		int x = 7;
		for(x=7; x>=0; x--){
			if(i!=0 && ((block >> x) & 0x01) == 0){
				res = i*8 + x;
				break;
			}
		}

		offset++;
		fseek(f,offset,SEEK_END);
	}
	fclose(f);
	return res;

}
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

static void updateRootOnDisk(cs1550_root_directory *newRoot){
	FILE *f = fopen(".disk", "rb+");
	if(f != NULL){
		fseek(f,0,SEEK_END);
		int size = ftell(f);
		char *buffer = (char *)malloc(size);
		rewind(f);
		fread(buffer,size,1,f);
		rewind(f);
		memmove(buffer,newRoot,BLOCK_SIZE);
		fwrite(buffer,size,1,f);
		fclose(f);
		free(buffer);

	}
}
static void createNewDir(char* dirName){
	int blockNum = getNextBlock();
	if(blockNum!= -1){
		cs1550_root_directory r;
		getRoot(&r);
		strcpy(r.directories[r.nDirectories].dname, dirName);
		r.directories[r.nDirectories].nStartBlock = (long)(BLOCK_SIZE * blockNum);
		r.nDirectories++;
		updateRootOnDisk(&r);
		updateBitmap(blockNum,1);
	}
}
void writeBlockToDisk(cs1550_disk_block *block, long seek){
	FILE *f = fopen(".disk","rb+");
	fseek(f,0,SEEK_END);
	int size = ftell(f);
	char *buffer = (char *)malloc(size);
	rewind(f);
	fread(buffer,size,1,f);
	rewind(f);
	memmove(buffer+seek, block, BLOCK_SIZE);
	fwrite(buffer,size,1,f);
	fclose(f);
	free(buffer);


}
void update_directory_on_disk(cs1550_directory_entry *new_dir, char *directory) {
	cs1550_root_directory root;
	getRoot(&root);
	int i;
	for (i = 0; i < root.nDirectories; i++) {
		if (strcmp(directory, root.directories[i].dname) == 0) {
			// get start block of this directory on .disk
			long dir_nStartBlock = root.directories[i].nStartBlock;
			//replace it on disk wit the new updated directory
			FILE *file_ptr = fopen(".disk", "rb+");
			if (file_ptr != NULL) {
				fseek(file_ptr, 0, SEEK_END);
				int disk_size = ftell(file_ptr);
				rewind(file_ptr);
				char *disk_buffer = (char *)malloc(disk_size);
				fread(disk_buffer, disk_size, 1, file_ptr);
				rewind(file_ptr);
				memmove(disk_buffer+(int)dir_nStartBlock, new_dir, BLOCK_SIZE);
				fwrite(disk_buffer, disk_size, 1, file_ptr);
				free(disk_buffer);
				fclose(file_ptr);
			}
			break;
		}
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
	int res = 0;

	memset(stbuf, 0, sizeof(struct stat));

	 char dir[MAX_FILENAME+1];
	 char fileName[MAX_FILENAME+1];
	 char ext[MAX_EXTENSION+1];

	 dir[0] = '\0';
	 fileName[0] = '\0';
	 ext[0] = '\0';

	 sscanf(path, "/%[^/]/%[^.].%s", dir, fileName, ext);

	 dir[MAX_FILENAME] = '\0';
	 fileName[MAX_FILENAME] = '\0';
	 ext[MAX_EXTENSION] = '\0';



	//is path the root dir?
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else {
		int pathType = getPathType(path,dir,fileName, ext);
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
			stbuf->st_size = (size_t) fSize; //file size - make sure you replace with real size!
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
	char ext [MAX_EXTENSION+1];

	dir[0] = '\0';
	fileName[0] = '\0';
	ext[0] = '\0';

	sscanf(path, "/%[^/]/%[^.].%s", dir, fileName, ext);

	dir[MAX_FILENAME] = '\0';
	fileName[MAX_FILENAME] = '\0';
	ext[MAX_EXTENSION] = '\0';

	int pathType = getPathType(path,dir,fileName,ext);

	if(pathType == 0){
		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);
		cs1550_root_directory r;
		getRoot(&r);
		int i;
		for(i = 0; i < r.nDirectories; i++) {
			filler(buf, r.directories[i].dname, NULL, 0);
		}
	}
	else if(pathType == 1){
		int exists = dirExists(dir);
		if(exists == 1){
			filler(buf, ".", NULL,0);
			filler(buf, "..", NULL, 0);
			cs1550_directory_entry curDir;
			getDir(&curDir, dir);
			int i =0;
			for(i =0; i<curDir.nFiles; i++){
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
	//(void) path;
	(void) mode;

	char dir[MAX_FILENAME+1];
	char fileName[MAX_FILENAME+1];
	char ext[MAX_EXTENSION+1];

	dir[0] = '\0';
	fileName[0] = '\0';
	ext[0] = '\0';

	sscanf(path, "/%[^/]/%[^.].%s", dir, fileName, ext);

	dir[MAX_FILENAME] = '\0';
	fileName[MAX_FILENAME] = '\0';
	ext[MAX_EXTENSION] = '\0';

	int pathType = getPathType(path,dir,fileName,ext);
	cs1550_root_directory r;
	getRoot(&r);

	if(strlen(dir) >= MAX_FILENAME){
		return -ENAMETOOLONG;
	}
	else if(pathType!=1){
		return -EPERM;
	}
	else if(dirExists(dir) == 1){
		return -EEXIST;
	}
	else if(r.nDirectories >= MAX_DIRS_IN_ROOT){
		//Not sure what to return here
		return 0;
	}
	else{
		createNewDir(dir);
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
 * Does the actual creation of a file. Mode and dev can be ignored.
 *
 */

static int cs1550_mknod(const char *path, mode_t mode, dev_t dev)
{
	(void) mode;
	(void) dev;

	int res =0;
	char dir[MAX_FILENAME+1];
	char fileName[MAX_FILENAME+1];
	char ext[MAX_EXTENSION+1];

	dir[0] = '\0';
	fileName[0] = '\0';
	ext[0] = '\0';

	sscanf(path, "/%[^/]/%[^.].%s", dir, fileName, ext);

	dir[MAX_FILENAME] = '\0';
	fileName[MAX_FILENAME] = '\0';
	ext[MAX_EXTENSION] = '\0';

	int pathType = getPathType(path,dir,fileName,ext);
	int fileSize = fileExists(dir,fileName, ext, pathType);

	if(pathType<2){
		res = -EPERM;
	}
	else if(strlen(fileName) > MAX_FILENAME || strlen(ext) > MAX_EXTENSION){
		res = -ENAMETOOLONG;
	}
	else if(fileSize!= -1){
		res = -EEXIST;
	}
	else{
		cs1550_root_directory  r;
		getRoot(&r);
		int i =0;
		for(i =0; i<r.nDirectories; i++){
			if(strcmp(r.directories[i].dname, dir) == 0){
				int blockNum = getNextBlock();
				long fileStart = (long)(blockNum * BLOCK_SIZE);
				updateBitmap(blockNum, 1);

				cs1550_directory_entry parent;
				getDir(&parent, dir);
				strcpy(parent.files[parent.nFiles].fname,fileName);
				strcpy(parent.files[parent.nFiles].fext,ext);

				parent.files[parent.nFiles].fsize =0;
				parent.files[parent.nFiles].nStartBlock = fileStart;
				parent.nFiles++;

				int parentDirBlock = r.directories[i].nStartBlock;
				FILE *f = fopen(".disk", "rb+");
				if(f != NULL){
					fseek(f,0,SEEK_END);
					int size = ftell(f);
					rewind(f);
					char * buffer = (char*) malloc(size);
					fread(buffer,size,1,f);

					rewind(f);
					memmove(buffer + parentDirBlock,&parent,BLOCK_SIZE);
					fwrite(buffer,size,1,f);
					fclose(f);
					free(buffer);
				}

			}
		}
	}


	return res;
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
	(void) fi;

	//check to make sure path exists
	//check that size is > 0
	//check that offset is <= to the file size
	//write data
	//set size (should be same as input) and return, or error

	int res =0;
	char dir[MAX_FILENAME+1];
	char fileName[MAX_FILENAME+1];
	char ext[MAX_EXTENSION+1];

	dir[0] = '\0';
	fileName[0] = '\0';
	ext[0] = '\0';

	sscanf(path, "/%[^/]/%[^.].%s", dir, fileName, ext);

	dir[MAX_FILENAME] = '\0';
	fileName[MAX_FILENAME] = '\0';
	ext[MAX_EXTENSION] = '\0';

	int pathType = getPathType(path,dir,fileName,ext);
	int fileSize = fileExists(dir,fileName, ext, pathType);

	if(dirExists(dir) == 1 && fileSize != -1 && size > 0 && pathType>1){
		cs1550_directory_entry parent;
		getDir(&parent,dir);
		int i =0;
		for(i =0; i<parent.nFiles; i++){
			if((pathType == 2 && strcmp(parent.files[i].fname, fileName) == 0)  || (pathType == 3 && strcmp(parent.files[i].fname,fileName) == 0 && strcmp(parent.files[i].fext,ext) ==0)){
				if(offset > parent.files[i].fsize){
					return -EFBIG;
				}
				else{
					long startBlock = parent.files[i].nStartBlock;
					int offsetBlock = offset/BLOCK_SIZE;

					long s = startBlock;
					long t = 0;
					FILE * f = fopen(".disk", "rb+");
					int x =0;
					for(x =0; x<=offsetBlock;x++ ){
						t = s;
						fseek(f,s,SEEK_SET);
						cs1550_disk_block block;
						fread(&block, BLOCK_SIZE,1,f);
						s = block.magic_number;
					}
					rewind(f);

					int offsetFromBlock = (int) offset - (offsetBlock*BLOCK_SIZE);

					int count = offsetBlock;
					s = startBlock;
					fseek(f,s,SEEK_SET);
					cs1550_disk_block curBlock;
					fread(&curBlock,BLOCK_SIZE,1,f);
					int buffer= 0;
					for(buffer =0; buffer<strlen(buf); buffer++){
						if(count<MAX_DATA_IN_BLOCK){
							curBlock.data[count] = (char)buf[buffer];
							count++;
						}
						else{
							count =0;
							if(curBlock.magic_number != 0){
								writeBlockToDisk(&curBlock,s);
								s = curBlock.magic_number;
								fseek(f,s,SEEK_SET);
								fread(&curBlock,BLOCK_SIZE,1,f);
							}
							else{
								int curSeek = s;
								int nextBlock = getNextBlock();
								s = nextBlock * BLOCK_SIZE;
								curBlock.magic_number = s;
								writeBlockToDisk(&curBlock,curSeek);
								fseek(f,s,SEEK_SET);
								fread(&curBlock,BLOCK_SIZE,1,f);
								updateBitmap(nextBlock,1);
							}
						}
						if(buffer == strlen(buf) -1 ){
							writeBlockToDisk(&curBlock, s);
							count =0;
						}
					}
					fclose(f);
					int oldSize = parent.files[i].fsize;
					int newSize = (oldSize - (oldSize - offset)) + (size - (oldSize - offset));
					parent.files[i].fsize = newSize;
					update_directory_on_disk(&parent,dir);

				}
			}
		}
	}

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

void parse_path_str(const char *path, char *directory, char *filename, char *extension) {
	/*
	 * Initialize strings
	 */
	directory[0] = '\0';
	filename[0] = '\0';
	extension[0] = '\0';
	/*
	 * sscanf or strtok() can both be used to parse the path name
	 * I chose sscanf to do this
	 */
	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	/*
	 * Add a null terminator to the endo of the  parsed strings to determine the end of
	 * the file/directory.
	 */
	directory[MAX_FILENAME] = '\0';
	filename[MAX_FILENAME] = '\0';
	extension[MAX_EXTENSION] = '\0';
}


static int cs1550_mknod2(const char *path, mode_t mode, dev_t dev)
{
	(void) mode; (void) dev;
	int ret = 0;

	char directory[MAX_FILENAME + 1];
	char filename[MAX_FILENAME + 1];
	char extension[MAX_EXTENSION + 1];
	parse_path_str(path, directory, filename, extension);
	int path_type = getPathType(path, directory, filename, extension);
	int file_size = fileExists(directory, filename, extension, path_type);

	if (path_type < 2) {
		// the file is trying to be created in the root dir
		printf("---Wrong directory to write a file to.\n");
		ret = -EPERM;
	} else {
		if (file_size != -1) {
			// file already exists
			printf("---File already exists.\n");
			ret = -EEXIST;
		} else {
			if (strlen(filename) > MAX_FILENAME || strlen(extension) > MAX_EXTENSION) {
				printf("---File name is too long.\n");
				ret = -ENAMETOOLONG;
			} else {
				// file doesn't exist. add it.
				// traverse from the root block, locate the subdirectory in which the new file will reside
				cs1550_root_directory root;
				getRoot(&root);
				int i;
				for(i = 0; i < root.nDirectories; i++) {
					if (strcmp(root.directories[i].dname, directory) == 0) {
						// get a new starting block for this file according to bitmap
						int block_number = getNextBlock();
						long file_nStartBlock = (long)(BLOCK_SIZE * block_number);
						// allocate the file by updating the bitmap
						const char *action = "allocate";
						updateBitmap(block_number, 1);
						// update parent dir
						cs1550_directory_entry parent_dir;
						getDir(&parent_dir, directory);
						strcpy(parent_dir.files[parent_dir.nFiles].fname, filename);
						strcpy(parent_dir.files[parent_dir.nFiles].fext, extension);
						parent_dir.files[parent_dir.nFiles].fsize = 0;
						parent_dir.files[parent_dir.nFiles].nStartBlock = file_nStartBlock;
						parent_dir.nFiles = parent_dir.nFiles + 1;
						// Write the updated subdirectory (with the new file entry) to .disk
						int parent_dir_nStartBlock = root.directories[i].nStartBlock;
						FILE *file_ptr = fopen(".disk", "rb+");
						if (file_ptr != NULL) {
							fseek(file_ptr, 0, SEEK_END);
							int disk_size = ftell(file_ptr);
							rewind(file_ptr);
							char *disk_buffer = (char *)malloc(disk_size);
							fread(disk_buffer, disk_size, 1, file_ptr);
							rewind(file_ptr);
							// write new updated parent directory to buffer
							memmove(disk_buffer+parent_dir_nStartBlock, &parent_dir, BLOCK_SIZE);
							// write updated disk_buffer to .disk
							fwrite(disk_buffer, disk_size, 1, file_ptr);
							fclose(file_ptr);
							free(disk_buffer);
						}
					}
				}
			}
		}
	}
	return ret;
}

/*
 * Write size bytes from buf into file starting from offset
 *
 * Return values:
 * size on success
 * -EFBIG if the offset is beyond the file size (but handle appends)
 *
 * test with the echo command
 */
static int cs1550_write2(const char *path, const char *buf, size_t size,
			  off_t offset, struct fuse_file_info *fi)
{
	int ret = 0;
	(void) fi;

	char directory[MAX_FILENAME + 1];
	char filename[MAX_FILENAME + 1];
	char extension[MAX_EXTENSION + 1];
	parse_path_str(path, directory, filename, extension);
	int path_type = getPathType(path, directory, filename, extension);
	int file_size = fileExists(directory, filename, extension, path_type);

	//check to make sure path exists and check that size is > 0
	if (dirExists(directory) == 1 && path_type >= 2 && file_size != -1 && size > 0) {
		// get the parent directory of the file
		cs1550_directory_entry parent_dir;
		getDir(&parent_dir, directory);
		int i;
		// find the file in the parent directory
		for (i = 0; i < parent_dir.nFiles; i++) {
			if ((path_type==2 && strcmp(parent_dir.files[i].fname, filename)==0) || (path_type==3 && strcmp(parent_dir.files[i].fname, filename) == 0 && strcmp(parent_dir.files[i].fext, extension) == 0)) {
				//check that offset is <= to the file size
				if (offset > parent_dir.files[i].fsize) {
					ret = -EFBIG;
				} else {
					//Locate the starting block of the file
					long file_start_block = parent_dir.files[i].nStartBlock;
					//find the block number the offset is located in
					int block_num_w_offset = offset / BLOCK_SIZE;
					//locate the start of the block that contains the offset (store in block_start)
					int j;
					long seek = file_start_block;
					long block_start = 0;
					FILE *file_ptr = fopen(".disk", "rb+");
					for (j = 0; j <= block_num_w_offset; j++) {
						block_start = seek;
						fseek(file_ptr, seek, SEEK_SET);
						cs1550_disk_block file_block;
						fread(&file_block, BLOCK_SIZE, 1, file_ptr);
						seek = file_block.magic_number;
					}
					rewind(file_ptr);
					//Locate the first byte to be modified relative to the BLOCK (not file) using: the given "global" offset
					//and the number of the block that contains the offset
					int offset_from_file_block = (int)offset - (block_num_w_offset * BLOCK_SIZE);
					//start overwriting from the offset that was just found relative to the file block
					int buf_char;
					int count = offset_from_file_block;
					seek = block_start;
					fseek(file_ptr, seek, SEEK_SET);
					cs1550_disk_block curr_file_block;
					fread(&curr_file_block, BLOCK_SIZE, 1, file_ptr);
					for (buf_char = 0; buf_char < strlen(buf); buf_char++) {
						//keep writing until the end of block is reached
						if (count < MAX_DATA_IN_BLOCK) {
							curr_file_block.data[count] = (char)buf[buf_char];
							count++;
						} else {
							count = 0; //reset the counter
							//move on to the next block
							if (curr_file_block.magic_number != 0) {
								//write the block up to this point in the buffer to disk
								writeBlockToDisk(&curr_file_block, seek);
								//there exists an already allocated block past this current block that we can continue writing to
								seek = curr_file_block.magic_number;
								fseek(file_ptr, seek, SEEK_SET);
								fread(&curr_file_block, BLOCK_SIZE, 1, file_ptr);
							} else {
								//this was the last block in the file
								//update the block that was just written with the new next block pointer
								//allocate a new block and continue
								long curr_seek = seek;
								int next_free_index_from_bitmap = getNextBlock();
								seek = next_free_index_from_bitmap * BLOCK_SIZE;
								//write the block up to this point in the buffer to disk
								curr_file_block.magic_number = seek;
								writeBlockToDisk(&curr_file_block, curr_seek);
								fseek(file_ptr, seek, SEEK_SET);
								fread(&curr_file_block, BLOCK_SIZE, 1, file_ptr);
								//update bit map to indicate new blocks have been allocated
								const char *action = "allocate";
								updateBitmap(next_free_index_from_bitmap, 1);
							}
						}
						// if the end of block is not reached and buffer is done we still want to write this block to disk
						if (buf_char == strlen(buf)-1) {
							writeBlockToDisk(&curr_file_block, seek);
							count = 0;//reset the counter
						}
					}
					fclose(file_ptr);
					//set size (should be same as input) and return, or error
					//in case an offset other than 0 is given calculate the change in size and return an updated size
					//change in size = [old - (old-offset)] + [new - (old-offset)]
					int old = parent_dir.files[i].fsize;
					int new_full_size = (old - (old - offset)) + (size - (old - offset));
					parent_dir.files[i].fsize = new_full_size;
					//update file size in parent directory on disk
					update_directory_on_disk(&parent_dir, directory);
					ret = size;
				}
			}
		}
	}
	return ret;
}



//register our new functions as the implementations of the syscalls
static struct fuse_operations hello_oper = {
    .getattr	= cs1550_getattr,
    .readdir	= cs1550_readdir,
    .mkdir	= cs1550_mkdir,
	.rmdir = cs1550_rmdir,
    .read	= cs1550_read,
    .write	= cs1550_write2,
	.mknod	= cs1550_mknod2,
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
