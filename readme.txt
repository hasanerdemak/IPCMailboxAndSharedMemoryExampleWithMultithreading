This program is written to perform matrix multiplication using server-client logic. It covers multithreaded programming, IPC mailbox and shared memory.

The server thread is constantly listening for requests from clients. Clients send the necessary information and matrices to the server thread. Communication between server-client is done via IPC mailbox, data sharing is done via shared memory.

Compile
gcc -pthread -o server server.c
gcc -o client client.c

Run
./server
./client matrix1.txt matrix2.txt


Utilized resources:

https://stackoverflow.com/questions/49570961/message-queue-msgget-msgsnd-msgrcv-linux-eidrm
http://www.cs.nuim.ie/~dkelly/CS240-05/Practical10.htm
http://osr600doc.xinuos.com/en/SDK_sysprog/IC_CntllMsgQueues.html
https://stackoverflow.com/questions/37147851/how-do-i-read-a-matrix-from-a-file
https://www.geeksforgeeks.org/inter-process-communication-ipc/
https://www.geeksforgeeks.org/ipc-shared-memory/
