#include <pthread.h>
#include <unistd.h>
#include <stdio.h>


int yo = 0;

void *sub_thread(int * ptr) {

    while (1) {
        usleep(500000);
        printf("sub ptr = %d, yo = %d\n", *((int * ) ptr), yo);
    }
}

int main() {
    pthread_t recv_thread;
    int val = 0;
    pthread_create(&recv_thread, NULL, sub_thread, (void *) &val);
    int i = 0;


    struct timespec rem;
    while (1) {
        usleep(1000000);
        yo++;
        printf("main i = %d\n", i);
        if (i == 10) {
            val++;
            i = 0;
        }
        i++;
    }
    return 0;
}