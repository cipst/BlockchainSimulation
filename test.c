#include "master.h"

struct timespec remaining, request;

int main() {
    long transGenNsec, transGenSec;
    srand(time(NULL));
    transGenNsec = (rand() % (5000000000 - 1000000000 + 1)) + 1000000000;
    printf("1)NanoSec: %ld\n", transGenNsec);

    if (transGenNsec > 999999999) {
        transGenSec = transGenNsec / 1000000000;    /* trovo la parte in secondi */
        transGenNsec -= (transGenSec * 1000000000); /* tolgo la parte in secondi dai nanosecondi */
    }

    printf("2)NanoSec: %ld\n", transGenNsec);
    printf("Sec: %ld\n", transGenSec);

    request.tv_nsec = transGenNsec;
    request.tv_sec = transGenSec;
    remaining.tv_nsec = transGenNsec;
    remaining.tv_sec = transGenSec;

    if (nanosleep(&request, &remaining) < 0) {
        error("FAIL");
    }
}