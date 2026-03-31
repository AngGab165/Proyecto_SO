#include <string.h>
#include "APFS.h"
#include "checksum.h"




int leeAPFS(char *base, nx_superblock_t *sb);


int leeContenedor(char *base){

    nx_superblock_t sb;
    memcpy(&sb, base, sizeof(nx_superblock_t));

    if(sb.nx_magic != NX_MAGIC) return -1;

    if(!is_cksum_valid((uint32_t *)base, sb.nx_block_size)) return -1;

    uint64_t ini = sb.nx_xp_desc_base * sb.nx_block_size;
    uint64_t ultima_xid = 0;
    uint64_t idx = 0;

    for(uint32_t i=0; i < sb.nx_xp_desc_blocks; i++){
        char *bl = base + ini + i*sb.nx_block_size;

        if(!is_cksum_valid((uint32_t *)bl, sb.nx_block_size)) return -1;

        obj_phys_t enc;
        memcpy(&enc, bl, sizeof(enc));

        if(enc.o_type & OBJECT_TYPE_NX_SUPERBLOCK){
            if(enc.o_xid > ultima_xid){
                ultima_xid = enc.o_xid;
                idx = i;
            }
        }
    }

    char *nbs = base + (sb.nx_xp_desc_base + idx) * sb.nx_block_size;
    memcpy(&sb, nbs, sizeof(nx_superblock_t));

    return 0;
}

