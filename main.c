//Kyle Moore
//CPMSCI 4760 Project 3
//This program uses 2 different methods to add integers from a file.
//It uses semaphores to protect the log file.


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

//struture for the semaphores
struct sembuf sem;


void semLock();         //function to lock semaphore
void semRelease();      //function to release the semaphore
void method1(int totalChildren1, char* name, int increment);    //function to add the integers using method 1

//interrupt hander for the alarm
void timer_handler(int signum)
{
    fprintf(stderr, "Error: Program timed out.\n");
    shmdt(sharedInt);
    shmctl(intShmid, IPC_RMID, NULL);
    semctl(fileSem, 0, IPC_RMID);
    kill(0, SIGKILL);
}

//interrupt handler for ctrl C
void ctrlc_handler(int signum)
{
    fprintf(stderr, "\n^C interrupt received.\n");
    shmdt(sharedInt);
    shmctl(intShmid, IPC_RMID, NULL);
    semctl(fileSem, 0, IPC_RMID);
    kill(0, SIGKILL);
}

int main(int argc, char *argv[0])
{
    int opt;
    int childPid;
    char numberFile[255] = "numbers.txt";
    FILE *numbers = NULL;
    FILE *output = NULL;

    //signal calls for the interrupt handlers
    signal(SIGALRM, timer_handler);
    signal(SIGINT, ctrlc_handler);

    //getopt to handle any options passed to the program
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

    //allows user to select the integer file
    if (argv[1] != NULL)
    {
        strncpy(numberFile, argv[1], 255);
    }

    alarm(100);     //sets an alarm for 100 seconds.


    key_t semFileKey = ftok("master", 1);                   //get a key for semaphore
    fileSem = semget(semFileKey, 1, IPC_CREAT | 0666);      //creates the shared semaphore
    if (fileSem == -1)
    {
        fprintf(stderr, "%s: Error: ", argv[0]);
        perror("");
        exit(EXIT_FAILURE);
    }

    semctl(fileSem, 0, SETVAL, 1);          //set the value of semaphore to 1

    numbers = fopen(numberFile, "r");       //opens file to read integers
    if(numbers == NULL)
    {
        fprintf(stderr, "%s: Error: ", argv[0]);
        perror("");
        semctl(fileSem, 0, IPC_RMID);
        exit(EXIT_FAILURE);
    }

    output = fopen("adder_log.txt", "w");   //opens log file
    if(output == NULL)
    {
        fprintf(stderr, "%s: Error: ", argv[0]);
        perror("");
        semctl(fileSem, 0, IPC_RMID);
        exit(EXIT_FAILURE);
    }
    fclose(output); //closes log file

    //counts the number of integers in the file
    while(fgets(temp, 40, numbers) != NULL)
    {
        count = count + 1;
    }

    key_t sharedIntKey = ftok("master", 2);         //gets a key for shared memory
    if(sharedIntKey == -1)
    {
        fprintf(stderr, "%s: Error: ", argv[0]);
        perror("");
        semctl(fileSem, 0, IPC_RMID);
        exit(EXIT_FAILURE);
    }

    intShmid = shmget(sharedIntKey, sizeof(int) * count, 0666|IPC_CREAT);   //creates shared memory
    if(intShmid == -1)
    {
        fprintf(stderr, "%s: Errror: ", argv[0]);
        perror("");
        semctl(fileSem, 0, IPC_RMID);
        exit(EXIT_FAILURE);
    }

    sharedInt = (int*) shmat(intShmid, NULL, 0);            //attaches to shared memory
    if(sharedInt == -1)
    {
        fprintf(stderr, "%s: Error: ", argv[0]);
        perror("");
        shmctl(intShmid, IPC_RMID, NULL);
        exit(EXIT_FAILURE);
    }

    rewind(numbers);     //goes back to the start of the file
    int i = 0;
    int* original[count];

    //puts all the ints from the file into shared memory
    for(i = 0; i < count; i++)
    {
        fgets(temp, 40, numbers);
        sharedInt[i] = atoi(temp);
        original[i] = sharedInt[i];
    }
    fclose(numbers); //closes the file
    numbers = NULL;


    totalChildren1 = count / 2;             //calculate how many groups for method 1.
    method1(totalChildren1, argv[0], 1);    //sum integers using method 1
    printf("The total for method 1 was %d\n", sharedInt[0]); //print results

    //put the original ints back in shared memory
    for(i = 0; i < count; i++)
    {
        sharedInt[i] = original[i];
    }

    int groups;

    increment = ceil(log(count));           //calculate how many numbers to be summed per group
    groups = ceil(count/ceil(log(count)));  //calculate group size for method 2

    //sum integers using method 2
    while(numChildren1 < groups)
    {
        if(activeChildren < 19)         //keep chldren under 20
        {
            pid_t processPid = fork();  //create new child

            if(processPid == -1)
            {
                fprintf(stderr, "%s: Error: ", argv[0]);
                perror("");
                shmdt(sharedInt);
                shmctl(intShmid, IPC_RMID, NULL);
                semctl(fileSem, 0, IPC_RMID);
                exit(EXIT_FAILURE);
            }

            //exec to bin_adder program
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
            numChildren1++;         //count children
            activeChildren++;
            startIndex = startIndex + increment;    //calculate next index
        }
        for(i = 0; i < activeChildren; i++)     //check for finished children
        {
            childPid = waitpid(-1, NULL, WNOHANG);
            if(childPid > 0)
            {
                activeChildren--;
            }
        }

    }
    //wait for any remaining children
    for(i = 0; i < activeChildren; i++)
    {
        childPid = wait();
    }

    method1(groups, argv[0], increment); //sum the results of method 2 using method 1
    printf("The total for method 2 was %d\n", sharedInt[0]);  //print results

    //remove shared memory and semaphore
    shmdt(sharedInt);
    shmctl(intShmid, IPC_RMID, NULL);
    semctl(fileSem, 0, IPC_RMID);
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

//Method 1 for summing integers
void method1(int totalChildren1, char* name, int increment)
{
    int numChildren1 = 0;
    int startIndex;
    int childPid;
    int activeChildren = 0;
    int i;
    while(totalChildren1 > 0)       //keeps running while more children are needed
    {
        startIndex = 0;
        while(numChildren1 < totalChildren1)
        {
            if(activeChildren < 19) //keeps children unter 20
            {
                pid_t processPid = fork();  //new child

                if(processPid == -1)
                {
                    fprintf(stderr, "%s: Error: ", name);
                    perror("");
                    shmdt(sharedInt);
                    shmctl(intShmid, IPC_RMID, NULL);
                    semctl(fileSem, 0, IPC_RMID);
                    exit(EXIT_FAILURE);
                }

                //execs to the bin_adder program
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

                numChildren1++;         //keeps track of children
                activeChildren++;
                startIndex = startIndex + increment + increment;  //caclulates next index

            }

            //checks for finished children
            for(i = 0; i < activeChildren; i++)
            {
                childPid = waitpid(-1, NULL, WNOHANG);
                if(childPid > 0)
                {
                    activeChildren--;
                }
            }

        }
        //waits for the rest of the children
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
