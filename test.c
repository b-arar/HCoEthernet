#include <time.h>
#include <stdlib.h>
#include <stdio.h>

char data[8][1024];

//char buffer1[8][8];
char buffer2[8][8];

char high_f[] = {-1, 2, -1};
char low_f[] =  {-1, 2, 6, 2, -1};


signed char buffer1[8][8];

struct timespec res;
struct timespec before;
struct timespec after;

void print_tv(struct timespec tv);
void perform_transform(void);

int main() {
    char temp = 0;
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 1024; j++) {
            data[i][j] = temp % 256;
            temp++;
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &before);
    for (int i = 0; i < 1024; i += 8) {
        for (int j = 0; j < 8; j++)
            for (int k = 0; k < 8; k++)
                buffer1[j][k] = data[j][i + k];
        perform_transform();
        for (int j = 0; j < 8; j++)
            for (int k = 0; k < 8; k++)
                data[j][i + k] = buffer1[j][k];

    }
    clock_gettime(CLOCK_MONOTONIC, &after);
    printf("time = %ld - %ld = %ld\n", after.tv_nsec, before.tv_nsec,after.tv_nsec - before.tv_nsec );
    return 0;
}

void print_tv(struct timespec tv) {
    printf("seconds = %ld, microseconds = %ld\n", tv.tv_sec, tv.tv_nsec);
}

void perform_transform(void) {
    signed short temp = 0;
    for (int level = 8; level > 1; level /= 2) {
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                if (j < level && i < level) {
                    temp = 0;
                    if (j % 2 == 0) {
                        for (int w = 0; w < 5; w++) { // low-pass run
                            temp += buffer1[i][level - 1 - abs(level - 1 - ((j + w - 2) + 2*(level - 1))%(2*(level -1)))] * low_f[w];
                        }
                        temp = temp >> 3;
                        buffer2[j/2][i] = temp;
                    } else {
                        for (int w = 0; w < 3; w++) { // high-pass run
                            temp += buffer1[i][level - 1 - abs(level - 1 - ((j + w - 1) + 2*(level - 1))%(2*(level -1)))] * high_f[w];
                        }
                        temp = temp >> 1;
                        buffer2[j/2 + level/2][i] = temp;
                    }
                } else {
                    buffer2[j][i] = buffer1[i][j];
                }
            }
        }
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                if (j < level && i < level) {
                    temp = 0;
                    if (j % 2 == 0) {
                        for (int w = 0; w < 5; w++) { // low-pass run
                            temp += buffer2[i][level - 1 - abs(level - 1 - ((j + w - 2) + 2*(level - 1))%(2*(level -1)))] * low_f[w];
                        }
                        temp = temp >> 3;
                        buffer1[j/2][i] = temp;
                    } else {
                        for (int w = 0; w < 3; w++) { // high-pass run
                            temp += buffer2[i][level - 1 - abs(level - 1 - ((j + w - 1) + 2*(level - 1))%(2*(level -1)))] * high_f[w];
                        }
                        temp = temp >> 1;
                        buffer1[j/2 + level/2][i] = temp;
                    }
                } else {
                    buffer1[j][i] = buffer2[i][j];
                }
            }
        }
    }
}
/*
void perform_transform(void) {
    char temp = 0;
    for (int i = 8; i > 1; i /= 2) {
        for (int j = 0; j < i; j++) {
            for (int k = 0; k < i; k++) {
                temp = 0;
                if (k % 2 == 0) {
                    for (int w = -2; w < 3; w++)
                        temp += (buffer1[j][(i - 1) - abs((i - 1) - (w + k + 2*(i - 1))&(2*(i - 1)))] * low_f[w + 2]) >> 1;
                    buffer2[k/2][j] = temp;
                } else {
                    for (int w = -1; w < 2; w++)
                        temp += (buffer1[j][(i - 1) - abs((i - 1) - (w + k + 2*(i - 1))&(2*(i - 1)))] * high_f[w + 1]) >> 3;
                    buffer2[i/2 + k/2][j] = temp;
                }
            }
        }
        for (int j = 0; j < i; j++) {
            for (int k = 0; k < i; k++) {
                temp = 0;
                if (k % 2 == 0) {
                    for (int w = -2; w < 3; w++)
                        temp += (buffer2[j][(i - 1) - abs((i - 1) - (w + k + 2*(i - 1))&(2*(i - 1)))] * low_f[w + 2]) >> 1;

                    buffer1[k/2][j] = temp;
                } else {
                    for (int w = -1; w < 2; w++)
                        temp += (buffer2[j][(i - 1) - abs((i - 1) - (w + k + 2*(i - 1))&(2*(i - 1)))] * high_f[w + 1]) >> 3;
                    buffer1[i/2 + k/2][j] = temp;
                }
            }
        }
    }
}

char temp = 0;
for (int level = 8; level > 1; level /= 2) {
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            if (j < level && i < level) {
                temp = 0;
                if (j % 2 == 0) {
                    for (int w = 0; w < 5; w++) { // low-pass run
                        temp += buffer1[i][level - 1 - abs(level - 1 - ((j + w - 2) + 2*(level - 1))%(2*(level -1)))] * low_f[w];
                    }
                    temp = temp >> 3;
                } else {
                    for (int w = 0; w < 3; w++) { // high-pass run
                        temp += buffer1[i][level - 1 - abs(level - 1 - ((j + w - 1) + 2*(level - 1))%(2*(level -1)))] * high_f[w];
                    }
                    temp = temp >> 1;
                }
                buffer2[j][i] = temp;
            } else {
                buffer2[j][i] = buffer1[i][j];
            }
        }
    }
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            if (j < level && i < level) {
                temp = 0;
                if (j % 2 == 0) {
                    for (int w = 0; w < 5; w++) { // low-pass run
                        temp += buffer2[i][level - 1 - abs(level - 1 - ((j + w - 2) + 2*(level - 1))%(2*(level -1)))] * low_f[w];
                    }
                    temp = temp >> 3;
                } else {
                    for (int w = 0; w < 3; w++) { // high-pass run
                        temp += buffer2[i][level - 1 - abs(level - 1 - ((j + w - 1) + 2*(level - 1))%(2*(level -1)))] * high_f[w];
                    }
                    temp = temp >> 1;
                }
                buffer1[j][i] = temp;
            } else {
                buffer1[j][i] = buffer2[i][j];
            }
        }
    }
}*/