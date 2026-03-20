# Lector de MBR y GPT

Este programa esta hecho en C y sirve para leer una imagen del discoAPFS.dmg y mostrar la informacion de sus particiones.
Primero se muestra el MBR y se pueden seleccionar las particiones, despues entra a la tabla GPT y muetras las particiones
que contiene.

Para este programa se ocuparon los siguientes archivos:
- leeMBR.c esto tiene funciones de leer el MBR, es la pantalla principal.
- leeGPT.c es la lectura de la tabla GPT.
- leeAPFS.c es la lectura de la particion APFS.
- mbr.h es la estructura que tiene para que funcione MBR.
- gpt.h es la estructura que tiene para que funcione GPT.
- APFS.h es la estructura que tiene para que funcione APFS.
- Makefile sirve este para poder compilar todo el programa.

Los requisitos para que no se tengan problemas de probar este programa son:
- Se tienen que tener instalado la libreria ncurses.
- En caso de que no se tengan instalado esta libreria lo puedes instalar con el siguiente comando en la terminal
del sistema operativo Linux:
Para la libreria ncurses es el siguiente
apt install libncurses-dev

Para poder compilar este programa se debe escribir en la terminalel siguiente comando:
make

Para poder ejecutar este programa nos debemos de asegurar primero tener el archivo DiscoAPFS.dmg sin este no se
puede ejecutar este programa.

Luego de esto se nos crea un ejecutable y solo debemos escribir el siguiente comando en la terminal:
- ./leeMBR

Ya estando adentro de la tabla de particiones de MBR se tienen las siguientes funciones:
- Flechas ↑ ↓ para moverse entre particiones.
- ENTER para que podamos seleccionar una particion.
- q la tecla q sirve para salirse del programa.

Para la tabla de particiones GPT del disco que usamos que es DiscoAPFS.dmg tiene las siguientes funciones:
- Flechas ↑ ↓ para moverse entre particiones.
- ENTER para acceder a la particion o ver datos hexadecimales en la particion.
- q para regresar a la pantalla principal de particiones MBR o salirte del programa.

Este programa lee el disco directamente ya que usamos mmap, para las direcciones se ocupo el siguiente calculo:
base + LBA * 512.





