/*
 * oss.c
 * Author: Cynthia Brown
 * Date: 2026-02-24
 *
 * This program simulates an operating system that manages worker processes.
 * It creates and manages a shared memory segment for a custom clock and a process table.
 * The program handles signals for graceful shutdown and manages worker processes.
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>


int total_p = 0; // total number of processes created
int max_sp = 0; // maximum number of processes that can be running at the same time
float time_limit = 0.0; // time limit for the worker to run
float launch_interval = 0.0; // interval at which new processes are launched
int shm_key; // key for shared memory segment
int shm_id; // shared memory segment ID

struct CustomClock{
    int seconds;
    int nanoseconds;
};

void incrementClock(struct CustomClock *shmClock) {
    const int INCREMENT_NANO = 700; // 700 nanoseconds = 10 milliseconds
    
    // Increment the clock by the specified amount
    // Had to stop myself from making this with overkill int overflow checks, but I did make sure to handle the case where nanoseconds exceed 1 second.
    shmClock->nanoseconds += INCREMENT_NANO;
    
    if (shmClock->nanoseconds >= 1000000000) {
        shmClock->seconds += 1;
        shmClock->nanoseconds -= 1000000000;   
    }
}

struct PCB
{
    int occupied; // either true or false
    pid_t pid; // process ID
    int startSeconds; // time when process was forked
    int startNanoseconds; // time when process was forked
    int endingSeconds; // time when process should end
    int endingNanoseconds; // time when process should end
};

struct PCB processTable[20]; // process table with 20 entries

int findEmptySlot(struct PCB table[], int size)
{
    for (int i = 0; i < size; i++)
    {
        if (table[i].occupied == 0) {
            return i; // return the index of the first empty slot
        }
    }
    return -1; // return -1 if no empty slot is found
}


void signal_handler(int sig) {
    // access process table and kill all child processes
    for (int i = 0; i < 20; i++) {
        if (processTable[i].occupied) {
            kill(processTable[i].pid, SIGTERM);
        }
    }

    shmctl(shm_id, IPC_RMID, NULL); // remove shared memory segment

    exit(0);
}


int main(int argc, char *argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "hn:s:t:i:")) != -1) {
        switch (opt) {
            case 'h':
                printf("Usage: %s -n <proc> -s <simul> -t <timelimitForChildren> -i <intervalInSecondsToLaunchChildren>\n", argv[0]);
                exit(0);
            case 'n':
                total_p = atoi(optarg); 
                break;
            case 's':
                max_sp = atoi(optarg); 
                break;
            case 't':
                time_limit = atof(optarg); 
                break;
            case 'i':
                launch_interval = atof(optarg); 
                break;
        }
    }

    printf("OSS starting, PID: %d PPID: %d\n", getpid(), getppid());
    printf("Called with:\n-n %d\n-s %d\n-t %.2f\n-i %.2f\n", total_p, max_sp, time_limit, launch_interval);

    shm_key = ftok("makefile", 1337); // generate a unique key for shared memory
    shm_id = shmget(shm_key, sizeof(struct CustomClock), IPC_CREAT | 0666); // create shared memory segment
    struct CustomClock *shmClock = (struct CustomClock *)shmat(shm_id, NULL, 0); // attach to shared memory segment

    // close program after 60 seconds to prevent infinite loop
    signal(SIGINT, signal_handler); // handle Ctrl+C
    alarm(60); // set alarm for 60 seconds


    // initialize the clock to 0 seconds and 0 nanoseconds
    shmClock->seconds = 0;
    shmClock->nanoseconds = 0;

    // initialize process table
    for (int i = 0; i < 20; i++) {
        processTable[i].occupied = 0;
        processTable[i].pid = 0;
        processTable[i].startSeconds = 0;
        processTable[i].startNanoseconds = 0;
        processTable[i].endingSeconds = 0;
        processTable[i].endingNanoseconds = 0;
    }

    int activeWorkers = 0; // number of currently active worker processes
    int totalWorkersLaunched = 0; // total number of worker processes launched
    int nextLaunchSec = 0;
    int nextLaunchNano = 0;
    int lastReportSec = 0;
    int lastReportNano = 0;
    int combinedTimeSec = 0; // combined time workers ran in seconds
    int combinedTimeNano = 0; // combined time workers ran in nanoseconds
    char secStr[20];
    char nanoStr[20];
    int s = (int)time_limit;
    int ns = (int)((time_limit - s) * 1000000000);
    sprintf(secStr, "%d", s);
    sprintf(nanoStr, "%d", ns);

    while (totalWorkersLaunched < total_p || activeWorkers > 0)
    {
        incrementClock(shmClock); // increment the clock
        
        int status;
        pid_t pid;
        // check for any child processes that have finished without blocking
        while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
            // find the corresponding PCB entry and mark it as unoccupied
            for (int i = 0; i < 20; i++) {
                if (processTable[i].occupied == 1 && processTable[i].pid == pid) {
                    int runSec = shmClock->seconds - processTable[i].startSeconds;
                    int runNano = shmClock->nanoseconds - processTable[i].startNanoseconds;
                    if (runNano < 0) {
                        runSec -= 1;
                        runNano += 1000000000;
                    }
                    combinedTimeSec += runSec;
                    combinedTimeNano += runNano;    
                    if (combinedTimeNano >= 1000000000) {
                        combinedTimeSec += 1;
                        combinedTimeNano -= 1000000000;
                    }
                    processTable[i].occupied = 0;
                    processTable[i].pid = 0;
                    processTable[i].startSeconds = 0;
                    processTable[i].startNanoseconds = 0;
                    processTable[i].endingSeconds = 0;
                    processTable[i].endingNanoseconds = 0;
                    activeWorkers--;
                    break;
                }
            }
        }
   
    

        int slot = findEmptySlot(processTable, 20); // find an empty slot in the process table

        if (totalWorkersLaunched < total_p && activeWorkers < max_sp && slot != -1 && 
            (shmClock->seconds > nextLaunchSec || (shmClock->seconds == nextLaunchSec && shmClock->nanoseconds >= nextLaunchNano))) {
            // fork new worker process
            pid_t pid = fork();
            if (pid == 0) {
                // child process
                execl("./worker", "./worker", secStr, nanoStr, (char *)NULL);
                perror("execl failed");
                exit(1);
            } else if (pid > 0) {
                // parent process
                processTable[slot].occupied = 1;
                processTable[slot].pid = pid;
                processTable[slot].startSeconds = shmClock->seconds;
                processTable[slot].startNanoseconds = shmClock->nanoseconds;
                processTable[slot].endingSeconds = processTable[slot].startSeconds + (int)time_limit; // set ending time based on time limit
                processTable[slot].endingNanoseconds = processTable[slot].startNanoseconds + (int)((time_limit - (int)time_limit) * 1000000000); // add fractional part of time limit to nanoseconds
                // Normalize (The Carry)
                if (processTable[slot].endingNanoseconds >= 1000000000) {
                    processTable[slot].endingSeconds += 1;
                    processTable[slot].endingNanoseconds -= 1000000000;
                }
                activeWorkers++;
                totalWorkersLaunched++;

                nextLaunchSec = processTable[slot].startSeconds + (int)launch_interval; // set next launch time based on launch interval
                nextLaunchNano = processTable[slot].startNanoseconds + (int)((launch_interval - (int)launch_interval) * 1000000000); // add fractional part of launch interval to nanoseconds
                if (nextLaunchNano >= 1000000000) {
                    nextLaunchSec += 1;
                    nextLaunchNano -= 1000000000;
                }
            } else {
                // fork failed
                processTable[slot].occupied = 0;
            }
        }

        // Check if 0.5 simulated seconds (500,000,000 ns) have passed
        int elapsedSec = shmClock->seconds - lastReportSec;
        int elapsedNano = shmClock->nanoseconds - lastReportNano;

        // Adjust for borrow if nanoseconds is negative
        if (elapsedNano < 0) {
            elapsedSec -= 1;
            elapsedNano += 1000000000;
        }

        if (elapsedSec > 0 || elapsedNano >= 500000000) {
            // Report current status
            printf("OSS PID:%d SysClockS: %d SysclockNano: %d\n", getpid(), shmClock->seconds, shmClock->nanoseconds);
            printf("Process Table:\nEntry\tOccupied\tPID\tStartS\tStartN\tEndingS\tEndingN\n");
            for (int i = 0; i < 20; i++) {
                printf("%d\t%d\t\t%d\t%d\t%d\t%d\t%d\n", 
                    i, processTable[i].occupied, processTable[i].pid,
                    processTable[i].startSeconds, processTable[i].startNanoseconds,
                    processTable[i].endingSeconds, processTable[i].endingNanoseconds);
            }
            lastReportSec = shmClock->seconds;
            lastReportNano = shmClock->nanoseconds;
        }



    }

    printf("\nOSS PID:%d Terminating\n", getpid());
    printf("%d workers were launched and terminated.\n", totalWorkersLaunched);
    printf("Workers ran for a combined time of %d seconds %d nanoseconds.\n", 
       combinedTimeSec, combinedTimeNano);

    // printf("Program ran for a total of %d seconds %d nanoseconds.\n", shmClock->seconds, shmClock->nanoseconds);

    // clean up shared memory
    shmdt(shmClock);
    shmctl(shm_id, IPC_RMID, NULL);

    return 0;
}