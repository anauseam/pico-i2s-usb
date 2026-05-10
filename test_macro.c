#include <stdio.h>
#include <stdint.h>
#define TU_ATTR_PACKED __attribute__((packed))
typedef struct TU_ATTR_PACKED { uint32_t bMin; uint32_t bMax; uint32_t bRes; } audio_control_subrange_4_t;
#define audio_control_range_4_n_t(num) struct TU_ATTR_PACKED { uint16_t wNumSubRanges; audio_control_subrange_4_t subrange[num]; }

int main() {
    audio_control_range_4_n_t(1) range = { .wNumSubRanges = 1, .subrange[0] = { .bMin = 8000, .bMax = 8000, .bRes = 0 } };
    printf("Size: %zu\n", sizeof(range));
    return 0;
}
