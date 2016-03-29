#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#define MAX_ITER 10000000    /*do 10 million iterations*/
#define BILLION  1000000000  /*because it's hard to keep track of 0s*/

unsigned long gettimedif(struct timespec *start, struct timespec *end);
void error(const char *msg);
void my_emptyfcn();

int main(int argc, char *argv[])
{
	if(argc != 2)
	{
		fprintf(stderr, "usage: ./syscall_cost [ABC]\n");
		exit(1);
	}
	
	struct timespec start, end;
	unsigned long long i, time_diff, loop_time, fcn_time, syscall_time;
	double ns_per_operation;

	if(clock_gettime(CLOCK_REALTIME, &start) == -1) 
		error("clock_gettime");
	for(i = 0; i < MAX_ITER; i++) { ; }
	if(clock_gettime(CLOCK_REALTIME, &end) == -1) 
		error("clock_gettime");
	loop_time = gettimedif(&start, &end);
	
	switch(*argv[1])
	{
		case 'A':
			ns_per_operation = (double)((long double)loop_time/MAX_ITER);
			printf("Empty loop with %llu iterations took %llu ns to complete\n",
				(unsigned long long) MAX_ITER, loop_time); 
			printf("The average time per loop iteration was %f ns\n",
				ns_per_operation);
			return 0;
		case 'B':
			if(clock_gettime(CLOCK_REALTIME, &start) == -1) 
				error("clock_gettime");
			for(i = 0; i < MAX_ITER; i++) { my_emptyfcn(); }
			if(clock_gettime(CLOCK_REALTIME, &end) == -1) 
				error("clock_gettime");
			time_diff = gettimedif(&start, &end); /*time_diff = loop_time + fcn_time*/
			
			fcn_time = time_diff - loop_time;    /*we only want time incurred by fcn*/
			ns_per_operation = (double)((long double)fcn_time/MAX_ITER);
			printf("time_diff = %llu fcn_time = %llu loop_time = %llu\n",
				time_diff, fcn_time, loop_time);
			printf("Empty function called in loop with %llu ", 
				(unsigned long long) MAX_ITER);
			printf("iterations took %llu ns to complete\n", fcn_time); 
			printf("The average time per empty function call was %f ns\n",
				ns_per_operation);
			return 0;
		case 'C':
			if(clock_gettime(CLOCK_REALTIME, &start) == -1) 
				error("clock_gettime");
			for(i = 0; i < MAX_ITER; i++) { getuid(); }
			if(clock_gettime(CLOCK_REALTIME, &end) == -1) 
				error("clock_gettime");
			time_diff = gettimedif(&start, &end); /*time_diff = loop_time + sc_time*/			
			
			syscall_time = time_diff - loop_time;
			ns_per_operation = (double)((long double)syscall_time/MAX_ITER);
			printf("time_diff = %llu syscall_time = %llu loop_time = %llu\n",
				time_diff, syscall_time, loop_time);
			printf("System call getuid() called in loop with %llu ", 
				(unsigned long long) MAX_ITER);
			printf("iterations took %llu ns to complete\n", syscall_time);
			printf("The average time per system call was %f ns\n",
				ns_per_operation); 
			return 0;
		default:
			fprintf(stderr, "usage: ./syscall_cost [ABC]\n");
			exit(1);
	}
}

void my_emptyfcn() {}

unsigned long gettimedif(struct timespec *start, struct timespec *end)
{
	long start_s, start_ns, end_s, end_ns, tot_start_ns, tot_end_ns;
	
	start_s = start->tv_sec;
	start_ns = start->tv_nsec;
	end_s = end->tv_sec;
	end_ns = end->tv_nsec;
	
	tot_start_ns = (start_s * BILLION) + start_ns;
	tot_end_ns = (end_s * BILLION) + end_ns;
	
	return (tot_end_ns - tot_start_ns);
}

void error(const char *msg)
{
	perror(msg);
	exit(1);
}

