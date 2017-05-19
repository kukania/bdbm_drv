
#define _BSD_SOURCE
#include"utils.h"
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<fcntl.h>
#include<time.h>
#include<sys/time.h>
int main(){
	struct timeval start,end;
	gettimeofday(&start,NULL);
	char data[PAGESIZE];
	int fd=open("data/test.skip",O_CREAT|O_TRUNC|O_WRONLY,0666);

	for(int i=0; i<INPUTSIZE; i++){
		write(fd,data,sizeof(data));
	}
	gettimeofday(&end,NULL);
	struct timeval result;
	timersub(&end,&start,&result);
	fsync(fd);
	close(fd);
	printf("laps time : %ld and %lf\n",result.tv_sec, (float)result.tv_usec/1000000);
}
