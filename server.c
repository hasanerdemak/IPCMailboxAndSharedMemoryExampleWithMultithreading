#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <string.h>

void *serverMainThreadFunc();
void createWorkerThread(int pId);
void *workerThreadFunc(void *A);

// structure for message queue
struct mesg_buffer {
    long mesg_type;
    char mesg_text[100];
} message;

int main()
{
    pthread_t thread_id;
    //Create main thread to run as a server
    pthread_create(&thread_id, NULL, serverMainThreadFunc, "A");
    pthread_join(thread_id, NULL);

    exit(0);
}

void *serverMainThreadFunc()
{
    printf("Main Thread is running\n");
    key_t key;
    int msgid;

    // Generate unique key with ftok
    key = ftok("progfile", 65);

    // Create a message queue to get messages from clients
    msgid = msgget(key, 0666 | IPC_CREAT);

    //Constantly listen to clients' connection requests
    while (1){
        // Receive clients' pIds
        msgrcv(msgid, &message, sizeof(message), 1, 0);
        printf("Recieved message for the client pId is %s\n", message.mesg_text);
        int pId = (intptr_t)atoi(message.mesg_text);

        // Create worker thread
        createWorkerThread(pId);
    }

}

void createWorkerThread(int pId){
    pthread_t thread_id;
    pthread_create( &thread_id, NULL, workerThreadFunc, (void*)(intptr_t) pId);
    pthread_join(thread_id, NULL);
    pthread_detach(thread_id);
}

void *workerThreadFunc(void *A)
{
    int pId = (intptr_t) A;
    printf("Worker Thread is running for client %d\n", pId);

    int msgIdSent, msgIdRcv;
    struct mesg_buffer msgBufferSent, msgBufferRcv;
    msgBufferSent.mesg_type = 1;

    // Create 2 queues to send and receive messages from the client
    msgIdSent = msgget(pId, IPC_CREAT | 0666);
    msgIdRcv = msgget(pId + 32768, IPC_CREAT | 0666);

    // Receive matrices' sizes.
    msgrcv(msgIdRcv, &msgBufferRcv, sizeof msgBufferRcv.mesg_text, 1, 0);

    //Parse message
    char *end = msgBufferRcv.mesg_text;
    int arr_size[4];
    for (int i = 0; i < 4; i++) {
        int n = strtol(msgBufferRcv.mesg_text, &end, 10);
        arr_size[i] = n;
        while (*end == ' ') {
            end++;
        }
        strcpy(msgBufferRcv.mesg_text, end);
    }

    // Compute the size of the shared memory
    int shmSize = arr_size[0] * arr_size[1] * sizeof(int) +
                  arr_size[2] * arr_size[3] * sizeof(int) +
                  arr_size[0] * arr_size[3] * sizeof(int);
    key_t shmKey = pId;
    printf("server: Shared memory size: %d\n", shmSize);

    int shmId;
    void *virtualAddr;

    // Create Shared Memory
    shmId = shmget(shmKey, shmSize, 0666 | IPC_CREAT);
    virtualAddr = shmat(shmId, 0, 0);

    // Notify client that shared memory is created
    strcpy(msgBufferSent.mesg_text, "shared memory olusturuldu.");
    if (msgsnd(msgIdSent, &msgBufferSent, sizeof msgBufferSent.mesg_text, 0) == -1) {
        perror("server: msgsnd failed:");
        exit(1);
    }
    else
        printf("server: message sent => %s\n",msgBufferSent.mesg_text);

    // Verify that the matrices to be multiplied are written to shared memory
    if (msgrcv(msgIdRcv, &msgBufferRcv, sizeof msgBufferRcv.mesg_text, 1, 0) == -1)
        perror("server: msgrcv failed:");
    else
        printf("server: message received => %s\n", msgBufferRcv.mesg_text);


    int row1 = arr_size[0];
    int row2 = arr_size[2];
    int column2 = arr_size[3];

    int *index1, *index2, *index3;
    index1 = (int *)virtualAddr;
    index2 = index1 + row1 * row2;
    index3 = index1 + row1 * row2 + row2 * column2;

    //Matrix multiplication
    int sum;
    for (int row = 0; row < row1; row++) {
        for (int col = 0; col < column2; col++) {
            sum = 0;
            for (int k = 0; k < row2; k++) {
                sum += (*(index1 + row * row2 + k)) * (*(index2 + k * column2 + col));
            }
            *index3 = sum;
            index3++;
        }
    }

    strcpy(msgBufferSent.mesg_text,"Matrix multiplication is complete.");
    msgsnd(msgIdSent, &msgBufferSent, sizeof msgBufferSent.mesg_text, 0 );

    //Detach the shared memory
    shmdt(virtualAddr);

    printf("Worker Thread completed the matrix multiplication for client %d\n", pId);
    printf("--------------------------------------\n");
}

