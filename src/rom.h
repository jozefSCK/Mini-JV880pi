#ifndef _rom_h
#define _rom_h

#include <circle/types.h>
#include <fatfs/ff.h>
#include <circle/spinlock.h>

class RomLoader {
public:
    RomLoader();
    ~RomLoader();

    // Main functions
    bool collectPatchData();
    bool loadMainRoms();
    void cleanupRoms();
    bool switchPatchBank(int bankNumber);

    // Accessor functions
    uint8_t* getRomData(int index) const;
    uint8_t* getCurrentExpData() const;

private:
    // ROM sizes
    static const size_t sz32K = 32 * 1024;
    static const size_t sz256K = 256 * 1024;
    static const size_t sz2M = 2 * 1024 * 1024;
    static const size_t sz8M = 8 * 1024 * 1024;
    static const size_t sz128K = 128 * 1024;

    static const int romCount = 26;
    static const int romCountRequired = 6;

    struct RomInfo {
        size_t size;
        const char* filename;
        bool needsUnscramble;
        bool loaded;
        uint8_t* data;
    };

    struct PatchBankInfo {
        uint8_t bankselect;
        int expromindex;
        uint32_t patch_offset;
        int patch_count;
        char bankname[13];
        uint8_t patches[64 * 362]; // Max 64 patches per bank
    };

    CSpinLock m_spinlock;
    RomInfo m_romInfos[romCount];
    uint8_t* m_loadedRoms[romCount];
    PatchBankInfo m_patchBanks[200];
    int m_totalPatchBanks;
    uint8_t* m_current_exp_data;

    // Helper functions
    uint32_t readBigEndian32(const uint8_t* data, uint32_t offset);
    bool loadFile(const char* filename, uint8_t* buffer, size_t size);
    void unscrambleRom(const uint8_t *src, uint8_t *dst, int len);
};

#endif