#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>
#include <sys/stat.h>

#define BLOCK_SIZE 1024 //Bytes
#define NUM_BLOCKS 66370
#define NUM_BLOCKS_FOR_FILE_DATA 65258
#define BLOCKS_PER_FILE 1024 // = a file that is 1MB or 1,048,576 Bytes
#define NUM_FILES 256
#define FIRST_DATA_BLOCK 1112 // block our data begins at. if we start at 1,112 and add
                              // the required number of blocks that should be available
                              // to files (65,258), we get to a new NUM_BLOCKS of 66370

#define MAX_FILE_SIZE 1048576 //Bytes

uint8_t data[NUM_BLOCKS][BLOCK_SIZE];

// 64 blocks needed for free_blocks
uint8_t * free_blocks;

// 1 block needed for free_inodes
uint8_t * free_inodes;


// directory
struct directoryEntry
{
  char    filename[64];
  short   in_use;
  int32_t inode; //max inode
};

struct directoryEntry* directory;

// inode
struct inode
{
  int32_t  blocks[BLOCKS_PER_FILE];
  short    in_use;
  uint8_t  attribute;
  uint32_t file_size;
};

struct inode* inodes;
FILE* fp;
char image_name[64];
uint8_t image_open;





#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
                                // so we need to define what delimits our tokens.
                                // In this case  white space
                                // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255    // The maximum command-line size
#define MAX_NUM_ARGUMENTS 15




// "free_blocks" points to block number 1047
// each index number directly corresponds to
// a block that is allocated for file data 
int32_t findFreeBlock()
{
  int i;
  for( i = 0; i < NUM_BLOCKS_FOR_FILE_DATA; i++)
  {
    if(free_blocks[i])
    {
      // Mark block as now unavailable
      free_blocks[i] = 0;
      return i;
    }
  }

  return -1;
}


// we will index block 19 moving 1 byte at a time
// to find a free inode. Only moving 256 times
// because we have 1 inode per file
int32_t findFreeInode()
{
  int i;
  for(i = 0; i < NUM_FILES; i++)
  {
    if(free_inodes[i])
    {
      // Mark inode as now unavailable
      free_inodes[i] = 0;
      return i;
    }
  }

  return -1;
}


// Looking for a free block within our
// inode we found to be free
int32_t findFreeInodeBlock(int32_t inode)
{
  int i;
  for(i = 0; i < BLOCKS_PER_FILE; i++)
  {
    if(inodes[inode].blocks[i] == -1)
    {
      return i;
    }
  }

  return -1;
}



void init()
{
  // directory pointer will point to the beginning of our directory (blocks 0-18)
  // we can fit 256 directory entries in 18 blocks
  directory = (struct directoryEntry*) &data[0][0];


  // inodes will point to beginning of our inodes treated
  // as inode structs from (blocks 20-1046)
  // enough for 256 inode structs
  inodes = (struct inode*) &data[20][0];
  

  // (blocks 1,047 - 1,111) can store enough 1 byte ints to reference
  // 65,258 blocks of file data.
  free_blocks = (uint8_t*) &data[1047][0];


  // we have enough space in one block to keep track of 256 inodes
  // using a 1 or 0 to represent free or not
  free_inodes = (uint8_t*) &data[19][0];

  // zero out the image name and set it as not open
  memset(image_name, 0, 64);
  image_open = 0;


  int i;
  for(i = 0; i < NUM_FILES; i++)
  {
    // setting the directory entries to available
    directory[i].in_use = 0;

    // setting each directory entry's inode to -1
    directory[i].inode = -1;
    
    // indexing block 19 where our free_inode map is kept
    // for 256 files and setting them as available
    free_inodes[i] = 1;

    memset(directory[i].filename, 0, 64);


    // initialize our block indexes stored in
    // our inode structs to not in use.
    int j;
    for(j = 0; j < BLOCKS_PER_FILE; j++)
    {
      inodes[i].blocks[j] = -1;
    }

    // Initialize the rest of the data kept
    inodes[i].in_use = 0;
    inodes[i].attribute = 0;
    inodes[i].file_size = 0;
  }

  
  // initialize our free block map stored at blocks 1047 - 1111.
  // Start at 0 run to NUM_BLOCKS_FOR_FILE_DATA
  int j;
  for(j = 0; j < NUM_BLOCKS_FOR_FILE_DATA; j++)
  {
    free_blocks[j] = 1;
  }

}


uint32_t df()
{
  int j;
  int count = 0;

  // Use our free block map location (not where the actual data is stored)
  // and run from 0 to NUM_BLOCKS_FOR_FILE_DATA
  // checking if they are in use
  for(j = 0; j < NUM_BLOCKS_FOR_FILE_DATA; j++)
  {
    if(free_blocks[j])
    {
      count++;
    }
  }

  // return the numb of inuse blocks
  // multiplied by the # bytes stored in 
  // each block
  return count * BLOCK_SIZE;
}


//creating a filesystem image and zeroing out all memory
void createfs(char* filename)
{
  fp = fopen(filename, "w");
  
  // copy new filesystem filename to image_name
  strncpy(image_name, filename, strlen(filename));

  memset(data, 0, NUM_BLOCKS * BLOCK_SIZE);

  image_open = 1;

  int i;
  for(i = 0; i < NUM_FILES; i++)
  {
    directory[i].in_use = 0;
    directory[i].inode = -1;
    free_inodes[i] = 1;

    memset(directory[i].filename, 0, 64);

    int j;
    for(j = 0; j < BLOCKS_PER_FILE; j++)
    {
      inodes[i].blocks[j] = -1;
    }

    // Initialize the rest of the data kept
    inodes[i].in_use = 0;
    inodes[i].attribute = 0;
    inodes[i].file_size = 0;
  }

  int j;
  for(j = 0; j < NUM_BLOCKS_FOR_FILE_DATA; j++)
  {
    free_blocks[j] = 1;
  }

  fclose(fp);
}


//saving the filesystem image to disk and clearing out the imagename.
void savefs()
{
    if(image_open == 0)
    {
        printf("ERROR: Disk image is not open\n");
        return;
    }

    fp = fopen(image_name, "w");

    //writing to fp, from data
    fwrite(&data[0][0], BLOCK_SIZE, NUM_BLOCKS, fp);

    //zero it back out, arent using it anymore
    memset(image_name, 0, 64);

    fclose(fp);
}


//open the filesytem image and parse all of our data
void openfs(char* filename)
{
  //open image to read from
  fp = fopen(filename, "r");

  strncpy(image_name, filename, strlen(filename));

  //reading from the fp and storing to data
  fread(&data[0][0], BLOCK_SIZE, NUM_BLOCKS, fp);

  image_open = 1;

  fclose(fp);
}


//close our filesytem image
void closefs()
{
  if(image_open == 0)
  {
    printf("ERROR: Disk image is not open\n");
    return;
  }

  fclose(fp);

  image_open = 0;
  memset(image_name, 0, 64);
}


//list files in our filesystem image
void list()
{

  int i;
  int not_found = 1;

  for(i = 0; i < NUM_FILES; i++)
  {
    //\TODO Add a check to not list if the file is hidden
    if(directory[i].in_use == 1)
    {
      not_found = 0;

      char filename[65];
      memset(filename, 0, 65);
      strncpy(filename, directory[i].filename, strlen(directory[i].filename));

      printf("%s\n",filename);
    }
  }

  if(not_found)
  {
    printf("list: No files found.\n");
  }
}


// insert a file into system
void insert(char* filename)
{
  // verify filename is not NULL
  if(filename == NULL)
  {
    printf("ERROR: Filename is Null\n");
  }


  // verify the file exists
  struct stat buf;
  int ret = stat(filename, &buf);
  if(ret == -1)
  {
    printf("ERROR: File does not exist\n");
    return;
  }


  //verify the file is not too big
  if(buf.st_size > MAX_FILE_SIZE)
  {
    printf("ERROR: File is too large\n");
    return;
  }


  //verify there is enough space
  if(buf.st_size > df())
  {
    printf("ERROR: Not enough free disk space\n");
    return;
  }


  //find an empty directory
  int i;
  int directory_entry = -1;
  for( i = 0; i < NUM_FILES; i++)
  {
    if(directory[i].in_use == 0)
    {
      directory_entry = i;
      break;
    }

  }

  if(directory_entry == -1)
  {
    printf("ERROR: Could not find a free directory entry\n");
    return;
  }


  // Open the input file read-only
  FILE* ifp = fopen(filename, "r");
  printf("Reading %d bytes from %s\n", (int) buf.st_size, filename);


  // Save off the size of the input file since we'll use it in a couple of places and
  // also initialize our index variables to zero
  int32_t copy_size = buf.st_size;


  // We want to copy and write in chunks of BLOCK_SIZE. So to do this
  // we are going to use fseek to move along our file stream in chunks of BLOCK_SIZE.
  // We will copy bytes, increment our file pointer by BLOCK_SIZE and repeat.
  int32_t offset = 0;


  // Initializing to -1 we will then look for freeblocks we can store our
  // data to  
  int32_t block_index = -1; //using our block map to find a free block


  // find a free inode from our inode map
  int32_t inode_index = findFreeInode();
  if(inode_index == -1)
  {
    printf("ERROR: Can not find a free inode\n");
    return;
  }


  // place the file info in the directory
  directory[directory_entry].in_use = 1;
  directory[directory_entry].inode = inode_index;
  strncpy(directory[directory_entry].filename, filename, strlen(filename));


  // Take our found free indoe and set file size and set unavailable
  inodes[inode_index].file_size = buf.st_size;
  inodes[inode_index].in_use = 1;


  // copy_size is initialized to the size of the input file so each loop iteration we
  // will copy BLOCK_SIZE bytes from the file then reduce our copy_size counter by
  // BLOCK_SIZE number of bytes. When copy_size is less than or equal to zero we know
  // we have copied all the data from the input file
  while(copy_size > 0)
  {

    // Index into the input file by offset number of bytes. Initially offset is set to
    // zero so we copy BLOCK_SIZE number of bytes from the front of the file. We
    // then increase the offset by BLOCK_SIZE and continue the process. This will
    // make us copy from offset 0, BLOCK_SIZE, 2*BLOCK_SIZE, 3*BLOCK_SIZE, etc.
    fseek(ifp, offset, SEEK_SET);


    // find a free block
    block_index = findFreeBlock();
    if(block_index == -1)
    {
      printf("ERROR: Can not find a free block\n");
      return;
    }

    
    // Reading our data from file into our data array
    int32_t bytes = fread(data[block_index], BLOCK_SIZE, 1, ifp);


    // With our freely found inode, locate a free block with
    // the inode blocks array and record the block_index into 
    // our blocks array in the inode
    int32_t free_inode_block = findFreeInodeBlock(inode_index);
    inodes[inode_index].blocks[free_inode_block] = block_index;


    // If bytes == 0 and we haven't reached the end of the file then something is
    // wrong. If 0 is returned and we also have the EOF flag set then that is OK.
    // It means we've reached the end of our input file.
    if( bytes == 0 && !feof(ifp))
    {
      printf("ERROR: An error occured reading from the input file\n");
      return;
    }

    // Clear the EOF file flag
    clearerr(ifp);

    // Reduce copy_size by the BLOCK_SIZE bytes
    copy_size -= BLOCK_SIZE;

    // Increase the offset into our input file by BLOCK_SIZE. This will allow
    // the fseek at the top of the loop to position us to the correct spot.
    offset += BLOCK_SIZE;
  } 

  // We are done copying from the input file so close it out
  fclose(ifp);
}

int main()
{
  char * command_string = (char*) malloc( MAX_COMMAND_SIZE );

  fp = NULL;

  init();

  while( 1 )
  {
    // Print out the msh prompt
    printf ("msf> ");

    // Read the command from the commandline.  The
    // maximum command that will be read is MAX_COMMAND_SIZE
    // This while command will wait here until the user
    // inputs something since fgets returns NULL when there
    // is no input
    while( !fgets (command_string, MAX_COMMAND_SIZE, stdin) );

   
    /* Parse input */
    char *token[MAX_NUM_ARGUMENTS];

    for( int i = 0; i < MAX_NUM_ARGUMENTS; i++ )
    {
      token[i] = NULL;
    }

    int   token_count = 0;                                 
                                                           
    // Pointer to point to the token
    // parsed by strsep
    char *argument_ptr = NULL;                                         
                                                           
    char *working_string  = strdup( command_string );                

    // we are going to move the working_string pointer so
    // keep track of its original value so we can deallocate
    // the correct amount at the end
    char *head_ptr = working_string;

    // Tokenize the input strings with whitespace used as the delimiter
    while ( ( (argument_ptr = strsep(&working_string, WHITESPACE ) ) != NULL) && 
              (token_count<MAX_NUM_ARGUMENTS))
    {
      token[token_count] = strndup( argument_ptr, MAX_COMMAND_SIZE );
      if( strlen( token[token_count] ) == 0 )
      {
        token[token_count] = NULL;
      }
        token_count++;
    }




    //handle blank line input
    if(token[0] == NULL)
    {
      free(head_ptr);
      continue;
    }


    // **process the filesystem commands**

    //createfs
    if(!strcmp("createfs", token[0]))
    {
      if(token[1] == NULL)
      {
        printf("ERROR: No filename specified\n");
        continue;
      }

      createfs(token[1]);
    }


    //savefs
    if(!strcmp("savefs", token[0]))
    {
      savefs();
    }


    //open
    if(!strcmp("open", token[0]))
    {
      if(token[1] == NULL )
      {
        printf("ERROR: no filename specified\n");
        continue;
      }

      openfs(token[1]);
    }


    //close
    if(!strcmp("close", token[0]))
    {
      closefs();
    }


    //list
    if(!strcmp("list", token[0]))
    {
      if(!image_open)
      {
        printf("ERROR: Disk image is not opened\n");
        continue;
      }

      list();
    }


    //disk free space
    if(!strcmp("df", token[0]))
    {
      if(!image_open)
      {
        printf("ERROR: Disk image is not open\n");
        continue;
      }

      printf("%d bytes free\n", df());
    }


    //quit
    if(!strcmp("quit", token[0]))
    {
      exit(0);
    }


    //insert
    if(!strcmp("insert", token[0]))
    {
      if(!image_open)
      {
        printf("ERROR: Disk image is not open\n");
        continue;
      }

      if(token[1] == NULL)
      {
        printf("ERROR: No filename specified\n");
        continue;
      }

      insert(token[1]);
    }

   

    // Cleanup allocated memory
    for( int i = 0; i < MAX_NUM_ARGUMENTS; i++ )
    {
      if( token[i] != NULL )
      {
        free( token[i] );
      }
    }

    free( head_ptr );

  }

  free( command_string );

  return 0;
  // e2520ca2-76f3-90d6-0242ac120003
}