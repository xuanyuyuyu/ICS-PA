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
    uint64_t last = get_time_us();

    while(1) {
        uint64_t now = get_time_us();

        if(now - last >= 500000) {
            printf("0.5 second passed, uptime = %llums\n", (unsigned long long)(now / 1000));
            fflush(stdout);
            last = now;
        }
    }

    return 0;
}