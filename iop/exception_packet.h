#ifndef PS2LINK_EXCEPTION_PACKET_H
#define PS2LINK_EXCEPTION_PACKET_H

/* The EE receiver expects the module name at word 43 of the SIF packet. */
#define PKO_IOP_EXCEPTION_NAME_BYTES 128

typedef struct {
    u32 command;
    exception_frame_t frame;
    u32 relative_epc;
    u32 name_length;
    char module_name[PKO_IOP_EXCEPTION_NAME_BYTES];
} pko_iop_exception_packet_t;

/* Always leave a NUL inside the fixed-size packet name field. */
static unsigned int pko_copy_exception_name(pko_iop_exception_packet_t *packet,
                                            const char *src,
                                            unsigned int source_limit)
{
    unsigned int len = 0;

    if (source_limit >= PKO_IOP_EXCEPTION_NAME_BYTES)
        source_limit = PKO_IOP_EXCEPTION_NAME_BYTES - 1;

    while (len < source_limit && src[len] != '\0') {
        packet->module_name[len] = src[len];
        len++;
    }
    packet->module_name[len] = '\0';
    return len;
}

#endif
