/*
 * worker.c
 * Author: Cynthia Brown
 * Date: 2026-02-24
 *
 * This program simulates a worker process that interacts with a shared clock in shared memory.
 * It takes two command-line arguments representing the worker's lifetime in seconds and nanoseconds.
 * The worker periodically checks the shared clock and terminates when its lifetime has elapsed.
 * The worker also prints its PID, PPID, current time, and termination time at the start, every simulated second, and at termination.
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>

struct CustomClock {
    int seconds;
    int nanoseconds;
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Worker Error: Missing time arguments\n");
        exit(1);
    }

    // Parse lifetime arguments passed from oss
    int lifetimeSeconds = atoi(argv[1]);
    int lifetimeNano = atoi(argv[2]);

    // Attach to Shared Memory
    key_t shm_key = ftok("makefile", 1337);
    int shm_id = shmget(shm_key, sizeof(struct CustomClock), 0666);
    if (shm_id == -1) {
        perror("Worker: shmget failed");
        exit(1);
    }
    struct CustomClock *shmClock = (struct CustomClock *)shmat(shm_id, NULL, 0);

    // Calculate Termination Time
    int termSec = shmClock->seconds + lifetimeSeconds;
    int termNano = shmClock->nanoseconds + lifetimeNano;

    // Handle nanosecond carry
    if (termNano >= 1000000000) {
        termSec += 1;
        termNano -= 1000000000;
    }

    // Initial Output 
    printf("WORKER PID:%d PPID:%d\n", getpid(), getppid());
    printf("SysClockS: %d SysclockNano: %d TermTimeS: %d TermTimeNano: %d\n--Just Starting\n",
           shmClock->seconds, shmClock->nanoseconds, termSec, termNano);

    int startSec = shmClock->seconds;
    int startNano = shmClock->nanoseconds;
    int currentReportSec = shmClock->seconds;

    // 4. Monitoring Loop (NO SLEEP ALLOWED) [cite: 50, 56]
    while (1) {
        // Check if a simulated second has passed for periodic output 
        if (shmClock->seconds > currentReportSec && startNano <= shmClock->nanoseconds) {
            currentReportSec = shmClock->seconds;
            int totalElapsed = currentReportSec - startSec;
            printf("WORKER PID:%d PPID:%d\n", getpid(), getppid());
            printf("SysClockS: %d SysclockNano: %d TermTimeS: %d TermTimeNano: %d\n--%d seconds have passed since starting\n",
                   shmClock->seconds, shmClock->nanoseconds, termSec, termNano, totalElapsed);
        }

        // Check for termination using int-based comparison
        if (shmClock->seconds > termSec || 
           (shmClock->seconds == termSec && shmClock->nanoseconds >= termNano)) {
            break;
        }
    }

    // Final message once time has elapsed
    printf("WORKER PID:%d PPID:%d\n", getpid(), getppid());
    printf("SysClockS: %d SysclockNano: %d TermTimeS: %d TermTimeNano: %d\n--Terminating\n",
           shmClock->seconds, shmClock->nanoseconds, termSec, termNano);

    shmdt(shmClock); // Detach before exiting
    return 0;
}