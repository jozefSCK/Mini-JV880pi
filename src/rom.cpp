//
// rom.cpp
//
// Mini-JV880pi - synthesizer for bare metal Raspberry Pi
// Copyright (C) 2025  Giulio Zausa/Gene J.B.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

// rom.cpp
#include "rom.h"
#include <circle/logger.h>
#include <cstdlib>
#include <cstring>
#include <circle/memory.h>

LOGMODULE("rom");

RomLoader::RomLoader() : m_totalPatchBanks(0), m_current_exp_data(nullptr) {
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
    for (int i = 0; i < romCount; i++) {
        m_loadedRoms[i] = nullptr;
    }
}

RomLoader::~RomLoader() {
    cleanupRoms();
}

bool RomLoader::loadEmuFiles() {
    LOGNOTE("Loading emu files");
    
    // Load all ROM files
    if (!preloadAll()) {
        LOGERR("Error loading ROM files");
        return false;
    }
    
    // Extract patches from all ROMs
    if (!extractAllPatches()) {
        LOGWARN("Error extracting patches (continuing operation)");
    }
    
    // Display bank information
    printPatchBankInfo();
    
    LOGNOTE("All files loaded, created %d patch banks", m_totalPatchBanks);
    
    return true;
}

void RomLoader::cleanupRoms() {
    // Clean up loaded ROMs
    for (int i = 0; i < romCount; i++) {
        if (m_loadedRoms[i]) {
            free(m_loadedRoms[i]);
            m_loadedRoms[i] = nullptr;
            m_romInfos[i].loaded = false;
        }
    }
    
    // Clean up current expansion ROM
    if (m_current_exp_data) {
        free(m_current_exp_data);
        m_current_exp_data = nullptr;
    }
    
    // Reset patch banks
    m_totalPatchBanks = 0;
}

uint8_t* RomLoader::getRomData(int index) const {
    if (index >= 0 && index < romCount) {
        return m_loadedRoms[index];
    }
    return nullptr;
}

int RomLoader::getRomIndex(const char* filename) const {
    for (int i = 0; i < romCount; i++) {
        if (strcmp(filename, m_romInfos[i].filename) == 0)
            return i;
    }
    return -1;
}

int RomLoader::getTotalPatchBanks() const {
    return m_totalPatchBanks;
}

const RomLoader::PatchBankInfo* RomLoader::getPatchBank(int index) const {
    if (index >= 0 && index < m_totalPatchBanks) {
        return &m_patchBanks[index];
    }
    return nullptr;
}

uint8_t* RomLoader::getCurrentExpData() const {
    return m_current_exp_data;
}

bool RomLoader::preloadAll() {
    for (int i = 0; i < romCount; i++) {
        if (!loadRom(i, nullptr) && i < romCountRequired)
            return false;
    }
    return true;
}

bool RomLoader::loadRom(int romI, uint8_t *dst) {
    if (romI < romCount && m_loadedRoms[romI] != nullptr) {
        if (dst != nullptr)
            memcpy(dst, m_loadedRoms[romI], m_romInfos[romI].size);
        m_romInfos[romI].loaded = true;
        return true;
    }

    RomInfo *romInfo = &m_romInfos[romI];

    // Allocate memory
    uint8_t *data = (uint8_t *)malloc(romInfo->size);
    if (!data) {
        LOGERR("Not enough memory for %s", romInfo->filename);
        return false;
    }

    // Load file
    if (!loadFile(romInfo->filename, data, romInfo->size)) {
        LOGERR("Error loading %s", romInfo->filename);
        free(data);
        return false;
    }

    // Descramble if needed
    if (romInfo->needsUnscramble) {
        uint8_t *dataOut = (uint8_t *)malloc(romInfo->size);
        if (!dataOut) {
            LOGERR("Not enough memory for descrambled %s", romInfo->filename);
            free(data);
            return false;
        }
        unscrambleRom(data, dataOut, romInfo->size);
        free(data);
        data = dataOut;
    }

    m_loadedRoms[romI] = (uint8_t *)malloc(romInfo->size);
    memcpy(m_loadedRoms[romI], data, romInfo->size);
    if (dst != nullptr)
        memcpy(dst, data, romInfo->size);
    free(data);
    
    m_romInfos[romI].loaded = true;
    LOGNOTE("Loaded %s", romInfo->filename);
    return true;
}

// Function to read 4-byte value from memory (big-endian)
uint32_t RomLoader::readBigEndian32(const uint8_t* data, uint32_t offset) {
    return (data[offset] << 24) | (data[offset + 1] << 16) | 
           (data[offset + 2] << 8) | data[offset + 3];
}

// Function to extract patches from RD500 patches file
bool RomLoader::extractPatchesFromRD500() {
    int rd500_patches_index = getRomIndex("rd500_patches.bin");
    if (rd500_patches_index == -1 || !m_loadedRoms[rd500_patches_index]) {
        LOGERR("RD500 patches file not loaded");
        return false;
    }
    
    uint8_t* patch_data = m_loadedRoms[rd500_patches_index];
    int nPatches = 192; // RD500 always has 192 patches
    LOGNOTE("RD500 patches file contains %d patches", nPatches);
    
    // Create 3 banks for RD500 (64 patches each)
    int rd500_offsets[3] = {0x0ce0, 0x8370, 0x12b82};
    const char* rd500_bank_names[3] = {"RD500-Bank1", "RD500-Bank2", "RD500-Bank3"};
    
    for (int bank = 0; bank < 3 && m_totalPatchBanks < 200; bank++) {
        PatchBankInfo* patchBank = &m_patchBanks[m_totalPatchBanks];
        
        patchBank->bankselect = m_totalPatchBanks + 1;
        patchBank->expromindex = rd500_patches_index;
        patchBank->patch_offset = rd500_offsets[bank];
        patchBank->patch_count = 64;
        
        // Copy bank name
        memcpy(patchBank->bankname, rd500_bank_names[bank], 12);
        patchBank->bankname[12] = '\0';
        
        // Copy patch data
        uint32_t source_offset = rd500_offsets[bank];
        uint32_t copy_size = 64 * 362;
        
        if (source_offset + copy_size <= m_romInfos[rd500_patches_index].size) {
            memcpy(patchBank->patches, &patch_data[source_offset], copy_size);
            LOGNOTE("RD500 Bank %d: copied 64 patches from address 0x%08X", 
                   bank + 1, source_offset);
            m_totalPatchBanks++;
        } else {
            LOGERR("Error: out of bounds RD500 patch data");
            return false;
        }
    }
    
    return true;
}

// Function to extract patches from one expansion ROM
bool RomLoader::extractPatchesFromExpRom(int expRomIndex) {
    // Special handling for RD500
    if (expRomIndex == 6) { // rd500_expansion.bin
        return true; // Handle separately
    }
    if (expRomIndex == 5) { // rd500_patches.bin
        return extractPatchesFromRD500();
    }
    
    if (expRomIndex < romCountRequired) return true; // Skip main ROMs
    
    if (!m_loadedRoms[expRomIndex]) {
        LOGERR("Expansion ROM %d not loaded", expRomIndex);
        return false;
    }
    
    uint8_t* exp_data = m_loadedRoms[expRomIndex];
    
    // Read patch count from offset 0x66-0x67
    uint16_t patch_count = (exp_data[0x66] << 8) | exp_data[0x67];
    LOGNOTE("Exp ROM %s contains %d patches", 
           m_romInfos[expRomIndex].filename, patch_count);
    
    // Read patch data offset from 0x8C-0x8F
    uint32_t patch_data_offset = readBigEndian32(exp_data, 0x8C);
    LOGNOTE("Patch data offset: 0x%08X", patch_data_offset);
    
    // Read bank name from offset 0x7FCC3A-0x7FCC45 (12 characters)
    char bank_name[13];
    memcpy(bank_name, &exp_data[0x7FCC3A], 12);
    bank_name[12] = '\0';
    LOGNOTE("Bank name: %s", bank_name);
    
    // Calculate number of banks (64 patches per bank)
    int bank_count = (patch_count + 63) / 64; // Round up
    LOGNOTE("Need %d banks for %d patches", bank_count, patch_count);
    
    // Create patch banks
    for (int bank = 0; bank < bank_count && m_totalPatchBanks < 200; bank++) {
        PatchBankInfo* patchBank = &m_patchBanks[m_totalPatchBanks];
        
        patchBank->bankselect = m_totalPatchBanks + 1; // Numbering from 1
        patchBank->expromindex = expRomIndex;
        patchBank->patch_offset = patch_data_offset + (bank * 64 * 362);
        
        // Determine number of patches in this bank
        int remaining_patches = patch_count - (bank * 64);
        patchBank->patch_count = (remaining_patches > 64) ? 64 : remaining_patches;
        
        // Copy bank name
        memcpy(patchBank->bankname, bank_name, 12);
        patchBank->bankname[12] = '\0';
        
        // Copy patch data
        uint32_t source_offset = patch_data_offset + (bank * 64 * 362);
        uint32_t copy_size = patchBank->patch_count * 362;
        
        // Check bounds
        if (source_offset + copy_size <= m_romInfos[expRomIndex].size) {
            memcpy(patchBank->patches, &exp_data[source_offset], copy_size);
            LOGNOTE("Bank %d: copied %d patches from address 0x%08X", 
                   patchBank->bankselect, patchBank->patch_count, source_offset);
            m_totalPatchBanks++;
        } else {
            LOGERR("Error: out of bounds exp ROM data");
            return false;
        }
    }
    
    return true;
}

// Function to extract system patches from rom2 (bank 0 only)
bool RomLoader::extractSystemPatches() {
    int rom2Index = getRomIndex("jv880_rom2.bin");
    if (rom2Index == -1 || !m_loadedRoms[rom2Index]) {
        LOGERR("ROM2 not loaded for system patches");
        return false;
    }
    
    uint8_t* rom2_data = m_loadedRoms[rom2Index];
    
    // Create bank 0 (patches 0-63 from offset 0x008ce0)
    if (m_totalPatchBanks < 200) {
        PatchBankInfo* bank0 = &m_patchBanks[m_totalPatchBanks];
        bank0->bankselect = 0;  // Special bank 0
        bank0->expromindex = rom2Index;
        bank0->patch_offset = 0x008ce0;
        bank0->patch_count = 64;
        strcpy(bank0->bankname, "Internal");
        
        // Copy patch data
        uint32_t source_offset = 0x008ce0;
        uint32_t copy_size = 64 * 362;
        
        if (source_offset + copy_size <= m_romInfos[rom2Index].size) {
            memcpy(bank0->patches, &rom2_data[source_offset], copy_size);
            m_totalPatchBanks++;
            LOGNOTE("Created system bank 0 with 64 patches");
        } else {
            LOGERR("Out of bounds system patch data for bank 0");
            return false;
        }
    }
    
    return true;
}

// Function to extract patches from all expansion ROMs
bool RomLoader::extractAllPatches() {
    LOGNOTE("Extracting patches from all expansion ROMs");
    
    // Extract system patches first
    if (!extractSystemPatches()) {
        LOGWARN("Error extracting system patches (continuing)");
    }
    
    // Extract patches from expansion ROMs
    for (int i = romCountRequired; i < romCount; i++) {
        LOGNOTE("Processing exp ROM %d: %s", i, m_romInfos[i].filename);
        if (!extractPatchesFromExpRom(i)) {
            LOGWARN("Failed to extract patches from %s", m_romInfos[i].filename);
            // Continue with other ROMs
        }
    }
    
    LOGNOTE("Total %d patch banks created", m_totalPatchBanks);
    return true;
}

// Function to display information about all banks
void RomLoader::printPatchBankInfo() {
    LOGNOTE("=== Patch Bank Information ===");
    for (int i = 0; i < m_totalPatchBanks; i++) {
        const PatchBankInfo* bank = &m_patchBanks[i];
        LOGNOTE("Bank %d: ExpROM[%d] '%s', Name: '%s', %d patches, offset 0x%08X", 
               bank->bankselect, 
               bank->expromindex,
               m_romInfos[bank->expromindex].filename,
               bank->bankname,
               bank->patch_count,
               bank->patch_offset);
    }
}

// Function to load patches to synthesizer memory by command
bool RomLoader::loadPatchBankToSynth(int bankIndex) {
    if (bankIndex < 0 || bankIndex >= m_totalPatchBanks) {
        LOGERR("Invalid patch bank index: %d", bankIndex);
        return false;
    }
    
    const PatchBankInfo* bank = &m_patchBanks[bankIndex];
    LOGNOTE("Loading bank %d (%s) to synthesizer memory", 
           bank->bankselect, bank->bankname);
    
    // Load corresponding expansion ROM if not already loaded
    // or if different one is loaded
    if (bank->expromindex < romCountRequired) {
        // System patch from main ROMs - already loaded
        LOGNOTE("System patch bank, no expansion ROM needed");
    } else {
        // Load expansion ROM data to m_current_exp_data
        if (m_current_exp_data) {
            free(m_current_exp_data);
            m_current_exp_data = nullptr;
        }
        
        m_current_exp_data = (uint8_t*)malloc(m_romInfos[bank->expromindex].size);
        if (!m_current_exp_data) {
            LOGERR("Not enough memory for expansion ROM %s", 
                  m_romInfos[bank->expromindex].filename);
            return false;
        }
        
        memcpy(m_current_exp_data, m_loadedRoms[bank->expromindex], m_romInfos[bank->expromindex].size);
        LOGNOTE("Loaded expansion ROM %s for bank %d", 
               m_romInfos[bank->expromindex].filename, bank->bankselect);
    }
    
    // Here will be code for loading patches to synthesizer memory
    // Just logging for now
    LOGNOTE("Ready to load %d patches from bank %d (%s)", 
           bank->patch_count, bank->bankselect, bank->bankname);
    
    // TODO: Add real loading to synthesizer memory
    // This will be implemented later by command
    
    return true;
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