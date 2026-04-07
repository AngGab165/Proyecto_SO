#define BTREE_H
#include <stdint.h>

typedef struct {
    char nombre[256];
    uint64_t id;
    uint8_t tipo;
} info_archivo_t;