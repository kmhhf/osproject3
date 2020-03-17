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
struct sembuf sem;

void semLock();
void semRelease();

int main(int argc, char *argv[0])
{
    FILE *output = NULL;

    key_t semFileKey = ftok("master", 1);
    if (semFileKey == -1)
    {
        fprintf(stderr, "%s: Error: ", argv[0]);
        perror("");
        exit(EXIT_FAILURE);
    }

    fileSem = semget(semFileKey, 1, 0666);
    if(fileSem == -1)
    {
        fprintf(stderr, "%s: Error: ", argv[0]);
        perror("");
        exit(EXIT_FAILURE);
    }

    key_t sharedIntKey = ftok("master", 2);
    if(sharedIntKey == -1)
    {
        fprintf(stderr, "%s: Error: ", argv[0]);
        perror("");
        exit(EXIT_FAILURE);
    }
    int count = atoi(argv[3]);
    int intShmid = shmget(sharedIntKey, sizeof(int) *count, 0666);
    if(intShmid == -1)
    {
        fprintf(stderr, "%s: Error: ", argv[0]);
        perror("");
        exit(EXIT_FAILURE);
    }

    int* sharedInt = (int*) shmat(intShmid, NULL, 0);
    if(sharedInt == -1)
    {
        fprintf(stderr, "%s: Error: ", argv[0]);
        perror("");
        exit(EXIT_FAILURE);
    }

    if(atoi(argv[4]) == 1)
    {
        int sharedIndex = atoi(argv[1]);
        int increment = atoi(argv[2]);
        if (sharedIndex + increment < count)
        {
            sharedInt[sharedIndex] = sharedInt[sharedIndex] + sharedInt[sharedIndex + increment];

            semLock();
            time_t currentTime;
            time(&currentTime);
            fprintf(stderr, "PID %d entered critical section at %s\n", getpid(), ctime(&currentTime));
#ifdef DEBUG
            sleep(1);
#endif
            output = fopen("adder_log.txt", "a");
            fprintf(output, "%d %d %d\n", getpid(), sharedIndex, 2);
            time(&currentTime);
#ifdef DEBUG
            sleep(1);
#endif
            fprintf(stderr, "PID %d exited critical section at %s\n", getpid(), ctime(&currentTime));
            semRelease();
        }
    }

    if(atoi(argv[4]) == 2)
    {
        int sharedIndex = atoi(argv[1]);
        int increment = atoi(argv[2]);
        int i;
        for(i = 1; i < increment && sharedIndex + i < count; i++)
        {
            sharedInt[sharedIndex] = sharedInt[sharedIndex] + sharedInt[sharedIndex + i];
            sharedInt[sharedIndex + i] = 0;
        }

        semLock();
        time_t currentTime;
        time(&currentTime);
        fprintf(stderr, "PID %d entered critical section at %s\n", getpid(), ctime(&currentTime));

#ifdef DEBUG
        sleep(1);
#endif
        output = fopen("adder_log.txt", "a");
        fprintf(output, "%d %d %d\n", getpid(), sharedIndex, increment);
        time(&currentTime);
#ifdef DEBUG
        sleep(1);
#endif
        fprintf(stderr, "PID %d exited critical section at %s\n", getpid(), ctime(&currentTime));
        semRelease();
    }

    return 0;
}

void semLock()
{
    sem.sem_num = 0;
    sem.sem_op = -1;
    sem.sem_flg = 0;
    semop(fileSem, &sem, 1);
}

void semRelease()
{
    sem.sem_num = 0;
    sem.sem_op = 1;
    sem.sem_flg = 0;
    semop(fileSem, &sem, 1);
}
