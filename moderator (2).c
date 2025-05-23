#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/select.h>
#include <unistd.h>
#include <signal.h>

#define MAX_GROUPS 30
#define MAX_USERS 50
#define MAX_FILTERED_WORDS 50
#define MAX_WORD_LENGTH 20
#define MAX_MESSAGE_LENGTH 256


typedef struct { 
    long mtype;
    int timestamp;
    int user;
    char mtext[256];
    int modifyingGroup;
} Message;

char filt_words[MAX_FILTERED_WORDS][MAX_WORD_LENGTH];  
int num_filtered_words = 0;  
int violations[MAX_GROUPS][MAX_USERS] = {0}; 
int userCount[MAX_GROUPS][MAX_USERS] = {0}; 
Message messages[100000]; 
int messageCount = 0;


int gmKey,cnt,N,gvKey,gaKey;; 

int compareMessage(const void *a, const void *b) {
    const Message *m1 = a;
    const Message *m2 = b;
    if (m1->timestamp < m2->timestamp) {
        return -1;
    } else if (m1->timestamp > m2->timestamp) {
        return 1;
    } else {
        return 0;
    }
}

void to_lowercase(char *str) {
    for (int i = 0; str[i]; i++) {
        str[i] = tolower((unsigned char)str[i]);
    }
}


int read_filtered_words(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Error opening filtered_words.txt:");
        return -1;
    }

    num_filtered_words = 0;
    while (num_filtered_words < MAX_FILTERED_WORDS && fscanf(file, "%s", filt_words[num_filtered_words]) == 1) {
       
        num_filtered_words++;
    }
    fclose(file);
}


int count_violations(const char *message) {
    char msg_copy[MAX_MESSAGE_LENGTH];
    strncpy(msg_copy, message, MAX_MESSAGE_LENGTH);
    msg_copy[MAX_MESSAGE_LENGTH - 1] = '\0';
    to_lowercase(msg_copy);

    int violation_count = 0;
    int counted[MAX_FILTERED_WORDS] = {0}; // Tracks the  counted words

    for (int i = 0; i < num_filtered_words; i++) {
        char word[MAX_WORD_LENGTH];
        strncpy(word, filt_words[i], MAX_WORD_LENGTH);
        word[MAX_WORD_LENGTH - 1] = '\0';
        to_lowercase(word);

        if (strstr(msg_copy, word) != NULL && !counted[i]) {
            violation_count++;
            counted[i] = 1;
        }
    }

    return violation_count;
}

int process_messages() {
    int msgid = msgget(gmKey, 0666 | IPC_CREAT);
    if (msgid == -1) {
        printf("Error connecting to message queue");
        return -2;
    }
    int inputFinish=0;
    Message message;
    while(1){
        if (msgrcv(msgid, &message, sizeof(Message) - sizeof(long), 1, 0) == -1) {
            printf("msgrcv failed\n");
            return -3;
        }
       

        if(message.user==100){
            
            inputFinish++;
            if(inputFinish==N)break;
            continue;
        }
        messages[messageCount++]=message;
        userCount[message.modifyingGroup][message.user]++;
       
    }
   
    qsort(messages,messageCount,sizeof(Message),compareMessage);

    for(int i=0;i<messageCount;++i){
        message = messages[i];
        int group = message.modifyingGroup;
        int user = message.user;
        int new_violations = count_violations(message.mtext);

        
        if (violations[group][user] < cnt && violations[group][user]!=-1) {  
           
            Message allowed_message;
            allowed_message.mtype = 30+group; 
            allowed_message.timestamp = message.timestamp;
            allowed_message.user = user;
            allowed_message.modifyingGroup = group;
            strncpy(allowed_message.mtext, message.mtext, sizeof(allowed_message.mtext) - 1);
            allowed_message.mtext[sizeof(allowed_message.mtext) - 1] = '\0'; //to Ensure null termination
            
            //usleep(100000);
            if (msgsnd(msgid, &allowed_message, sizeof(Message) - sizeof(long), 0|IPC_NOWAIT) == -1) {
                perror("msgsnd");
                return -4;
            }
            else{
                userCount[group][user]--;
                if(userCount[group][user]==0){
                    Message termMsg = {
                        .mtype = 30+group,
                        .modifyingGroup = -1,
                        .user = user
                    };
                    //violations[group][user] = -1;
                    //usleep(100000);
                    msgsnd(msgid, &termMsg, sizeof(termMsg)-sizeof(termMsg.mtype), 0|IPC_NOWAIT);
                }
            }
           
        }
        if (violations[group][user] >= cnt && violations[group][user]!=-1) { 
           
            violations[group][user] = -1;  
        
            Message ban_message;
            ban_message.mtype = 30+group; 
            ban_message.timestamp = message.timestamp;
            ban_message.user = user;
            ban_message.modifyingGroup = -1;
            strncpy(ban_message.mtext, message.mtext, sizeof(ban_message.mtext) - 1);
            ban_message.mtext[sizeof(ban_message.mtext) - 1] = '\0'; //to Ensure null termination
            
            //usleep(100000);
            if (msgsnd(msgid, &ban_message, sizeof(Message) - sizeof(long), 0|IPC_NOWAIT) == -1) {
                printf("Failed to send allowed message to groups.c:");
                return -4;
            }
            printf("\n[SENT TO GROUPS.C]\n");
            printf("  Type       : %ld\n", ban_message.mtype);
            printf("  Timestamp  : %d\n", ban_message.timestamp);
            printf("  User       : %d\n", ban_message.user);
            printf("  Group      : %d\n", ban_message.modifyingGroup);
            printf("  Message    : %s\n", ban_message.mtext);
            printf("-----------------------------\n");
        }
        if(violations[group][user]!=-1)violations[group][user] += new_violations;
    }
return 0;
}


// Read gmKey & cnt from input.txt
int read_input_file(const char *filename) {
    FILE *inp = fopen(filename, "r");
    if (!inp) {
        printf("Error opening input.txt");
        return -1;
    }
    fscanf(inp, "%d", &N);  // Number of groups
    fscanf(inp, "%d", &gvKey);  // Validation queue key
    fscanf(inp, "%d", &gaKey);  // App queue key
    fscanf(inp, "%d", &gmKey);  // Moderator queue key
    fscanf(inp, "%d", &cnt);    // Violation threshold
    fclose(inp);
   
    return 0;
}

int main(int argc,char *argv[]) {
        char input[30];
        char input_2[30];
        snprintf(input,30,"testcase_%s/input.txt",argv[1]);
        snprintf(input_2,30,"testcase_%s/filtered_words.txt",argv[1]);
        read_input_file(input);  // Read queue key & threshold
        read_filtered_words(input_2);  // Load filtered words
        process_messages();  // Start message queue processing
        return 0;
}