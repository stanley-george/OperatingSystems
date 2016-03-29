/*
  shell.c
  Author: Stanley George
  ECE 460: Computer Operating Systems
  Professor Jeff Hakner
*/

#include <stdio.h>          //for printf(), fprintf(), stderr, stdin, etc...
#include <stdlib.h>         //for exit()
#include <string.h>         //for strtok()
#include <errno.h>          //for global variable errno
#include <fcntl.h>          //for O_XXX constants
#include <unistd.h>         //for fork() and execvp()
#include <sys/types.h>      //for typedef'ed types like pid_t
#include <sys/wait.h>       //for wait(), WIFEXITED(), WEXITSTATUS()
#include <sys/resource.h>   //for getrusage(), struct rusage
#include <sys/time.h>       //for struct timeval
  
#define MAX_CMD_CHARS 256   //max # of chars we expect from input
#define MAX_TOKENS     32   //max # of tokens we expect after tokenizing input
#define NO_OP           0   //no redirection operation for current command
#define L               1   // < open file and redirect stdin 
#define R               2   // > open/create/truncate file and redirect stdout
#define RR              3   // >> open/create/append file and redirect stdout
#define R2              4   // 2> open/create/truncate and redirect stderr
#define RR2             5   // 2>> open/create/append and redirect stderr

static char *f_table[3] = { NULL, NULL, NULL };
static int op_table[3] = { NO_OP, NO_OP, NO_OP };
static unsigned char NUM_REDIRS = 0;
static unsigned char IS_SCRIPT = 0;
struct timeval start, end;

inline void error(const char *msg);
unsigned int getinput(char *buf);
unsigned int tokenize(char *buf, char **cmdv);
unsigned int flushcmdv(char **cmdv, int n);
unsigned int checkredir(char *cmd, char *operator, int stream, int off, int op_type);
int redirect(char *f_table[], int op_table[]);
inline void clrtables();
inline void clrbuf(char *buf);
void pdebuginfo(char *cmdv[], char *f_table[], int op_table[], int n);
void pprocessinfo(char *cmdv[], pid_t pid, int status, struct rusage usage);

int main(int argc, char *argv[], char *envp[]) {
  char buf[MAX_CMD_CHARS];       //buffer containing input string
  char *cmdv[MAX_TOKENS + 1];    //command vector of ptrs to parsed tokens

  if(argc == 2) {                //argc = 2 --> process script file
    IS_SCRIPT = 1;
    f_table[0] = argv[1];
    op_table[0] = L;
    redirect(f_table, op_table); //redirect script to stdin
    clrtables();                 //reset the tables so everything is clean
  }

  while(getinput(buf)) {
    int n_tokens = tokenize(buf, cmdv);
    if(n_tokens > 0) {
        int status;
        gettimeofday(&start, NULL);
        struct rusage usage;
        pid_t pid = fork();
        switch(pid) {
          case -1:  //ERROR
            perror("shell: fork");
            _exit(-1);
            break;
          case 0:   //CHILD PROCESS
            redirect(f_table, op_table);
            execvp(*cmdv, cmdv);
            perror("shell: execvp"); //if we get here, there was an error in execvp()
            _exit(1);
            break;
          default:  //PARENT PROCESS
            if(wait3(&status, 0, &usage) == -1) perror("shell: wait3");
            else {
              if(status != 0) {
                if(WIFSIGNALED(status)) 
                  fprintf(stderr, "shell: %s exited with signal %d\n", 
                    cmdv[0], WTERMSIG(status));
                else
                  fprintf(stderr, "shell: %s exited with nonzero value %d\n",
                    cmdv[0], WEXITSTATUS(status));
              }
              else
                pprocessinfo(cmdv, pid, WEXITSTATUS(status), usage);
            }
            flushcmdv(cmdv, n_tokens); 
            clrtables();
            NUM_REDIRS = 0;
            break;
        }
    }
  }

  return 0;
}

inline void error(const char *msg) {
  fprintf(stderr, "%s\n", msg);
}

inline void clrbuf(char *buf) {
  int i;
  for(i = 0; i < MAX_CMD_CHARS; i++)
    *(buf + i) = '\0';
}

unsigned int getinput(char *buf) {
  if(IS_SCRIPT == 0) printf("$ simple shell prompt > ");
  char i, c;
  for(i = 0; i < MAX_CMD_CHARS && (c = getchar()); i++) {
    switch(c) {
      case '\n':
        *(buf + i) = '\0';
        return 1;
      case EOF:
        printf("\n");
        return 0;
      default:
        *(buf + i) = c;
        break;
    }
  }
}

unsigned int tokenize(char *buf, char **cmdv) {
  int ntok = 0;
  char delims [2] = {" \t"}; 
  char *cmd = strtok(buf, delims);
  if(!cmd) 
    return ntok;
  ntok++;
  *(cmdv + (ntok - 1)) = cmd;     //first token is our command
  if(strncmp(cmd, "#", 1) == 0)   //check for leading #
    return 0;
  while((cmd = strtok(NULL, delims)) && ntok < MAX_TOKENS && NUM_REDIRS <= 3) {
    if(checkredir(cmd, "<", 0, 1, L)) continue;
    else if(checkredir(cmd, "2>>", 2, 3, RR2)) continue;
    else if(checkredir(cmd, ">>", 1, 2, RR)) continue;
    else if(checkredir(cmd, "2>", 2, 2, R2)) continue;
    else if(checkredir(cmd, ">", 1, 1, R)) continue;
    else {
      ntok++;
      *(cmdv + (ntok - 1)) = cmd;
    }
  }

  if(NUM_REDIRS > 3) {
    NUM_REDIRS = 0; //reset this flag
    error("shell: too many redirections attempted");
    return 0;    //returning 0 will cause program to skip forking
  }

  *(cmdv + ntok) = NULL;
  return ntok;
}

unsigned int checkredir(char *cmd, char *operator, int stream, int off, int op_type) {
  char *p = NULL;
  if((p = strstr(cmd, operator)) != NULL) { //operator string is substring of cmd
    f_table[stream] = (p + off);            //store the file name to which the operator applies 
    op_table[stream] = op_type;             //store the operator type
    NUM_REDIRS++;                           //update redirection operator count
    return 1;
  }
  return 0;
}

unsigned int flushcmdv(char **cmdv, int n) {
  int i = 0;
  while(n-- > 0) {
    *(cmdv + i) = NULL;
    i++;
  }
  return 0;
}

int redirect(char *f_table[], int op_table[]) {
  int i;
  if(NUM_REDIRS > 0 && NUM_REDIRS < 4 || IS_SCRIPT == 1) {
    for(i = 0; i < 3; i++) {
      int fd;
      int stream = i;
      if(op_table[i] != NO_OP && f_table[i] != NULL) {
        switch(op_table[i]) {
          case L:
            fd = open(f_table[i], O_RDONLY);
            break;
          case R2:  //fall thru
          case R:
            fd = open(f_table[i], O_WRONLY | O_CREAT | O_TRUNC, 0666);
            break;
          case RR2: //fall thru
          case RR:
            fd = open(f_table[i], O_WRONLY | O_CREAT | O_APPEND, 0666);
            break;
        }
        if(fd < 0) { perror("shell: open"); _exit(1); }
        if(dup2(fd, stream) < 0) { perror("shell: dup2"); _exit(1); }
        if(close(fd) == -1) { perror("shell: close"); _exit(1); }
      }
    }
  }
  return 0;
}

inline void clrtables() {
  int i;
  for(i = 0; i < 3; i++) {
    f_table[i] = NULL;
    op_table[i] = NO_OP;
  }
}

void pprocessinfo(char *cmdv[], pid_t pid, int status, struct rusage usage) {
  gettimeofday(&end, NULL);
  printf("\nFinished executing command: %s ", cmdv[0]);
  if(cmdv[1] == NULL) printf("w/ arguments: NONE\n");
  else {
    printf("w/ arguments: ");
    int i;
    for(i = 1; cmdv[i] != NULL; i++) printf("\"%s\"  ", cmdv[i]);
    printf("\n");
  }
  printf("PID: %d\tExit Status: %d\n", (int) pid, status);
  int real_sec = end.tv_sec - start.tv_sec;
  long int real_micro_sec = end.tv_usec - start.tv_usec;
  if(real_micro_sec < 0) {
    real_micro_sec += 1000000;
    real_sec -= 1;
  }
  printf("Real: %1d.%.06ld seconds  ", real_sec, real_micro_sec);
  printf("User: %ld.%06ld seconds  ", usage.ru_utime.tv_sec, usage.ru_utime.tv_usec);
  printf("System: %ld.%06ld seconds\n", usage.ru_stime.tv_sec, usage.ru_stime.tv_usec);
  printf("\n");
}
