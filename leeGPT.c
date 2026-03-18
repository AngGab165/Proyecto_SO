#include <string.h>
#include <ncurses.h>

#include "gpt.h"

extern int leeAPFS(char *base);

struct gpt_header gpt;

efi_partition_entry part1, part2, part3;

void printGUID(int y, int x, uint8_t *u){
    mvprintw(y, x,
    "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
    u[3],u[2],u[1],u[0],
    u[5],u[4],
    u[7],u[6],
    u[8],u[9],
    u[10],u[11],u[12],u[13],u[14],u[15]);
}

int leeGPT(char *base, int ini){
    clear();

    int i = 0;
    int c;
    memcpy(&gpt, base + ini * 512, sizeof(gpt));
    char firma[9];


    memcpy(firma, &gpt.signature, 8);
    firma[8] = 0;

    //Lee particion

    int bp = gpt.partition_entry_lba;
    //Primer aparticion
    memcpy(&part1, base+bp*512, sizeof(part1));
    //Lee la segunda partcion:
    memcpy(&part2, base+bp*512+sizeof(part1), sizeof(part2));
    //lee la tercera particion
    memcpy(&part3, base+bp*512+2*sizeof(part1), sizeof(part3));

    do{
        clear();
        mvprintw(2,5,"Firma %s", firma);

        mvprintw(2,20,"LBA de tabla %d", gpt.partition_entry_lba);

        //UUID del disco
        mvprintw(4,5,"UUID Disco:");
        printGUID(4,18, gpt.disk_guid.uuid);
    
        //Encabezado de la tabla
        mvprintw(7,5,"Tipo");
        mvprintw(7,50,"Ini LBA");
        mvprintw(7,65,"Fin LBA");

        for(int j = 0; j<3; j++){

            if(j == i) attron(A_REVERSE);

            if(j==0){
                //particion 1
                printGUID(9,5, part1.PartitionTypeGUID.uuid);
                mvprintw(9,50,"%ld", part1.start);
                mvprintw(9,65,"%ld", part1.end);
            }

            if(j==1){
                //particion 2
                printGUID(10,5, part2.PartitionTypeGUID.uuid);
                mvprintw(10,50,"%ld", part2.start);
                mvprintw(10,65, "%ld", part2.end);
            }

            if(j==2){
                //particion 3
                printGUID(11,5, part3.PartitionTypeGUID.uuid);
                mvprintw(11,50,"%ld", part3.start);
                mvprintw(11,65,"%ld", part3.end);
            }

            attroff(A_REVERSE);
        }
    
        refresh();
        c = getch();

        switch(c){
            case KEY_UP:
                i = (i > 0) ? i - 1 : 2;
                break;
            
            case KEY_DOWN:
                i = (i < 2) ? i + 1 : 0;
                break;

            case 10:
                clear();

                for(int i = 0; i < 16; i++){
                    mvprintw(i, 0, "%08X: ", i*16);

                    for(int j = 0; j < 16; j++){
                        printw("%02X ",
                        (unsigned char) base[part1.start * 512 + i*16 + j]);
                    }
                }
                refresh();
                getch();
                break;
        }

    }while(c != 'q');

        //leeAPFS(base+part1.start*512);
        //Inicio de la imagen + inicio de la particion

        return 0;

}
