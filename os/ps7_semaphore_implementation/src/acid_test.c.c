#include "sem.h"
#include "fifo.h"

#define MAX_WRITE 1000				/*each writer shall write MAX_WRITE times to fifo*/
#define N_WRITERS 20					/*there shall be N_WRITERS write processes*/

int my_procnum;						/*virtual id (vid) of each forked process*/
int cpid_status[N_WRITERS + 1];  /*exit status of each child forked from parent*/
int cpid_array[N_WRITERS + 1];	/*pid of each child forked from parent*/
unsigned long *write_array; 		/*each writing process shall write to this array*/
unsigned long *pwrite_array;

void pchildstatus(int status, int pid);

int main()
{
  struct fifo *f;
  int i, j, status;

  if((f = (struct fifo *) mmap(0, sizeof(struct fifo), PROT_READ|PROT_WRITE,
    MAP_SHARED|MAP_ANONYMOUS, -1, 0)) == MAP_FAILED) { perror("map"); exit(1); }

  
  if((write_array = mmap(0, N_WRITERS * sizeof(unsigned long),
    PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0)) == MAP_FAILED)
    { perror("map"); exit(1); }
  
  if((pwrite_array = mmap(0, N_WRITERS * sizeof(unsigned long),
    PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0)) == MAP_FAILED)
    { perror("map"); exit(1); }

  pid_t reader, writer;
  fifo_init(f);

  for(my_procnum = 0; my_procnum < N_WRITERS; my_procnum++)
  {
    if((writer = fork()) == -1) { perror("fork"); exit(1); }
    if(writer == 0) {
      int i;
      for(i = 0; i < MAX_WRITE; i++)
      {
        unsigned long seq_num = i;           /*sequence # == current write iteration*/
        unsigned long datum = seq_num << 16; 
        datum |= my_procnum;
        fifo_wr(f, datum);                   /*write encoded vid and sequence # to fifo*/
      }
      exit(0);
    }
    else
      cpid_array[my_procnum] = writer;
  }

  for(my_procnum = N_WRITERS; my_procnum < N_WRITERS + 1; my_procnum++)
  {
    if((reader = fork()) == -1) { perror("fork"); exit(1); }
    if(reader == 0){
      int i, j, vid, data;
      unsigned long rdval, mask = 0x0000FFFF;
      for(i = 0; i < N_WRITERS * MAX_WRITE; i++)
      {
        rdval = fifo_rd(f);
        vid = rdval & mask;
        data = rdval >> 16;      				 
        
        write_array[vid] = data;	 /*write decoded data into writer array*/
        for(j = 0; j < N_WRITERS; j++)
        {
        	printf("%ld ", write_array[j]);
        }
        printf("\n");
        for(j = 0; j < N_WRITERS; j++)
        {
        	if((pwrite_array[j] + 1 != write_array[j]) &&
        		(pwrite_array[j] != write_array[j]))
        		printf("DATA INCONSISTENCY!!!!!\n");        			pwrite_array[j] = write_array[j];
        }
      }
      exit(0);
    }
    else
      cpid_array[my_procnum] = reader;
  }

  while(1) {
    i = 0;
    wait(&status);
    cpid_status[i] = status;
    if(errno == ECHILD) break;
    i++;
	}

  for(i = 0; i < N_WRITERS + 1; i++)
    pchildstatus(cpid_status[i], cpid_array[i]);


  printf("done\n");
  return 0;
}



void pchildstatus(int status, int pid) {
  int i;
  if(status != 0) {
    if(WIFSIGNALED(status)) {
      fprintf(stderr, "child process with pid=%d exited with signal %d",
        pid, WTERMSIG(status));
    }
    else
      fprintf(stderr, "child process with pid=%d exited with nonzero value %d\n",
        pid, WEXITSTATUS(status));
  }
  else
    fprintf(stderr, "child process with pid=%d exited normally\n",
      pid);
}

