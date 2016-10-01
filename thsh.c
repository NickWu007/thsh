/* COMP 530: Tar Heel SHell */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

// Assume no input line will be longer than 1024 bytes
#define MAX_INPUT 1024
#define MAX_VAR 256
#define MAX_JOB 16
#define DEFAULT_ARGS_NUM 32
#define DEFAULT_PATH_NUM 32
#define DEFAULT_PIPE_NUM 32
#define PATH_DELIM ":"
#define ARGS_DELIM " \n"
// For job status
#define UND 0 // undefined
#define FG  1 // foreground
#define BG  2 // background
#define STP 3 // stopped

struct job_t {
  char name[MAX_INPUT];
  int pid;
  int job_number;
  int state;
  int exit_status;
};

// Global variables
char **paths;
char **var_keys;
char **var_vals;
char cwd[MAX_INPUT];
char last_cwd[MAX_INPUT];
int var_count;
struct job_t jobs[MAX_JOB];
int job_count = 0;
int debugging;
int return_code;
static int thsh_pid = 0;

/**
 * Method for paring a string with a given demin and a length.
 * Returns array of parsed string tokens
 */
char ** parseLine(char *line, char *demin, int default_length) {
  char **tokens = malloc(sizeof(char *) * default_length);
  int tokenCount = 0;
  char * token;

  token = strtok(line, demin);
  while (token != NULL) {
    *(tokens + tokenCount) = token;
    tokenCount++;
    token = strtok(NULL, demin);
  }

  *(tokens + tokenCount) = NULL;
  return tokens;
}

/**
 * Method for finding if there is a binary in $PATH or current directory.
 * Returns abs path of binary, or NULL if no matches.
 */
char *findBin(char *cmd, char * cwd, char **paths) {
  if (cmd[0] == '/') return cmd;

  int i = 0;
  int cmdLen = strlen(cmd);
  int expectedLen = strlen(cwd) + 2 + cmdLen;
  char *potentialBin = (char *)malloc(sizeof(char) * expectedLen);
  strcpy(potentialBin, cwd);
  strcat(potentialBin, "/");
  strcat(potentialBin, cmd);
  struct stat fileStat;
  if(stat(potentialBin,&fileStat) >= 0) {
    return potentialBin;
  }
  free(potentialBin);
  while (*(paths + i) != NULL) {
    int expectedLen = strlen(paths[i]) + 2 + cmdLen;
    char *potentialBin = (char *)malloc(sizeof(char) * expectedLen);
    strcpy(potentialBin, *(paths + i));
    strcat(potentialBin, "/");
    strcat(potentialBin, cmd);
    struct stat fileStat;
    if(stat(potentialBin,&fileStat) >= 0) {
      return potentialBin;
    }
    i++;
    free(potentialBin);
  }
  return NULL;
}

/**
 * Method for adding a new variable in the variable table.
 */
void addVariable(char* key, char *value) {
  var_keys[var_count] = malloc(strlen(key) + 1);
  strcpy(var_keys[var_count], key);
  var_vals[var_count] = malloc(strlen(value) + 1);
  strcpy(var_vals[var_count], value);
  var_count++;
}

/**
 * Method for checking if a given symbol exists in variable table.
 * Returns index of the key if found, or -1 otherwise.
 */
int getVar(char *key) {
  int i;
  for (i = 0; i < var_count; i++) {
    if (var_keys[i] != NULL && strcmp(var_keys[i], key) == 0) {
      return i;
    }
  }

  return -1;
}

/**
 * Method for checking if a new symbol is legal by the spec:
 * "only have either alphanumerical names or a single "special" character (e.g., '?', '@', etc.)".
 * Returns 0 if legal, -1 otherwise.
 */
int checkSymbol(char *var_name) {
  if (strlen(var_name) == 1 && (var_name[0] == '!' || 
    var_name[0] == '@' || var_name[0] == '%' ||
    var_name[0] == '^' || var_name[0] == '&' ||
    var_name[0] == '*' || var_name[0] == '#'))
    return 0;
  int i;
  for (i = 0; i < strlen(var_name); i++) {
    if (!((var_name[i] >= 'a' && var_name[i] <= 'z') || (var_name[i] >= 'A' && var_name[i] <= 'Z')
      || (var_name[i] >= '0' && var_name[i] <= '9')))
      return -1;
  }
  return 0;
}

/**
 * Method for removing any leading spaces in a string.
 * Returns the input string, without leading spaces.
 */
char *stripLeadingSpaces(char *str) {
  while (*str == ' ') str ++;
  return str;
}
/**
 * Method for removing any quotation marks in a string.
 */
char *stripQuotes(char *str) {
  char *clean_str = malloc(MAX_INPUT);
  int i = 0, j = 0;
  while (i < strlen(str)) {
    if (str[i] != '\"') {
      clean_str[j] = str[i];
      j++;
    } 
    i++;
  }

  clean_str[j] = '\0';
  return clean_str;
}

/**
 * Method for substitute any variables in hte input cmd line.
 * Returns the command line with variable values added. 
 * Returns NULL if there's unrecognized variables.
 */
char *substituteVars(char *line) {
  char *abs_cmd = malloc(MAX_INPUT);
  int i = 0, j = 0;
  while (i < strlen(line)) {
    if (line[i] == '~') {
      int k = 0;
      char homeDir[MAX_INPUT];
      strcpy(homeDir, getenv("HOME"));
      while (k < strlen(homeDir)) {
        abs_cmd[j + k] = homeDir[k];
        k++;
      }
      j += k;
    } else if (line[i] != '$') {
      abs_cmd[j] = line[i];
      j++;
    } else {
      char name[MAX_INPUT];
      int k = i + 1;
      while (k < strlen(line) && line[k] != ' ' && line[k] != '\n') {
        name[k - i - 1] = line[k];
        k++;
      }
      name[k - i - 1] = '\0';
      int index = getVar(name);
      if (index < 0) {
        return NULL;
      }

      i = k - 1;
      if (strcmp(name, "?") != 0) {
        k = 0;
        while (k < strlen(var_vals[index])) {
          abs_cmd[j + k] = var_vals[index][k];
          k++;
        }
        j += k;
      } else {
        abs_cmd[j] = return_code + 48;
        j++;
      }
    }
    i++;
  }

  abs_cmd[j + 1] = '\0';
  return abs_cmd;
}

/**
 * Method for executing a single parsed command.
 * pipeIndicator: 0--first command in the pipe
 *                1--middle command in the pipe
 *                2--last command in the pipe 
 * Returns the read fd for the next command.
 */
int execute(char ** args, int input, int pipeIndicator) {
  int command_pipe[2];
  pipe(command_pipe);

  int last = 0;
  char *inputFile = NULL;
  char *outputFile = NULL;
  int extraInFd = -1;
  while (args[last] != NULL) {
    if (strchr(args[last], '<') != NULL) {
      // File come right after '<'
      if (strlen(strchr(args[last], '<')) > 1) {
        inputFile = strchr(args[last], '<') + 1;
      } else {
        // There is a space between '<' and file, grab the next args.
        inputFile = args[last + 1];
        args[last + 1] = NULL;
      }
      if (debugging) printf("Gotten input file: %s\n", inputFile);
      args[last] = NULL;
    } else if (strchr(args[last], '>') != NULL) {
      if (strcmp(strchr(args[last], '>'), args[last]) != 0) {
        printf("File handle before >\n");
        int i = 0;
        extraInFd = 0;
        while (args[last][i] != '>') {
          extraInFd = extraInFd * 10 + (args[last][i] - 48);
          i++;
        }

        printf("Gotten extra input handle: %d\n", extraInFd);
      }

      if (debugging) printf("%s contains >/<, index: %d\n", args[last], last);
      // File come right after '>'
      if (strlen(strchr(args[last], '>')) > 1) {
        outputFile = strchr(args[last], '>') + 1;
      } else {
        // There is a space between '>' and file, grab the next args.
        outputFile = args[last + 1];
        args[last + 1] = NULL;
      }
      if (debugging) printf("Gotten output file: %s\n", outputFile);
      args[last] = NULL;
    }
    last++;
  }
  
  pid_t pid = fork();
  if (pid == 0) {
    if (debugging) fprintf(stderr, "RUNNING: %s\n", args[0]);
    if (pipeIndicator == 0 && input == 0) {
      // First command, only dup pipe_out to STDOUT.
      dup2(command_pipe[1], 1);
    } else if (pipeIndicator == 1 && input != 0) {
      // Middle command, dup both.
      dup2(input, 0);
      dup2(command_pipe[1], 1);
    } else {
      // Last command, only dup pipe_in to STDIN
      dup2(input, 0);
    }

    int inFd, outFd;
    if (inputFile != NULL) {
      char inputDir[MAX_INPUT];
      strcpy(inputDir, cwd);
      strcat(inputDir, "/");
      strcat(inputDir, inputFile);
      if (debugging) printf("inputDir: %s\n", inputDir);
      inFd = open(inputDir, O_RDONLY);
      if (inFd < 0) {
        fprintf(stderr, "Error: couldn't find file %s\n", inputDir);
        exit(1);
      }
      dup2(inFd, 0);
      close(inFd);
    }
    if (outputFile != NULL) {
      char outputDir[MAX_INPUT];
      strcpy(outputDir, cwd);
      strcat(outputDir, "/");
      strcat(outputDir, outputFile);
      if (debugging) printf("outputDir: %s\n", outputDir);
      outFd = open(outputDir, O_WRONLY);
      if (outFd < 0) {
        fprintf(stderr, "Error: couldn't find file %s\n", outputDir);
        exit(1);
      }
      if (extraInFd > 0) {
        dup2(outFd, extraInFd);
      } else {
        dup2(outFd, 1);
      }
      close(outFd);
    }

    if (strcmp(args[0], "goheels") == 0) {
      printf("                                                             \n");
      printf(" ,----.    ,-----. ,--.  ,--.,------.,------.,--.    ,---.   \n");
      printf("'  .-./   '  .-.  '|  '--'  ||  .---'|  .---'|  |   '   .-'  \n");
      printf("|  | .---.|  | |  ||  .--.  ||  `--, |  `--, |  |   `.  `-.  \n");
      printf("'  '--'  |'  '-'  '|  |  |  ||  `---.|  `---.|  '--..-'    | \n");
      printf(" `------'  `-----' `--'  `--'`------'`------'`-----'`-----'  \n");
      printf("                                                             \n");
      exit(0);
    }

    if (strcmp(args[0], "echo") == 0) {
      if (args[1] == NULL) {
        fprintf(stderr, "Error: echo requires an argument\n");
        exit(1);
      } else {
        printf("%s", stripQuotes(args[1]));
        int i = 2;
        while (args[i] != NULL) {
          printf(" %s", stripQuotes(args[i]));
          i++;
        }
        printf("\n");
        exit(0);
      } 
    } 

    if (args[0][0] != '.' && args[0][1] != '/') {
      char *abs_path = findBin(args[0], cwd, paths);
      if (abs_path == NULL) {
        fprintf(stderr, "Error: %s: command not found.\n", args[0]);
        exit(1);
      } else {
        args[0] = abs_path;
      }
    }
  
    int exec_status = execv(args[0], args);
    fprintf(stderr, "Error: %s command fails with exit code: %d\n", args[0], exec_status);
    exit(exec_status);
  }
    
  if (input != 0) close(input);
 
  // Nothing more needs to be written
  close(command_pipe[1]);
 
  // If it's the last command, nothing more needs to be read
  if (pipeIndicator == 2) close(command_pipe[0]);

  int child_status;
  if (waitpid(pid, &child_status, 0) == -1) {
    fprintf(stderr, "Error: waitpid failed.");
  } else {
    if (WIFEXITED(child_status)) {
      return_code = WEXITSTATUS(child_status);
      if (debugging) fprintf(stderr, "ENDED: %s (ret=%d)\n", args[0], return_code);
    }
  }
  
  return command_pipe[0];
}

/**
 * Method for running a single raw command.
 * Returns input fd for the next command.
 */
int run(char *cmd, int input, int pipeIndicator) {
  cmd = stripLeadingSpaces(cmd);
  char ** args;
  args = parseLine(cmd, ARGS_DELIM, DEFAULT_ARGS_NUM);
  if (debugging) fprintf(stderr, "Debug Info: command successfully read and parsed.\n");
  if (strcmp(args[0], "exit") == 0) exit(0);
    
  if (strcmp(args[0], "cd") == 0) {
    if (debugging) fprintf(stderr, "RUNNING: %s\n", args[0]);
    char cwd_temp[MAX_INPUT];
    if (getcwd(cwd_temp, sizeof(cwd_temp)) == NULL) {
      fprintf(stderr, "Error: getting current directory.\n");
      exit(1);
    }
    if (args[1] == NULL) {
      strcpy(last_cwd, cwd_temp);
      chdir(getenv("HOME"));
      return_code = 0;
    } else if (strcmp(args[1], "-") == 0) {
      if (chdir(last_cwd) < 0) {
        fprintf(stderr, "Error: no such directory %s\n", last_cwd);
        return_code = 1;
      } else {
        return_code = 0;
      }
      memset(last_cwd, sizeof(last_cwd), 0);
    } else {
      char targetDir[MAX_INPUT];
      memset(targetDir, sizeof(targetDir), 0);

      if (args[1][0] == '~') {
        strcpy(targetDir, getenv("HOME"));
        strcat(targetDir, args[1] + 1);
      } else {
        strcpy(targetDir, args[1]);
      }

      if (chdir(targetDir) < 0) {
        fprintf(stderr, "Error: no such directory %s\n", args[1]);
        return_code = 1;
      } else {
        strcpy(last_cwd, cwd_temp);
        return_code = 0;
      }
    }
    if (debugging) fprintf(stderr, "ENDED: %s (ret=%d)\n", args[0], return_code);
    return input;
  } 

  if (strcmp(args[0], "set") == 0) {
    if (debugging) fprintf(stderr, "RUNNING: %s\n", args[0]);
    char **var_info = parseLine(args[1], "=", 2);
    if (var_info[0] != NULL && var_info[1] != NULL) {
      int index = getVar(var_info[0]);
      if (index >= 0) {
        strcpy(var_vals[index],var_info[1]);
        return_code = 0;
      } else {
        if (checkSymbol(var_info[0]) == 0) {
          addVariable(var_info[0], var_info[1]);
          if (debugging) fprintf(stderr, "New variable registered\n");
          var_count++;
          return_code = 0;
        } else {
          fprintf(stderr, "Error: %s is not a vaild variable name.\n", var_info[0]);
          return_code = 1;
        }
      }
    } else {
      fprintf(stderr, "Error: SET syntax is wrong, try 'set variable_name=variable_value'.\n");
      return_code = 1;
    }
    if (debugging) fprintf(stderr, "ENDED: %s (ret=%d)\n", args[0], return_code);
    return input; 
  }

  return execute(args, input, pipeIndicator);
}

int main (int argc, char ** argv, char **envp) {

  int finished = 0, interactive = 1, i;
  debugging = 0;
  char *prompt = "thsh> ";
  char cmd[MAX_INPUT];
  var_keys = malloc(sizeof(char *) * MAX_VAR);
  var_vals = malloc(sizeof(char *) * MAX_VAR);
  var_count = 0;
  char *path = getenv("PATH");
  job_count = 0;

  i = 0;
  while (envp[i] != NULL) {
    char *env_key = malloc(MAX_INPUT);
    int len = 0;
    while (envp[i][len] != '=') {
      env_key[len] = envp[i][len];
      len++;
    }

    env_key[len] = '\0';
    var_keys[var_count] = env_key;
    var_vals[var_count] = getenv(env_key);
    var_count++;
    i++;
  }

  if (getcwd(cwd, sizeof(cwd)) == NULL) {
    fprintf(stderr, "Error: getting current directory.\n");
    exit(1);
  }

  i = 1;
  int scFd = -1;
  while (argv[i] != NULL) {
    if (strcmp(argv[i], "-d") == 0) {
      debugging = 1;
      fprintf(stderr, "Using Debug Mode\n");
    } else {
      interactive = 0;
      char scriptDir[MAX_INPUT];
      strcpy(scriptDir, cwd);
      strcat(scriptDir, "/");
      strcat(scriptDir, argv[i]);
      if (debugging) printf("scriptDir: %s\n", scriptDir);
      scFd = open(scriptDir, O_RDONLY);
      if (scFd < 0) {
        fprintf(stderr, "Error: couldn't find file %s\n", scriptDir);
        exit(1);
      }

      dup2(scFd, 0);
    }
    i++;
  }  

  // Make a throwaway copy of getenv("PATH") so that strtok will not compromise it.
  char throwaway_path[MAX_INPUT];
  strcpy(throwaway_path, path);
  paths = parseLine(throwaway_path, PATH_DELIM, DEFAULT_PATH_NUM);

  while (!finished) {
    char *cursor;
    char last_char;

    int rv;
    int count;
    int background =0;

    memset(cwd, sizeof(cwd), 0);
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
      fprintf(stderr, "Error: getting current directory.\n");
      exit(1);
    } 

    // Print the prompt
    if (interactive) {
      printf("[%s] %s", cwd, prompt);
      fflush(stdout);
    }
    // read and parse the input
    for(rv = 1, count = 0, cursor = cmd, last_char = 1;
	    rv && (++count < (MAX_INPUT-1)) && (last_char != '\n');
	    cursor++) { 

      rv = read(0, cursor, 1);
      last_char = *cursor;
    }
    *cursor = '\0';

    if (!rv) { 
      finished = 1;
      break;
    }

    if (cmd[0] != '#') {
      if (cmd[strlen(cmd) - 1] == '&') {
        background = 1;
      }
      char *abs_cmd = substituteVars(cmd);
      if (abs_cmd == NULL) {
        fprintf(stderr, "Error: unregistered variable found.\n");
      } else {
        char **commands = parseLine(abs_cmd, "|" , DEFAULT_PIPE_NUM);

        // The input fd for each command. set to 0(stdin) for the first one.
        int input = 0;
        int pipelen = 0;
        while (commands[pipelen] != NULL) pipelen++;
        if (pipelen == 1) {
          if (debugging) fprintf(stderr, "Running first/single command\n");
          input = run(commands[0], input, 2);
        } else {
          input = run(commands[0], input, 0);
          for (i = 1; i < pipelen - 1; i++) {
            if (debugging) fprintf(stderr, "Running %d-th command\n", i + 1);
            input = run(commands[i], input, 1);
          }
          if (debugging) fprintf(stderr, "Running last command.\n");
          input = run(commands[pipelen - 1], input, 2);
        }
      }
    }
  }

  if (scFd > 0) close(scFd);
  free(var_keys);
  free(var_vals);
  return 0;
}
