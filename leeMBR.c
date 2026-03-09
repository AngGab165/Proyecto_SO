#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <curses.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "mbr.h"
#include "gpt.h"

MASTER_BOOT_RECORD mbr;

struct gpt_header gpt;
struct gpt_partentry part[128];

char *mapFile(char *filePath) {
    /* Abre archivo */
    int fd = open(filePath, O_RDONLY);
    if (fd == -1) {
    	perror("Error abriendo el archivo");
	    return(NULL);
    }

    /* Mapea archivo */
    struct stat st;
    fstat(fd,&st);
    long fs = st.st_size;

    char *map = mmap(0, fs, PROT_READ, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
    	close(fd);
	    perror("Error mapeando el archivo");
	    return(NULL);
    }

  return map;
}

char *leeMBR(char *ruta){
    char *base = mapFile(ruta);
    memcpy(&mbr, base, sizeof(mbr));
    return base;
}

char *leeGPT(char *base){
   memcpy(&gpt, base, sizeof(struct gpt_header));

   clear();
   printw("----- GPT HEADER -----\n");
   printw("Signature: %lx\n", gpt.signature);
   printw("Revision: %x\n", gpt.revision);
   printw("Numero de particiones: %d\n", gpt.npartition_entries);
   printw("Tamano entrada: %d\n", gpt.sizeof_partition_entry);

   char *diskStart = base - 0x200;

   char *partBase = diskStart + (gpt.partition_entry_lba * 0x200);

   memcpy(part, partBase, sizeof(struct gpt_partentry) * gpt.npartition_entries);

   printw("----- Particiones GPT -----\n");
   for(int i = 0; i<gpt.npartition_entries; i++){
      if(part[i].start != 0){
         printw("\n Particion %d", i);
         printw("\n Inicio LBA: %lu", part[i].start);
         printw("\n Fin LBA: %lu", part[i].end);
         printw("\n");
      }
   }

   printw("\n Presiona una tecla para regresar...");
   refresh();
   getch();
}

int main()
{
   char *lista[] = {"Uno", "Dos", "Tres", "Cuatro" };
   char *base = leeMBR("DiscoAPFS.dmg");
   int i = 0;
   int c;
   initscr();
   raw();
   keypad(stdscr, TRUE);
   noecho(); /* No muestres el caracter leido */
   cbreak(); /* Haz que los caracteres se le pasen al usuario */
   do {
      mvprintw(3,5, "Particion Tipo Ci Hi Si LBA ini Cf Hf Sf Tamaño");
      for (int j=0; j < 4; j++) {
         if (j == i) {
           attron(A_REVERSE);
         }
         mvprintw(5+j,5, "%9x %4x %2x %2x %2x %7x %2x %2x %2x %7x",
         j,
         mbr.Partition[j].OSIndicator,
         mbr.Partition[j].StartTrack,
         mbr.Partition[j].StartHead,
         mbr.Partition[j].StartSector,
         mbr.Partition[j].StartingLBA,
         mbr.Partition[j].EndTrack,
         mbr.Partition[j].EndHead,
         mbr.Partition[j].EndSector,
         mbr.Partition[j].SizeInLBA
        );
         attroff(A_REVERSE);
      }
      move(5+i,5);
      refresh();
      c = getch();
      switch(c) {
         case KEY_UP:
            i = (i>0) ? i - 1 : 3;
            break;
         case KEY_DOWN:
            i = (i<3) ? i + 1 : 0;
            break;

         case KEY_ENTER:
         case 10:
         case 13:
            leeGPT(base + 0x200);
         clear();
         break;


         default:
            // Nothing 
            break;
      }
      move(10,5);
      printw("Estoy en %d: Lei %d",i,c);
   } while (c != 'q');
   endwin();
   return 0;
}


