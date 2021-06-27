/**
Aleksas Murauskas
260718389
ECSE 427: Operating Systems
Assignemnt 3: File Systems
**/
//DEFINITIONS and LIBRARIES

//Libraries 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fuse.h>
#include <strings.h>
#include <stdint.h>
#include <inttypes.h>

//Header Files 
#include "disk_emu.h"
#include "sfs_api.h"

//Defining Constants
#define DISK_NAME "created.sfs" //Name of disk
#define BYTE_SIZE 8 //Size of Byte
#define MAX_FILE_NAME 20 //maximum size of file name 
#define TOTAL_BLOCKS 1024  //maximum number of data blocks on the disk.
#define BLOCK_SIZE 1024 //Size of a block of memory
#define FL_ROW_SIZE (TOTAL_BLOCKS/BYTE_SIZE) // this essentially mimcs the number of rows we have in the Free list. we will have 128 rows.
#define FL_SIZE (TOTAL_BLOCKS/BYTE_SIZE) //1024 Divided by 8 = 128 
#define FL_LOC 1023 //Free List's location in memory
#define TOTAL_FILES 100 //Total number of Inodes in memory
#define RD_INODE 0 //Inode Number of the Root Directory
#define INODE_LIST_LOC 1022
#define MAGIC 0xABCD0005 //Holds a location in memory, value taken from the handout
#define MAX_FILE_SIZE BLOCK_SIZE * ((BLOCK_SIZE / sizeof(unsigned int)) + 12)   //maximum size of a file 

//Important Data Structures 
typedef struct superblock{ //Represents the single superblock at the begginning of the disk that holds key information
    unsigned int magic; //Holds a given magic number 
    unsigned int block_size; //holds the size of a block
    unsigned int system_size; //holds the size of the disk
    unsigned int IN_length; //Length of Inode
    unsigned int RD_Inode; //Length of Root Directory Inoe 
} superblock;

typedef struct inode_member{ //Struct for an Inode
    unsigned int mode; //I node's mode
    unsigned int linkCount; //Number of links the Inode has 
    unsigned int size; //Size of file
    unsigned int data_ptrs[12]; //Pointers to the data blocks making up the file
    unsigned int indPointer; // points to a data block that points to other data blocks (Single indirect)
} inode_member;

typedef struct file_descriptor_member {
    unsigned int IN_num; //holds the Inodes number in the table 
    inode_member* inode; //points to the inode
    unsigned int readPointer; //Holds the pointer for the read pointer 
    unsigned int writePointer;//Holds the pointer for write
} file_descriptor_member;

typedef struct directory_member{ //member of the Root Directory
    char name[MAX_FILE_NAME+1]; //Holds the filename
    int num; // Inode number 
}directory_member;

//**Initialiaze structs **/

superblock SB; //Declared to start since there is only ever one superblock
int inodes_active[TOTAL_FILES]; //Holds a boolean whether a inode is active or not
inode_member inodeTable[TOTAL_FILES]; //holds all the inodes 
file_descriptor_member open_Files[TOTAL_FILES];//holds a descriptor of all open files 
directory_member rootDirectory[TOTAL_FILES];//Holds all files 
// Allows us to keep track of occupied inodes. 1 = occupied, 0 = not
int current_loc=0; // Keeps track of our location in the directory, needed for get next file name 
int indirect_mem[BLOCK_SIZE/sizeof(int)]; //used as a 
unsigned int Free_List[FL_SIZE];

// Initializes the inode table, by setting all inodes as unused

void clear_inode(int x){ //Set an I node to inactive and clear all values 
	unsigned int empty_point[12];
	for (int j = 0; j < 12; j++){
		empty_point[j]= -1;
	}
	update_Inode(x,-1,-1,-1,empty_point,-1);//Use update Inode to empty the Inode
	inodes_active[x] = 0; //Set inode to inactive on status table  	
}

// Sets the values of inode i in the inode table in memory
void update_Inode(int x, int mode, int linkCount, int size, int data_ptrs[12], int indPointer){
	inodes_active[x] = 1; //Set node to active on the status table
	inodeTable[x].mode = mode; //holds the mode of the
	inodeTable[x].linkCount = linkCount; //holds the number of blocks this inode is linked to in memory
	inodeTable[x].size = size;//size fo the file
	inodeTable[x].indPointer = indPointer;
	for (int y = 0; y < 12; y++){ //Loop through to set datapointers 
		inodeTable[x].data_ptrs[y] = data_ptrs[y]; 
	}
}
/**
	These helper methods implement a Free List, it represents 
	what portion of the disk has not been allocated for a file.
	It is used to determine where to store directories in memory
**/

/** Helper Methods for Free List  **/
void start_FL(){//Sets each portion of the Freelist to free
	for(int x=0;x<FL_SIZE;x++){
		Free_List[x] =255; //255 is the max value and unsigned int can hold in a byte 
	}
}

void set_bit_to_used(unsigned int index) {//Sets a bit in the Free List to True 
    unsigned int loc_inFL = index / BYTE_SIZE; // get index in array of which bit to free
    unsigned int bit = index % BYTE_SIZE;  // find which individual bit to flip
    Free_List[loc_inFL] = Free_List[loc_inFL] & ~(1 << bit); // flip bit
}

unsigned int find_empty_mem() {// This method finds an empty memory location in the Free List
    unsigned int bit_index = 0;
    // find the first section with a free bit
    // let's ignore overflow for now...
    while (Free_List[bit_index] == 0) { //Keep looping until free bit is found 
        bit_index ++; 
        if (bit_index  >= FL_SIZE){//If you have gone beyonf the Free List size, there is no space available
            return 0; //
        }
    }
    unsigned int bit = ffs(Free_List[bit_index]) ; //the ffs() function returns the least signifigant bit in the passed point in memory,
    unsigned int offset_bit = bit -1; // returns 1 as smallest so it must be subtracted by 1
    // set the bit to used
    Free_List[bit_index] = Free_List[bit_index] & ~(1 << offset_bit);//Updates Free list bit to on
    return bit_index*BYTE_SIZE + bit;
}

void clear_FreeList_bit(unsigned int index) { //clear a bit so it 
 	unsigned int bit = index % BYTE_SIZE; //find individual bit in it's byte
    unsigned int loc = index / BYTE_SIZE; //Find byte in in Free List
    Free_List[loc] =Free_List[loc] | (1 << bit);
}
/** End of Free list mapping methods **/

/** 					Helper Methods                   **/


int find_file_num(char *path){// Returns the inode for a given file, returns negative 1 if doesnt exist?
	char *arr = malloc(sizeof(char) * (MAX_FILE_NAME));
	for (int x = 0; x < TOTAL_FILES; x++){ //searh through all files 
		int test_num=rootDirectory[x].num; 
		if (test_num != -1){
			strcpy(arr, rootDirectory[x].name); //Copy the name to see if this is the right file 
			if (strcmp(arr, path) == 0){
				free(arr);
				return test_num;
			}
		}
	}
	free(arr);
	return -1;
}

int adjust_block(int mem){ //Adjusts block to next size up if neccesary 
	int mem_blocks= (mem/BLOCK_SIZE); //find whole number of blocks 
			if (mem % BLOCK_SIZE != 0){ //if remainder andother block is needed
				mem_blocks += 1; //add a block
			}
	return mem_blocks;
}

int activate_Inode(){ //Returns the first available I-node 
	int x=0; 
	while(inodes_active[x] !=0){ //Find a node not in use 
		x++;
		if(x== TOTAL_FILES){//Reached the end of possible files, there are no inodes to activate 
			return -1;
		}
	}
	inodes_active[x] =1;
	return x;
}

//**                 End of helper methods                                    **//

//** 											API Methods 				 **//
void mksfs(int fresh) { //creates the file system
	if (fresh == 0){
		current_loc = 0; //Since new disk reset the current location pointer
		start_FL();
		 //Build open file descriptor table 
		for (int i = 0; i < TOTAL_FILES; i++){ //Set all to empty since no files start open
			open_Files[i].IN_num = -1;
		}
		init_disk(DISK_NAME, BLOCK_SIZE, TOTAL_BLOCKS); //Use give init disk, to create disk
		void *arr = malloc(BLOCK_SIZE); //Allocate for superblock
		read_blocks(0, 1, arr);//Store Superblock in memory
		memcpy(&SB, arr, sizeof(superblock)); //Copy existing superblock into struct here
		free(arr); //Free memory for other reads and writes
		int numInodeBlocks = adjust_block(sizeof(inodeTable)); //calculate size of inode table 
		arr = malloc(BLOCK_SIZE * numInodeBlocks); //allocate for inode table 
		read_blocks(1, numInodeBlocks, arr); //read inode table into arr
		memcpy(&inodeTable, arr, sizeof(inodeTable)); //copy arr into the inode table
		free(arr); //Reuse Mem later
		arr = malloc(BLOCK_SIZE); //set arr to one block
		read_blocks(INODE_LIST_LOC, 1 , arr); //Read Inode table to memory 
		memcpy(&inodes_active, arr, sizeof(inodes_active));
		free(arr);  //Reuse Mem later
		// Read the Free List into memory
		arr = malloc(BLOCK_SIZE); //allocate space for the free list 
		read_blocks(FL_LOC, 1, arr); //Read in Free lsit 
		memcpy(&Free_List, arr, sizeof(Free_List));
		free(arr);
		// Read the root directory into memory
		arr = malloc(BLOCK_SIZE * (inodeTable[RD_INODE].linkCount)); //allocate for root directory	
		read_blocks(inodeTable[RD_INODE].data_ptrs[0], inodeTable[RD_INODE].linkCount, arr);//Read in the root directory
		memcpy(&rootDirectory, arr, sizeof(rootDirectory)); //copy arr into the rootdirectory
		free(arr);//clear mem for later 
	} 
	else { //Fresh Disk
		start_FL();
		//Initialize SuberBlock
		SB.magic = MAGIC;
   		SB.block_size = BLOCK_SIZE;
    	SB.system_size = TOTAL_BLOCKS;
    	SB.IN_length = TOTAL_FILES;
    	SB.RD_Inode = RD_INODE;
		current_loc = 0; //Since new disk reset the current location pointer
		for (int x = 0; x < TOTAL_FILES; x++){ //Creates the Root directory
			rootDirectory[x].num = -1;
			for (int y = 0; y < MAX_FILE_NAME; y++){
				rootDirectory[x].name[y] = NULL;
			}
		}	
		//build_RD(); //Since this a fresk disk the Root directory table must be made from scratch 
		//Since this a fresk disk File descriptor table must be made from scratch 
		for (int x = 0; x < TOTAL_FILES; x++){ //Set all to empty since no files start open
			open_Files[x].IN_num = -1; //set to empty 
		}
		init_fresh_disk(DISK_NAME, BLOCK_SIZE, TOTAL_BLOCKS); //Fresh disk called 
		for (int x = 0; x < TOTAL_FILES; x++){  //mark all inodes used so the blocks wont be used for memory
		clear_inode(x); //Clear all inodes to unused 
		}
		int total_IN_Blocks = adjust_block(sizeof(inodeTable)); //adjust the size of 
		int RD_blocks_num = adjust_block(sizeof(rootDirectory));//calculate number of blocks needed for Root Directory
		set_bit_to_used(0);//Set space for superblock
		for (int x = 1; x < total_IN_Blocks + 1; x++){ //Set space for Inodes 
			set_bit_to_used(x); //Set used for inodes 

		}
		for (int x = total_IN_Blocks+1; x < RD_blocks_num + (total_IN_Blocks + 1); x++){ //Set Space 
			set_bit_to_used(x); //Set space for Root Directory
		}
		int rd_pointer[12];//assign Root Directory pointers 
		for (int x = 0; x < RD_blocks_num; x++){
			rd_pointer[x] = x + total_IN_Blocks+1;
		}
		update_Inode(RD_INODE, 0, RD_blocks_num, -1, rd_pointer, -1);//Update the inode that will hold the root directory
		write_blocks(0, 1, &SB);//Write the superblock to disk
		write_blocks(1, total_IN_Blocks, &inodeTable); //write inode table to disk
		void *arr = malloc(BLOCK_SIZE * RD_blocks_num); //set arr to the size of Root Directory 
		memcpy(arr, &rootDirectory, sizeof(rootDirectory)); //copy the directory into r
		write_blocks(inodeTable[RD_INODE].data_ptrs[0], RD_blocks_num, arr);//Write Inodes to disk
		write_blocks(INODE_LIST_LOC, 1, &inodes_active); //Write active Inode list to mem
		write_blocks(FL_LOC, 1, &Free_List); //Write Free List that acts as Free List to disk
		free(arr);//Free up mem
	}
}
//done 
int sfs_get_next_filename(char *fname){ //get the name of the next file in directory
	if(current_loc >= TOTAL_FILES){ // In case of extedning past allocated memory 
		current_loc = 0;
		return 0; //invalid memory
	} //Valid memory
	while (rootDirectory[current_loc].num == -1){ // Checking incase next directory is empty
		current_loc++;//update to next file
		if (current_loc >= TOTAL_FILES){ //Check if the end of the disk has been hit 
			current_loc = 0;
			return 0;//End of disk hit failure 
		}
	}
	char * found_name = rootDirectory[current_loc].name; //Store found directory name 
	strcpy(fname, found_name); //copy the name 
	current_loc++; //Update to next file
	return 1; //return success
}
//done
int sfs_GetFileSize(char* path){ // get the size of the given file
	int active_IN = find_file_num(path);//Retrieve active Inode index
	if (active_IN == -1){//Check if inode index is valid 
		return -1; //Invalid inode index
	} else {
		int ret_size =inodeTable[active_IN].size;// get file size 
		return ret_size; //Return
	}
}
//done
int sfs_fopen(char *name){ // opens the given file
	int name_len = 0;
	while (*(name + name_len) != NULL){//count the length of the filename 
		name_len++;
	}
	if (name_len > MAX_FILE_NAME + 1){//Filename is too long
		return -1;
	}
	int opf_index = 0;
	int x=0;
	while(open_Files[x].IN_num!=-1){ //Iterate Until the maximum 
		x++;
		if(x== TOTAL_FILES){ //Limit of Of Inodes 
			opf_index=-1; //set this to -1 to indicate failure 
		}
	}
	if(opf_index!=-1){ //If there was no failure
		opf_index=x; //update the the new files Open File descriptor table number 
	}
	if (opf_index != -1){ //If getting the file succeeded
		int active_IN = find_file_num(name);
		if (active_IN != -1){ // file inode exists
			for (int i = 0; i < TOTAL_FILES; i++){ //Search Inode Table 
				if (open_Files[i].IN_num == active_IN){//If the inode is already an open file, return fail
					return -1;
				}
			}
			//Create an entry in Open File Descriptor 
			open_Files[opf_index].IN_num = active_IN;
			open_Files[opf_index].inode = &(inodeTable[active_IN]);
			open_Files[opf_index].readPointer = 0;
			open_Files[opf_index].writePointer = inodeTable[active_IN].size;
			return opf_index;
		} 
		else { //Inode doesnt exist need to be created 
			int created_IN = activate_Inode(); //Create Inode 
			if (created_IN == -1){
				return -1;
			}
			int rd_num = 0; //Placeholder directory index
			int x=0;
			while(rootDirectory[x].num!=-1){//Search until an free spot is found 
				x++;
				if(x== TOTAL_FILES){ //If space is run out
					rd_num=-1; //return fail
				}
			}
			if(rd_num!=-1){ //If there wasn't a failure, set rd to the found x
				rd_num=x;
			}
			if (rd_num == -1){//Check for failure to find a directory
				return -1;//return fail
			}
			int mem_block = find_empty_mem();//Find a blcok of memory for the data 
			if (mem_block == 0){ //Failure to find space in memory
				return -1;
			}
			int data_ptrs[12];
			data_ptrs[0] = mem_block;//Assign first pointer to the found mem_block
			for (int i = 1; i < 12; i++){ //Assign the rest to unused
				data_ptrs[i] = -1;//set pointer to unused 
			} 
			open_Files[opf_index].IN_num = created_IN;//Update open file descriptor with new inode		
			rootDirectory[rd_num].num = created_IN;//umpdate Root directory with new inode
			strcpy(rootDirectory[rd_num].name, name); //copy name into the root Directory name 
			update_Inode(created_IN, 0, 1, 0, data_ptrs, -1); //Create Inode
			//Update Open file descriptor with neccesary info
			open_Files[opf_index].writePointer = inodeTable[created_IN].size; //write pointer points to the end of the file
			open_Files[opf_index].readPointer = 0; //set read pointer to beginning of file
			open_Files[opf_index].inode = &(inodeTable[created_IN]); 
			int RD_blocks_num= adjust_block(sizeof(rootDirectory)); //Adjust the block size if needed
			inodeTable[RD_INODE].size += 1; //Update inode count
			void *arr = malloc(BLOCK_SIZE * RD_blocks_num); //Allocate mem to write RD to disk
			memcpy(arr, &rootDirectory, sizeof(rootDirectory)); //copy RD into disk
			write_blocks(inodeTable[RD_INODE].data_ptrs[0], RD_blocks_num, arr); //write arr to disk as Root Directory
			free(arr); //free mem
			int numInodeBlocks = adjust_block(sizeof(inodeTable));//Adjust the number of blocks for the Inode table if needed 
			write_blocks(1, numInodeBlocks, &inodeTable);//Write I node table to disk
			write_blocks(INODE_LIST_LOC, 1, &inodes_active);//Write Active Inode list 
			write_blocks(FL_LOC, 1, &Free_List);//Write Free List 
			return opf_index;
		}
	} else {
		return -1;
	}
}

//Done
int sfs_fclose(int fileID) { // closes the given file
	if (open_Files[fileID].IN_num != -1){ //If the file is open, 
		open_Files[fileID].IN_num = -1;//Update the Open file Descriptor table to show it is closed 
		return 0;
	} else {
		return -1;//Failed it is already closed or doesnt exist
	}
}
 
//Done 
/** sfs_frseek method moves the read pointer  **/
int sfs_frseek(int fileID, int loc) { // seek (Read) to the location from beginning
	int file_size = inodeTable[open_Files[fileID].IN_num].size;
	if (file_size >= loc){ // Ensure the new location is not greater than the total size of the file 
		open_Files[fileID].readPointer = loc; //update pointer 
		return 0;
	}
	return -1; //location ivalid 
}

//Done 
/** sfs_frseek method moves the write pointer  **/
int sfs_fwseek(int fileID, int loc) { // seek (Write) to the location from beginning
	int file_size = inodeTable[open_Files[fileID].IN_num].size;
	if (file_size >= loc){ // Ensure the new location is not greater than the total size of the file 
		open_Files[fileID].writePointer = loc; //update pointer
		return 0;
	}
	return -1; //location invalid 
}

//done
int sfs_fwrite(int fileID, const char *buf, int length) { // write buf characters into disk
	int inode_to_write = open_Files[fileID].IN_num; //set inode to be worked on
	int written_len = length; //length to write to disk
	if (inode_to_write == -1){ //Catch 
	 return -1;
	}
	if (fileID < 0){ //catch bad file ID
	 return -1;
	}
	int new_end = open_Files[fileID].writePointer + length; //New location that the file must be larger than to write 
	if (new_end > MAX_FILE_SIZE){ //If the new end goes beyond file size 
		new_end = MAX_FILE_SIZE; //set hard limit 
		written_len = MAX_FILE_SIZE - open_Files[fileID].writePointer; //sets new end so we dont go beyond max file size 
	}
	int current_size = inodeTable[inode_to_write].linkCount;//link count represents how many blocks of data are connected 
	int new_block_size = adjust_block(new_end); //adjust to find a new block size if neccesary 
	int blocks_tobe_allocated = new_block_size - current_size; //The number of blocks that need to allocated to write 
	void *indirect_arr = malloc(BLOCK_SIZE); // will hold data to be written to disk
	if (inodeTable[inode_to_write].linkCount > 12){//Cheack if indirect pointer needs to be used 
		read_blocks(inodeTable[inode_to_write].indPointer, 1, indirect_arr); //read the indirect pointer 
		memcpy(&indirect_mem, indirect_arr, BLOCK_SIZE); //Store indirect pointer 
	} else if (inodeTable[inode_to_write].linkCount + blocks_tobe_allocated > 12) { //If more memory needed exceeds num of pointers
		int new_block = find_empty_mem();//find a new me
		if (new_block == 0){ //if 0, then no new blocks availabe
			return -1; //report failure 
		}		
		inodeTable[inode_to_write].indPointer = new_block;//set the indirect pointer to point to the desired 
	}
	free(indirect_arr);// Clear mem to be used later 
	if (blocks_tobe_allocated > 0){//Blocks need to be allocated 
		for (int x = inodeTable[inode_to_write].linkCount; x < inodeTable[inode_to_write].linkCount + blocks_tobe_allocated; x++){ //iterate to find enough blocks to accomodate the write 
			int found_block = find_empty_mem(); //find new mem
			if (found_block == 0){ //no new data is available 
				return -1; //report failure 
			} else { 
				if (x < 12){ //pointers are full
					inodeTable[inode_to_write].data_ptrs[x] = found_block;	//point to found block
				} else {
					indirect_mem[x - 12] = found_block; //need to use and indirect block
				}
			}
		}
	} else {//No blocks need to be allocated 
		blocks_tobe_allocated = 0;
	}
	int wptr_end = new_block_size; //new endpoint 
	int wptr_off = open_Files[fileID].writePointer % BLOCK_SIZE; //bit offset 
	int wptr_loc = open_Files[fileID].writePointer / BLOCK_SIZE; //starting block
	void *arr = malloc(BLOCK_SIZE * new_block_size);//allocate space for new block
	for (int x = wptr_loc;x < wptr_end && x < inodeTable[inode_to_write].linkCount; x++){ //iterate while not at the and and x is lower than the link count
		if (x < 12){//
			read_blocks(inodeTable[inode_to_write].data_ptrs[x], 1, (arr + (x-wptr_loc) * BLOCK_SIZE)); //read data 
		} else {
			read_blocks(indirect_mem[x-12], 1, (arr + (x-wptr_loc) * BLOCK_SIZE));// more than 12 links, must read from the indirect member 
		}
	}
	memcpy((arr + wptr_off), buf, written_len); //copy given writing into arr, with the offset
	write_blocks(INODE_LIST_LOC, 1, &inodes_active); //Update actice inode list
	for (int x = wptr_loc; x < wptr_end; x++){  //Iterare  
		if (x < 12){
			write_blocks(inodeTable[inode_to_write].data_ptrs[x], 1, (arr + ((x-wptr_loc) * BLOCK_SIZE)));//write to direct pointers 
		} else {
			write_blocks(indirect_mem[x-12], 1, (arr + (x-wptr_loc) * BLOCK_SIZE));//write to indirect pointers 
		}
	}
	if (inodeTable[inode_to_write].size < new_end){ //check if inode size needs to be updated
		inodeTable[inode_to_write].size = new_end;//update size of file 
	} 
	inodeTable[inode_to_write].linkCount += blocks_tobe_allocated; //update the link count of i node 
	open_Files[fileID].writePointer = new_end; //update writing pointer 
	if (inodeTable[inode_to_write].linkCount > 12){ //If links exceed direct pointers 
		write_blocks(inodeTable[inode_to_write].indPointer, 1, &indirect_mem); //write the indirect block 
	}
	int numInodeBlocks = adjust_block(sizeof(inodeTable)); //Adjust number of blocks for inode table 
	write_blocks(1, numInodeBlocks, &inodeTable); //Write table of Inodes 
	write_blocks(INODE_LIST_LOC, 1, &inodes_active); //Write active inode list 
	write_blocks(FL_LOC, 1, &Free_List); //Write Free List to 
	free(arr);// Free mem for later use 
	return written_len;
}

int sfs_fread(int fileID, char *buf, int length) { // read characters from disk into buf
	int inode_tobe_read = open_Files[fileID].IN_num;//set file inode
	int len_tobe_read;
	int end_of_Read;
	if (fileID < 0){ //Check valid ID 
		return -1;
	} 
	if (inode_tobe_read == -1){ //Check valid inode 
		return -1;
	}
	int curr_size =inodeTable[inode_tobe_read].size;//find file size 
	if (curr_size <= 0){ //check valid size of file 
	 	return length;
	} 
	if (curr_size < (open_Files[fileID].readPointer + length)){//Check for read to large
		len_tobe_read = curr_size - open_Files[fileID].readPointer;
		end_of_Read = adjust_block(curr_size); //adjust endpoint of file 
	} 
	else {//no problem with read size 
		len_tobe_read = length; 
		end_of_Read = adjust_block((open_Files[fileID].readPointer + length));
	}
	void *indirect_arr = malloc(BLOCK_SIZE); //buffer to read indirect block if needed
	int read_off = open_Files[fileID].readPointer % BLOCK_SIZE; //get bit in block
	int read_loc = open_Files[fileID].readPointer / BLOCK_SIZE; //get block
	if (inodeTable[inode_tobe_read].linkCount > 12){ //Check number of links to see if the indirect pointer is needed
		read_blocks(inodeTable[inode_tobe_read].indPointer, 1, indirect_arr); //obtain indirect pointer values 
		memcpy(&indirect_mem, indirect_arr, BLOCK_SIZE); //copy the read buffer into 
	}
	void *arr = malloc(BLOCK_SIZE * end_of_Read); //alloc space to read into arr
	for (int x = read_loc; x < inodeTable[inode_tobe_read].linkCount && x < end_of_Read; x++){
		if (x >= 12){//if link is larger than 12 the 
			read_blocks(indirect_mem[x-12], 1, (arr + (x-read_loc) * BLOCK_SIZE)); //read indirect memory
		} else {
			read_blocks(inodeTable[inode_tobe_read].data_ptrs[x], 1, (arr + (x-read_loc) * BLOCK_SIZE)); //read direct memory 
		}
	}
	memcpy(buf, (arr + read_off), len_tobe_read); //copy readed material to return buf
	open_Files[fileID].readPointer += len_tobe_read; //update read pointer 
	free(arr);//free mem for later 
	free(indirect_arr);//Free mem for later 
	return len_tobe_read; //return total bits read 
}

int sfs_remove(char *file) { //This method will remove a file from the system
	int active_IN = find_file_num(file); //find the file in the 
	if(active_IN <=0){  //Inode doesnt exist 
		return -1; //return fail
	}
	else{
		int links =inodeTable[active_IN].linkCount; //holds the link count of the inode 
		if (links> 12){ //If links is too big we need to clear the 
			void *indirect_arr = malloc(BLOCK_SIZE);
			read_blocks(inodeTable[active_IN].indPointer, 1, indirect_arr); //read indirect pointers into indirect_arr
			memcpy(&indirect_mem, indirect_arr, BLOCK_SIZE);
			for (int x = 12; x < links; x++){ //Clear indirect pointers 
				clear_FreeList_bit(indirect_mem[x-12]);
			}
			free(indirect_arr);//free mem for later use 
		}
		for (int x = 0; x < links && x < 12; x++){ //iterates through standard pointers 
			clear_FreeList_bit(inodeTable[active_IN].data_ptrs[x]); //clear pointer list 
		}
		clear_inode(active_IN); //Clears the active inode 
		for (int x = 0; x < TOTAL_FILES; x++){// search through the root directory to 
			if (strcmp(rootDirectory[x].name, file) == 0){ //Search for the files root Directory
				rootDirectory[x].num = -1; //if found set it empty
				for (int y = 0; y < MAX_FILE_NAME; y++){//Clear the file name 
					rootDirectory[x].name[0]=NULL; //clears char in name to null
				}
				break;
			}
		}
		inodeTable[RD_INODE].size -= 1; //Decrease size of Inode table
		int RD_blocks_num = adjust_block(sizeof(rootDirectory)); //Adjust the number 
		void *arr = malloc(BLOCK_SIZE * RD_blocks_num); //allocate space for root directory 
		memcpy(arr, &rootDirectory, sizeof(rootDirectory)); //copy root directory 
		write_blocks(inodeTable[RD_INODE].data_ptrs[0], RD_blocks_num, arr); //write root directory to disk
		int numInodeBlocks = adjust_block(sizeof(inodeTable)); //adjust size of Inode table 
		write_blocks(1, numInodeBlocks, &inodeTable); //Write Inode table 
		write_blocks(INODE_LIST_LOC, 1, &inodes_active); //write Inodes 
		write_blocks(FL_LOC, 1, &Free_List); //Write Free list 
		free(arr); //Free mem for later 
		return 0;
	}
}