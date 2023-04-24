#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>

#define BLOCK_SIZE 1024
#define NUM_BLOCKS 65536
#define BLOCKS_PER_FILE 1024
#define NUM_FILES 256
#define FIRST_DATA_BLOCK 790

uint8_t data[NUM_BLOCKS][BLOCK_SIZE];

// 512 blocks just for free block map
uint8_t * free_blocks;
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
  int32_t blocks[BLOCKS_PER_FILE];
  short   in_use;
  uint8_t attribute;
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

#define MAX_NUM_ARGUMENTS 11  //11   // Mav shell only supports four arguments

#define MAX_HIST_STORED 15   //15   // Maximum number of commands to store in history

void init()
{
  //directory pointer will point to the beginning of our directory (blocks 0-18)
  directory = (struct directoryEntry*) &data[0][0];

  //inodes will point to beginning of our inodes (blocks 20-276)
  inodes = (struct inode*) &data[20][0];

  free_blocks = (uint8_t*) &data[277][0];

  free_inodes = (uint8_t*) &data[19][0];

  //zero out the image name and set it as not open
  memset(image_name, 0, 64);
  image_open = 0;

  int i;
  for(i = 0; i < NUM_FILES; i++)
  {
    //setting the directory entries to available
    directory[i].in_use = 0;

    //setting each directory entry's inode to -1
    directory[i].inode = -1;

    free_inodes[i] = 1;

    memset(directory[i].filename, 0, 64);

    int j;
    for(j = 0; j < NUM_BLOCKS; j++)
    {
      //setting the blocks within inodes to -1
      inodes[i].blocks[j] = -1;

      //setting the inodes to be available
      inodes[i].in_use = 0;

      //setting attribute init value
      inodes[i].attribute = 0;


    }
  }

  int j;
  for(j = 0; j < NUM_BLOCKS; j++)
  {
    free_blocks[j] = 1;
  }

}


void df()
{
  int j;
  int count = 0;
  for(j = FIRST_DATA_BLOCK; j < NUM_BLOCKS; j++)
  {
    if(free_blocks[j])
    {
      count++;
    }
  }

  printf("%d bytes free\n", count * BLOCK_SIZE);
}


//creating a filesystem image and zeroing out all memory
void createfs(char* filename)
{
  fp = fopen(filename, "w");

  strncpy(image_name, filename, strlen(filename));

  memset(data, 0, NUM_BLOCKS * BLOCK_SIZE);

  image_open = 1;

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
  //open image to write to
  fp = fopen(filename, "w");

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

      df();
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