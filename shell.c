#include <stdio.h>
#include <string.h>
#include "tokens.h"
#include <assert.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_SIZE 255
char* prevCommands[MAX_SIZE];

void parseCommands(char** tokens);
void execCommand(char **currCommands, int stdin, int stdout);
int pipeIndex(char* currCommands[]);
int isBuiltIn(char* currCommand);
void execBuiltin(char** currCommands, int cmdToExec);
void execCd(char* directory);
void execSource(char** currCommands);
void execPrev();
void execHelp();
void storePrevCommands(char** currCommands, int token_i);
bool isPrevCalled(char** currCommands);
int arrLength(char** array);


int main(int argc, char **argv) {
  printf("Welcome to mini-shell.\n");

  char input[MAX_SIZE];
  char **tokens;

  while(1) {
    printf("shell $ ");

    if(fgets(input, MAX_SIZE, stdin) == 0) {
      printf("\nBye bye\n");
      exit(0);
    }

    tokens = get_tokens(input);
    assert(tokens != NULL);

    parseCommands(tokens);
  }

  free_tokens(tokens);

  return 0;
}

void parseCommands(char** tokens) {
  char *currCommands[MAX_SIZE];
  int token_i = 0;
  int curr_i = 0;

  while(tokens[token_i] != NULL) {
    if(strcmp(tokens[token_i], ";") != 0) {
      currCommands[curr_i] = tokens[token_i];
      curr_i++;
      token_i++;
    }
    else {
      currCommands[curr_i] = NULL;
      tokens[token_i] = NULL;
      curr_i = 0;
      execCommand(currCommands, 0, 1);
      token_i++;
    }
  }
  currCommands[token_i] = NULL;
  tokens[token_i] = NULL;

  if (!isPrevCalled(tokens)) {
      storePrevCommands(tokens, token_i);
  }
  execCommand(currCommands, 0, 1);
}

void execCommand(char **currCommands, int stdi, int stdou) {
  int pipe_i = pipeIndex(currCommands);
  if (pipe_i != -1) {
    int pipe_fds[2];
    pipe(pipe_fds);
    int read_fd = pipe_fds[0];
    int write_fd = pipe_fds[1];
    int stdinCpy = dup(stdi);
    int stdoutCpy = dup(stdou);

    char* commandsBeforePipe[arrLength(currCommands)];
    char* commandsAfterPipe[arrLength(currCommands)];
    
    int i;
    for(i = 0; i < pipe_i; i++) {
      commandsBeforePipe[i] = currCommands[i];
    }
    commandsBeforePipe[i] = NULL;

    int j;
    int index = 0;
    for(j = pipe_i + 1; j < arrLength(currCommands); j++) {
      commandsAfterPipe[index] = currCommands[j];
      index++;
    }
    commandsAfterPipe[index] = NULL;

    close(stdou);
    dup2(write_fd, 1);

    execCommand(commandsBeforePipe, stdi, write_fd);

    close(write_fd);
    dup2(stdoutCpy, 1);
    dup2(read_fd, 0);
    close(stdi);

    execCommand(commandsAfterPipe, read_fd, stdoutCpy);

    close(read_fd);
    dup2(stdinCpy, 0);

    close(stdinCpy);
    close(stdoutCpy);

    return;
  }
  else if (strcmp(currCommands[arrLength(currCommands)-2], ">") == 0 || strcmp(currCommands[arrLength(currCommands)-2], "<") == 0) {
    char* redirect = currCommands[arrLength(currCommands)-2];
    currCommands[arrLength(currCommands)-2] = NULL;
    int file;
    int stdinCpy;
    int stdoutCpy;

    if(strcmp(redirect, ">") == 0) {
      stdoutCpy = dup(stdou);
      file = open(currCommands[arrLength(currCommands)+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
      dup2(file, 1);
      close(stdou);
      execCommand(currCommands, stdi, file);
      dup2(stdoutCpy, 1);
      close(stdoutCpy);
      return;
    }

    if(strcmp(redirect, "<") == 0) {
      stdinCpy = dup(stdi);
      file = open(currCommands[arrLength(currCommands)+1], O_RDONLY);
      dup2(file, 0);
      close(stdi);
      execCommand(currCommands, file, stdou);
      dup2(stdinCpy, 0);
      close(stdinCpy);
      return;
    }
  }
  else {
    int cmdToExec = isBuiltIn(currCommands[0]);
    if(cmdToExec != 0) {
      execBuiltin(currCommands, cmdToExec);
      return;
    }
    else {
      pid_t pid = fork();
      if(pid == 0) {
        if(stdi != 0) {
          dup2(stdi, 0);
          close(stdi);
        }

        if(stdou != 1) {
          dup2(stdou, 1);
          close(stdou);
        }

        execvp(currCommands[0], currCommands);
        printf(strcat(currCommands[0], ": command not found\n"));
        exit(1);
      }
      wait(NULL);
      return;
    }
  }
}

int arrLength(char** array) {
  int length = 0;
  while(array[length] != NULL) {
    length++;
  }

  return length;
}

bool isPrevCalled(char** currCommand) {
  int counter = 0;
  for(counter = 0; counter < MAX_SIZE; counter++) {
    if(currCommand[counter] != NULL) {
      if(strcmp(currCommand[counter], "prev") == 0) {
        return true;
      }
    }
  }
  return false;
}

void storePrevCommands(char** currCommands, int token_i) {
  int i;
  for(i = 0; i < token_i; i++) {
    prevCommands[i] = currCommands[i];
  }
}

int pipeIndex(char* currCommands[]) {
  int i = 0;
  int final_i = -1;

  while(currCommands[i] != NULL) {
    if (strcmp(currCommands[i], "|") == 0) {
      final_i = i;
    }
    i++;
  }
  return final_i;
}

int isBuiltIn(char* currCommand) {
  char* builtins[] = {"exit", "cd", "source", "prev", "help", "\0"};
  int length = sizeof(builtins)/sizeof(builtins[0]);
  int i;
  for(i = 0; i < length; i++) {
    if(strcmp(currCommand, builtins[i]) == 0) {
      return i + 1;
    }
  }
  return 0;
}

void execBuiltin(char** currCommands, int cmdToExec) {
  switch(cmdToExec) {
    case 1:
      printf("Bye bye\n");
      exit(0);
    case 2:
      execCd(currCommands[1]);
      break;
    case 3:
      execSource(currCommands);
      break;
    case 4:
      execPrev();
      break;
    case 5:
      execHelp();
      break;
    default:
      break;
  }
}

void execCd(char* directory) {
  if(directory != NULL) {
    if(chdir(directory) != 0) {
      printf("shell: cd: %s: No such file or directory\n", directory);
    }
  }
  else {
    if(chdir(getenv("HOME")) != 0) {
      printf("shell: cd: Unable to change file or directory\n");
    }
  }

  return;
}

void execSource(char** currCommands) {
  char* tokens[4];
  tokens[0] = "./shell";
  tokens[1] = "<";
  tokens[2] = currCommands[1];
  tokens[3] = NULL;
  execCommand(tokens, 0, 1);
}

void execPrev() {
  int i = 0;
  char* tempCurrCommand[MAX_SIZE];
  int token_i = 0;
  int curr_i = 0;
  int length = sizeof(prevCommands)/sizeof(prevCommands[0]);
  while(i < length && (prevCommands[token_i] != NULL || prevCommands[token_i+1] != NULL)) {
    if(prevCommands[token_i] != NULL) {
      tempCurrCommand[curr_i] = prevCommands[token_i];
      curr_i++;
      token_i++;
    } else {
      curr_i = 0;
      execCommand(tempCurrCommand, 0, 1);
      token_i++;
    }
    i++;
  }
  int counter;
  for(counter = 0; counter < MAX_SIZE; counter++) {
    prevCommands[counter] = NULL;
  }
  execCommand(tempCurrCommand, 0, 1);
}

void execHelp() {
  puts("\n*** WELCOME TO SHELL HELP ***"
        "\nList of built-in commands supported:"
        "\n> exit - exits out of the shell"
        "\n> cd - changes the directory of your choice"
        "\n> source - takes in a file input and executes it line by line"
        "\n> prev - executes the previous command and prints it"
        "\n> help - opens up all built-in commands users can use\n");
  return;
}

