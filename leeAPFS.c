#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <stdint.h>
#include <stdbool.h>

#include "gpt.h"
#include "APFS.h"
#include "checksum.h"
#include "BTree.h"


// busca en el B-tree esto fue prueba-error pero ya funciona bien
omap_val_t *encuentraFijo(uint64_t tree_phys_block, oid_t target_oid, uint32_t tam, char *base) {
    char *bt = base + (tree_phys_block * tam);
    btree_node_phys_t *node = (btree_node_phys_t *) bt;
    
    while (1) {
        // saco partes del nodo
        char *toc = (char *)&node->btn_data + node->btn_table_space.off;
        char *llaves = toc + node->btn_table_space.len;
        
        char *valores;

        // según yo el root guarda info al final
        if (node->btn_flags & BTNODE_ROOT) {
            valores = (char *)node + tam - sizeof(btree_info_t);
        } else {
            valores = (char *)node + tam;
        }

        kvoff_t *ent = (kvoff_t *) toc;

        // si es hoja ya no baja más
        if (node->btn_flags & BTNODE_LEAF) {
            omap_val_t *res = NULL;

            for(int i = 0; i < node->btn_nkeys; i++) {
                oid_t *k = (oid_t *)(llaves + ent[i].k);
                if(*k == target_oid) {
                    res = (omap_val_t *)(valores - ent[i].v);
                }
            }
            return res; 
        } else {
            // si no es hoja, busco hacia abajo
            uint64_t child_phys = 0;

            // recorro al revés porque así sí encuentra bien
            for(int i = node->btn_nkeys - 1; i >= 0; i--) {
                oid_t *k = (oid_t *)(llaves + ent[i].k);
                if(target_oid >= *k) {
                    child_phys = *(uint64_t *)(valores - ent[i].v);
                    break;
                }
            }

            // fallback por si no entró al if
            if (child_phys == 0 && node->btn_nkeys > 0) {
                child_phys = *(uint64_t *)(valores - ent[0].v);
            }
            
            if (child_phys == 0) return NULL;

            node = (btree_node_phys_t *)(base + child_phys * tam);
        }
    }
}



// busca archivos dentro de un directorio
void encuentraNoFijo(paddr_t root_dir_phys, uint64_t vol_tree_phys, int dir_id, uint32_t tam, char *base, info_archivo_t *directorio, int *total_archivos) {
    uint64_t target_id = dir_id; 
    char *rb = base + (root_dir_phys * tam);
    btree_node_phys_t *node = (btree_node_phys_t *)rb;

    while(1) {
        char *n_toc = (char *)&node->btn_data + node->btn_table_space.off;
        char *n_llaves = n_toc + node->btn_table_space.len;
        
        char *n_valores;

        if (node->btn_flags & BTNODE_ROOT) {
            n_valores = (char *)node + tam - sizeof(btree_info_t);
        } else {
            n_valores = (char *)node + tam;
        }

        kvloc_t *n_ent = (kvloc_t *)n_toc;

        if (node->btn_flags & BTNODE_LEAF) {
            // ya es hoja, aquí están los archivos
            for(int i = 0; i < node->btn_nkeys; i++) {
                char *k_ptr = n_llaves + n_ent[i].k.off;
                char *v_ptr = n_valores - n_ent[i].v.off;

                j_key_t *hdr = (j_key_t *)k_ptr;
                uint64_t obj_id = hdr->obj_id_and_type & OBJ_ID_MASK;
                uint64_t obj_type = hdr->obj_id_and_type >> OBJ_TYPE_SHIFT;

                if (obj_id == target_id && obj_type == APFS_TYPE_DIR_REC) {
                    j_drec_hashed_key_t *drec_k = (j_drec_hashed_key_t *)k_ptr;
                    j_drec_val_t *drec_v = (j_drec_val_t *)v_ptr;
                    
                    uint32_t name_len = drec_k->name_len_and_hash & J_DREC_LEN_MASK;

                    // Extraemos el nombre del archivo o directorio, su ID y tipo, lo guardamos en la estructura local para mostrarlo después
                    char file_name[256] = {0};
                    memcpy(file_name, drec_k->name, name_len);
                    file_name[name_len] = '\0';

                    directorio[*total_archivos].id = drec_v->file_id;
                    strcpy(directorio[*total_archivos].nombre, file_name);
                    directorio[*total_archivos].tipo = obj_type;
                    (*total_archivos)++;
                }
            }
            break; 
        } else {
            // nodo interior, bajo
            uint64_t child_oid = 0;

            for(int i = node->btn_nkeys - 1; i >= 0; i--) {
                char *k_ptr = n_llaves + n_ent[i].k.off;
                j_key_t *hdr = (j_key_t *)k_ptr;
                uint64_t obj_id = hdr->obj_id_and_type & OBJ_ID_MASK;
                
                if (target_id >= obj_id) {
                    char *v_ptr = n_valores - n_ent[i].v.off;
                    child_oid = *(uint64_t*)v_ptr;
                    break;
                }
            }

            if (child_oid == 0 && node->btn_nkeys > 0) {
                char *v_ptr = n_valores - n_ent[0].v.off;
                child_oid = *(uint64_t*)v_ptr;
            }

            if (child_oid == 0) break;

            omap_val_t *child_omap = encuentraFijo(vol_tree_phys, child_oid, tam, base);
            if (!child_omap) break;
            
            rb = base + (child_omap->ov_paddr * tam);
            node = (btree_node_phys_t *)rb;
        }
    }
}

// busca extent donde está el archivo físicamente
bool encuentraExtent(paddr_t root_dir_phys, uint64_t vol_tree_phys, uint64_t archivo_id, uint32_t tam, char *base, uint64_t *ext_phys_block, uint64_t *ext_size, uint64_t *ext_log_block) {
    char *rb = base + (root_dir_phys * tam);
    btree_node_phys_t *node = (btree_node_phys_t *)rb;

    while(1) {
        char *n_toc = (char *)&node->btn_data + node->btn_table_space.off;
        char *n_llaves = n_toc + node->btn_table_space.len;
        
        char *n_valores;

        if (node->btn_flags & BTNODE_ROOT) {
            n_valores = (char *)node + tam - sizeof(btree_info_t);
        } else {
            n_valores = (char *)node + tam;
        }

        kvloc_t *n_ent = (kvloc_t *)n_toc;

        if (node->btn_flags & BTNODE_LEAF) {
            for(int i = 0; i < node->btn_nkeys; i++) {
                char *k_ptr = n_llaves + n_ent[i].k.off;
                char *v_ptr = n_valores - n_ent[i].v.off;

                j_key_t *hdr = (j_key_t *)k_ptr;
                uint64_t obj_id = hdr->obj_id_and_type & OBJ_ID_MASK;
                uint64_t obj_type = hdr->obj_id_and_type >> OBJ_TYPE_SHIFT;

                if (obj_id == archivo_id && obj_type == APFS_TYPE_FILE_EXTENT) { 
                    j_file_extent_key_t *ext_k = (j_file_extent_key_t *)k_ptr;
                    j_file_extent_val_t *ext_v = (j_file_extent_val_t *)v_ptr;
                    
                    *ext_log_block = ext_k->logical_addr;
                    *ext_size = ext_v->len_and_flags & J_FILE_EXTENT_LEN_MASK;
                    *ext_phys_block = ext_v->phys_block_num;
                    return true;
                }
            }
            return false;
        } else {
            uint64_t child_oid = 0;

            for(int i = node->btn_nkeys - 1; i >= 0; i--) {
                char *k_ptr = n_llaves + n_ent[i].k.off;
                j_key_t *hdr = (j_key_t *)k_ptr;
                uint64_t obj_id = hdr->obj_id_and_type & OBJ_ID_MASK;
                
                if (archivo_id >= obj_id) {
                    char *v_ptr = n_valores - n_ent[i].v.off;
                    child_oid = *(uint64_t*)v_ptr;
                    break;
                }
            }

            if (child_oid == 0 && node->btn_nkeys > 0) {
                char *v_ptr = n_valores - n_ent[0].v.off;
                child_oid = *(uint64_t*)v_ptr;
            }

            if (child_oid == 0) return false;

            omap_val_t *child_omap = encuentraFijo(vol_tree_phys, child_oid, tam, base);
            if (!child_omap) return false;
            
            rb = base + (child_omap->ov_paddr * tam);
            node = (btree_node_phys_t *)rb;
        }
    }
}

// guarda el archivo en disco (para abrirlo en hex editor)
void extraer_archivo(char *base, uint64_t phys_block, uint32_t tam, uint64_t ext_size, const char* filename) {
    char *data = base + (phys_block * tam);
    FILE *f = fopen(filename, "wb");

    if (f) {
        fwrite(data, 1, ext_size, f);
        fclose(f);
    }
}


// Función principal para leer el contenedor APFS, validar su estructura y mostrar información relevante
int leeAPFS(char *base, nx_superblock_t *sb){
    memcpy(sb, base, sizeof(*sb));

    
    if(sb->nx_magic != NX_MAGIC){
        return -1;
    }

    // Valida el checksum del bloque del superbloque
    bool ck = is_cksum_valid((uint32_t *) base, sb->nx_block_size);
    if(ck == false){
        return -1;
    }

    // Valida y obten la transaccion mas alta reciente
    uint64_t ini_nx = sb->nx_xp_desc_base * sb->nx_block_size;
    uint64_t ultima_xid = 0;
    uint64_t idx_ultima_xid = 0;

    for(uint32_t i=0; i < sb->nx_xp_desc_blocks; i++){
        char *bl = base + ini_nx + i*sb->nx_block_size;
        bool ck_bl = is_cksum_valid((uint32_t *) bl, sb->nx_block_size);
        if(ck_bl == false){
            continue; 
        }

        obj_phys_t enc;
        memcpy(&enc, bl, sizeof(enc));
        if(enc.o_type & OBJECT_TYPE_NX_SUPERBLOCK){
            if(ultima_xid < enc.o_xid){
                ultima_xid = enc.o_xid;
                idx_ultima_xid = i;
            }
        }
    }

    //El superbloque que queremos esta en idx_ultima_xid
    char *nbs = base + (sb->nx_xp_desc_base + idx_ultima_xid) * sb->nx_block_size;
    memcpy(sb, nbs, sizeof(*sb));

    char *omap = base + sb->nx_omap_oid * sb->nx_block_size;
    if(is_cksum_valid((uint32_t *)omap, sb->nx_block_size) == false){
        return -1;
    }

    omap_phys_t mapa;
    memcpy(&mapa, omap, sizeof(mapa));

    btree_node_phys_t *arbb;
    char *bt = base + mapa.om_tree_oid * sb->nx_block_size;
    if(is_cksum_valid((uint32_t *)bt, sb->nx_block_size) == false){
        return -1;
    }

    arbb = (btree_node_phys_t *)bt;

    char *toc = (char *)&arbb->btn_data + arbb->btn_table_space.off;

    //Tercera son las llaves
    char *llaves = toc + arbb->btn_table_space.len;
    //La cuarta son los valores y este nodo es raiz
    char *valores = bt + sb->nx_block_size - sizeof(btree_info_t);

    btree_info_t *bt_info = (btree_info_t *)valores;

    //En el TOC tenemos Ivoff_t
    kvoff_t *ent = (kvoff_t *)toc;

    oid_t fs = sb->nx_fs_oid[0];

    //Obten la llave
    oid_t *k = (oid_t *)(llaves + ent->k);
    
    //Obten el oma_val DIRECTO de la memoria validada
    omap_val_t *om = (omap_val_t *)(valores - ent->v);

    //El valor del superbloque APFS
    char *sbv = base + om->ov_paddr * sb->nx_block_size;
    if(is_cksum_valid((uint32_t *)sbv, sb->nx_block_size) == false){
        return -1;
    }
    
    // Ahora tenemos el superbloque APFS, lo copiamos a una estructura para trabajar con él
    apfs_superblock_t apsb;
    memcpy(&apsb, sbv, sizeof(apsb));

    // Validamos el superbloque APFS
    //Aqui se le pone el breakpoint para revisar la informacion del superbloque APFS
    if(apsb.apfs_magic != APFS_MAGIC){
        mvprintw(18, 5, "APFS SB invalido");
        return -1;
    }

    
    uint64_t tam_total_particion = (uint64_t)sb->nx_block_count * sb->nx_block_size;
    
    // Si es la Primera particion si es aprox 39 MB, LBA 40 a 78063
    if (tam_total_particion == 39948288) {
        clear();
        uint32_t cont = 0;
        for (int j = 0; j < NX_MAX_FILE_SYSTEMS; j++) {
            if (sb->nx_fs_oid[j] != 0) {
                cont++;
            }
        }

        // Para mostrar la firma del contenedor
        char firma[5];
        memcpy(firma, &sb->nx_magic, sizeof(sb->nx_magic));
        firma[4] = 0;

        // Información del contenedor
        mvprintw(1, 5, "Informacion del contenedor:\n");
        mvprintw(2, 5, "No. Volumenes: %u", cont);
        mvprintw(3, 5, "Firma: %s Tamaño: %ld", firma, (long)sb->nx_block_count * sb->nx_block_size);
        mvprintw(4, 5, "UUID: ");
        // para imprimir el UUID de forma entendible
        for (int j = 0; j < 16; j++)
        {
            printw("%02X", sb->nx_uuid[j]);
            if (j == 3 || j == 5 || j == 7 || j == 9)
                printw("-");
        }
        
        // Información de los volumenes
        mvprintw(8, 5, "Informacion de los volumenes");
        mvprintw(9, 5, "Nombre del volumen %s", apsb.apfs_volname);
        mvprintw(10, 5, "No. de archivos: %llu", apsb.apfs_num_files);
        mvprintw(11, 5, "No. de directorios: %llu", apsb.apfs_num_directories);
        mvprintw(12, 5, "Tam del volumen: %llu", apsb.apfs_fs_alloc_count * sb->nx_block_size);
        mvprintw(13, 5, "UUID: ");
        for (int j = 0; j < 16; j++)
        {
            printw("%02X", apsb.apfs_vol_uuid[j]);
            if (j == 3 || j == 5 || j == 7 || j == 9)
                printw("-");
        }

        refresh();
        getch();
        return 0; 
    }
    
    // Si es la Segunda particion, muestra el menu interactivo para buscar archivos
    //Aqui se cambia la direccion id que te da la tabla de archivos
    uint64_t id_buscado = 0x02; 
    
    // Variable para saber qué tabla mostrar 1 o 2
    int tabla_activa = 2; 
    
    keypad(stdscr, TRUE);
    noecho();
    cbreak();

    while(1) {
        clear();

        // Según la tabla activa, ajustamos la base para leer el contenedor correcto
        char *base_trabajo = base;

        // La segunda partición empieza aprox en el byte 39 MB, así que ajustamos la base para leer desde ahí
        if (tabla_activa == 1) {
            base_trabajo = base - 39948288;
        }

        // Volvemos a validar el superbloque y obtener la información necesaria para navegar el sistema de archivos
        nx_superblock_t sb_t;
        memcpy(&sb_t, base_trabajo, sizeof(sb_t));

        // Valida el checksum del bloque del superbloque
        uint64_t ini_nx_t = sb_t.nx_xp_desc_base * sb_t.nx_block_size;
        uint64_t ultima_xid_t = 0;
        uint64_t idx_ultima_xid_t = 0;

        // Recorremos los bloques de transacciones para encontrar el más reciente con un superbloque válido
        for(uint32_t i=0; i < sb_t.nx_xp_desc_blocks; i++){
            char *bl = base_trabajo + ini_nx_t + i*sb_t.nx_block_size;
            if(is_cksum_valid((uint32_t *)bl, sb_t.nx_block_size) == false) continue;

            obj_phys_t enc;
            memcpy(&enc, bl, sizeof(enc));
            if(enc.o_type & OBJECT_TYPE_NX_SUPERBLOCK){
                if(ultima_xid_t < enc.o_xid){
                    ultima_xid_t = enc.o_xid;
                    idx_ultima_xid_t = i;
                }
            }
        }

        // El superbloque que queremos esta en idx_ultima_xid_t
        char *nbs_t = base_trabajo + (sb_t.nx_xp_desc_base + idx_ultima_xid_t) * sb_t.nx_block_size;
        memcpy(&sb_t, nbs_t, sizeof(sb_t));

        // Ahora obtenemos el superbloque APFS para esta partición
        char *omap_ptr = base_trabajo + sb_t.nx_omap_oid * sb_t.nx_block_size;
        omap_phys_t mapa_t;
        memcpy(&mapa_t, omap_ptr, sizeof(mapa_t));

        // Validamos el B-tree del omap
        char *bt_t = base_trabajo + mapa_t.om_tree_oid * sb_t.nx_block_size;
        btree_node_phys_t *arbb_t = (btree_node_phys_t *)bt_t;
        char *toc_t = (char *)&arbb_t->btn_data + arbb_t->btn_table_space.off;
        char *valores_t = bt_t + sb_t.nx_block_size - sizeof(btree_info_t);
        kvoff_t *ent_t = (kvoff_t *)toc_t;

        // Obtenemos el omap_val del superbloque APFS para esta partición
        omap_val_t *om_t = (omap_val_t *)(valores_t - ent_t[0].v);
        char *sbv_t = base_trabajo + om_t->ov_paddr * sb_t.nx_block_size;

        // Validamos el superbloque APFS
        apfs_superblock_t apsb_t;
        memcpy(&apsb_t, sbv_t, sizeof(apsb_t));

        // Información para navegar el sistema de archivos
        uint32_t tam_t = sb_t.nx_block_size;
        uint64_t ini_vol_t = apsb_t.apfs_omap_oid * tam_t;
        omap_phys_t *om_vol_t = (omap_phys_t *)(base_trabajo + ini_vol_t);
        oid_t dir_raiz_t = apsb_t.apfs_root_tree_oid;

        // Obtenemos el omap_val del directorio raíz para esta partición
        omap_val_t *drv_t = encuentraFijo(om_vol_t->om_tree_oid, dir_raiz_t, tam_t, base_trabajo);
        if (!drv_t) {
            mvprintw(0,0, "Error leyendo volumen de particion %d", tabla_activa);
            getch();
            break;
        }
        
        // Información del directorio raíz
        paddr_t root_dir_t = drv_t->ov_paddr;
        uint64_t ext_phys_block = 0, ext_size = 0, ext_log_block = 0;

        // Buscamos el ID del archivo en el directorio raíz para obtener su ubicación física y lógica, así como su tamaño.
        bool es_archivo = encuentraExtent(root_dir_t, om_vol_t->om_tree_oid, id_buscado, tam_t, base_trabajo, &ext_phys_block, &ext_size, &ext_log_block);

        // Si es un archivo, mostramos su información y damos la opción de extraerlo. Si es un directorio, listamos su contenido.
        if (es_archivo && id_buscado != 0x02) {
            mvprintw(1, 5, "DirLog 0x%llx, DirFis 0x%llx", ext_log_block, ext_phys_block);
            
            // Para extraer el archivo, construimos un nombre de salida basado en su ID y lo guardamos en el mismo directorio del programa.
            char nombre_salida[128];
            sprintf(nombre_salida, "archivo_extraido_0x%llx.bin", id_buscado);
            
            // Extraemos el archivo usando la función que lee directamente de la memoria validada y lo guarda en disco
            extraer_archivo(base_trabajo, ext_phys_block, tam_t, ext_size, nombre_salida);
            
            // SE imprime un mensaje indicando que el archivo fue extraído y guardado, y se da la opción de salir o buscar en la otra partición
            mvprintw(3, 5, "[!] El archivo fue extraido y guardado como '%s'.", nombre_salida);
            mvprintw(5, 5, "-> Presiona 'Q' para salir o [< IZQ / DER >] para buscar en la otra particion.");
            
        } else {
            // Es un directorio Imprime la tabla Raíz 0x02 y lista su contenido
            info_archivo_t directorio_t[100];
            int total_archivos_t = 0;

            // Listamos siempre el root a 0x02 de la partición activa
            encuentraNoFijo(root_dir_t, om_vol_t->om_tree_oid, 0x02, tam_t, base_trabajo, directorio_t, &total_archivos_t);

            //Se imprime la informacion del directorio raíz y se listan los archivos encontrados, mostrando su nombre e ID. También se da la opción de cambiar de partición o salir.
            mvprintw(0, 5, "EXPLORANDO TABLA RAIZ: (Particion %d) ", tabla_activa);
            mvprintw(1, 5, "[< IZQ] Primera tabla raiz  |  [DER >] Segunda tabla raiz  |  [Q] Salir");

            if (total_archivos_t == 0) {
                mvprintw(3, 5, "Directorio vacio o ID no encontrado en Particion %d.", tabla_activa);
            } else {
                for(int i = 0; i < total_archivos_t; i++) {
                    mvprintw(3 + i, 5, "Nombre %-30s , ID 0x%llx", directorio_t[i].nombre, directorio_t[i].id);
                }
            }
        }

        refresh();

        // Esperamos la entrada del usuario para cambiar de partición o salir
        int tecla = getch();

        // Según la tecla presionada, cambiamos la tabla activa para mostrar la otra partición o salimos del programa
        if (tecla == 'q' || tecla == 'Q') {
            break;
        } else if (tecla == KEY_LEFT) {
            tabla_activa = 1; 
        } else if (tecla == KEY_RIGHT) {
            tabla_activa = 2; 
        }
    }

    return 0;
}
