#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/select.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>

#define rep(i,a,n) for(int i=a;i<n;i++)
#define ll long


struct Message{
    ll mtype;
    int timestamp;
    int user;
    char mtext[256];
    int modifyingGroup;
};



int main(int argc, char *argv[]) {
    //printf("%s %s %s %s\n",argv[1],argv[2],argv[3],argv[4]);
    int N,gvKey=atoi(argv[2]),gmKey=atoi(argv[3]),gaKey=atoi(argv[4]),cnt=atoi(argv[5]),gNum=0,n;

    for(int i=0;i<strlen(argv[1]);++i){
        if(argv[1][i]>='0' && argv[1][i]<='9'){
            gNum=gNum*10+(argv[1][i]-'0');
        }
    }
    // printf("group %d started\n",gNum);
    // printf("%s",argv[1]);
    // printf("gvKey: %d, gmKey: %d, cnt: %d\n", gvKey, gmKey, cnt);

    int gvId = msgget(gvKey, 0666|IPC_CREAT);   
    struct Message creation={.mtype=1,.timestamp=0,.user=0,.mtext="",.modifyingGroup=gNum};
    //printf("%d",creation.modifyingGroup);

    msgsnd(gvId,&creation,sizeof(creation)-sizeof(long),0);  //CHECK IN END

    //printf("%s\n",argv[1]);
    FILE *group = fopen(argv[1],"r");
    if(!group)printf("Error opening group file\n");

    fscanf(group,"%d",&N);
    n=N;
    //printf("users: %d\n",N);
    char uFiles[N][256];
    rep(i,0,N)fscanf(group,"%s",uFiles[i]);
    //rep(i,0,N)printf("%s\n",uFiles[i]);
    fclose(group);
    pid_t uPids[N];
    int pipes[N][2];
    rep(i,0,N){
        pipe(pipes[i]);
        pid_t pid=fork();
        if(pid==0){
            close(pipes[i][0]);
            //printf("%s\n",uFiles[i]);
            FILE *uFile = fopen(uFiles[i],"r");
            if(!uFile)printf("Error opening user file\n");
            //printf("%s\n",uFiles[i]);
            int uNum = 0;
            char *ptr = strrchr(uFiles[i],'_');
            ptr++;
            while((*ptr)>='0' && (*ptr)<='9'){
                uNum*=10;
                uNum+=(*ptr)-'0';
                ptr++;
            }
            //printf("user %d started\n",uNum);
            struct Message uCreation={2,0,uNum,"",gNum};
            msgsnd(gvId,&uCreation,sizeof(uCreation)-sizeof(uCreation.mtype),0);

            //printf("user %d started of grp%d\n",uNum,gNum);
            ll ts;
            char umsg[256];
            while(fscanf(uFile,"%ld %s",&ts,umsg)!=-1){
                //printf("user %d: %s\n",uNum,umsg);
                struct Message msg={1,ts,uNum,"",gNum};
                memset(msg.mtext, 0, sizeof(msg.mtext));
                strncpy(msg.mtext, umsg, sizeof(msg.mtext)-1);
                write(pipes[i][1],&msg,sizeof(msg));
                //printf("%ld user %d: %s\n",msg.mtype,uNum,msg.mtext);
            }
            fclose(uFile);
            close(pipes[i][1]);
            exit(0);
        }
        else if(pid>0){
            uPids[i]=pid;
            close(pipes[i][1]);
        }
        else printf("Error forking\n");
    }
    int gmId = msgget(gmKey, 0666|IPC_CREAT);
    if(gmId==-1)printf("Error creating message queue\n");   

    
    for (int i=0; i<N;i++) {
        struct Message temp;
        ssize_t bytesRead;
        while ((bytesRead = read(pipes[i][0], &temp, sizeof(temp))) > 0) {
            //usleep(100000);
            if(msgsnd(gmId, &temp, sizeof(temp) - sizeof(long), 0|IPC_NOWAIT)==-1){
                perror("msgsnd");
                exit(1);
            }
            //printf("%d %ld User %d: %s\n",gNum,temp.mtype,temp.user,temp.mtext);
        }
        close(pipes[i][0]);  
    }
    struct Message inputFinish = {
        .mtype = 1,
        .modifyingGroup = gNum,
        .user = 100
    };
    msgsnd(gmId, &inputFinish, sizeof(inputFinish) - sizeof(long), 0);
    printf("Messages sent by group %d\n",gNum);
    struct Message modMsg;
    while(n>1){
        //printf("%d %d %d %s\n", modMsg.timestamp, modMsg.user, modMsg.modifyingGroup, modMsg.mtext);
        if(msgrcv(gmId, &modMsg, sizeof(modMsg) - sizeof(modMsg.mtype), 30+gNum, 0)!=-1){
            if(modMsg.modifyingGroup==-1){
                n--;
                printf("\nActive users in group %d: %d\n\n",gNum,n);

            }
            else{
                //usleep(100000);
                printf("%d %ld %d User %d: %s\n",modMsg.timestamp,modMsg.mtype,modMsg.modifyingGroup,modMsg.user,modMsg.mtext);
                if (msgsnd(gvId, &modMsg, sizeof(modMsg) - sizeof(modMsg.mtype), 0) == -1) {
                    perror("msgsnd");
                    exit(1);
                }
            }
        }
        else{
            perror("msgrcv");
            exit(1);
        }
    }
    struct Message termMsg = {
        .mtype = 3,
        .modifyingGroup = gNum,
    };
    int gaId = msgget(gaKey, 0666|IPC_CREAT);
    msgsnd(gvId, &termMsg, sizeof(termMsg) - sizeof(termMsg.mtype), 0);
    msgsnd(gaId, &termMsg, sizeof(termMsg) - sizeof(termMsg.mtype), 0);
    sleep(1);
    exit(0);
    return 0;
}