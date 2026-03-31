#include <string.h>
#include <ncurses.h>
#include "gpt.h"
#include "APFS.h"
#include "checksum.h"

// Función para buscar un valor en un nodo de árbol B dado su clave
omap_val_t *buscar_en_btree(char *bt, uint32_t block_size, oid_t clave){

    btree_node_phys_t *arbb = (btree_node_phys_t *) bt;

    char *toc = (char *)&arbb->btn_data + arbb->btn_table_space.off;
    char *llaves = toc + arbb->btn_table_space.len;
    char *valores = bt + block_size - sizeof(btree_info_t);

    kvoff_t *ent = (kvoff_t *) toc;

    //solo toma la primera entrada
    for(int i=0; i<arbb->btn_nkeys; i++){
        oid_t *k = (oid_t *)(llaves + ent[i].k);
        if(*k == clave){
            omap_val_t *om = (omap_val_t *)(valores - ent[i].v);
            return om;
        }
    }
    return NULL;
}

int leeAPFS(char *base, nx_superblock_t *sb){
    memcpy(sb, base, sizeof(*sb));

    char firma[5];
    
    memcpy(firma, &sb->nx_magic, sizeof(sb->nx_magic));
    firma[4] = 0;

    mvprintw(10, 5, "Firma: %s Tamaño: %ld", firma, sb->nx_block_count*sb->nx_block_size);

    // Contador de volumenes
    uint32_t cont = 0;
    for (int j = 0; j < NX_MAX_FILE_SYSTEMS; j++) {
        if (sb->nx_fs_oid[j] != 0) {
            cont++;
        }
    }

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
    refresh();

    if(sb->nx_magic != NX_MAGIC){
        mvprintw(12, 5, "No es un sistema de archivos APFS");
        return -1;
    }

    bool ck = is_cksum_valid((uint32_t *) base, sb->nx_block_size);

    if(ck == false){
        return -1;
    }

    //Valida y obten la transaccion mas alta(reciente)
    uint64_t ini =sb->nx_xp_desc_base * sb->nx_block_size;
    uint64_t ultima_xid = 0;
    uint64_t idx_ultima_xid = 0;

    for(uint32_t i=0; i < sb->nx_xp_desc_blocks; i++){
        //Valida los bloques 
        char *bl = base + ini + i*sb->nx_block_size;
        bool ck = is_cksum_valid((uint32_t *) bl, sb->nx_block_size);
        if(ck == false){
            return -1;
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
    /*if(*k != fs){
        return -1;
    }*/
    //Obten el oma_val
    omap_val_t *om = (omap_val_t *)(valores - ent->v);

    //El valor del superbloque APFS
    char *sbv = base + om->ov_paddr * sb->nx_block_size;
    if(is_cksum_valid((uint32_t *)sbv, sb->nx_block_size) == false){
        return -1;
    }
    apfs_superblock_t apsb;
    memcpy(&apsb, sbv, sizeof(apsb));

    if(apsb.apfs_magic != APFS_MAGIC){
    mvprintw(18, 5, "APFS SB invalido");
    return -1;
    }

    // Inpresion del contenedor y volumenes
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


    //La informacion esta en el apfs_omap_oid
    char *o = base + apsb.apfs_omap_oid * sb->nx_block_size;
    memcpy(&mapa, o, sizeof(mapa));

     //busca el bloque del superblock del volumen
    char *bts = base + mapa.om_tree_oid * sb->nx_block_size;
    if(is_cksum_valid((uint32_t *)bts, sb->nx_block_size) == false){
        return -1;
    }


    oid_t root_oid = apsb.apfs_root_tree_oid;

    omap_val_t *om_root = buscar_en_btree(bts, sb->nx_block_size, root_oid);
    if(om_root == NULL){
        return -1;
    }

    char *rb = base + om_root->ov_paddr * sb->nx_block_size;
    arbb = (btree_node_phys_t *)rb;

    toc = (char *)&arbb->btn_data + arbb->btn_table_space.off;
    //Tercera son las llaves
    llaves = toc + arbb->btn_table_space.len;
    //La cuarta son los valores y este nodo es raiz
    valores = rb + sb->nx_block_size - sizeof(btree_info_t);

    ent = (kvoff_t *)toc;
    
    for(int i = 0; i < arbb->btn_nkeys; i++){

        uint64_t *kd = (uint64_t *)(llaves + ent[i].k);

        oid_t id = (*kd & OBJ_ID_MASK);
        int tipo = (*kd >> OBJ_TYPE_SHIFT);

        if(id == root_oid){

            mvprintw(20,5,"Tipo root: %d", tipo);

            om = (omap_val_t *)(valores - ent[i].v);
            break;
        }
    }


    j_drec_hashed_key_t *dir = (j_drec_hashed_key_t *)(llaves + ent[0].k);


    refresh();
    getch();
    return 0;


}