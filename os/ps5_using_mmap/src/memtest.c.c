/*
 * memtest.c
 * Author: Stanley George
 * ECE 460: Computer Operating Systems
 * Professor Jeff Hakner
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <signal.h>

#define FILESIZE_ABC 64    //64 bytes shall be our testfile size for cases A thru C
#define FILESIZE_DE  8195  //8195 bytes shall be our testfile size for cases D and E
#define FILESIZE_F   10    //10 bytes shall be our testfile size for case F
#define TRUE         1
#define FALSE        0

static char *testfileA = "/home/george/Desktop/ECE460/PS5/testA.txt";
static char *testfileBC = "/home/george/Desktop/ECE460/PS5/testBC.txt";
static char *testfileDE = "/home/george/Desktop/ECE460/PS5/testDE.txt";
static char *testfileF = "/home/george/Desktop/ECE460/PS5/testF.txt";
static struct stat st;
static char *addr;
static int fd;

void answerA();
void answerBC(int flag);
void answerD();
void answerE(int fd, int new_size);
void answerF();
void dump(int fd, int offset, char addr[], char buffer[], int bufsiz);
int openfile(char *fn, int acc_flags, int mode_flags, int fsize);
int closefile(int fd);
void error(const char *msg, int fd);
void signal_handler(int);


int main(int argc, char *argv[]) {
  printf("num args = %d\n", argc);
  if(argc != 2) { fprintf(stderr, "usage: memtest A/B/C/D/E/F\n"); exit(1); }
  int i;
  for(i = 1; i < 32; i++) signal(i, signal_handler);
  char q = *(argv[1]);
  switch(q) {
    case 'A':
      printf("Answering question A ...\n");
      answerA();
      printf("\n");
      break;
    case 'B':
      printf("Answering question B ...\n");
      answerBC(MAP_SHARED);
      printf("\n");
      break;
    case 'C':
      printf("Answering question C ...\n");
      answerBC(MAP_PRIVATE);
      printf("\n");
      break;
    case 'D':
      printf("Answering question D ...\n");
      answerD();
      printf("\n");
      break;
    case 'E':
      printf("Answering question E ...\n");
      printf("First answering question D ...\n");
      answerD();
      printf("\n");
      break;
    case 'F':
      printf("Answering question F ...\n");
      answerF();
      printf("\n");
      break;
    default:
      error("memtest: Invalid test option", -1);
      break;
  }
  return 0;
}


void answerA() {
  fd = openfile(testfileA, O_RDWR | O_CREAT | O_TRUNC, 0666, FILESIZE_ABC);
  if(fstat(fd, &st) == -1) error("memtest: fstat", fd);
  printf("The size of %s as reported by fstat is %d bytes\n", testfileA, (int) st.st_size);
  printf("Now mapping image of %s into memory via mmap() ", testfileA);
  printf("with PROT_READ and MAP_PRIVATE ...\n");
  if((addr = (char *) mmap((void *) NULL, (size_t) st.st_size, PROT_READ,
    MAP_PRIVATE, fd, 0)) == MAP_FAILED)
    error("memtest: mmap", fd);
  printf("Now writing value 0x01 at first byte of mapped memory ...\n");
  *addr = 0x01;
  closefile(fd);
}

void answerBC(int flag) {
  int i, change_is_visible = TRUE;
  char *_flag;
  char *_case;
  if(flag == MAP_PRIVATE) { _flag = "MAP_PRIVATE"; _case = "C"; }
  else { _flag = "MAP_SHARED"; _case = "B"; }
  fd = openfile(testfileBC, O_RDWR | O_CREAT | O_TRUNC, 0666, FILESIZE_ABC);
  if(fstat(fd, &st) == -1) error("memtest: fstat", fd);
  printf("The size of %s as reported by fstat is %d bytes\n", testfileBC, (int) st.st_size);
  printf("Now mapping image of %s into memory via mmap() ", testfileBC);
  printf("with PROT_READ | PROT_WRITE and %s ...\n", _flag);
  if((addr = (char *) mmap((void *) NULL, (size_t) st.st_size, PROT_READ|PROT_WRITE,
    flag, fd, 0)) == MAP_FAILED)
    error("memtest: mmap", fd);
  printf("Now writing 64 bytes to mapped memory region starting at offset 0 ...\n");
  for(i = 0; i < FILESIZE_ABC; i++) addr[i] = 'A';
  char buffer[FILESIZE_ABC];
  printf("Now reading from file %s into buffer of %d bytes ...\n", testfileBC, FILESIZE_ABC);
  //if(read(fd, buffer, FILESIZE_ABC) == -1) error("memtest: read", fd);
  dump(fd, 0, addr, buffer, (int)st.st_size);
  for(i = 0; i < FILESIZE_ABC; i++) {
    if(addr[i] != buffer[i]) {
      change_is_visible = FALSE;
      break;
    }
  }
  printf("In response to question %s, when a file is mapped with %s ", _case, _flag);
  printf("and then writes to the mapped memory,\n");
  if(change_is_visible == TRUE) printf("  the update IS ");
  else printf("  the update IS NOT ");
  printf("visible when accessing the file through the read() system call\n");
  closefile(fd);
}

void answerD() {
  int i;
  char buffer[4];
  fd = openfile(testfileDE, O_RDWR | O_CREAT | O_TRUNC, 0666, FILESIZE_DE);
  if(fstat(fd, &st) == -1) error("memtest: fstat", fd);
  int init_size = (int) st.st_size;
  printf("The initial size of %s as reported by fstat is %d bytes\n", testfileDE, init_size);
  printf("Now mapping image of %s into memory via mmap() ", testfileDE);
  printf("with PROT_READ | PROT_WRITE and MAP_SHARED ...\n");
  if((addr = (char *) mmap((void *) NULL, (size_t) st.st_size, PROT_READ|PROT_WRITE,
    MAP_SHARED, fd, 0)) == MAP_FAILED)
    error("memtest: mmap", fd);
  printf("Now writing 4 bytes to memory map starting at byte offset %d ...\n", init_size);
  for(i = 0; i < 4; i++)
    addr[i + init_size] = (char) (i + 1);
  if(fstat(fd, &st) == -1) error("memtest: fstat", fd);
  int new_size = (int) st.st_size;
  printf("The new size of %s as reported by fstat is %d bytes\n", testfileDE, new_size);
  dump(fd, FILESIZE_DE, addr, buffer, 4);
  printf("In response to question D, the file ");
  if(new_size == init_size) {
    printf("HAS NOT changed size\n");
    answerE(fd, new_size);
  }
  else {
    printf("HAS changed size\n");
    if(munmap(addr, FILESIZE_DE) == -1) error("memtest: munmap", fd);
    closefile(fd);
  }
}

void answerE(int fd, int new_size) {
  char buffer[16];
  int i, data_is_visible = TRUE;
  printf("Now expanding %s by 13 bytes and write(2) to new end of file ...\n", testfileDE);
  if(lseek(fd, new_size + 12, SEEK_SET) ==  -1) error("memtest: lseek", fd);
  char c = 'A';
  if(write(fd, &c, 1) != 1) error("memtest: could not write last byte of file", fd);
  dump(fd, new_size, addr, buffer, 16);
  for(i = 0; i < 4; i++)
    if(buffer[i] != addr[new_size + i]) {
      data_is_visible = FALSE;
      break;
    }
  printf("In response to question E, the data previously written to the hole ");
  if(data_is_visible == TRUE)
    printf("ARE visible\n");
  else printf("ARE NOT visible\n");
  if(munmap(addr, new_size + 12) == -1) error("memtest: munmap", fd);
  closefile(fd);
}

void answerF() {
  char val;
  fd = openfile(testfileF, O_RDWR | O_CREAT | O_TRUNC, 0666, FILESIZE_F);
  printf("Now verifying file size ...\n");
  if(fstat(fd, &st) == -1) error("memtest: fstat", fd);
  printf("The size of %s as reported by fstat is %d bytes\n",
    testfileF, (int) st.st_size);
  printf("Now mapping image of %s into memory via mmap() ", testfileF);
  printf("with PROT_READ | PROT_WRITE and MAP_SHARED ...\n");
  if((addr = (char *) mmap((void *) NULL, 8192, PROT_READ|PROT_WRITE,
    MAP_SHARED, fd, 0)) == MAP_FAILED)
    error("memtest: mmap", fd);
  printf("About to access memory map at byte offset 4000 ");
  printf("which is in the first page ...\n");
  val = addr[4000];
  /*
    This is a valid test because if no signal was generated then we will
    surely go to the next line of code
  */
  printf("No signal was generated after accesing first page\n");
  printf("About to access memory map at byte offset 8000 ");
  printf("which is in the second first page ...\n");
  val = addr[8000];
  printf("No signal was generated after accessing second page\n");
  if(munmap(addr, 8192) == -1) error("memtest: munmap", fd);
  closefile(fd);
}

void signal_handler(int signum) {
  if(signum == SIGSEGV) printf("memtest: Received signal SIGSEGV\n\n");
  else if(signum == SIGBUS) printf("memtest: Received signal SIGBUS\n\n");
  else psignal(signum, "memtest");
  exit(-1);
}

void dump(int fd, int offset, char addr[], char buffer[], int bufsiz) {
  int i, read_val;
  if(lseek(fd, offset, SEEK_SET) == -1) error("memtest: lseek", fd);
  if((read_val = read(fd, buffer, bufsiz)) == -1) error("memtest: read", fd);
   printf("File dump starting at offset %d:\n", offset);
  for(i = 0; i < read_val; i++)
    printf("<0x%.2X> ", buffer[i]);
  printf("<EOF>");
  printf("\nMemory dump starting at offset %d for %d bytes:\n", offset, bufsiz);
  for(i = offset; i < offset + bufsiz; i++) printf("<0x%.2X> ", addr[i]);
  printf("\n");
}

int openfile(char *fn, int acc_flags, int mode_flags, int fsize) {
  int fd;
  printf("Creating %d byte random test file %s ...\n", fsize, fn);
  if((fd = open(fn, acc_flags, mode_flags)) == -1) error("memtest: open", -1);
  if(lseek(fd, fsize - 1, SEEK_SET) == -1) error("memtest: lseek", fd);
  char c = 'A';
  if(write(fd, &c, 1) != 1) error("memtest: could not write last byte of file", fd);
  if(lseek(fd, 0, SEEK_SET) == -1) error("memtest: lseek", fd);
  return fd;
}

int closefile(int fd) {
  if(close(fd) == -1) error("memtest: close", -1);
  return 0;
}

void error(const char *msg, int fd) {
  perror(msg);
  if(fd >= 0) closefile(fd);
  exit(-1);
}
