/*
	copycat.c
	Author: Stanley George
	ECE 460: Computer Operating Systems
	Professor Jeff Hakner
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

/* A structure holding key info that may be passed in as parameters */
struct globalArgs_t {
	long BUFSIZE;	//specified buffer size from -b option
	char *ofname;				//output file specified from -o option
	int num_infiles;			//# of input files
} globalArgs;

/* Initializes globalArgs. BUFSIZE is initialized to 1 as the defualt buffer size */
void init_globalArgs(void);

/* 
	readwrite() calls read() and write() system calls to copy infile(s) to outfile.
	ifd and ofd are the infile and outfile file descriptors respectively. They are 
	assumed to have been successfully opened prior to calling this function. If all 
	goes well, then readwrite() calls closefd() to close ifd. If an error occurs 
	during the system calls, an appropriate error message is reported and the program 
	terminates with -1 exit status
*/
signed readwrite(int ifd, int ofd, char *fname);

/* 
	closefd() calls close() system call to close file descriptor fd unless fd is
	stdin or stdout. If something  occurs, an error is reported and the program 
	terminates with -1 exit status
 */
signed closefd(int fd, char *fname);

static const char *optstring = "b:o:"; //for processing options -b and/or -o

int main(int argc, char *argv[]) {
	int ifd = 0, ofd = 1;		//default: ifd = stdin = 0 & ofd = stdout = 1
	init_globalArgs();			//set up our struct

	if (argc == 1)					//no parameters, copy from stdin to stdout
		return readwrite(ifd, ofd, NULL);

	int opt = getopt(argc, argv, optstring);
	while(opt != -1) {			//loop thru optional parameters
		switch(opt) {
			case 'b':
				globalArgs.BUFSIZE = atol(optarg);
				if(globalArgs.BUFSIZE < 1) {
					globalArgs.BUFSIZE = 1;
					fprintf(stderr, "copycat: Invalid buffer size. Using unbuffered mode...\n");
				}
				break;
			case 'o':
				if((ofd = open(optarg, O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU)) < 0) {
					fprintf(stderr, "copycat: Can't open file %s for writing: %s\n", 
						optarg, strerror(errno));
					return -1;
				}
				break;
		}
		opt = getopt(argc, argv, optstring);
	}

	int i = globalArgs.num_infiles = argc - optind;
	int j = optind;
	int n_read = 0, n_write = 0;

	while(i-- > 0) {
		if(strcmp(argv[j], "-") != 0)	{	//if no '-' then open input files
			if((ifd = open(argv[j], O_RDONLY)) < 0) {
				fprintf(stderr, "copycat: Can't open file %s for reading: %s\n",
					argv[j], strerror(errno));
				return -1;
			}
		}
		else										//otherwise read from stdin
			ifd = 0;
		readwrite(ifd, ofd, argv[j]);		//copy data from ifd to ofd
		j++;
	}
		
		closefd(ofd, globalArgs.ofname); //closes output file if it isn't stdout
	return 0;
}

void init_globalArgs() {
	globalArgs.BUFSIZE = 1;			//default buffer size
	globalArgs.ofname = NULL;
	globalArgs.num_infiles = 0;
}

signed closefd(int fd, char *fname) {
	if(fd != 0 && fd != 1) {		//close file descriptor if it isn't stdin or stdout
		if(close(fd) < 0) {
			fprintf(stderr, "copycat: Can't close file %s: %s\n", fname, strerror(errno));
			exit(-1);
		}
	}
	return 0;
}

signed readwrite(int ifd, int ofd, char *fname) {
	int n_read = 0, n_write = 0;
	char buffer[globalArgs.BUFSIZE];

	while((n_read = read(ifd, buffer, sizeof buffer)) > -2) {
		if(n_read == -1) {
			fprintf(stderr, "copycat: Can't read from file %s: %s\n", fname, strerror(errno));
			exit(-1);
		}
		else if(n_read == 0) break;
		else {
			n_write = write(ofd, buffer, n_read);
			if(n_write <= 0) {	//no write: report error and exit
				fprintf(stderr, "copycat: Can't write to file %s: %s\n", 
					fname,strerror(errno));
				exit(-1);
			}
			else if(n_write > 0 && n_write < n_read) { //incomplete write: report error and exit
				fprintf(stderr, "copycat: Incomplete write to file %s: %s\n", fname, strerror(errno));
				exit(-1);
			}
		}
	}

	closefd(ifd, fname);			//close current input fd
	return 0;
}