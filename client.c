#include <stdio.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>

int** readMatrixFromFile (const char* file_name, int i);

int row1Count, col1Count;
int row2Count, col2Count;

// structure for message queue
struct mesg_buffer {
    long mesg_type;
    char mesg_text[100];
} message;

int main(int argc, char *argv[])
{
    int pId = getpid();
    printf("Client started with pId: %d\n", pId);

    printf("client: Matrices are being read from files....\n");
    int** matrix1 = readMatrixFromFile(argv[1], 1);
    int** matrix2 = readMatrixFromFile(argv[2], 2);

    // Check the suitability of matrices for multiplication
    if (col1Count != row2Count){
        printf("client: The number of rows of the 2nd matrix is not equal to the number of columns of the 1st matrix!\n");
        return -1;
    }

    key_t key;
    int msgIdSent, msgIdRcv;
    struct mesg_buffer msgBufferSent, msgBufferRcv;
    msgBufferSent.mesg_type = 1;

    // Generate unique key with ftok
    key = ftok( "progfile", 65);

    // Create a message queue to send messages to server
    msgIdSent = msgget(key, IPC_CREAT | 0666 );

    char msgPId[10];
    sprintf(msgPId, "%d", pId);
    strcpy(msgBufferSent.mesg_text, msgPId );

    // Send pId
    if (msgsnd(msgIdSent, &msgBufferSent, sizeof msgBufferSent.mesg_text, 0 ) == -1 ) {
        perror( "client: msgsnd failed:" );
        exit( 3 );
    } else
        printf("client: pId sent => %s\n", msgBufferSent.mesg_text);

    // Create 2 queues to send and receive messages from server
    if ((msgIdRcv = msgget(pId, IPC_CREAT | 0666 ) ) == -1 ) {
        perror( "client: Failed to create message queue:" );
        exit( 1 );
    }
    if ((msgIdSent = msgget(pId + 32768, IPC_CREAT | 0666 ) ) == -1 ) {
        perror( "client: Failed to create message queue:" );
        exit( 1 );
    }

    // Create message for matrices' sizes.
    char matricesSizesMsg[] = "";
    char row1CountS[5];
    char col1CountS[5];
    char row2CountS[5];
    char col2CountS[5];
    sprintf(row1CountS, "%d", row1Count);
    sprintf(col1CountS, "%d", col1Count);
    sprintf(row2CountS, "%d", row2Count);
    sprintf(col2CountS, "%d", col2Count);
    strcat(matricesSizesMsg,row1CountS);
    strcat(matricesSizesMsg," ");
    strcat(matricesSizesMsg,col1CountS);
    strcat(matricesSizesMsg," ");
    strcat(matricesSizesMsg,row2CountS);
    strcat(matricesSizesMsg," ");
    strcat(matricesSizesMsg,col2CountS);
    strcpy(msgBufferSent.mesg_text, matricesSizesMsg);

    // Send matrices' sizes.
    if (msgsnd(msgIdSent, &msgBufferSent, sizeof msgBufferSent.mesg_text, 0 ) == -1 ) {
        perror( "client: msgsnd failed:" );
        exit( 1 );
    } else
        printf("client: matrices' sizes sent => %s\n", msgBufferSent.mesg_text);


    // Receive message for confirmation of sharing memory is created
    if (msgrcv(msgIdRcv, &msgBufferRcv, sizeof msgBufferSent.mesg_text, 1, 0 ) == -1){
        perror( "client: msgrcv failed:" );
        exit(1);
    } else
        printf("client: Message received = %s\n", msgBufferRcv.mesg_text );

    // Attach the shared memory created by server
    int shmSize = row1Count * col1Count * sizeof(int) +
                  row2Count * col2Count * sizeof(int) +
                  col1Count * row2Count * sizeof(int);
    int shmId;
    void *virtualAddr;
    int *tempPtr;

    shmId = shmget(pId, shmSize, 0);
    printf("client: shmId value is %d\n", shmId);

    virtualAddr = shmat(shmId, (void *)0, 0); // Attach the shared segment
    tempPtr = (int *)virtualAddr; // Put two integer values into it

    // Write matrices to shared memory
    for (int i = 0; i < row1Count; i++) {
        for (int j = 0; j < col1Count; j++) {
            *tempPtr = matrix1[i][j];
            tempPtr++;
        }
    }
    for (int i = 0; i < row2Count; i++) {
        for (int j = 0; j < col2Count; j++) {
            *tempPtr = matrix2[i][j];
            tempPtr++;
        }
    }

    // Notify server that matrices are written to shared memory
    strcpy(msgBufferSent.mesg_text, "The matrices to be multiplied are written to shared memory.");
    if (msgsnd(msgIdSent, &msgBufferSent, sizeof msgBufferSent.mesg_text, 0 ) == -1 ) {
        perror( "client: msgsnd failed:" );
        exit( 3 );
    } else
        printf("client: message sent => %s\n", msgBufferSent.mesg_text);

    //Receive message to confirm matrix multiplication is finished
    if (msgrcv(msgIdRcv, &msgBufferRcv, sizeof msgBufferSent.mesg_text, 1, 0) == -1){
        perror("client: msgrcv failed:");
        exit(1);
    } else
        printf("client: message received => %s\n", msgBufferRcv.mesg_text);

    // Print the result matrix
    printf("Result:\n");
    for (int i = 0; i < row1Count; i++) {
        for (int j = 0; j < col2Count; j++) {
            printf("%d ",*tempPtr);
            tempPtr++;
        }
        printf("\n");
    }


    //Detach the shared memory
    shmdt(virtualAddr);
    if (shmctl(shmId, IPC_RMID, 0) == -1) {
        perror("shmctl");
        exit(1);
    }

    // Remove the message queues
    struct msqid_ds msgCtlBuf;
    if (msgctl(msgIdSent, IPC_RMID, &msgCtlBuf ) == -1 || msgctl(msgIdRcv, IPC_RMID, &msgCtlBuf ) == -1) {
        perror( "client: msgctl failed:" );
        exit(1);
    }

    return 0;
}

//function that reads from the file and puts integer into an array
int** readMatrixFromFile (const char* fileName, int i)
{
    FILE* file = fopen (fileName, "r");
    int rowCount = 0;
    int colCount = 0;
    fscanf(file, "%d %d", &rowCount, &colCount);

    if (i == 1){
        row1Count = rowCount;
        col1Count = colCount;
    }else if(i == 2){
        row2Count = rowCount;
        col2Count = colCount;
    }

    int **arr=(int **)malloc(sizeof(int *) * rowCount);
    for (int i = 0; i < rowCount; i++) {
        arr[i]=(int *)malloc(sizeof(int) * colCount);
        for (int j = 0; j < colCount; j++) {
            fscanf(file,"%d", &arr[i][j]);
        }
    }

    fclose (file);
    return arr;
}


