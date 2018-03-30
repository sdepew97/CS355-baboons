/*
* Referenced:
* http://pubs.opengroup.org/onlinepubs/009604599/functions/shm_open.html
* https://users.cs.duke.edu/~chase/cps110-archive/prob1_s00_sol.pdf (used to help base first solution off of)
* Rachel (TA)
* Dylan Slack
* https://www.chegg.com/homework-help/questions-and-answers/1-baboons-better-15-points-solution-baboon-problem-discussed-class-shown-setup-multiplex-s-q1783649
*/

//Defined variables
#define BABOONS 100
#define EAST 0
#define WEST 1

//Imports
#include <zconf.h>
#include <memory.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>

//Structures
struct region {        /* Defines "structure" of shared memory */
    int numBlocked[2]; //number waiting on each side
    int numDirection[2]; //number of baboons heading to a side
    sem_t mutex; //protect critical sections
    sem_t blocked[2];  //semaphores for each direction
    sem_t turstile; //provide strict alternation for the movement across the rope, so that only one process is awake and moving at a time
};

struct region *rptr;
int fd;

int directionMoving() {
    struct timeval now;
    unsigned int secs;

    gettimeofday(&now, NULL);
    secs = now.tv_usec;

    return rand_r(&(secs)) % 2; //number either 1 or 0
}

int randomTime() {
    struct timeval now;
    unsigned int secs;

    gettimeofday(&now, NULL);
    secs = now.tv_usec;

    return (rand_r(&(secs)) % 6) + 1; //number either 1-6
}

/* Rachel, Dylan, and https://users.cs.duke.edu/~chase/cps110-archive/prob1_s00_sol.pdf used as resource to help understand logic for blocking. */
void crossRope(int direction) {
    struct timeval now;
    int reverseDirection = !direction; //opposite direction
    sem_wait(&rptr->turstile); //only one baboon at a time allowed, which prevents starvation, since they strictly alternate
    sem_wait(&rptr->mutex); //protect the shared variables
    gettimeofday(&now, NULL);
    if (direction == EAST){
      printf("Eastward baboon created at %d with id %d.\n", now.tv_sec, getpid());
    }
    else {
        printf("Westward baboon created at %d with id %d.\n", now.tv_sec,  getpid());
    }
    while (rptr->numDirection[reverseDirection]) { //while there are baboons coming from the opposite direction...
        rptr->numBlocked[direction]++; //denote that a baboon from the direction is blocked
        sem_post(&rptr->mutex); //release the mutex
        sem_wait(&rptr->blocked[direction]); //block baboons from the direction, so that we don't reach a deadlock
        sem_wait(&rptr->mutex); //protect the rope and the baboon count on the rope
    }

    rptr->numDirection[direction]++; //denote that there is a baboon on the rope
    sleep(1); //one second to get on the rope
    gettimeofday(&now, NULL);
    if (direction == EAST)
      printf("Eastward baboon id %d crossing at %d.\n", getpid(), now.tv_sec);
    else
      printf("Westward baboon id %d crossing at %d.\n", getpid(), now.tv_sec);
    sem_post(&rptr->mutex); //unlock the rope

    sleep(4); //four seconds to cross the rope
    gettimeofday(&now, NULL);
    if (direction == EAST)
      printf("Eastward baboon id %d done crossing at %d.\n", getpid(), now.tv_sec);
    else
      printf("Westward baboon id %d done crossing at %d.\n", getpid(), now.tv_sec);
    sem_wait(&rptr->mutex);
    rptr->numDirection[direction]--; //one fewer baboon on the rope
    if (!rptr->numDirection[direction]) { //if there are no baboons in the direction unlock the other direction and allow baboons from that direction to go
      while (rptr->numBlocked[reverseDirection]--) { //release all the baboons of the other direction
    sem_post(&rptr->blocked[reverseDirection]); //keep the direction unblocked
        }
    }

    sem_post(&rptr->mutex); //unlock the shared variables
    sem_post(&rptr->turstile); //unlock the critical region
}

int main(void) {
    //set up shared memory map in parent
    /* Create shared memory object and set its size */
  //copied code from http://pubs.opengroup.org/onlinepubs/009604599/functions/shm_open.html
    fd = shm_open("/myregion", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        /* Handle error */
        printf("error on file descriptor setup\n");
        exit(EXIT_FAILURE);
    }

    if (ftruncate(fd, sizeof(struct region)) == -1) {
        /* Handle error */
        printf("error on ftruncate setup\n");
        exit(EXIT_FAILURE);
    }

    /* Map shared memory object */
    rptr = mmap(NULL, sizeof(struct region), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (rptr == MAP_FAILED) {
        /* Handle error */
        printf("error on rptr setup\n");
        exit(EXIT_FAILURE);
    }

    /* Now we can refer to mapped region using fields of rptr;
 for example, rptr->len */

    //initialize shared memory values in struct
    if (sem_init(&rptr->mutex, 1, 1) == -1) {
        printf("error on sem init\n");
        exit(EXIT_FAILURE);
    }

    if (sem_init(&rptr->blocked[0], 1, 0) == -1) {
        printf("error on sem init\n");
        exit(EXIT_FAILURE);
    }

    if (sem_init(&rptr->blocked[1], 1, 0) == -1) {
        printf("error on sem init\n");
        exit(EXIT_FAILURE);
    }

    if (sem_init(&rptr->turstile, 1, 1) == -1) { //the first process will be let through
        printf("error on sem init\n");
        exit(EXIT_FAILURE);
    }

    rptr->numBlocked[0] = 0;
    rptr->numBlocked[1] = 0;
    rptr->numDirection[0] = 0;
    rptr->numDirection[1] = 0;

    //Let 100 baboons cross the canyon
    for (int i = 0; i < BABOONS; i++) { //Local variables
        pid_t pid;
        int status;

        //create the "baboon"
        pid = fork();

        if (pid == 0) {
            //else if child called function and then exited child, crossRope
            crossRope(directionMoving());
            exit(EXIT_SUCCESS);
        } else if (pid > 0) { //if parent then sleep random time
            waitpid(pid, &status, 0); //wait on the forked child...
            sleep(randomTime());
        } else {    //something went wrong with forking
            exit(EXIT_FAILURE);
        }
    }

    printf("Baboons done crossing.\n");

    //clean up
    sem_destroy(&rptr->blocked[0]);
    sem_destroy(&rptr->blocked[1]);
    sem_destroy(&rptr->mutex);
    shm_unlink("/myregion");

    return 0;
}
