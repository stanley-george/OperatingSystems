/*
 * catgrepmore.c
 * Author: Stanley George
 * ECE 460: Computer Operating Systems
 * Professor Jeff Hakner
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <setjmp.h>

#define BUFSIZE   4096
#define READ_END  0     /*pipe[0] is reading end*/              
#define WRITE_END 1     /*pipe[1] is writing end*/
#define STDIN     0
#define STDOUT    1

void inthandler(int signum);         
int *openpipe(int pfds[2]);       
int readwrite(int ifd, int ofd);
int closefd(int fd);
int redirect(int old_fd, int new_fd);
int catgrepmore(char *fname, int fds1[2], int fds2[2]);
void pchildstatus(pid_t child_pid, int status, char *n);

char *pattern = "";
long long BYTES_READ = 0, BYTES_WRITTEN = 0, N_INFILES = 0;

int main(int argc, char *argv[]) {
  if (argc < 3) {
    fprintf(stderr, "usage: catgrepmore pattern infile1 [...infile2...]\n");
    exit(1);
  }
 
  pattern = argv[1];                 

  if(signal(SIGINT, inthandler) == SIG_ERR) 
    fprintf(stderr, "catgrepmore: signal: failed to catch SIGINT\n");
  if(signal(SIGPIPE, SIG_IGN) == SIG_ERR) 
    fprintf(stderr, "catgrepmore: signal: failed to catch SIGPIPE\n");

  for(int i = 2; argv[i] != NULL; i++) {
    fprintf(stderr, "THE CURRENT INFILE IS: %s\n", argv[i]);
    int fds1[2], fds2[2];
    N_INFILES++;
    openpipe(fds1);            /* open pipe between parent and grep: fds1 */
    openpipe(fds2);            /* open pipe between grep and more: fds2 */
    catgrepmore(argv[i], fds1, fds2);
  }
 
  return 0;
}

int catgrepmore(char *fname, int fds1[2], int fds2[2]) {
  int ifd, grep_status, more_status;
   /* This is essentially how the pipes are set up:
            --------------      -------------
      parent     PIPE1     grep     PIPE2    more
            --------------      -------------
            ^            ^      ^           ^
          Write         Read  Write        Read
  */
  //CHILD: grep
  pid_t grep_pid = fork();         
  if(grep_pid == -1) { perror("fork"); exit(1); }
  if(grep_pid == 0) {
    closefd(fds1[WRITE_END]);            /* close write end of pipe1 */            
    closefd(fds2[READ_END]);             /* close read end of pipe2 */  
    redirect(fds1[READ_END], STDIN);     /* redirect stdin to read end of pipe1, then close read end */ 
    redirect(fds2[WRITE_END], STDOUT);   /* redirect stdout to write end of pipe2, then close write end */
    execlp("grep", "grep", pattern, (char *) NULL);
    perror("execlp: grep");
    exit(1);
  }
  
  pid_t more_pid = fork();
  if(more_pid == -1) { perror("fork"); exit(1); }
  //CHILD: more
  if(more_pid == 0) {
    closefd(fds1[WRITE_END]);            /* close write end of pipe1 */
    closefd(fds1[READ_END]);             /* close read end of pipe1 */
    closefd(fds2[WRITE_END]);            /* close write end of pipe2 */
    redirect(fds2[READ_END], STDIN);     /* redirect stdin to read end of pipe2, then close read end */
    execlp("more", "more", (char *) NULL);
    perror("execlp: more");
    exit(1);
  }
 
  //PARENT
  if(grep_pid != 0 && more_pid != 0) {
    if((ifd = open(fname, O_RDONLY, 0666)) == -1) {
      fprintf(stderr, "open: Can't open %s for reading: %s\n", fname, strerror(errno));
      exit(1);
    }
    closefd(fds1[READ_END]);              /* close read end of pipe1 */
    closefd(fds2[WRITE_END]);             /* close write end of pipe2 */
    closefd(fds2[READ_END]);              /* close read end of pipe2 */
    readwrite(ifd, fds1[WRITE_END]);      /* fill read end of pipe1 with data from infile */
    closefd(fds1[WRITE_END]);             /* close write end of pipe1 cuz we're done with it */
    closefd(ifd);                         /* close input fd cuz we're done with it */
    grep_pid = waitpid(grep_pid, &grep_status, 0); 
    more_pid = waitpid(more_pid, &more_status, 0);
    pchildstatus(grep_pid, grep_status, "grep"); /* report exit status of grep */
    pchildstatus(more_pid, more_status, "more"); /* report exit status of more */
  }

  return 0;
}

int redirect(int old_fd, int new_fd) {
  if(dup2(old_fd, new_fd) == -1) { perror("catgrepmore: dup2"); exit(1); }
  if(close(old_fd) == -1) { perror("catgrepmore: close"); exit(1); }
  return 0;
}

void inthandler(int signum) {
  if(signum == SIGPIPE) { }    /*do nothing here; using SIG_IGN causes a write error in grep*/
  else {
    fprintf(stderr, "\n\nReceived signal SIGINT\n");
    fprintf(stderr, "Number of input files processed = %lld\n", N_INFILES);
    fprintf(stderr, "Bytes read = %lld\nBytes written = %lld\n",
      BYTES_READ, BYTES_WRITTEN);
    exit(1);
  }
}

int *openpipe(int pfds[2]) {
  if(pipe(pfds) == -1) { perror("catgrepmore: Can't create pipe"); exit(1); }
  return pfds;
}

int closefd(int fd) {
  if(close(fd) == -1) { perror("catgrepmore: close"); exit(1); }  
  return 0;
}

int readwrite(int ifd, int ofd) {
  int n_read = 0, n_write = 0;
  char buffer[BUFSIZE];

  while((n_read = read(ifd, buffer, BUFSIZE)) > -2) {
    if(n_read == -1) { perror("catgrepmore: read"); exit(1); }
    else if(n_read == 0) break;
    else {
      if((n_write = write(ofd, buffer, n_read)) <= 0) {
        if(errno == EPIPE) break;
        else { perror("catgrepmore: write"); exit(1); }
      }
      else if(n_write > 0 && n_write < n_read) { perror("catgrepmore: partial write"); exit(1); }
      BYTES_READ += n_read;
      BYTES_WRITTEN += n_write;
    }
  }
  return 0;
}

void pchildstatus(pid_t child_pid, int status, char *n) {
  if(child_pid == -1) 
    fprintf(stderr, "catgrepmore: error waiting for child process: %s: %s\n",
      n, strerror(errno));
  else if(status != 0) {
    if(WIFSIGNALED(status)) {
      if(WTERMSIG(status) != SIGPIPE)
        fprintf(stderr, "catgrepmore: %s: child process with pid=%d exited with signal %d\n",
          n, (int) child_pid, WTERMSIG(status));
    }
    else
      fprintf(stderr, "catgrepmore: %s: child process with pid=%d exited with nonzero value %d\n",
        n, (int) child_pid, WEXITSTATUS(status));
  }
  else
    fprintf(stderr, "catgrepmore: %s: child process with pid=%d exited normally\n",
      n, (int) child_pid);
}
