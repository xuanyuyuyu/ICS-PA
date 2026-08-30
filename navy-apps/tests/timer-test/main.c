#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>

static uint64_t get_time_us(void) {
    struct timeval tv;

    int ret = gettimeofday(&tv, NULL);
    if(ret != 0) {
        return 0;
    }

    return (uint64_t)tv.tv_sec * 1000000
       + (uint64_t)tv.tv_usec;
}

int main(void) {
    NDL_Init(0);

    uint32_t last = NDL_GetTicks();

    while(1) {
        uint64_t now = NDL_GetTicks();

        if(now - last >= 500) {
            printf("0.5 second passed, uptime = %llums\n", now);
            fflush(stdout);
            last = now;
        }
    }
    NDL_Quit();
    return 0;
}