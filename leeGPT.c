#include <string.h>
#include <ncurses.h>

#include "gpt.h"

extern int leeAPFS(char *base);

struct gpt_header gpt;

efi_partition_entry part1, part2, part3;

int leeGPT(char *base, int ini){
    clear();
    memcpy(&gpt, base + ini * 512, sizeof(gpt));
    char firma[9];


    memcpy(firma, &gpt.signature, sizeof(gpt.signature));
    firma[8] = 0;
    mvprintw(5,5,"Firma %s", firma);

    //Lee particion

    int bp = gpt.partition_entry_lba;

    //Primer aparticion

    memcpy(&part1, base+bp*512, sizeof(part1));

    
    //Lee la segunda partcion:
    memcpy(&part2, base+bp*512+sizeof(part1), sizeof(part2));

    //lee la tercera particion
    memcpy(&part3, base+bp*512+2*sizeof(part1), sizeof(part3));
    
    char *bb = base+bp*512+3*sizeof(part1);
    
    mvprintw(5,25,"Inicio %d", part1.start);

    getch();

    leeAPFS(base+part1.start*512);
    //Inicio de la imagen + inicio de la particion

    return 0;

}
