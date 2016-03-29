/*
  walkfs.c
  Author: Stanley George
  ECE 460: Computer Operating Systems
  Professor Jeff Hakner
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <pwd.h>  
#include <grp.h>    

#define GOOD_SIZE 16
#define MAX_OPEN_FILES 1024

static int SET_DEV_NUM = 1;
static int EVIL_LOOP = 0;
static long INO_TABLE [MAX_OPEN_FILES];
static char *PATH_TABLE[MAX_OPEN_FILES];
static long DEV_TABLE[MAX_OPEN_FILES];
static int TAB_INDEX = 0;
static int LOOP_DETECTED = 0;
static int LAST_SEEN = 0;

/* 
  globalArgs is a structure containing fields whose initializations depend on which
  options are specified when this program is invoked from the command line. This 
  structure is set its default state by the the function init_globalArgs() 
*/
static struct globalArgs_t {
  char *path;           //string containing pathname
  char *user;           //for -u opt: string containing specified user
  double mtime;         //for -m opt: last modified time
  char *target;         //for -l opt: string containing specified target
  dev_t init_mnt_dev;   //this is the device number of the starting filesystem
  int flags;
} globalArgs;
/*
  A structure containing the information from the structure returned by the stat()
  family of system calls
*/
struct i_stat {
  ino_t ino_n;        //inode number
  dev_t ino_dev;      //inode device
  time_t ino_tmod;    //inode last modified time
  char *ino_grp;      //inode group
  char *ino_usr;      //inode user
  char *ino_size;     //inode size in bytes
  int ino_nlinks;     //number of links to inode
  char *ino_perms;    //inode file type and permissions
  char *ino_fpath;    //inode path
  char *ino_link;     //inode link content
  int dontprint;
};
/*
  opt_flags defines 4 hexidecimal constants. Its purpose is to flag the use of the
  -u -m -l and -x options that may be used when invoking this program.
*/
enum opt_flags {
  NO_FLAG = 0x00,
  U_FLAG  = 0x01,
  M_FLAG  = 0x02,
  L_FLAG  = 0x04,
  X_FLAG  = 0x08
};
/*
  init_globalArgs() sets the default values of the fields in globalArgs
*/
inline void init_globalArgs();
inline void init_TABLES();

/*
  parse_opt() calls the library function getopt() to help it parse the various 
  optional parameters (u, l, m, x). For each option, parse_opts() initializes the
  appropriate field in the globalArgs and sets the appropriate bit flag in 
  globalArgs.flags. For example, if the the -u option is specified, then parse_opts()
  will initialize globalArgs.user to the argument supplied with the -u and then it 
  will turn on the U_FLAG bit in globalArgs.flags 
*/
int parseopts(int argc, char *argv[]);
/*
  error() sends the error message given by msg to stderr and then calls exit(-1)
*/
void error(char *msg);
/*
  dirwalk(). Version 1.0 was a success =)
  Version 1.1 will attempt to integrate dirwalk with pstat()
*/
int dirwalk(struct stat *st_buf, char *path);
/*
  pstat() prints metadata information. The exact content is determined by which options
  are flaged in globalArgs.flags
*/
int pstat(struct stat *st_buf, char *path);
/*
  getfperms() gets the file permissions associated with path
*/
inline int getfperms(mode_t mode, char *pm);
/*
  getftype() get the file type
*/
inline int getftype(mode_t mode, char *ftype);
inline int getfusr(uid_t uid, char *u);
inline int getfgrp(gid_t gid, char *g);
inline char *getlnktarget(struct stat *st_buf, struct i_stat *info_stat);
void print_info(struct i_stat *info_stat);
inline int chkuser(const char *user);
struct i_stat *new_istat(struct i_stat *info_stat);
struct i_stat *set_istat(struct i_stat *info_stat, struct stat *st_buf, char *fpath);
inline int destroy_istat(struct i_stat *info_stat);
inline void destroy_TABLES();

int main(int argc, char *argv[]) {
  struct stat st_buf;
  init_globalArgs();
  init_TABLES();
  parseopts(argc, argv);
  dirwalk(&st_buf, globalArgs.path);
  /*
    free memory that may have been allocated by parseopts()
  */
  if(globalArgs.user != NULL)
    free(globalArgs.user);
  if(globalArgs.target != NULL)
    free(globalArgs.target);
  if(globalArgs.path != NULL)
    free(globalArgs.path);

  //destroy_TABLES();
  return 0;
}

inline void error(char *msg) {
  fprintf(stderr, "%s\n", msg);
  exit(-1);
}

int parseopts(int argc, char *argv[]) {
  int opt;
  
  if(argc == 1) error("walkfs: invalid usage: walkfs [options] path");
  while((opt = getopt(argc, argv, "u:m:l:x")) != -1) {      //loop thru optional parameters
    switch(opt) {
      case 'u':
        globalArgs.user = strdup(optarg);
        globalArgs.flags |= U_FLAG;
        break;
      case 'm':
        globalArgs.mtime = atoi(optarg);
        if(globalArgs.mtime == 0) error("walkfs: mtime cannot be 0");
        globalArgs.flags |= M_FLAG;
        break;
      case 'l':
        globalArgs.target = strdup(optarg);
        globalArgs.flags |= L_FLAG;
        break;
      case 'x':
        globalArgs.flags |= X_FLAG;
        break;
    }
  }
  if(!argv[optind]) error("walkfs: did not specify path");
  globalArgs.path = strdup(argv[optind]);
  return 0;
}

inline void init_globalArgs() {
  globalArgs.path = NULL;
  globalArgs.user = NULL;
  globalArgs.mtime = 0;
  globalArgs.target = NULL;
  globalArgs.flags = NO_FLAG;
}

inline void init_TABLES() {
  int i;
  for(i = 0; i < MAX_OPEN_FILES; i++) {
    INO_TABLE[i] = 0;
    DEV_TABLE[i] = 0;
    PATH_TABLE[i] = NULL;
  }
}

inline void destroy_TABLES() {
  int i;
  for(i = 0; i < MAX_OPEN_FILES; i++)
    if(PATH_TABLE[i] != NULL)
      free(PATH_TABLE[i]);
} 

int dirwalk(struct stat *st_buf, char *path) {
  DIR *dirp;        
  struct dirent *de;

  if(!(dirp = opendir(path))) {
    fprintf(stderr, "walkfs: Can't open directory %s: %s\n", path, strerror(errno));
    exit(-1);
  }

  while(de = readdir(dirp)) {
    /*
      Don't process . or .. 
    */
    if(strcmp(".", de->d_name) && strcmp("..", de->d_name)) {
      /*
        Construct full path for each directory entry. This means allocating space for
        '/' and terminating null char
      */
      char *fpath = malloc(sizeof(path) + sizeof(de->d_name) + 2);
      if(!fpath) error("walkfs: Out of memory: malloc");
      sprintf(fpath, "%s/%s", path, de->d_name);
      /*
        Report failed lstat sys call and exit
      */
      if(lstat(fpath, st_buf) == -1) {
        fprintf(stderr, "walkfs: Cannot lstat file %s w. ino=%ld: %s\n", fpath, 
          (long) de->d_ino, strerror(errno));
        free(fpath);
        continue;
      }

      if((globalArgs.flags & X_FLAG) && SET_DEV_NUM == 1) {
        SET_DEV_NUM = 0;
        globalArgs.init_mnt_dev = st_buf->st_dev;
      }

      if(SET_DEV_NUM == 0 && globalArgs.init_mnt_dev != st_buf->st_dev) {
        fprintf(stderr, "note: not crossing mount point at %s\n", fpath);
        free(fpath);
        continue;
      }

      pstat(st_buf, fpath);

      
      if((st_buf->st_mode & S_IFMT) == S_IFDIR) {
        INO_TABLE[TAB_INDEX] = de->d_ino;
        PATH_TABLE[TAB_INDEX] = fpath; //strdup(fpath);
        DEV_TABLE[TAB_INDEX] = st_buf->st_dev;

        int i;
        for(i = 0; i < TAB_INDEX;) {
          if(INO_TABLE[i] == de->d_ino && DEV_TABLE[i] == (long) st_buf->st_dev) {
            LAST_SEEN = i;
            LOOP_DETECTED = 1;
            break;
          }
          else 
            i++;
        }

        if(LOOP_DETECTED == 1) {
          printf("Not traversing %s w. dev=0%.4lo ino=%4ld : previously seen as %s\n", 
            fpath, (long) st_buf->st_dev, (long) de->d_ino, PATH_TABLE[LAST_SEEN]);
          LOOP_DETECTED = 0;
          free(fpath);
          continue;
        }
        else
          TAB_INDEX++;

        dirwalk(st_buf, fpath);
        INO_TABLE[TAB_INDEX] = 0;
        DEV_TABLE[TAB_INDEX] = 0;
        //free(PATH_TABLE[TAB_INDEX]);
        PATH_TABLE[TAB_INDEX] = NULL;
        TAB_INDEX--;
      }
      free(fpath);
    }
  }
 
  if(closedir(dirp) == -1) {
    fprintf(stderr, "walks: Cannot close directory: %s\n", strerror(errno));
  } 
  return 0;
}

inline int getftype(mode_t mode, char *ftype) {
  switch(mode & S_IFMT) { 
    case S_IFREG: //Regular file
      *ftype = '-';
      break;
    case S_IFDIR: //Directory
      *ftype = 'd';
      break;
    case S_IFCHR: //Character device
      *ftype = 'c';
      break;
    case S_IFBLK: //Block device
      *ftype = 'b';
      break;
    case S_IFIFO: //FIFO (named pipe)
      *ftype = 'p';
      break;
    case S_IFLNK: //Symbolic link
      *ftype = 'l';
      break;
    case S_IFSOCK: //Socket
      *ftype = 's';
      break;
  }
  ftype++;
  return 0;
}

inline int getfperms(mode_t mode, char *pm) {
  int i;
  for (i = 0; i < 3; ++i) {
    if(mode & (S_IREAD >> i*3))
      *pm = 'r';
      pm++;
    if(mode & (S_IWRITE >> i*3))
      *pm = 'w';
      pm++;
    if(mode & (S_IEXEC >> i*3))
      *pm = 'x';
    pm++;
  }
  return 0;
}

inline int getfusr(uid_t uid, char *user) {
  struct passwd *pwd = getpwuid(uid);
  if(!pwd) {
    perror("walkfs: getpwuid: ");
    exit(-1);
  }
  strcpy(user, pwd->pw_name);
  return 0;
}

inline int getfgrp(gid_t gid, char *group) {
  struct group *grp = getgrgid(gid);
  if(!grp) {
    perror("walkfs: getgrgid: ");
    exit(-1);
  }
  strcpy(group, grp->gr_name);
  return 0;
}

inline char *getlnktarget(struct stat *st_buf, struct i_stat *info_stat) {
  /*
    From man 2 stat:  The size of a symbolic link is the length of
    the pathname it contains, without a terminating null byte.
  */
  info_stat->ino_link = (char *) malloc(st_buf->st_size + 1);
  int bytes_read = readlink(info_stat->ino_fpath, info_stat->ino_link, st_buf->st_size + 1);
  if(bytes_read == -1) {
    perror("walkfs: readlink");
    exit(-1);
  }
  *(info_stat->ino_link + bytes_read) = '\0';
  return info_stat->ino_link;
}

void print_info(struct i_stat *info_stat) {
  if(!info_stat->dontprint) {
    printf("0%.4lo/%4ld\t", (long) info_stat->ino_dev, (long) info_stat->ino_n);
    printf("%-10s   ", info_stat->ino_perms);
    printf("%-4d   ", info_stat->ino_nlinks);
    printf("%-7s\t", info_stat->ino_usr);
    printf("%-7s\t", info_stat->ino_grp);
    printf("%-s  ", info_stat->ino_size);
    char *t = ctime(&(info_stat->ino_tmod));
    *(t + strlen(t) - 1) = '\0';
    printf("%-24s  ", t);
    printf("%s", info_stat->ino_fpath);
    /*
      If the file is a symlink, then print the path to which it is linked
    */
    if(info_stat->ino_link != NULL)
      printf("  --> %s\n", info_stat->ino_link);
    else
      printf("\n");
  }
  return;
}

inline int chkuser(const char *user) {
  int i = 0;
  while(*(user + i) != '\0') {
    if (isalpha(*(user + i)))
      return -1;
    i++;
  }
  return 1;
}

struct i_stat *new_istat(struct i_stat *info_stat) {
  info_stat = (struct i_stat *) malloc(sizeof(struct i_stat));
  if(!info_stat) error("walkfs: Out of memory: malloc");
  info_stat->ino_perms = (char *) malloc(sizeof(char) + 11);
  if(!info_stat->ino_perms) error("walkfs: Out of memory: malloc");
  info_stat->ino_usr = (char *) malloc(GOOD_SIZE);
  if(!info_stat->ino_usr) error("walkfs: Out of memory: malloc");
  info_stat->ino_grp = (char *) malloc(GOOD_SIZE);
  if(!info_stat->ino_grp) error("walkfs: Out of memory: malloc");
  info_stat->ino_size = (char *) malloc(GOOD_SIZE);
  if(!info_stat->ino_size) error("walkfs: Out of memory: malloc");
  
  info_stat->ino_link = NULL;
  info_stat->ino_fpath = NULL;
  info_stat->dontprint = 0;
  return info_stat;
}

struct i_stat *set_istat(struct i_stat *info_stat, struct stat *st_buf, char *fpath) {
  strcpy(info_stat->ino_perms, "----------");
  info_stat->ino_n = st_buf->st_ino;
  info_stat->ino_dev = st_buf->st_dev;
  info_stat->ino_nlinks = st_buf->st_nlink;
  info_stat->ino_fpath = fpath;
  info_stat->ino_tmod = st_buf->st_mtime;
  getftype(st_buf->st_mode, info_stat->ino_perms);
  getfperms(st_buf->st_mode, (info_stat->ino_perms) + 1);
  getfusr(st_buf->st_uid, info_stat->ino_usr);
  getfgrp(st_buf->st_gid, info_stat->ino_grp);

  if((st_buf->st_mode & S_IFMT) == S_IFBLK || 
    (st_buf->st_mode & S_IFMT) == S_IFCHR)
    sprintf(info_stat->ino_size, "0x%-10.5lX", (long) st_buf->st_rdev);
  else
    sprintf(info_stat->ino_size, "%-9ld", (long) st_buf->st_size);

  if((st_buf->st_mode & S_IFMT) == S_IFLNK)
    getlnktarget(st_buf, info_stat);

  return info_stat;
}

inline int destroy_istat(struct i_stat *info_stat) {
  free(info_stat->ino_usr);
  free(info_stat->ino_grp);
  free(info_stat->ino_size);
  free(info_stat->ino_perms);
  if(info_stat->ino_link != NULL)
    free(info_stat->ino_link);
  free(info_stat);
  return 0;
}

int pstat(struct stat *st_buf, char *path) {
  struct passwd *pwd = getpwuid(st_buf->st_uid);
  if(!pwd) {
    perror("walkfs: getpwuid");
    exit(-1);
  }
  char *usr = pwd->pw_name;
  uid_t uid = st_buf->st_uid;
  struct i_stat *info_stat = new_istat(info_stat);

  if((globalArgs.flags == NO_FLAG) || (globalArgs.flags & X_FLAG) ||
    (globalArgs.flags & L_FLAG))
    set_istat(info_stat, st_buf, path);

  if(globalArgs.flags & U_FLAG) {
    if((chkuser(globalArgs.user) == 1) && ((int) uid == atoi(globalArgs.user)))
      set_istat(info_stat, st_buf, path);
    else if((chkuser(globalArgs.user) == -1) && (strcmp(usr, globalArgs.user) == 0))
      set_istat(info_stat, st_buf, path);
    else
      info_stat->dontprint = 1;
  }
  
  if((globalArgs.flags & M_FLAG) && info_stat->dontprint != 1) {
    if(globalArgs.mtime > 0) {
      if(difftime(time(NULL), st_buf->st_mtime) >= globalArgs.mtime) 
        set_istat(info_stat, st_buf, path);
      else 
        info_stat->dontprint = 1;
    }
    else { //mtime < 0 //if mtime was 0 we woulda exited the program already
      if(difftime(time(NULL), st_buf->st_mtime) <= -globalArgs.mtime)
        set_istat(info_stat, st_buf, path);
      else 
        info_stat->dontprint = 1;
    }
  }

  /*
    If -l is flagged and all our other conditions are met (-u -m -x)
    then we check if the current file is a symbolic link. If the current file is 
    not a symlink then we dont perform any more checks and mark info_stat for no 
    printing. If it is, then we get the file to which it links and check if it
    matches our target. If it doesn't, then we stop and mark info_stat for no 
    printing. If there is a match, we stat the target to find if it exists. If stat
    fails, then we know the file doesnt exist so we set info_stat for no printing.
    If stat succeeded, the file exists
  */
  if((globalArgs.flags & L_FLAG) && info_stat->dontprint != 1) {
    if((st_buf->st_mode & S_IFMT) == S_IFLNK) {
      char *file_link = getlnktarget(st_buf, info_stat);
      if(strcmp(globalArgs.target, file_link) == 0) {
        struct stat st;
        if(stat(globalArgs.target, &st) == -1) {
          perror("walkfs: Cannot stat");
          info_stat->dontprint = 1;
        }
        else
          set_istat(info_stat, st_buf, path);
      }
      else
        info_stat->dontprint = 1;
    }
    else
      info_stat->dontprint = 1;
  }
  
  print_info(info_stat);
  destroy_istat(info_stat);
  return 0;
}
