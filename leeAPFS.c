#include <string.h>
#include <ncurses.h>
#include "gpt.h"
#include "APFS.h"
#include "checksum.h"

nx_superblock_t sb;

int leeAPFS(char *base){

    memcpy(&sb, base, sizeof(sb));

    char firma[5];
    
    memcpy(firma, &sb.nx_magic, sizeof(sb.nx_magic));
    firma[4] = 0;

    mvprintw(10, 5, "Firma: %s Tamaño: %ld", firma, sb.nx_block_count*sb.nx_block_size);

    if(sb.nx_magic != NX_MAGIC){
        mvprintw(12, 5, "No es un sistema de archivos APFS");
        return -1;
    }

    bool ck = is_cksum_valid((uint32_t *) base, sb.nx_block_size);

    if(ck == false){
        return -1;
    }

    //Valida y obten la transaccion mas alta(reciente)
    uint64_t ini =sb.nx_xp_desc_base * sb.nx_block_size;
    uint64_t ultima_xid = 0;
    uint64_t idx_ultima_xid = 0;

    for(uint32_t i=0; i < sb.nx_xp_desc_blocks; i++){
        //Valida los bloques 
        char *bl = base + ini + i*sb.nx_block_size;
        bool ck = is_cksum_valid((uint32_t *) bl, sb.nx_block_size);
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
    char *nbs = base + (sb.nx_xp_desc_base + idx_ultima_xid) * sb.nx_block_size;
    memcpy(&sb, nbs, sizeof(sb));

    char *omap = base + sb.nx_omap_oid * sb.nx_block_size;
    if(is_cksum_valid((uint32_t *)omap, sb.nx_block_size) == false){
        return -1;
    }

    omap_phys_t mapa;
    memcpy(&mapa, omap, sizeof(mapa));

    btree_node_phys_t arbb;
    char *bt = base + mapa.om_tree_oid * sb.nx_block_size;
    if(is_cksum_valid((uint32_t *)bt, sb.nx_block_size) == false){
        return -1;
    }

    memcpy(&arbb, bt, sizeof(arbb));

    int fs = sb.nx_fs_oid[0];

    char *ns = base + (75)*sb.nx_block_size;
    memcpy(&sb, ns, sizeof(sb));



    refresh();
    getch();
    return 0;

}