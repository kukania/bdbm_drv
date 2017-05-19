#include<string.h>
#include<stdlib.h>
#include<fcntl.h>
#include<stdio.h>
int main(int argc, char *argv[]){
    if(argc<4){
	printf("USAGE:%s [TARGET FILE] [TARGET STRING] [CHANGE STRING]",argv[0]);
	return 0;
    }
    FILE *f=fopen(argv[1],"r");
    char temp_f[100];
    sprintf(temp_f,"%s.tmp",argv[1]);
    FILE *f2=fopen(temp_f,"wt");
    //    int fd=open(argv[1],O_RDONLY);
    char *line=NULL;
    size_t len=0;
    char read[1];
    printf("%s\n",temp_f);

    char *str,*token, *save;
    int flag;
    while((getline(&line,&len,f))!=-1){
	str=line;
	save=strtok(str," ");
	while(save !=NULL){
	    fwrite(" ",1,1,f2);
	    if(strcmp(argv[2],save)==0){
		fwrite(argv[3],strlen(argv[3]),1,f2);
	    }
	    else{
		fwrite(save,strlen(save),1,f2);
	    }
	    save=strtok(NULL," ");
	}
    }
    free(line);
}
