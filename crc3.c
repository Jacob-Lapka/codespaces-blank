#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

/* --- Bit layout you gave (kept for readability) ------------------------ */
typedef struct
{
    unsigned CRC : 3;     /* [0..2]  */
    unsigned : 5;         /* [3..7]  */
    unsigned DATA : 8;    /* [8..15] */
    unsigned ADDRESS : 7; /* [16..22]*/
    unsigned C : 1;       /* [23]    */
    unsigned : 8;         /* [24..31] not part of the SPI frame */
} IfxTLE9180_SpiTx_bits;

typedef union
{
    uint32_t U;
    IfxTLE9180_SpiTx_bits B;
} IfxTLE9180_SpiTx;

/* --- CRC3 for TLE9180 over bits [23..3], MSB-first ---------------------
   Polynomial: x^3 + x + 1    (0b1011)
   Seed: "101" -> start value "100" (per datasheet note)
   Process exactly 21 bits: 23,22,...,3
   No final XOR/inversion.
*/
static inline uint8_t tle9180_crc3_tx24(uint32_t frame_wo_crc)
{
    uint8_t lfsr = 0b100; // start value derived from seed "101"
    for (int i = 23; i >= 3; --i)
    { // *** only [23..3], not [31..3]
        uint8_t in = (frame_wo_crc >> i) & 1u;
        uint8_t fb = in ^ ((lfsr >> 2) & 1u); // XOR with MSB tap (x^3)
        uint8_t b2 = (lfsr >> 1) & 1u;        // shift down
        uint8_t b1 = ((lfsr >> 0) & 1u) ^ fb; // x^1 tap
        uint8_t b0 = fb;                      // x^0 tap
        lfsr = (uint8_t)((b2 << 2) | (b1 << 1) | b0);
    }
    return (uint8_t)(lfsr & 0x7u);
}

/* --- Build a 24-bit TX frame with CRC bits cleared --------------------- */
static inline uint32_t pack_tx24_wo_crc(uint8_t addr, uint8_t data, uint8_t c)
{
    // [23]=C, [22:16]=ADDR(7), [15:8]=DATA(8), [7:3]=reserved(0), [2:0]=CRC
    uint32_t w = 0;
    w |= ((uint32_t)(c ? 1u : 0u)) << 23;
    w |= ((uint32_t)addr & 0x7Fu) << 16;
    w |= ((uint32_t)data & 0xFFu) << 8;
    // reserved [7:3] are zero
    return w;
}

/* --- Your table -------------------------------------------------------- */
static const IfxTLE9180_SpiTx IfxTLE9180_startupConfig[] =
    {
        {.B.C = 1, .B.ADDRESS = 0x01, .B.DATA = 0x81, .B.CRC = 4},
        {.B.C = 1, .B.ADDRESS = 0x02, .B.DATA = 0x0F, .B.CRC = 0},
        {.B.C = 1, .B.ADDRESS = 0x06, .B.DATA = 0x70, .B.CRC = 6},
        {.B.C = 1, .B.ADDRESS = 0x07, .B.DATA = 0x9A, .B.CRC = 6},
        {.B.C = 1, .B.ADDRESS = 0x08, .B.DATA = 0x32, .B.CRC = 1},
        {.B.C = 1, .B.ADDRESS = 0x0A, .B.DATA = 0x2A, .B.CRC = 3},
        {.B.C = 1, .B.ADDRESS = 0x0B, .B.DATA = 0x4A, .B.CRC = 3},
        {.B.C = 1, .B.ADDRESS = 0x13, .B.DATA = 0x2A, .B.CRC = 5},
        {.B.C = 1, .B.ADDRESS = 0x00, .B.DATA = 0xAC, .B.CRC = 2},
        {.B.C = 1, .B.ADDRESS = 0x20, .B.DATA = 0x44, .B.CRC = 3},
        {.B.C = 1, .B.ADDRESS = 0x21, .B.DATA = 0x44, .B.CRC = 7},
        {.B.C = 1, .B.ADDRESS = 0x22, .B.DATA = 0x44, .B.CRC = 0},
        {.B.C = 1, .B.ADDRESS = 0x23, .B.DATA = 0x9F, .B.CRC = 0}};

/* --- Test --------------------------------------------------------------- */
int main(void)
{
    size_t n = sizeof IfxTLE9180_startupConfig / sizeof IfxTLE9180_startupConfig[0];
    unsigned fails = 0;

    printf("Idx  C  ADDR  DATA  Given  CRC(23..3,MSB)  Result\n");
    printf("---- -- ----- ----- ------ --------------  ------\n");

    for (size_t i = 0; i < n; ++i)
    {
        const IfxTLE9180_SpiTx *e = &IfxTLE9180_startupConfig[i];

        uint32_t w_no_crc = pack_tx24_wo_crc((uint8_t)e->B.ADDRESS,
                                             (uint8_t)e->B.DATA,
                                             (uint8_t)e->B.C);

        uint8_t crc = tle9180_crc3_tx24(w_no_crc);
        int ok = (crc == (e->B.CRC & 0x7u));
        if (!ok)
            ++fails;

        printf("%3zu  %u  0x%02X  0x%02X   0x%01X       0x%01X         %s\n",
               i, (unsigned)e->B.C, (unsigned)e->B.ADDRESS, (unsigned)e->B.DATA,
               (unsigned)e->B.CRC, (unsigned)crc, ok ? "PASS" : "FAIL");
    }

    printf("\nSummary: %u/%zu mismatches.\n", fails, n);
    return (fails == 0) ? 0 : 1;
}
