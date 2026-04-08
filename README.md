# Lector de MBR y GPT

Este programa esta hecho en C y sirve para leer una imagen del discoAPFS.dmg y mostrar la informacion de sus particiones.
Primero se muestra el MBR y se pueden seleccionar las particiones, despues entra a la tabla GPT y muetra la particion APFS en donde se puede navegar el sistema de archivos, listar directorios y hasta extraer archivos.

Para este programa se ocuparon los siguientes archivos:
- leeMBR.c esto tiene funciones de leer el MBR, es la pantalla principal.
- leeGPT.c es la lectura de la tabla GPT.
- leeAPFS.c es la lectura de la particion APFS.
- leeContenedor.c es la lectura y despliegue de la informacion del contenedor APFS y sus volumenes
- checksum.c esta contiene el algoritmo y las funciones para validar la integridad de los bloques leidos en APFS
- checksum.h es la cabecera que define las funciones de validacion utilizadas en el calculo
- mbr.h es la estructura que tiene para que funcione MBR.
- gpt.h es la estructura que tiene para que funcione GPT.
- APFS.h es la estructura que tiene para que funcione APFS.
- BTree.h es el que define estructuras para manejar nodos B-tree usados en APFS.
- Makefile sirve este para poder compilar todo el programa.

Los requisitos para que no se tengan problemas de probar este programa son:
- Se tienen que tener instalado la libreria ncurses.
- En caso de que no se tengan instalado esta libreria lo puedes instalar con el siguiente comando en la terminal
del sistema operativo Linux:
Para la libreria ncurses es el siguiente
apt install libncurses-dev

Para poder compilar este programa se debe escribir en la terminalel siguiente comando:
make

Para poder ejecutar este programa nos debemos de asegurar primero tener el archivo DiscoAPFS.dmg sin este no se puede ejecutar este programa.

Luego de esto se nos crea un ejecutable y solo debemos escribir el siguiente comando en la terminal:
- ./leeMBR

Ya estando adentro de la tabla de particiones de MBR se tienen las siguientes funciones:
- Flechas ↑ ↓ para moverse entre particiones.
- ENTER para que podamos seleccionar una particion.
- q la tecla q sirve para salirse del programa.

Para la tabla de particiones GPT del disco que usamos que es DiscoAPFS.dmg tiene las siguientes funciones:
- Flechas ↑ ↓ para moverse entre particiones.
Importante: ya que le dimos ENTER a la primera particion de APFS nos aparece la informacion completa del contenedor y sus volumenes como  la Firma, el Tamaño total, UUID, nombre del Volumen, cantidad de archivos y directorios, y el Root OID del disco.
Al dar ENTER a la segunda particion de APFS nos aparece la informacion de los archivos de las tablas raiz de la particion 1 y particion 2 para navegar y cambiar entre tablas es con flecha derecha ←  izquierda → .
Ya que vimos en las tablas raiz un ID de un archivo se cambia en la clase leeAPFS.c en la parte donde dice uint64_t id_buscado = 0x02; se recompila el programa con make y se vuelve a rejecutar con ./leeMBR y hacemos los mismo pasos de entrar a la segunda particion y nos va dar la direccion logica y fisica, te va dar un archivo binario que se guardo para que cheques en un editor hexadecimal la direccion logica y fisica y veas que este bien las direcciones.
- q para regresar a la pantalla principal de particiones MBR o salirte del programa.

Este programa lee el disco directamente ya que usamos mmap, entonces trabaj sobre memoria.
Para calcular los offsets se usa:
base + (bloque * tamaño_bloque)




