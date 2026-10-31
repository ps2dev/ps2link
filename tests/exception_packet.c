#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef uint32_t u32;
typedef struct {
    u32 epc, cause, badvaddr, sr, regs[32], hi, lo, dcic;
} exception_frame_t;

#include "../iop/exception_packet.h"

static int check_name(const char *src, unsigned int source_length,
                      unsigned int expected_length)
{
    struct {
        unsigned char before;
        pko_iop_exception_packet_t packet;
        unsigned char after;
    } data;
    unsigned int length;

    memset(&data, 0xa5, sizeof(data));
    length = pko_copy_exception_name(&data.packet, src, source_length);
    return length == expected_length &&
           data.packet.module_name[length] == '\0' &&
           data.before == 0xa5 && data.after == 0xa5 &&
           memcmp(data.packet.module_name, src, length) == 0;
}

int main(void)
{
    char long_name[256];
    unsigned int i;

    for (i = 0; i < sizeof(long_name) - 1; i++)
        long_name[i] = 'X';
    long_name[sizeof(long_name) - 1] = '\0';

    /* The EE receiver uses the existing wire offsets, including word 43. */
    if (sizeof(exception_frame_t) != 156 ||
        offsetof(pko_iop_exception_packet_t, frame) != 4 ||
        offsetof(pko_iop_exception_packet_t, relative_epc) != 160 ||
        offsetof(pko_iop_exception_packet_t, name_length) != 164 ||
        offsetof(pko_iop_exception_packet_t, module_name) != 168 ||
        sizeof(pko_iop_exception_packet_t) != 296) {
        fputs("FAIL IOP exception wire layout\n", stderr);
        return 1;
    }

    if (!check_name("", 0, 0) || !check_name("unknown", 7, 7) ||
        !check_name(long_name, sizeof(long_name) - 1, 127)) {
        fputs("FAIL IOP exception name bounds\n", stderr);
        return 1;
    }
    long_name[123] = '\0';
    if (!check_name(long_name, 123, 123)) {
        fputs("FAIL boundary-length module name\n", stderr);
        return 1;
    }
    puts("PASS exception packet layout and bounded module names");
    return 0;
}
