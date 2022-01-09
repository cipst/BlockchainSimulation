#include "header.h"

transaction* pool;

int main(int argc, char** argv) {
    int SO_TP_SIZE = 10;

    pool = (transaction*)malloc(sizeof(transaction) * SO_TP_SIZE);
    (pool + 0)->timestamp = 1231871412314;
    printf("TIMESTAMP: %lu\n", ((pool + 0)->timestamp));

    memset((pool + 0), 0, sizeof(transaction));

    printf("TIMESTAMP: %lu\n", ((pool + 0)->timestamp));

    free(pool);

    return 0;
}