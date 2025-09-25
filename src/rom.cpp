// rom.cpp
#include "rom.h"
#include <circle/logger.h>
#include <cstdlib>
#include <cstring>
#include <circle/memory.h>
#include <cstdio>

LOGMODULE("rom");

RomLoader::RomLoader() : 
    m_totalPatchBanks(0), 
    m_current_exp_data(nullptr),
    m_nvram(nullptr)
     {
    // Initialize m_romInfos
    m_romInfos[0] = {sz32K, "jv880_nvram.bin", false, false, nullptr};
    m_romInfos[1] = {sz32K, "jv880_rom1.bin", false, false, nullptr};
    m_romInfos[2] = {sz256K, "jv880_rom2.bin", false, false, nullptr};
    m_romInfos[3] = {sz2M, "jv880_waverom1.bin", true, false, nullptr};
    m_romInfos[4] = {sz2M, "jv880_waverom2.bin", true, false, nullptr};
    m_romInfos[5] = {sz128K, "rd500_patches.bin", false, false, nullptr};
    m_romInfos[6] = {sz8M, "rd500_expansion.bin", true, false, nullptr};
    m_romInfos[7] = {sz8M, "SR-JV80-01 Pop - CS 0x3F1CF705.bin", true, false, nullptr};
    m_romInfos[8] = {sz8M, "SR-JV80-02 Orchestral - CS 0x3F0E09E2.BIN", true, false, nullptr};
    m_romInfos[9] = {sz8M, "SR-JV80-03 Piano - CS 0x3F8DB303.bin", true, false, nullptr};
    m_romInfos[10] = {sz8M, "SR-JV80-04 Vintage Synth - CS 0x3E23B90C.BIN", true, false, nullptr};
    m_romInfos[11] = {sz8M, "SR-JV80-05 World - CS 0x3E8E8A0D.bin", true, false, nullptr};
    m_romInfos[12] = {sz8M, "SR-JV80-06 Dance - CS 0x3EC462E0.bin", true, false, nullptr};
    m_romInfos[13] = {sz8M, "SR-JV80-07 Super Sound Set - CS 0x3F1EE208.bin", true, false, nullptr};
    m_romInfos[14] = {sz8M, "SR-JV80-08 Keyboards of the 60s and 70s - CS 0x3F1E3F0A.BIN", true, false, nullptr};
    m_romInfos[15] = {sz8M, "SR-JV80-09 Session - CS 0x3F381791.BIN", true, false, nullptr};
    m_romInfos[16] = {sz8M, "SR-JV80-10 Bass & Drum - CS 0x3D83D02A.BIN", true, false, nullptr};
    m_romInfos[17] = {sz8M, "SR-JV80-11 Techno - CS 0x3F046250.bin", true, false, nullptr};
    m_romInfos[18] = {sz8M, "SR-JV80-12 HipHop - CS 0x3EA08A19.BIN", true, false, nullptr};
    m_romInfos[19] = {sz8M, "SR-JV80-13 Vocal - CS 0x3ECE78AA.bin", true, false, nullptr};
    m_romInfos[20] = {sz8M, "SR-JV80-14 Asia - CS 0x3C8A1582.bin", true, false, nullptr};
    m_romInfos[21] = {sz8M, "SR-JV80-15 Special FX - CS 0x3F591CE4.bin", true, false, nullptr};
    m_romInfos[22] = {sz8M, "SR-JV80-16 Orchestral II - CS 0x3F35B03B.bin", true, false, nullptr};
    m_romInfos[23] = {sz8M, "SR-JV80-17 Country - CS 0x3ED75089.bin", true, false, nullptr};
    m_romInfos[24] = {sz8M, "SR-JV80-18 Latin - CS 0x3EA51033.BIN", true, false, nullptr};
    m_romInfos[25] = {sz8M, "SR-JV80-19 House - CS 0x3E330C41.BIN", true, false, nullptr};

    // Initialize m_loadedRoms
    for (int i = 0; i < 26; i++) {
        m_loadedRoms[i] = nullptr;
    }
}

RomLoader::~RomLoader() {
    cleanupRoms();
}

bool RomLoader::collectPatchData() {
    // Check if datacollected file exists
    FIL infoFile;
    if (f_open(&infoFile, "datacollected", FA_READ | FA_OPEN_EXISTING) == FR_OK) {
        f_close(&infoFile);
        LOGNOTE("datacollected file exists, skipping patch data collection");
        return true;
    }
    
    LOGNOTE("Starting patch data collection");
    
    // Ensure patch directory exists
    f_mkdir("patch");
    
    // Load required ROMs for patch extraction
    uint8_t* rom2_data = nullptr;
    uint8_t* rd500_patches_data = nullptr;
    uint8_t* rd500_exp_data = nullptr;
    
    // Load jv880_rom2.bin
    rom2_data = (uint8_t*)malloc(sz256K);
    if (!rom2_data) {
        LOGERR("Not enough memory for rom2");
        return false;
    }
    if (!loadFile("jv880_rom2.bin", rom2_data, sz256K)) {
        LOGERR("Cannot load jv880_rom2.bin");
        free(rom2_data);
        return false;
    }
    
    // Load rd500_patches.bin
    rd500_patches_data = (uint8_t*)malloc(sz128K);
    if (!rd500_patches_data) {
        LOGERR("Not enough memory for rd500 patches");
        free(rom2_data);
        return false;
    }
    if (!loadFile("rd500_patches.bin", rd500_patches_data, sz128K)) {
        LOGERR("Cannot load rd500_patches.bin");
        free(rom2_data);
        free(rd500_patches_data);
        return false;
    }
    
    // Load rd500_expansion.bin
    rd500_exp_data = (uint8_t*)malloc(sz8M);
    if (!rd500_exp_data) {
        LOGERR("Not enough memory for rd500 expansion");
        free(rom2_data);
        free(rd500_patches_data);
        return false;
    }
    if (!loadFile("rd500_expansion.bin", rd500_exp_data, sz8M)) {
        LOGERR("Cannot load rd500_expansion.bin");
        free(rom2_data);
        free(rd500_patches_data);
        free(rd500_exp_data);
        return false;
    }
    
    FIL outFile;
    unsigned int bytesWritten;
    int bankNumber = 0;
    char filename[64];
    
    // Process system patches from rom2.bin (bank 0) - associated with waverom1.bin
    sprintf(filename, "patch/bank%d.bin", bankNumber);
    if (f_open(&outFile, filename, FA_WRITE | FA_CREATE_ALWAYS) == FR_OK) {
        // Prepare output structure: waverom_info[64], needdescramble[bool], patch_count[uint8_t], patchdata[x*362]
        char waverom_info[64] = {0};
        strcpy(waverom_info, "jv880_waverom1.bin");
        
        // Write waverom info (64 bytes)
        f_write(&outFile, waverom_info, 64, &bytesWritten);
        
        // Write needdescramble flag (1 byte)
        uint8_t need_descramble = true; // waverom1 needs unscrambling
        f_write(&outFile, &need_descramble, 1, &bytesWritten);
        
        // Write patch count (1 byte) - always 64 for system patches
        uint8_t patch_count = 64;
        f_write(&outFile, &patch_count, 1, &bytesWritten);
        
        // Write patch data (64 * 362 bytes)
        uint8_t patchData[64 * 362];
        memcpy(patchData, &rom2_data[0x008ce0], 64 * 362);
        f_write(&outFile, patchData, 64 * 362, &bytesWritten);
        
        f_close(&outFile);
        LOGNOTE("Created system bank file: %s", filename);
        bankNumber++;
    }
    
    // Process RD500 patches (3 banks) - associated with rd500_expansion.bin
    int rd500_offsets[3] = {0x0ce0, 0x8370, 0x12b82};
    
    for (int bank = 0; bank < 3; bank++) {
        sprintf(filename, "patch/bank%d.bin", bankNumber);
        
        if (f_open(&outFile, filename, FA_WRITE | FA_CREATE_ALWAYS) == FR_OK) {
            // Prepare output structure
            char waverom_info[64] = {0};
            strcpy(waverom_info, "rd500_expansion.bin");
            
            // Write waverom info (64 bytes)
            f_write(&outFile, waverom_info, 64, &bytesWritten);
            
            // Write needdescramble flag (1 byte)
            uint8_t need_descramble = true; // rd500_expansion needs unscrambling
            f_write(&outFile, &need_descramble, 1, &bytesWritten);
            
            // Write patch count (1 byte) - always 64 for RD500 banks
            uint8_t patch_count = 64;
            f_write(&outFile, &patch_count, 1, &bytesWritten);
            
            // Write patch data (64 * 362 bytes)
            uint32_t source_offset = rd500_offsets[bank];
            uint8_t patchData[64 * 362];
            memcpy(patchData, &rd500_patches_data[source_offset], 64 * 362);
            f_write(&outFile, patchData, 64 * 362, &bytesWritten);
            
            f_close(&outFile);
            LOGNOTE("Created RD500 bank file: %s", filename);
            bankNumber++;
        }
    }
    
    // Process all expansion ROMs from index 7 onwards
    for (int i = 7; i < 26; i++) {
        const char* exp_filename = m_romInfos[i].filename;
        
        // Load expansion ROM
        uint8_t* exp_rom_data = (uint8_t*)malloc(sz8M);
        if (!exp_rom_data) {
            LOGERR("Not enough memory for expansion ROM %s", exp_filename);
            continue;
        }
        
        if (!loadFile(exp_filename, exp_rom_data, sz8M)) {
            LOGERR("Cannot load expansion ROM %s", exp_filename);
            free(exp_rom_data);
            continue;
        }
        
        // Descramble if needed
        uint8_t* exp_rom_data_descrambled = (uint8_t*)malloc(sz8M);
        if (!exp_rom_data_descrambled) {
            LOGERR("Not enough memory for descrambled expansion ROM %s", exp_filename);
            free(exp_rom_data);
            continue;
        }
        
        if (m_romInfos[i].needsUnscramble) {
            unscrambleRom(exp_rom_data, exp_rom_data_descrambled, sz8M);
        } else {
            memcpy(exp_rom_data_descrambled, exp_rom_data, sz8M);
        }
        free(exp_rom_data);
        
        // Read patch count from offset 0x66-0x67
        uint16_t patch_count_total = (exp_rom_data_descrambled[0x66] << 8) | exp_rom_data_descrambled[0x67];
        LOGNOTE("Exp ROM %s contains %d patches", exp_filename, patch_count_total);
        
        // Skip if no patches
        if (patch_count_total == 0) {
            free(exp_rom_data_descrambled);
            continue;
        }
        
        // Read patch data offset from 0x8C-0x8F
        uint32_t patch_data_offset = readBigEndian32(exp_rom_data_descrambled, 0x8C);
        LOGNOTE("Patch data offset: 0x%08X", patch_data_offset);
        
        // Read bank name from offset 0x7FCC3A-0x7FCC45 (12 characters)
        char bank_name[13];
        memcpy(bank_name, &exp_rom_data_descrambled[0x7FCC3A], 12);
        bank_name[12] = '\0';
        LOGNOTE("Bank name: %s", bank_name);
        
        // Calculate number of banks (64 patches per bank)
        int bank_count = (patch_count_total + 63) / 64; // Round up
        LOGNOTE("Need %d banks for %d patches", bank_count, patch_count_total);
        
        for (int bank = 0; bank < bank_count; bank++) {
            sprintf(filename, "patch/bank%d.bin", bankNumber);
            
            if (f_open(&outFile, filename, FA_WRITE | FA_CREATE_ALWAYS) == FR_OK) {
                // Prepare output structure: waverom_info[64], needdescramble[bool], patch_count[uint8_t], patchdata[x*362]
                char waverom_info[64] = {0};
                strcpy(waverom_info, exp_filename);
                
                // Write waverom info (64 bytes)
                f_write(&outFile, waverom_info, 64, &bytesWritten);
                
                // Write needdescramble flag (1 byte)
                uint8_t need_descramble = m_romInfos[i].needsUnscramble;
                f_write(&outFile, &need_descramble, 1, &bytesWritten);
                
                // Calculate patches in this bank (could be less than 64 for the last bank)
                uint8_t patches_in_bank = (bank == bank_count - 1) ? 
                                         (uint8_t)(patch_count_total - (bank * 64)) : 64;
                
                // Write patch count (1 byte)
                f_write(&outFile, &patches_in_bank, 1, &bytesWritten);
                
                // Write patch data (patches_in_bank * 362 bytes)
                uint32_t source_offset = patch_data_offset + (bank * 64 * 362);
                uint8_t* patchData = (uint8_t*)malloc(patches_in_bank * 362);
                if (patchData) {
                    if (source_offset + (patches_in_bank * 362) <= sz8M) {
                        memcpy(patchData, &exp_rom_data_descrambled[source_offset], patches_in_bank * 362);
                    }
                    f_write(&outFile, patchData, patches_in_bank * 362, &bytesWritten);
                    free(patchData);
                }
                
                f_close(&outFile);
                LOGNOTE("Created expansion bank file: %s, patches: %d", 
                       filename, patches_in_bank);
                bankNumber++;
            }
        }
        
        free(exp_rom_data_descrambled);
    }
    
    // Create datacollected file to mark completion
    if (f_open(&infoFile, "datacollected", FA_WRITE | FA_CREATE_ALWAYS) == FR_OK) {
        uint8_t marker = 1;
        f_write(&infoFile, &marker, 1, &bytesWritten);
        f_close(&infoFile);
        LOGNOTE("Created datacollected marker file");
    }
    
    free(rom2_data);
    free(rd500_patches_data);
    free(rd500_exp_data);
    
    LOGNOTE("Patch data collection completed, created %d bank files", bankNumber);
    return true;
}

bool RomLoader::loadMainRoms() {
    LOGNOTE("Loading main ROMs for synthesizer");
    
    // Load 6 main files: nvram, rom1, rom2, waverom1, waverom2, and one expansion
    const char* main_files[] = {
        "jv880_nvram.bin", 
        "jv880_rom1.bin", 
        "jv880_rom2.bin", 
        "jv880_waverom1.bin", 
        "jv880_waverom2.bin",
        "SR-JV80-01 Pop - CS 0x3F1CF705.bin"
    };
    
    size_t sizes[] = {sz32K, sz32K, sz256K, sz2M, sz2M, sz8M};
    
    for (int i = 0; i < 6; i++) {
        // Allocate memory
        m_loadedRoms[i] = (uint8_t*)malloc(sizes[i]);
        if (!m_loadedRoms[i]) {
            LOGERR("Not enough memory for %s (size: %zu)", main_files[i], sizes[i]);
            return false;
        }
        
        // Load file
        if (!loadFile(main_files[i], m_loadedRoms[i], sizes[i])) {
            LOGERR("Cannot load %s", main_files[i]);
            free(m_loadedRoms[i]);
            m_loadedRoms[i] = nullptr;
            return false;
        }
        
        LOGNOTE("Loaded %s successfully", main_files[i]);
        
        // Find corresponding romInfo to check if descrambling is needed
        bool needs_descramble = false;
        for (int j = 0; j < 26; j++) {
            if (strcmp(m_romInfos[j].filename, main_files[i]) == 0) {
                needs_descramble = m_romInfos[j].needsUnscramble;
                break;
            }
        }
        
        if (needs_descramble) {
            uint8_t* descrambled_data = (uint8_t*)malloc(sizes[i]);
            if (!descrambled_data) {
                LOGERR("Not enough memory for descrambled %s", main_files[i]);
                free(m_loadedRoms[i]);
                m_loadedRoms[i] = nullptr;
                return false;
            }
            LOGNOTE("Descrambling %s...", main_files[i]);
            unscrambleRom(m_loadedRoms[i], descrambled_data, sizes[i]);
            free(m_loadedRoms[i]);
            m_loadedRoms[i] = descrambled_data;
            LOGNOTE("Descrambled %s successfully", main_files[i]);
        }
    }
    
    LOGNOTE("All main ROMs loaded successfully");
    return true;
}

void RomLoader::cleanupRoms() {
    // Clean up loaded ROMs
    for (int i = 0; i < 26; i++) {
        if (m_loadedRoms[i]) {
            free(m_loadedRoms[i]);
            m_loadedRoms[i] = nullptr;
        }
    }
    
    // Clean up current expansion ROM
    if (m_current_exp_data) {
        free(m_current_exp_data);
        m_current_exp_data = nullptr;
    }
}

uint8_t* RomLoader::getRomData(int index) const {
    if (index >= 0 && index < 26) {
        return m_loadedRoms[index];
    }
    return nullptr;
}

uint8_t* RomLoader::getCurrentExpData() const {
    return m_current_exp_data;
}

// Function to read 4-byte value from memory (big-endian)
uint32_t RomLoader::readBigEndian32(const uint8_t* data, uint32_t offset) {
    return (data[offset] << 24) | (data[offset + 1] << 16) | 
           (data[offset + 2] << 8) | data[offset + 3];
}

// Function to load file
bool RomLoader::loadFile(const char* filename, uint8_t* buffer, size_t size) {
    FIL f;
    unsigned int nBytesRead = 0;
    
    if (f_open(&f, filename, FA_READ | FA_OPEN_EXISTING) != FR_OK) {
        LOGERR("Cannot open %s", filename);
        return false;
    }
    f_read(&f, buffer, size, &nBytesRead);
    f_close(&f);
    
    return (nBytesRead == size);
}

// Function to descramble ROM
void RomLoader::unscrambleRom(const uint8_t *src, uint8_t *dst, int len) {
    for (int i = 0; i < len; i++) {
        int address = i & ~0xfffff;
        static const int aa[] = {2, 0, 3, 4, 1, 9, 13, 10, 18, 17, 6, 15, 11, 16, 8, 5, 12, 7, 14, 19};
        for (int j = 0; j < 20; j++) {
            if (i & (1 << j))
                address |= 1 << aa[j];
        }
        uint8_t srcdata = src[address];
        uint8_t data = 0;
        static const int dd[] = {2, 0, 4, 5, 7, 6, 3, 1};
        for (int j = 0; j < 8; j++) {
            if (srcdata & (1 << dd[j]))
                data |= 1 << j;
        }
        dst[i] = data;
    }
}

bool RomLoader::switchPatchBank(int bankNumber) {
    // Get the nvram pointer from outside (assuming it's passed as parameter or available)
    // For now, let's assume we get it from the outside
    uint8_t* nvram = m_nvram; // assuming we store nvram pointer in RomLoader class
    m_spinlock.Acquire();
    char filename[64];
    sprintf(filename, "patch/bank%d.bin", bankNumber);
    
    FIL file;
    if (f_open(&file, filename, FA_READ | FA_OPEN_EXISTING) != FR_OK) {
        // File doesn't exist, try bank 0
        LOGWARN("Bank file %s not found, trying bank 0", filename);
        
        sprintf(filename, "patch/bank0.bin");
        if (f_open(&file, filename, FA_READ | FA_OPEN_EXISTING) != FR_OK) {
            // Bank 0 also doesn't exist, cold reload all ROMs
            LOGERR("Bank 0 file not found, performing cold reload");
            cleanupRoms();
            if (!collectPatchData() || !loadMainRoms()) {
                LOGERR("Cold reload failed");
                m_spinlock.Release();
                return false;

            }
            m_spinlock.Release();
            return true;
        }
    }
    
    // Get file size
    unsigned int fileSize = f_size(&file);
    
    if (fileSize < 64 + 1 + 1) { // Minimum size: 64 bytes for waverom_info + 1 byte for needdescramble + 1 byte for patch_count
        LOGERR("Bank file %s is too small (size: %d)", filename, fileSize);
        f_close(&file);
        m_spinlock.Release();
        return false;
    }
    
    // Read waverom_info (64 bytes)
    char waverom_info[64];
    unsigned int bytesRead;
    f_read(&file, waverom_info, 64, &bytesRead);
    if (bytesRead != 64) {
        LOGERR("Failed to read waverom info from %s", filename);
        f_close(&file);
        m_spinlock.Release();
        return false;
    }
    
    // Read needdescramble flag (1 byte)
    uint8_t need_descramble;
    f_read(&file, &need_descramble, 1, &bytesRead);
    if (bytesRead != 1) {
        LOGERR("Failed to read descramble flag from %s", filename);
        f_close(&file);
        m_spinlock.Release();
        return false;
    }
    
    // Read patch count (1 byte)
    uint8_t patch_count;
    f_read(&file, &patch_count, 1, &bytesRead);
    if (bytesRead != 1) {
        LOGERR("Failed to read patch count from %s", filename);
        f_close(&file);
        m_spinlock.Release();
        return false;
    }
    
    LOGNOTE("Loading bank %d: waverom='%s', descramble=%d, patches=%d", 
           bankNumber, waverom_info, need_descramble, patch_count);
    
    // Load the waverom file specified in waverom_info
    if (m_current_exp_data) {
        free(m_current_exp_data);
        m_current_exp_data = nullptr;
    }
    
    // Find the corresponding ROM index for this waverom
    int romIndex = -1;
    for (int i = 0; i < 26; i++) {
        if (strcmp(m_romInfos[i].filename, waverom_info) == 0) {
            romIndex = i;
            break;
        }
    }
    
    if (romIndex != -1 && m_loadedRoms[romIndex]) {
        // Use already loaded ROM data
        m_current_exp_data = (uint8_t*)malloc(m_romInfos[romIndex].size);
        if (!m_current_exp_data) {
            LOGERR("Not enough memory for expansion data");
            f_close(&file);
            m_spinlock.Release();
            return false;
        }
        memcpy(m_current_exp_data, m_loadedRoms[romIndex], m_romInfos[romIndex].size);
    } else {
        // Load the waverom file
        size_t size;
        
        if (strcmp(waverom_info, "jv880_waverom1.bin") == 0) {
            size = sz2M;
        } else if (strcmp(waverom_info, "jv880_waverom2.bin") == 0) {
            size = sz2M;
        } else if (strcmp(waverom_info, "rd500_expansion.bin") == 0) {
            size = sz8M;
        } else {
            // Assume it's one of the expansion ROMs (8MB)
            size = sz8M;
        }
        
        m_current_exp_data = (uint8_t*)malloc(size);
        if (!m_current_exp_data) {
            LOGERR("Not enough memory for expansion data");
            f_close(&file);
            m_spinlock.Release();
            return false;
        }
        
        if (!loadFile(waverom_info, m_current_exp_data, size)) {
            LOGERR("Cannot load waverom file %s", waverom_info);
            free(m_current_exp_data);
            m_current_exp_data = nullptr;
            f_close(&file);
            m_spinlock.Release();
            return false;
        }
        
        if (need_descramble) {
            uint8_t* descrambled_data = (uint8_t*)malloc(size);
            if (!descrambled_data) {
                LOGERR("Not enough memory for descrambled data");
                free(m_current_exp_data);
                m_current_exp_data = nullptr;
                f_close(&file);
                m_spinlock.Release();
                return false;
            }
            unscrambleRom(m_current_exp_data, descrambled_data, size);
            free(m_current_exp_data);
            m_current_exp_data = descrambled_data;
        }
    }
    
    // Calculate expected patch data size
    unsigned int expectedPatchDataSize = patch_count * 362;
    unsigned int expectedFileSize = 64 + 1 + 1 + expectedPatchDataSize;
    
    if (fileSize != expectedFileSize) {
        LOGERR("File size mismatch: expected %d, got %d", expectedFileSize, fileSize);
        f_close(&file);
        m_spinlock.Release();
        return false;
    }
    
    // Read patch data
    uint8_t* patchData = (uint8_t*)malloc(64 * 362); // Always allocate space for 64 patches
    if (!patchData) {
        LOGERR("Not enough memory for patch data");
        f_close(&file);
        m_spinlock.Release();
        return false;
    }
    
    memset(patchData, 0, 64 * 362); // Initialize with zeros
    
    f_read(&file, patchData, expectedPatchDataSize, &bytesRead);
    if (bytesRead != expectedPatchDataSize) {
        LOGERR("Failed to read patch data from %s", filename);
        free(patchData);
        f_close(&file);
        m_spinlock.Release();
        return false;
    }
    
    // Load patch data into nvram memory at offset 0x0d70
    if (nvram) { // Use the nvram pointer passed from outside
        
        // If patch_count < 64, first load patches from bank 0 to fill remaining slots
        if (patch_count < 64) {
            // Try to load bank 0 to fill remaining patch slots
            char bank0_filename[64];
            sprintf(bank0_filename, "patch/bank0.bin");
            
            FIL bank0_file;
            if (f_open(&bank0_file, bank0_filename, FA_READ | FA_OPEN_EXISTING) == FR_OK) {
                // Skip 64 bytes waverom_info + 1 byte needdescramble + 1 byte patch_count
                f_lseek(&bank0_file, 64 + 1 + 1);
                
                // Read bank 0 patch data (for remaining patches)
                uint8_t* bank0_patchData = (uint8_t*)malloc(64 * 362);
                if (bank0_patchData) {
                    unsigned int bank0_patch_count;
                    f_lseek(&bank0_file, 64 + 1); // Go to patch count position
                    f_read(&bank0_file, &bank0_patch_count, 1, &bytesRead);
                    
                    if (bank0_patch_count == 64) { // Only use bank 0 if it has 64 patches
                        f_lseek(&bank0_file, 64 + 1 + 1); // Go back to patch data position
                        f_read(&bank0_file, bank0_patchData, 64 * 362, &bytesRead);
                        
                        // Copy bank 0 patches to slots after the current bank patches
                        memcpy(&nvram[0x0d70 + patch_count * 362], 
                               bank0_patchData, 
                               (64 - patch_count) * 362);
                    }
                    free(bank0_patchData);
                }
                f_close(&bank0_file);
            }
        }
        
        // Copy current bank patches to the beginning
        memcpy(&nvram[0x0d70], patchData, patch_count * 362);
        
        // Check if we're switching from drums to normal patches
        if (nvram[0x11] != 1) {
            // Set flag to indicate normal patches
            nvram[0x11] = 1;
        } 
    }
    
    free(patchData);
    
    f_close(&file);
    
    LOGNOTE("Switched to bank %d, loaded waverom: %s", bankNumber, waverom_info);
    m_spinlock.Release();
    return true;
}

void RomLoader::setNvram(uint8_t* nvram) 
    { 
        m_nvram = nvram; 
    }