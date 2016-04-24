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


void parsePath(const char *path, char *dir, char *fileName, char *ext) {
	dir[0] = '\0';
	fileName[0] = '\0';
	ext[0] = '\0';
	sscanf(path, "/%[^/]/%[^.].%s", dir, fileName, ext);
	dir[MAX_FILENAME] = '\0';
	fileName[MAX_FILENAME] = '\0';
	ext[MAX_EXTENSION] = '\0';
}


static int getPathType(const char *path, char *dir, char *fileName, char *ext) {
	int res = -1;
	if (strcmp(path, "/") == 0) { res = 0; }
	if (strcmp(dir, "\0") != 0)      { res = 1; }
	if (strcmp(fileName, "\0") != 0)       { res = 2; }
	if (strcmp(ext, "\0") != 0)      { res = 3; }
	return res;
}


static void getRoot(cs1550_root_directory *root) {
	FILE *f = fopen(".disk", "rb");
	if (f != NULL) {
		fread(root, sizeof(cs1550_root_directory), 1, f);
		fclose(f);
	}
}


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

/*
 * Get directory struct from .disk via the root
 */
static void getDir(cs1550_directory_entry *fill, char *directory) {
	// get the start block number of the directory we're interested in
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
		// set the fill directory entry passed in to the directory we found
		fseek(f, startBlock, SEEK_SET);
		fread(fill, BLOCK_SIZE, 1, f);
		fclose(f);
	}
}

/*
 * update a directory's entry in root
 */
void updateDir(cs1550_directory_entry *new_dir, char *dir) {
	cs1550_root_directory r;
	getRoot(&r);
	int i=0;
	for (i = 0; i < r.nDirectories; i++) {
		if (strcmp(dir, r.directories[i].dname) == 0) {
			// get start block of this directory on .disk
			long startBlock = r.directories[i].nStartBlock;
			//replace it on disk wit the new updated directory
			FILE *f = fopen(".disk", "rb+");
			if (f != NULL) {
				fseek(f, 0, SEEK_END);
				int size = ftell(f);
				rewind(f);
				char *buffer = (char *)malloc(size);
				fread(buffer, size, 1, f);
				rewind(f);
				memmove(buffer+(int)startBlock, new_dir, BLOCK_SIZE);
				fwrite(buffer, size, 1, f);
				free(buffer);
				fclose(f);
			}
			break;
		}
	}
}

/*
 * Check if directory exists.
 * If directory exists, returns 1.
 * If directory doesn't exist, returns 0.
 */
static int dirExists(char *dir) {
	int res = 0;
	cs1550_root_directory r;
	getRoot(&r);
	int i=0;
	for (i = 0; i < r.nDirectories; i++) {
		if (strcmp(dir, r.directories[i].dname) == 0) {
			res = 1;
		}
	}
	return res;
}

/*
 * Check if file exists
 * If file exists, returns fize size (integer).
 * If file doesn't exist returns -1.
 */
static int fileExists(char *dir, char *fileName, char *ext, int path_type) {
	int res = -1;

	if (dirExists(dir) == 0) {
		res = -1;
	} else {
		// parent dir exists. now get the struct for the parent dir
		cs1550_directory_entry parent_dir;
		getDir(&parent_dir, dir);

		// search files in the parent dir to see if the requested dir exists
		int i;
		for (i = 0; i < parent_dir.nFiles; i++) {
			if (path_type == 2 && strcmp(fileName, parent_dir.files[i].fname) == 0) {
				res = (int)parent_dir.files[i].fsize;
			} else if (path_type == 3 && strcmp(fileName, parent_dir.files[i].fname) == 0 && strcmp(ext, parent_dir.files[i].fext) == 0 ) {
					res = (int)parent_dir.files[i].fsize;
			}
		}
	}
	return res;
}

/*
 * Search the bitmap to get the block number of the next free block
 *
 * Returns the block number of free block on success.
 * Returns -1 on failure.
 */
static int getNextBlock() {
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

/*
 * Write the given directory to the .disk file and update the root entry on disk.
 *
 * Returns the start block number on .disk of that newly created directory on success.
 * Returns -1 on failure.
 */
static void createDir(char *dir) {
	// first get the block number of the next free block (from the bitmap).
	int block_number = getNextBlock();
	if (block_number != -1) {
			// Update root entry, update free block tracking bitmap
			// update root directory information on .disk
			cs1550_root_directory r;
			getRoot(&r);
			strcpy(r.directories[r.nDirectories].dname, dir);
			r.directories[r.nDirectories].nStartBlock = (long)(BLOCK_SIZE * block_number);
			r.nDirectories = r.nDirectories + 1;
			updateRootOnDisk(&r);
			updateBitmap(block_number, 1);
	} else {
		printf("No free blocks available.\n");
	}
}





/*
 * Write disk block to disk given a bloc and a seek position
 */
void writeBlock(cs1550_disk_block *file_block, long seek) {
	FILE *f = fopen(".disk", "rb+");
	if (f != NULL) {
		fseek(f, 0, SEEK_END);
		int size = ftell(f);
		rewind(f);
		char *buffer = (char *)malloc(size);
		fread(buffer, size, 1, f);
		rewind(f);
		memmove(buffer+seek, file_block, BLOCK_SIZE);
		fwrite(buffer, size, 1, f);
		fclose(f);
		free(buffer);
	}
}


/*
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not.
 *
 * UNIX Equivalent: man -s 2 stat will show the fields of a stat structure
 *
 * Return values:
 * 0 on success, with a correctly set structure
 * -ENOENT if the file is not found
 *
 * use the stat command to test
 */
static int cs1550_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;

	memset(stbuf, 0, sizeof(struct stat));

	char directory[MAX_FILENAME + 1];
	char filename[MAX_FILENAME + 1];
	char extension[MAX_EXTENSION + 1];

	parsePath(path, directory, filename, extension);

	int path_type = getPathType(path, directory, filename, extension);

	if (path_type == 0) {
		/*
		 * Path is the root dir
		 */
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else if (path_type == 1) {
		/*
		 * Path is subdirectory
		 */
		if (dirExists(directory) == 1) {
			/*
			 * Subdirectory exists
			 */
			 stbuf->st_mode = S_IFDIR | 0755;
			 stbuf->st_nlink = 2;
		} else {
			/*
			 * else return that directory doesn't exist; thus, path doesn't exist
			 */
			printf("Directory doesn't exist\n");
			res = -ENOENT;
		}

	} else if (path_type == 2 || path_type == 3) {
		/*
		 * Path is a file
		 * Check if file exists
		 */
		int file_size = fileExists(directory, filename, extension, path_type);
		if (file_size != -1) {
			stbuf->st_mode = S_IFREG | 0666;
			stbuf->st_nlink = 1; //file links
			stbuf->st_size = (size_t)file_size;
		} else {
			printf("File doesn't exist\n");
			res = -ENOENT;
		}
	} else {
		printf("Invalid path\n");
		res = -ENOENT;
	}

	return res;
}

/*
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 *
 * This function adds the new directory to the root level, and  updates
 * the .disk file appropriately.
 *
 * UNIX Equivalent: man -s 2 mkdir
 *
 * Return values:
 * 0 on success
 * -ENAMETOOLONG if the name is beyond 8 chars
 * -EPERM if the directory is not under the root dir only
 * -EEXIST if the directory already exists
 *
 * test with mkdir command
 */
static int cs1550_mkdir(const char *path, mode_t mode)
{
	(void) mode;
	int res = 0;

	/*
	 * Start by parsing the path name.
	 */
	char directory[MAX_FILENAME + 1];
	char filename[MAX_FILENAME + 1];
	char extension[MAX_EXTENSION + 1];

	parsePath(path, directory, filename, extension);
	int path_type = getPathType(path, directory, filename, extension);

	if (strlen(directory) >= MAX_FILENAME) {
		/*
		 * Directory name is too long (beyond 8 characters)
		 */
		printf("Directory name is too long (beyond 8 characters)\n");
		res = -ENAMETOOLONG;
	}
	else if (path_type != 1){

			/*
			 * Directory is not under the root dir only
			 */
			printf("Directory is not under the root dir only\n");
			res = -EPERM;
		}
	 else if (dirExists(directory) == 1){
				/*
				 * Directory already exists
				 */
				printf("Directory already exists\n");
				res = -EEXIST;
			}
	else {
				/*
				 * Add the new directory to the root level, and update the .disk file appropriately
				 * with a new updated root entry and a new empty directory
				 */
				cs1550_root_directory r;
				getRoot(&r);
				if (r.nDirectories >= MAX_DIRS_IN_ROOT) {
					printf("Maximum directories in root. Directory not created.\n");
				} else {
					// write a new empty directory to disk.
					createDir(directory);
				}
			}
	return res;
}

/*
 * Called whenever the contents of a directory are desired. Could be from an 'ls'
 * or could even be when a user hits TAB to do autocompletion
 *
 * UNIX Equivalent: man -s 2 readdir (However it’s not exactly equivalent)
 *
 * Return values:
 * 0 on success
 * -ENOENT if the directory is not valid or found
 *
 * test with ls -al command
 */
static int cs1550_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	//Since we're building with -Wall (all warnings reported) we need
	//to "use" every parameter, so let's just cast them to void to
	//satisfy the compiler
	(void) offset;
	(void) fi;
	int res = 0;

	char dir[MAX_FILENAME + 1];
	char filename[MAX_FILENAME + 1];
	char extension[MAX_EXTENSION + 1];
	parsePath(path, dir, filename, extension);
	int path_type = getPathType(path, dir, filename, extension);

	if (path_type == 0) {
		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);
		cs1550_root_directory root;
		getRoot(&root);
		int i =0;
		for(i = 0; i < root.nDirectories; i++) {
			filler(buf, root.directories[i].dname, NULL, 0);
		}
	} else if (path_type == 1) {
		int dir_exists = dirExists(dir);
		if ( dir_exists == 1) {
			filler(buf, ".", NULL,0);
			filler(buf, "..", NULL, 0);
			cs1550_directory_entry curDir;
			getDir(&curDir, dir);
			int i =0;
			for (i = 0; i < curDir.nFiles; i++) {
				if ((strcmp(curDir.files[i].fext, "\0") == 0)) {
					//print regular files
					filler(buf, curDir.files[i].fname, NULL, 0);
				} else {
					//print files with extensions. malloc extra space for '\0' and '.'
					char *filename_with_ext = (char *) malloc(2 + MAX_FILENAME + MAX_EXTENSION);
					strcpy(filename_with_ext, curDir.files[i].fname);
					strcat(filename_with_ext, ".");
					strcat(filename_with_ext, curDir.files[i].fext);
					filler(buf, filename_with_ext, NULL, 0);
				}
			}
		} else {
			res = -ENOENT;
		}
	} else {
		res = -ENOENT;
	}
	return res;
}

/*
 * Removes a directory. This function should not be modified.
 */
static int cs1550_rmdir(const char *path)
{
	(void) path;
    return 0;
}

/*
 * Does the actual creation of a file. Mode and dev can be ignored.
 *
 * 0 on success
 * -ENAMETOOLONG if the name is beyond 8.3 chars
 * -EPERM if the file is trying to be created in the root dir
 * -EEXIST if the file already exists
 *
 * test with touch command
 */
static int cs1550_mknod(const char *path, mode_t mode, dev_t dev)
{
	(void) mode; (void) dev;
	int res = 0;

	char directory[MAX_FILENAME + 1];
	char filename[MAX_FILENAME + 1];
	char extension[MAX_EXTENSION + 1];
	parsePath(path, directory, filename, extension);
	int path_type = getPathType(path, directory, filename, extension);
	int file_size = fileExists(directory, filename, extension, path_type);

	if (path_type < 2) {
		// the file is trying to be created in the root dir
		printf("---Wrong directory to write a file to.\n");
		res = -EPERM;
	} else {
		if (file_size != -1) {
			// file already exists
			printf("---File already exists.\n");
			res = -EEXIST;
		} else {
			if (strlen(filename) > MAX_FILENAME || strlen(extension) > MAX_EXTENSION) {
				printf("---File name is too long.\n");
				res = -ENAMETOOLONG;
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
						FILE *f = fopen(".disk", "rb+");
						if (f != NULL) {
							fseek(f, 0, SEEK_END);
							int disk_size = ftell(f);
							rewind(f);
							char *disk_buffer = (char *)malloc(disk_size);
							fread(disk_buffer, disk_size, 1, f);
							rewind(f);
							// write new updated parent directory to buffer
							memmove(disk_buffer+parent_dir_nStartBlock, &parent_dir, BLOCK_SIZE);
							// write updated disk_buffer to .disk
							fwrite(disk_buffer, disk_size, 1, f);
							fclose(f);
							free(disk_buffer);
						}
					}
				}
			}
		}
	}
	return res;
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
static int cs1550_write(const char *path, const char *buf, size_t size,
			  off_t offset, struct fuse_file_info *fi)
{
	int res = 0;
	(void) fi;

	char directory[MAX_FILENAME + 1];
	char filename[MAX_FILENAME + 1];
	char extension[MAX_EXTENSION + 1];
	parsePath(path, directory, filename, extension);
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
					res = -EFBIG;
				} else {
					//Locate the starting block of the file
					long file_start_block = parent_dir.files[i].nStartBlock;
					//find the block number the offset is located in
					int block_num_w_offset = offset / BLOCK_SIZE;
					//locate the start of the block that contains the offset (store in block_start)
					int j;
					long seek = file_start_block;
					long block_start = 0;
					FILE *f = fopen(".disk", "rb+");
					for (j = 0; j <= block_num_w_offset; j++) {
						block_start = seek;
						fseek(f, seek, SEEK_SET);
						cs1550_disk_block file_block;
						fread(&file_block, BLOCK_SIZE, 1, f);
						seek = file_block.magic_number;
					}
					rewind(f);
					//Locate the first byte to be modified relative to the BLOCK (not file) using: the given "global" offset
					//and the number of the block that contains the offset
					int offset_from_file_block = (int)offset - (block_num_w_offset * BLOCK_SIZE);
					//start overwriting from the offset that was just found relative to the file block
					int buf_char;
					int count = offset_from_file_block;
					seek = block_start;
					fseek(f, seek, SEEK_SET);
					cs1550_disk_block curr_file_block;
					fread(&curr_file_block, BLOCK_SIZE, 1, f);
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
								writeBlock(&curr_file_block, seek);
								//there exists an already allocated block past this current block that we can continue writing to
								seek = curr_file_block.magic_number;
								fseek(f, seek, SEEK_SET);
								fread(&curr_file_block, BLOCK_SIZE, 1, f);
							} else {
								//this was the last block in the file
								//update the block that was just written with the new next block pointer
								//allocate a new block and continue
								long curr_seek = seek;
								int next_free_index_from_bitmap = getNextBlock();
								seek = next_free_index_from_bitmap * BLOCK_SIZE;
								//write the block up to this point in the buffer to disk
								curr_file_block.magic_number = seek;
								writeBlock(&curr_file_block, curr_seek);
								fseek(f, seek, SEEK_SET);
								fread(&curr_file_block, BLOCK_SIZE, 1, f);
								//update bit map to indicate new blocks have been allocated
								updateBitmap(next_free_index_from_bitmap, 1);
							}
						}
						// if the end of block is not reached and buffer is done we still want to write this block to disk
						if (buf_char == strlen(buf)-1) {
							writeBlock(&curr_file_block, seek);
							count = 0;//reset the counter
						}
					}
					fclose(f);
					//set size (should be same as input) and return, or error
					//in case an offset other than 0 is given calculate the change in size and return an updated size
					//change in size = [old - (old-offset)] + [new - (old-offset)]
					int old = parent_dir.files[i].fsize;
					int new_full_size = (old - (old - offset)) + (size - (old - offset));
					parent_dir.files[i].fsize = new_full_size;
					//update file size in parent directory on disk
					updateDir(&parent_dir, directory);
					res = size;
				}
			}
		}
	}
	return res;
}

/*
 * Read size bytes from file into buf starting from offset
 *
 * test with the cat command
 */
static int cs1550_read(const char *path, char *buf, size_t size, off_t offset,
			  struct fuse_file_info *fi)
{
	int res = 0;
	(void) fi;

	char directory[MAX_FILENAME + 1];
	char filename[MAX_FILENAME + 1];
	char extension[MAX_EXTENSION + 1];
	parsePath(path, directory, filename, extension);
	int path_type = getPathType(path, directory, filename, extension);
	int file_size = fileExists(directory, filename, extension, path_type);

	if (path_type < 2) {
		res = -EISDIR; //path is a directory
	} else {
		if (dirExists(directory) == 1 && file_size != -1 && size > 0) {
			// get the parent directory of the file
			cs1550_directory_entry parent_dir;
			getDir(&parent_dir, directory);
			int i;
			// find the file in the parent directory
			for (i = 0; i < parent_dir.nFiles; i++) {
				if ((path_type==2 && strcmp(parent_dir.files[i].fname, filename)==0) || (path_type==3 && strcmp(parent_dir.files[i].fname, filename) == 0 && strcmp(parent_dir.files[i].fext, extension) == 0)) {
					//check that offset is <= to the file size
					if (offset <= parent_dir.files[i].fsize) {
						//Locate the starting block of the file
						long file_start_block = parent_dir.files[i].nStartBlock;
						//find the block number the offset is located in
						int block_num_w_offset = offset / BLOCK_SIZE;
						//locate the start of the block that contains the offset (store in block_start)
						int j;
						long seek = file_start_block;
						long block_start = 0;
						FILE *f = fopen(".disk", "rb+");
						for (j = 0; j <= block_num_w_offset; j++) {
							block_start = seek;
							fseek(f, seek, SEEK_SET);
							cs1550_disk_block file_block;
							fread(&file_block, BLOCK_SIZE, 1, f);
							seek = file_block.magic_number;
						}
						rewind(f);
						//Locate the first byte to be modified relative to the BLOCK (not file) using: the given "global" offset
						//and the number of the block that contains the offset
						int offset_from_file_block = (int)offset - (block_num_w_offset * BLOCK_SIZE);
						//start reading from the offset that was just found relative to the file block
						int count = offset_from_file_block;
						int buf_char = 0;
						seek = block_start;
						while(seek != 0) {
							fseek(f, seek, SEEK_SET);
							cs1550_disk_block curr_file_block;
							fread(&curr_file_block, BLOCK_SIZE, 1, f);
							//keep reading  until the end of block is reached
							if (count < MAX_DATA_IN_BLOCK) {
								buf[buf_char] = (char)curr_file_block.data[count];
								count++;
								buf_char++;
							} else {
								seek = curr_file_block.magic_number;
								count = 0;
							}
						}
						fclose(f);
						res = size;
					}
				}
			}
		}
	}
	return res;
}

/*
 * Deletes a file
 *
 * test with the rm command
 */
static int cs1550_unlink(const char *path)
{
	int res = 0;

	char directory[MAX_FILENAME + 1];
	char filename[MAX_FILENAME + 1];
	char extension[MAX_EXTENSION + 1];
	parsePath(path, directory, filename, extension);
	int path_type = getPathType(path, directory, filename, extension);
	int file_size = fileExists(directory, filename, extension, path_type);

	if (path_type < 2) {
		res = -EISDIR; //path is a directory
	} else {
		if (file_size == -1) {
			res = -ENOENT; //file is not found
		} else {
			//file exists
			//get the parent directory of the file
			cs1550_directory_entry parent_dir;
			getDir(&parent_dir, directory);
			int i;
			// find the file in the parent directory
			for (i = 0; i < parent_dir.nFiles; i++) {
				if ((path_type==2 && strcmp(parent_dir.files[i].fname, filename)==0) || (path_type==3 && strcmp(parent_dir.files[i].fname, filename) == 0 && strcmp(parent_dir.files[i].fext, extension) == 0)) {
					//Locate the starting block of the file
					long file_start_block = parent_dir.files[i].nStartBlock;
					long seek = file_start_block;
					FILE *f = fopen(".disk", "rb");
					while(seek != 0) {
						fseek(f, seek, SEEK_SET);
						cs1550_disk_block curr_file_block;
						fread(&curr_file_block, BLOCK_SIZE, 1, f);
						//remove current block from disk by replacing it with an empty one
						cs1550_disk_block empty_block;
						memset(&empty_block, 0, BLOCK_SIZE);
						writeBlock(&empty_block, seek);
						//update bitmap
						int bitmap_index = seek / BLOCK_SIZE;
						updateBitmap(bitmap_index, 0);
						//if there's a next block set seek to it
						if (curr_file_block.magic_number != 0) {
							seek = curr_file_block.magic_number;
						} else {
							seek = 0;
						}
					}
					fclose(f);
					//update parent directory entry
					int j;
					for (j = 0; j < parent_dir.nFiles; j++) {
						if (j >= i && j != parent_dir.nFiles-1) {
							//shift cells back by one at index we want to remove
							strcpy(parent_dir.files[j].fname, parent_dir.files[j+1].fname);
							strcpy(parent_dir.files[j].fext, parent_dir.files[j+1].fext);
							parent_dir.files[j].fsize = parent_dir.files[j+1].fsize;
							parent_dir.files[j].nStartBlock = parent_dir.files[j+1].nStartBlock;
						}
					}
//destory parent_dir.nFiles file reference in root ?
					// decrement total number of files
					parent_dir.nFiles = parent_dir.nFiles - 1;
					updateDir(&parent_dir, directory);
				}
			}
		}
	}
    return res;
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
