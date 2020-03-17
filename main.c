#include <sys/sem.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>
#include <math.h>

int fileSem;
int numChildren1;
int startIndex = 0;
int increment = 1;
int totalChildren1 = 0;
int activeChildren = 0;
int intShmid;
int* sharedInt;
int count = 0;
char temp[40];

struct sembuf sem;

void semLock();
void semRelease();
void method1(int totalChildren1, char* name, int increment);

int main(int argc, char *argv[0])
{
    int opt;
    int childPid;
    char numberFile[255] = "numbers.txt";
    FILE *numbers = NULL;
    FILE *output = NULL;


    while ((opt = getopt(argc, argv, "h")) != -1)
    {
        switch(opt)
        {
            case 'h':
                printf("Usage: %s [-h] [file]\n", argv[0]);
                exit(EXIT_FAILURE);
            default:
                fprintf(stderr, "Usage: %s [-h] [file]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (argv[1] != NULL)
    {
        strncpy(numberFile, argv[1], 255);
    }

    key_t semFileKey = ftok("master", 1);
    fileSem = semget(semFileKey, 1, IPC_CREAT | 0666);
    if (fileSem == -1)
    {
        fprintf(stderr, "%s: Error: ", argv[0]);
        perror("");
        exit(EXIT_FAILURE);
    }

    semctl(fileSem, 0, SETVAL, 1);

    numbers = fopen(numberFile, "r");
    if(numbers == NULL)
    {
        fprintf(stderr, "%s: Error: ", argv[0]);
        perror("");
        semctl(fileSem, 0, IPC_RMID);
        exit(EXIT_FAILURE);
    }

    output = fopen("adder_log.txt", "w");
    if(output == NULL)
    {
        fprintf(stderr, "%s: Error: ", argv[0]);
        perror("");
        semctl(fileSem, 0, IPC_RMID);
        exit(EXIT_FAILURE);
    }
    fclose(output);

    while(fgets(temp, 40, numbers) != NULL)
    {
        count = count + 1;
    }

    key_t sharedIntKey = ftok("master", 2);
    if(sharedIntKey == -1)
    {
        fprintf(stderr, "%s: Error: ", argv[0]);
        perror("");
        semctl(fileSem, 0, IPC_RMID);
        exit(EXIT_FAILURE);
    }

    intShmid = shmget(sharedIntKey, sizeof(int) * count, 0666|IPC_CREAT);
    if(intShmid == -1)
    {
        fprintf(stderr, "%s: Errror: ", argv[0]);
        perror("");
        semctl(fileSem, 0, IPC_RMID);
        exit(EXIT_FAILURE);
    }

    sharedInt = (int*) shmat(intShmid, NULL, 0);
    if(sharedInt == -1)
    {
        fprintf(stderr, "%s: Error: ", argv[0]);
        perror("");
        shmctl(intShmid, IPC_RMID, NULL);
        exit(EXIT_FAILURE);
    }

    rewind(numbers);
    int i = 0;
    int* original[count];
    for(i = 0; i < count; i++)
    {
        fgets(temp, 40, numbers);
        sharedInt[i] = atoi(temp);
        original[i] = sharedInt[i];
    }
    fclose(numbers);
    numbers = NULL;


    totalChildren1 = count / 2;
    method1(totalChildren1, argv[0], 1);
    printf("%d\n", sharedInt[0]);

    for(i = 0; i < count; i++)
    {
        sharedInt[i] = original[i];
    }

    int groups;

    increment = ceil(log(count));
    groups = ceil(count/ceil(log(count)));
    printf("%d\n", groups);


    while(numChildren1 < groups)
    {
        if(activeChildren < 19)
        {
            pid_t processPid = fork();

            if(processPid == -1)
            {
                fprintf(stderr, "%s: Error: ", argv[0]);
                perror("");
                shmdt(sharedInt);
                shmctl(intShmid, IPC_RMID, NULL);
                semctl(fileSem, 0, IPC_RMID);
                exit(EXIT_FAILURE);
            }

            if(processPid == 0)
            {
                char passIndex[12];
                char passIncrement[12];
                char passCount[12];
                sprintf(passIndex, "%d", startIndex);
                sprintf(passIncrement, "%d", increment);
                sprintf(passCount, "%d", count);
                execl("./bin_adder", "bin_adder", passIndex, passIncrement, passCount, "2", NULL);
                fprintf(stderr, "%s: Error: execl failed.", argv[0]);
            }
            numChildren1++;
            activeChildren++;
            startIndex = startIndex + increment;
        }
        for(i = 0; i < activeChildren; i++)
        {
            childPid = waitpid(-1, NULL, WNOHANG);
            if(childPid > 0)
            {
                activeChildren--;
            }
        }

    }
    for(i = 0; i < activeChildren; i++)
    {
        childPid = wait();
    }

    method1(groups, argv[0], increment);
    printf("%d\n", sharedInt[0]);

    shmdt(sharedInt);
    shmctl(intShmid, IPC_RMID, NULL);
    semctl(fileSem, 0, IPC_RMID);
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

void method1(int totalChildren1, char* name, int increment)
{
    int numChildren1 = 0;
    int startIndex;
    int childPid;
    int activeChildren = 0;
    int i;
    while(totalChildren1 > 0)
    {
        startIndex = 0;
        while(numChildren1 < totalChildren1)
        {
            if(activeChildren < 19)
            {
                pid_t processPid = fork();

                if(processPid == -1)
                {
                    fprintf(stderr, "%s: Error: ", name);
                    perror("");
                    shmdt(sharedInt);
                    shmctl(intShmid, IPC_RMID, NULL);
                    semctl(fileSem, 0, IPC_RMID);
                    exit(EXIT_FAILURE);
                }

                if(processPid == 0)
                {
                    char passIndex[12];
                    char passIncrement[12];
                    char passCount[12];
                    sprintf(passIndex, "%d", startIndex);
                    sprintf(passIncrement, "%d", increment);
                    sprintf(passCount, "%d", count);
                    execl("./bin_adder", "bin_adder", passIndex, passIncrement, passCount, "1", NULL);
                    fprintf(stderr, "%s: Error: execl failed.", name);
                }

                numChildren1++;
                activeChildren++;
                startIndex = startIndex + increment + increment;

            }

            for(i = 0; i < activeChildren; i++)
            {
                childPid = waitpid(-1, NULL, WNOHANG);
                if(childPid > 0)
                {
                    activeChildren--;
                }
            }

        }
        for(i = 0; i < activeChildren; i++)
        {
            childPid = wait();
        }
        totalChildren1 = totalChildren1 / 2;
        increment = increment + increment;
        activeChildren = 0;
        numChildren1 = 0;
    }
}
