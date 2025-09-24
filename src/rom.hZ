// rom.h
#ifndef ROM_H
#define ROM_H

#include <cstdint>
#include <cstddef>

// Include FATFS header
#include <fatfs/ff.h>

class RomLoader {
public:
    // Patch bank information structure
    struct PatchBankInfo {
        uint8_t bankselect;     // Bank number from 1 to N
        uint8_t expromindex;    // Expansion ROM index
        uint8_t patch_count;    // Number of patches in this bank (up to 64)
        uint32_t patch_offset;  // Offset to patch data in exprom
        char bankname[13];      // Bank name (12 chars + null terminator)
        uint8_t patches[64 * 362]; // Patch data (64 patches of 362 bytes each)
    };

    // Constructor/Destructor
    RomLoader();
    ~RomLoader();

    // Main functions
    bool loadEmuFiles();
    void cleanupRoms();
    
    // Access to loaded data
    uint8_t* getRomData(int index) const;
    int getRomIndex(const char* filename) const;
    
    // Patch bank functions
    int getTotalPatchBanks() const;
    const PatchBankInfo* getPatchBank(int index) const;
    bool loadPatchBankToSynth(int bankIndex);
    
    // Get current expansion ROM data
    uint8_t* getCurrentExpData() const;

private:
    // Constants
    static const size_t sz8M = 8 * 1024 * 1024;
    static const size_t sz2M = 2 * 1024 * 1024;
    static const size_t sz32K = 32 * 1024;
    static const size_t sz128K = 128 * 1024;
    static const size_t sz256K = 256 * 1024;

    // Simple ROM information structure
    struct RomInfo {
        size_t size;
        const char* filename;
        bool needsUnscramble;
        bool loaded;
        uint8_t* data; // Pointer to loaded data
    };

    static const int romCount = 26;
    static const int romCountRequired = 6;

    // Data members
    RomInfo m_romInfos[romCount];
    uint8_t* m_loadedRoms[romCount];
    PatchBankInfo m_patchBanks[200];
    int m_totalPatchBanks;
    uint8_t* m_current_exp_data;

    // Private methods
    bool preloadAll();
    bool loadRom(int romI, uint8_t *dst);
    uint32_t readBigEndian32(const uint8_t* data, uint32_t offset);
    bool extractPatchesFromRD500();
    bool extractPatchesFromExpRom(int expRomIndex);
    bool extractSystemPatches();
    bool extractAllPatches();
    void printPatchBankInfo();
    bool loadFile(const char* filename, uint8_t* buffer, size_t size);
    void unscrambleRom(const uint8_t *src, uint8_t *dst, int len);
};

#endif // ROM_H