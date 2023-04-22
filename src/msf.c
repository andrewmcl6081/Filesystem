// The MIT License (MIT)
// 
// Copyright (c) 2016 Trevor Bakker 
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
                                // so we need to define what delimits our tokens.
                                // In this case  white space
                                // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255    // The maximum command-line size

#define MAX_NUM_ARGUMENTS 11  //11   // Mav shell only supports four arguments

#define MAX_HIST_STORED 15   //15   // Maximum number of commands to store in history


void clear_history(char* history[]);
void print_pids(int* pid_history, int* curr_history_index);
void print_history(char** history, int* curr_hist_index);
void print_history_pids(int* pid_history, char** history, int* curr_hist_index);
void store_history(int* pid_history, char** history, int* curr_history_index, char* modified_string, int pid);

int main()
{

  char * command_string = (char*) malloc( MAX_COMMAND_SIZE );

  // temp string to tokenize command_string
  char cmd_str_cpy[255];

  // temp string to tokenize !n functionality
  char temp_string[255];

  // token after tokenizing !n
  char* temp_token;

  // int to store !(n)
  int n = 0;

  // the current # of items in our history
  int curr_hist_index = 0;

  // array to store pids
  int pid_history[MAX_HIST_STORED];

  // array of pointers to store command history
  char* history[MAX_HIST_STORED];

  pid_t pid;

  // initialize history array to NULL
  for(int i = 0; i < MAX_HIST_STORED; i++)
  {
    history[i] = NULL;
  }




  while( 1 )
  {
    // Print out the msh prompt
    printf ("msh> ");

    // Read the command from the commandline.  The
    // maximum command that will be read is MAX_COMMAND_SIZE
    // This while command will wait here until the user
    // inputs something since fgets returns NULL when there
    // is no input
    while( !fgets (command_string, MAX_COMMAND_SIZE, stdin) );

    // if we are utilizing "!n" functionality
    // tokenize the command string, check if n is
    // valid in history, and if it is copy its value to
    // command string. If it is not continue and prompt for
    // next input.
    if((command_string[0] == '!'))
    {
      strcpy(temp_string, command_string);
      temp_token = strtok(temp_string, "!");
      n = atoi(temp_token);

      if( (n >= 0) && (n < 15) && (n < curr_hist_index) )
      {
        strcpy(command_string, history[n]);
      }
      else
      {
        printf("Command not in history.\n");
        continue;
      }

    }

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


    // Remove newline from command string
    // There may or may not be one depending
    // If "!n" has been processed
    strcpy(cmd_str_cpy, command_string);
    char* modified_string = strtok(cmd_str_cpy, "\n");

    
    // Blank line input will continue for another prompted input
    if(token[0] == NULL)
    {
      free(head_ptr);
      continue;
    }
    // Exit or quit will free all memory for a graceful exit
    else if(!strcmp("exit", token[0]) || !strcmp("quit", token[0]))
    {
      for( int i = 0; i < MAX_NUM_ARGUMENTS; i++ )
      {
        if( token[i] != NULL )
        {
          free( token[i] );
        }
      }

      free(head_ptr);
      free(command_string);
      clear_history(history);
      
      exit(EXIT_SUCCESS);
    }
    // If user inputs history or history -p store command and execute proper choice
    else if(!strcmp("history", token[0]))
    {
      store_history(pid_history,history, &curr_hist_index, modified_string, -1);

      if( (token[1] != NULL) && (!strcmp("-p", token[1])) )
      {
        print_history_pids(pid_history,history,&curr_hist_index);
      }
      else
      {
        print_history(history,&curr_hist_index);
      }
    }
    // Store command and pid in history and display previous pids
    else if(!strcmp("showpids", token[0]))
    {
      store_history(pid_history,history, &curr_hist_index, modified_string, -1);

      print_pids(pid_history, &curr_hist_index);
    }
    // Store command in history and change directories if directory is valid
    else if(!strcmp("cd", token[0]))
    {
      store_history(pid_history,history, &curr_hist_index, modified_string, -1);

      int ret = chdir(token[1]);

      if(ret == -1)
      {
        printf("msh: %s: %s: No such file or directory\n", token[0], token[1]);
      }
    }
    // If our desired command isnt handled by parent fork a new child and execute
    // and store to history
    else
    {
      pid = fork();

      if(pid == -1)
      {
        exit(EXIT_FAILURE);
      }
      else if(pid == 0)
      {
        int ret = execvp(token[0], &token[0]);

        if(ret == -1)
        {
          printf("%s: Command not found.\n", token[0]);
          exit(EXIT_FAILURE);
        }
      }
      else
      {
        int status;
        wait(&status);

        store_history(pid_history,history, &curr_hist_index, modified_string, pid);
      }
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

// Clear history to avoid segfaults
void clear_history(char* history[])
{
  for(int i = 0; i < MAX_HIST_STORED; i++)
  {
    free(history[i]);
  }
}

// Print out just the pids to stdout
void print_pids(int* pid_history, int* curr_hist_index)
{
  for(int i = 0; i < *curr_hist_index; i++)
  {
    printf("%d: %d\n", i, pid_history[i]);
  }
}

// Display history of all previously entered commands
void print_history(char** history, int* curr_hist_index)
{
  for(int i = 0; i < *curr_hist_index; i++)
  {
    printf("%d: %s\n", i, history[i]);
  }
}

// Display history with respective pids for "history -p"
void print_history_pids(int* pid_history, char** history, int* curr_hist_index)
{
  for(int i = 0; i < *curr_hist_index; i++)
  {
    printf("%d: %s PID: %d\n", i, history[i], pid_history[i]);
  }
}

// Store history of commands and pids for future use
// and handle only 15 commands kicking out the oldest
// command and storing a new command 
void store_history(int* pid_history, char** history, int* curr_hist_index, char* modified_string, int pid)
{
  if(*curr_hist_index < MAX_HIST_STORED)
    {
      history[*curr_hist_index] = strdup(modified_string);
      pid_history[*curr_hist_index] = pid;

      *curr_hist_index = *curr_hist_index + 1;
    }
    else
    {
      free(history[0]);

      for(int i = 1; i < MAX_HIST_STORED; i++)
      {
        history[i-1] = history[i];
        pid_history[i-1] = pid_history[i];
      }

      history[MAX_HIST_STORED - 1] = strdup(modified_string);
      pid_history[MAX_HIST_STORED - 1] = pid;
    }
}
