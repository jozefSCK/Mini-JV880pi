#ifndef ROMS_H
#define ROMS_H

#include <cstddef>

struct RomInfo {
    size_t size;
    const char* filename;
    bool isWaveRom;
    bool isLoaded;
    bool needsUnscramble;
    void* data;
};

constexpr size_t sz32K = 32 * 1024;
constexpr size_t sz128K = 128 * 1024;
constexpr size_t sz256K = 256 * 1024;
constexpr size_t sz2M = 2 * 1024 * 1024;
constexpr size_t sz8M = 8 * 1024 * 1024;

static RomInfo g_romInfos[] = {
    {sz32K, "jv880_nvram.bin", false, false, false, nullptr},
    {sz32K, "jv880_rom1.bin", false, false, false, nullptr},
    {sz256K, "jv880_rom2.bin", false, false, false, nullptr},
    {sz2M, "jv880_waverom1.bin", true, false, true, nullptr},
    {sz2M, "jv880_waverom2.bin", true, false, true, nullptr},
    {sz128K, "rd500_patches.bin", false, false, false, nullptr},
    {sz8M, "rd500_expansion.bin", true, false, true, nullptr},
    {sz8M, "SR-JV80-01 Pop - CS 0x3F1CF705.bin", true, false, true, nullptr},
    {sz8M, "SR-JV80-02 Orchestral - CS 0x3F0E09E2.BIN", true, false, true, nullptr},
    {sz8M, "SR-JV80-03 Piano - CS 0x3F8DB303.bin", true, false, true, nullptr},
    {sz8M, "SR-JV80-04 Vintage Synth - CS 0x3E23B90C.BIN", true, false, true, nullptr},
    {sz8M, "SR-JV80-05 World - CS 0x3E8E8A0D.bin", true, false, true, nullptr},
    {sz8M, "SR-JV80-06 Dance - CS 0x3EC462E0.bin", true, false, true, nullptr},
    {sz8M, "SR-JV80-07 Super Sound Set - CS 0x3F1EE208.bin", true, false, true, nullptr},
    {sz8M, "SR-JV80-08 Keyboards of the 60s and 70s - CS 0x3F1E3F0A.BIN", true, false, true, nullptr},
    {sz8M, "SR-JV80-09 Session - CS 0x3F381791.BIN", true, false, true, nullptr},
    {sz8M, "SR-JV80-10 Bass & Drum - CS 0x3D83D02A.BIN", true, false, true, nullptr},
    {sz8M, "SR-JV80-11 Techno - CS 0x3F046250.bin", true, false, true, nullptr},
    {sz8M, "SR-JV80-12 HipHop - CS 0x3EA08A19.BIN", true, false, true, nullptr},
    {sz8M, "SR-JV80-13 Vocal - CS 0x3ECE78AA.bin", true, false, true, nullptr},
    {sz8M, "SR-JV80-14 Asia - CS 0x3C8A1582.bin", true, false, true, nullptr},
    {sz8M, "SR-JV80-15 Special FX - CS 0x3F591CE4.bin", true, false, true, nullptr},
    {sz8M, "SR-JV80-16 Orchestral II - CS 0x3F35B03B.bin", true, false, true, nullptr},
    {sz8M, "SR-JV80-17 Country - CS 0x3ED75089.bin", true, false, true, nullptr},
    {sz8M, "SR-JV80-18 Latin - CS 0x3EA51033.BIN", true, false, true, nullptr},
    {sz8M, "SR-JV80-19 House - CS 0x3E330C41.BIN", true, false, true, nullptr}
};

constexpr size_t ROM_COUNT = sizeof(g_romInfos) / sizeof(g_romInfos[0]);

#endif