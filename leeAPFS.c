#include "gpt.h"
#include <string.h>
#include <ncurses.h>

#include "APFS.h"

nx_superblock_t sb;

int leeAPFS(char *base){

    memcpy(&sb, base, sizeof(sb));

    char firma[5];
    
    memcpy(firma, &sb.nx_magic, sizeof(sb.nx_magic));
    firma[4] = 0;

    mvprintw(10, 5, "Firma: %s Tamaño: %ld", firma, sb.nx_block_count*sb.nx_block_size);

    getch();
    return 0;

}