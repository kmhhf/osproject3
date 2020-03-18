//Adds integers using the method selectd by the master program

#include <sys/sem.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>
#include <time.h>

int fileSem;

struct sembuf sem;  //struct for the semaphore

void semLock();         //lock the semaphore
void semRelease();      //release the semaphore

int main(int argc, char *argv[0])
{
    FILE *output = NULL;

    key_t semFileKey = ftok("master", 1);                   //get key for semaphore
    if (semFileKey == -1)
    {
        fprintf(stderr, "%s: Error: ", argv[0]);
        perror("");
        exit(EXIT_FAILURE);
    }

    fileSem = semget(semFileKey, 1, 0666);                  //use same semaphore as master program
    if(fileSem == -1)
    {
        fprintf(stderr, "%s: Error: ", argv[0]);
        perror("");
        exit(EXIT_FAILURE);
    }

    key_t sharedIntKey = ftok("master", 2);                 //get key for shared memeory
    if(sharedIntKey == -1)
    {
        fprintf(stderr, "%s: Error: ", argv[0]);
        perror("");
        exit(EXIT_FAILURE);
    }
    int count = atoi(argv[3]);
    int intShmid = shmget(sharedIntKey, sizeof(int) *count, 0666);  //use same shared memory as master program
    if(intShmid == -1)
    {
        fprintf(stderr, "%s: Error: ", argv[0]);
        perror("");
        exit(EXIT_FAILURE);
    }

    int* sharedInt = (int*) shmat(intShmid, NULL, 0);       //connect to shared memory
    if(sharedInt == -1)
    {
        fprintf(stderr, "%s: Error: ", argv[0]);
        perror("");
        exit(EXIT_FAILURE);
    }

    if(atoi(argv[4]) == 1)  //Sum using method 1
    {
        int sharedIndex = atoi(argv[1]);
        int increment = atoi(argv[2]);
        if (sharedIndex + increment < count)
        {
            sharedInt[sharedIndex] = sharedInt[sharedIndex] + sharedInt[sharedIndex + increment]; //add two ints

            semLock();  //lock semaphore
            time_t currentTime;
            time(&currentTime); //get time

            //print time entered the critical section
            fprintf(stderr, "PID %d entered critical section at %s\n", getpid(), ctime(&currentTime));

            //sleep used for debugging
#ifdef DEBUG
            sleep(1);
#endif

            output = fopen("adder_log.txt", "a");                   //open log
            fprintf(output, "%d %d %d\n", getpid(), sharedIndex, 2);//write to log

            //sleep used for debugging
#ifdef DEBUG
            sleep(1);
#endif
            time(&currentTime);  //get time

            //print time exited the critical section
            fprintf(stderr, "PID %d exited critical section at %s\n", getpid(), ctime(&currentTime));
            semRelease();   //release semaphore
        }
    }

    //sum using method 2
    if(atoi(argv[4]) == 2)
    {
        int sharedIndex = atoi(argv[1]);
        int increment = atoi(argv[2]);
        int i;

        //add the number of integers passed by the master program
        for(i = 1; i < increment && sharedIndex + i < count; i++)
        {
            sharedInt[sharedIndex] = sharedInt[sharedIndex] + sharedInt[sharedIndex + i];
            sharedInt[sharedIndex + i] = 0;
        }

        semLock();  //lock the semaphore
        time_t currentTime;
        time(&currentTime);  //get current time

        //print time entered the critical section
        fprintf(stderr, "PID %d entered critical section at %s\n", getpid(), ctime(&currentTime));

        //sleep used in debugging
#ifdef DEBUG
        sleep(1);
#endif
        output = fopen("adder_log.txt", "a"); //open log file
        fprintf(output, "%d %d %d\n", getpid(), sharedIndex, increment);  //print to log file

        //sleep used for debugging
#ifdef DEBUG
        sleep(1);
#endif
        time(&currentTime);

        //print time exited critical section
        fprintf(stderr, "PID %d exited critical section at %s\n", getpid(), ctime(&currentTime));
        semRelease();   //release semaphore
    }

    return 0;
}

//lock semaphore
void semLock()
{
    sem.sem_num = 0;
    sem.sem_op = -1;
    sem.sem_flg = 0;
    semop(fileSem, &sem, 1);
}

//release semaphore
void semRelease()
{
    sem.sem_num = 0;
    sem.sem_op = 1;
    sem.sem_flg = 0;
    semop(fileSem, &sem, 1);
}

