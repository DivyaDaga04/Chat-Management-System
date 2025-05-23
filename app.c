#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#define rep(i,a,n) for(int i=a;i<n;i++)
#define ll long 


struct Message{
    ll mtype;
    int timestamp;
    int user;
    char mtext[256];
    int modifyingGroup;
};


int main(int argc,char *argv[]) {
    char input[30];
    //printf("%s\n",argv[1]);

    snprintf(input,30,"testcase_%s/input.txt",argv[1]);
    //printf("Input file: %s\n",input);

    FILE *inp = fopen(input,"r");
    if(!inp)printf("Error opening input file\n");

    int N,gvKey,gaKey,gmKey,cnt,n;

    fscanf(inp,"%d",&N);
    char grpFiles[N][256];
    n=N;

    fscanf(inp,"%d",&gvKey);
    int gvId = msgget(gvKey, 0666|IPC_CREAT);
    if(gvId==-1)printf("Error creating message queue\n");

    fscanf(inp,"%d",&gaKey);
    int gaId = msgget(gaKey, 0666|IPC_CREAT);
    if(gaId==-1)printf("Error creating message queue\n");

    fscanf(inp,"%d",&gmKey);
    int gmId = msgget(gmKey, 0666|IPC_CREAT);
    if(gmId==-1)printf("Error creating message queue\n");

    fscanf(inp,"%d",&cnt);
    rep(i,0,N)fscanf(inp,"%s",grpFiles[i]);
    fclose(inp);

    //printf("%d\n%d\n%d\n%d\n%d\n",N,gvKey,gaKey,gmKey,cnt);
    //rep(i,0,N)printf("%s\n",grpFiles[i]);
    pid_t grpPids[N];
    rep(i,0,N){
        pid_t pid=fork();
        if(!pid){
            char gvKeyS[11],gaKeyS[11],gmKeyS[11],cntS[11];
            sprintf(gvKeyS,"%d",gvKey);
            sprintf(gmKeyS,"%d",gmKey);
            sprintf(gaKeyS,"%d",gaKey);
            sprintf(cntS,"%d",cnt);
            execl("./groups.out","groups.out",grpFiles[i],gvKeyS,gmKeyS,gaKeyS,cntS,NULL);
            exit(0);
        }
        else if(pid>0)grpPids[i]=pid;
        else printf("Error forking\n");
    }

    struct Message msg;
    while(n){
        if(msgrcv(gaId,&msg,sizeof(msg)-sizeof(msg.mtype),3,0)>0){
            printf("All users terminated. Exiting group process %d\n",msg.modifyingGroup);
            n--;
        }
    }
    //rep(i,0,N)waitpid(grpPids[i],NULL,0); use exit later
    msgctl(gaId,IPC_RMID,NULL);
    msgctl(gvId,IPC_RMID,NULL);
    msgctl(gmId,IPC_RMID,NULL);
    sleep(1);
    exit(0);
    return 0;
}