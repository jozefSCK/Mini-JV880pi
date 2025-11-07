//
// minidexed.cpp
//
// MiniDexed - Dexed FM synthesizer for bare metal Raspberry Pi
// Copyright (C) 2022  The MiniDexed Team
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
#include "minijv880.h" 
#include "midi.h"
#include "userinterface.h"
#include <assert.h>
#include <circle/memory.h>
#include <circle/devicenameservice.h>
#include <circle/gpiopin.h>
#include <circle/logger.h>
#include <circle/memory.h>
#include <circle/sound/hdmisoundbasedevice.h>
#include <circle/sound/i2ssoundbasedevice.h>
#include <circle/sound/pwmsoundbasedevice.h>
#include <circle/usb/usbmidihost.h>
#include <stdio.h>
#include <string.h>
#include <cstring>   // memcpy / memmove
#include <algorithm>

LOGMODULE("minijv880");

CMiniJV880 *CMiniJV880::s_pThis = 0;

CMiniJV880::RomInfo CMiniJV880::m_romInfos[26] = {
    m_romInfos[0] = {sz32K, "jv880_nvram.bin", false, false, false, nullptr},
    m_romInfos[1] = {sz32K, "jv880_rom1.bin", false, false, false, nullptr},
    m_romInfos[2] = {sz256K, "jv880_rom2.bin", false, false, false, nullptr},
    m_romInfos[3] = {sz2M, "jv880_waverom1.bin", true, false, true, nullptr},
    m_romInfos[4] = {sz2M, "jv880_waverom2.bin", true, false, true, nullptr},
    m_romInfos[5] = {sz128K, "rd500_patches.bin", false, false, false, nullptr},
    m_romInfos[6] = {sz8M, "rd500_expansion.bin", true, false, true, nullptr},
    m_romInfos[7] = {sz8M, "SR-JV80-01 Pop - CS 0x3F1CF705.bin", true, false, true, nullptr},
    m_romInfos[8] = {sz8M, "SR-JV80-02 Orchestral - CS 0x3F0E09E2.BIN", true, false, true, nullptr},
    m_romInfos[9] = {sz8M, "SR-JV80-03 Piano - CS 0x3F8DB303.bin", true, false, true, nullptr},
    m_romInfos[10] = {sz8M, "SR-JV80-04 Vintage Synth - CS 0x3E23B90C.BIN", true, false, true, nullptr},
    m_romInfos[11] = {sz8M, "SR-JV80-05 World - CS 0x3E8E8A0D.bin", true, false, true, nullptr},
    m_romInfos[12] = {sz8M, "SR-JV80-06 Dance - CS 0x3EC462E0.bin", true, false, true, nullptr},
    m_romInfos[13] = {sz8M, "SR-JV80-07 Super Sound Set - CS 0x3F1EE208.bin", true, false, true, nullptr},
    m_romInfos[14] = {sz8M, "SR-JV80-08 Keyboards of the 60s and 70s - CS 0x3F1E3F0A.BIN", true, false, true, nullptr},
    m_romInfos[15] = {sz8M, "SR-JV80-09 Session - CS 0x3F381791.BIN", true, false, true, nullptr},
    m_romInfos[16] = {sz8M, "SR-JV80-10 Bass & Drum - CS 0x3D83D02A.BIN", true, false, true, nullptr},
    m_romInfos[17] = {sz8M, "SR-JV80-11 Techno - CS 0x3F046250.bin", true, false, true, nullptr},
    m_romInfos[18] = {sz8M, "SR-JV80-12 HipHop - CS 0x3EA08A19.BIN", true, false, true, nullptr},
    m_romInfos[19] = {sz8M, "SR-JV80-13 Vocal - CS 0x3ECE78AA.bin", true, false, true, nullptr},
    m_romInfos[20] = {sz8M, "SR-JV80-14 Asia - CS 0x3C8A1582.bin", true, false, true, nullptr},
    m_romInfos[21] = {sz8M, "SR-JV80-15 Special FX - CS 0x3F591CE4.bin", true, false, true, nullptr},
    m_romInfos[22] = {sz8M, "SR-JV80-16 Orchestral II - CS 0x3F35B03B.bin", true, false, true, nullptr},
    m_romInfos[23] = {sz8M, "SR-JV80-17 Country - CS 0x3ED75089.bin", true, false, true, nullptr},
    m_romInfos[24] = {sz8M, "SR-JV80-18 Latin - CS 0x3EA51033.BIN", true, false, true, nullptr},
    m_romInfos[25] = {sz8M, "SR-JV80-19 House - CS 0x3E330C41.BIN", true, false, true, nullptr}
};


CMiniJV880::CMiniJV880(CConfig *pConfig, CInterruptSystem *pInterrupt,
                       CGPIOManager *pGPIOManager, CI2CMaster *pI2CMaster, CSPIMaster *pSPIMaster,
                       FATFS *pFileSystem, CScreenDevice *mScreenUnbuffered)
    : CMultiCoreSupport(CMemorySystem::Get()), m_pConfig(pConfig),
      m_pFileSystem(pFileSystem), 
      m_Serial(pInterrupt, TRUE),
      m_pSoundDevice(0),
      screenUnbuffered(mScreenUnbuffered),
      m_bChannelsSwapped(pConfig->GetChannelsSwapped()),
      m_UI(this, pGPIOManager, pI2CMaster, pSPIMaster, pConfig),
      m_lastTick(0),
      m_lastTick1(0),
      m_bNeedBankSwitch(false),
      m_nTargetBank(0),
      m_pTimer(nullptr),
      m_nBankSwitchTimer(0) {


      CTimer::Get();
  
      assert(m_pConfig);

      s_pThis = this;

      __atomic_store_n(&sample_write_idx, 0u, __ATOMIC_RELAXED);

  // select the sound device
  const char *pDeviceName = pConfig->GetSoundDevice();
  if (strcmp(pDeviceName, "i2s") == 0) {
    LOGNOTE("I2S mode");
    m_pSoundDevice = new CI2SSoundBaseDevice(
        pInterrupt, 32000, pConfig->GetChunkSize(), false, pI2CMaster,
        pConfig->GetDACI2CAddress(), CI2SSoundBaseDevice::DeviceModeTXOnly,
        2); // 2 channels - L+R
  } else if (strcmp(pDeviceName, "hdmi") == 0) {
#if RASPPI == 5
    LOGNOTE("HDMI mode NOT supported on RPI 5.");
#else
    LOGNOTE("HDMI mode");

    m_pSoundDevice =
        new CHDMISoundBaseDevice(pInterrupt, 32000, pConfig->GetChunkSize());

    // The channels are swapped by default in the HDMI sound driver.
    // TODO: Remove this line, when this has been fixed in the driver.
    m_bChannelsSwapped = !m_bChannelsSwapped;
#endif
  } else {
    LOGNOTE("PWM mode");

    m_pSoundDevice =
        new CPWMSoundBaseDevice(pInterrupt, 32000, pConfig->GetChunkSize());
  }
  
};

bool CMiniJV880::Initialize(void) {
  assert(m_pConfig);
  assert(m_pSoundDevice);

  n_mMCUcycles = m_pConfig->GetMCUcycles ();
  LOGNOTE("MCU cycles %d", n_mMCUcycles);
  //LOGNOTE("Temp: %d", m_CPUThrottle.GetTemperature ());

  if (!m_UI.Initialize ())
	{
    LOGERR("Failed to initialize UI");
		return false;
	}

	assert (m_pConfig);
	if (!m_Serial.Initialize(m_pConfig->GetMIDIBaudRate ())) 
    {
        LOGERR("Failed to initialize Serial MIDI");
        return false;
    }
	unsigned ser_options = m_Serial.GetOptions();
	// Ensure CR->CRLF translation is disabled for MIDI links
	ser_options &= ~(SERIAL_OPTION_ONLCR);
	m_Serial.SetOptions(ser_options);
  LOGNOTE("Serial MIDI Initialized");
    midiParser.Init(this);

    
  
    if (!LoadMainRoms(m_pConfig->GetExpRom())) {
        return false;
    }
    
    int ret = 0;

    uint8_t* nvram = (uint8_t*)m_romInfos[0].data;  // jv880_nvram.bin
    uint8_t* rom1 = (uint8_t*)m_romInfos[1].data;   // jv880_rom1.bin
    uint8_t* rom2 = (uint8_t*)m_romInfos[2].data;   // jv880_rom2.bin
    uint8_t* pcm1 = (uint8_t*)m_romInfos[3].data;   // jv880_waverom1.bin
    uint8_t* pcm2 = (uint8_t*)m_romInfos[4].data;   // jv880_waverom2.bin
    if (m_pConfig->GetExpRom() == 0) {
        ret = mcu.startSC55(rom1, rom2, pcm1, pcm2, nvram, nullptr); 
    } else {
        uint8_t* exp1 = (uint8_t*)m_romInfos[m_pConfig->GetExpRom() + 6].data;
        ret = mcu.startSC55(rom1, rom2, pcm1, pcm2, nvram, exp1);
    }
    if (!ret) {
        LOGNOTE("SC55 emulator started");
    }
    
  // setup and start the sound device
  int Channels = 2; // 16-bit Stereo
  // Need 2 x ChunkSize / Channel queue frames as the audio driver uses
  // two DMA channels each of ChunkSize and one single single frame
  // contains a sample for each of all the channels.
  //
  // See discussion here: https://github.com/rsta2/circle/discussions/453
  if (!m_pSoundDevice->AllocateQueueFrames(2 * m_pConfig->GetChunkSize() /
                                           Channels)) {
    LOGERR("Cannot allocate sound queue");

    return false;
  }

  m_pSoundDevice->SetWriteFormat(SoundFormatSigned16, Channels);

  m_nQueueSizeFrames = m_pSoundDevice->GetQueueSizeFrames();

  m_pSoundDevice->Start();

  CMultiCoreSupport::Initialize();
  LOGNOTE("initialised");

  return true;
}

void CMiniJV880::Process(bool bPlugAndPlayUpdated) {

  m_UI.Process ();

  int nRead = m_Serial.Read(m_MIDIBuffer, sizeof(m_MIDIBuffer));
    if (nRead > 0) {
        midiParser.FeedSerialBytes(m_MIDIBuffer, nRead);  // <<<<< ТУТ
    }

  if (!bPlugAndPlayUpdated)
    return;

  if (m_pMIDIDevice == 0) {
    m_pMIDIDevice =
        (CUSBMIDIDevice *)CDeviceNameService::Get()->GetDevice("umidi1", FALSE);
    if (m_pMIDIDevice != 0) {
      m_pMIDIDevice->RegisterPacketHandler(USBMIDIMessageHandler);
      m_pMIDIDevice->RegisterRemovedHandler(DeviceRemovedHandler, this);
    }
  }
}

void CMiniJV880::USBMIDIMessageHandler(unsigned nCable, u8 *pPacket,
                                       unsigned nLength) {
  if (!pPacket || nLength == 0) return;
  s_pThis->midiParser.FeedUSBMIDIPacket(pPacket, nLength);
}

// Schedule bank switch with 2-second delay to handle multiple rapid changes
void CMiniJV880::ScheduleBankSwitch(int bankNumber) {
    m_nTargetBank = bankNumber;
    m_bNeedBankSwitch = true;
    
    // Cancel previous timer if it was set
    if (m_nBankSwitchTimer != 0) {
        m_pTimer->CancelKernelTimer(m_nBankSwitchTimer);
        m_nBankSwitchTimer = 0;
    }
    
    // Set timer for 2 seconds to switch bank (to handle rapid encoder changes)
    // Convert 2 seconds to ticks (2000ms * HZ / 1000 = 2 * HZ ticks)
    unsigned nDelay = 2 * HZ; // 2 seconds in ticks (HZ = 100 ticks per second)
    m_nBankSwitchTimer = m_pTimer->StartKernelTimer(nDelay, BankSwitchTimerHandler, this, nullptr);
}

// Timer handler to perform actual bank switch
void CMiniJV880::BankSwitchTimerHandler(TKernelTimerHandle hTimer, void *pParam, void *pContext) {
    CMiniJV880 *pThis = static_cast<CMiniJV880*>(pParam);
    
    if (pThis->m_bNeedBankSwitch) {
        pThis->m_bNeedBankSwitch = false;
        pThis->m_nBankSwitchTimer = 0; // Clear timer handle
        
        // Perform bank switch
        //pThis->m_CMiniJV880.switchPatchBank(pThis->m_nTargetBank);

        LOGNOTE("Bank false switched to %d", pThis->m_nTargetBank);
        // pThis->mcu.SC55_Reset();
    }
}

void CMiniJV880::HandleFullMIDIMessage(const uint8_t* pData, uint8_t nLength)
{
    if (nLength == 0) return;

    uint8_t status = pData[0];

    // ===== Priority 1: Note On/Off =====
    if ((status & 0xF0) == 0x80 || (status & 0xF0) == 0x90) {
        if (nLength == 3) {
            mcu.postMidiSC55(pData, nLength);
            return;
        }
    }

    // ===== Priority 2: Pitch Bend =====
    if ((status & 0xF0) == 0xE0) {
        if (nLength == 3) {
            mcu.postMidiSC55(pData, nLength);
            return;
        }
    }

    // ===== Priority 3: Modulation (CC 1) =====
    if ((status & 0xF0) == 0xB0 && nLength == 3 && pData[1] == 1) {
        mcu.postMidiSC55(pData, nLength);
        return;
    }

    // ===== Priority 4: Bank Switch (CC 32) =====
    if ((status & 0xF0) == 0xB0 && nLength == 3 && pData[1] == 32) {
        //ScheduleBankSwitch(pData[2]);
    }

    // ===== Priority 5: NVRAM Save Trigger =====
    if ((status & 0xF0) == 0xB0 && nLength == 3 && pData[1] == m_UI.m_nMIDISaveNVRAM && pData[2] == 0) {
        SaveNVRAMIncremental();
        return;
    }

    // ===== Priority 6: UI CC Messages =====
    if ((status & 0xF0) == 0xB0 && nLength == 3) {
        uint8_t ccNumber = pData[1];
        uint8_t ccValue  = pData[2];

        if (m_UI.m_nMIDIButtonChannel != 0) {
            uint8_t channel = status & 0x0F;
            uint8_t expected = m_UI.m_nMIDIButtonChannel - 1;

            if (m_UI.m_nMIDIButtonChannel == 17 || expected == channel) {
                auto handleButton = [this, ccNumber, ccValue](uint8_t confCC, CUIButton::BtnEvent ev) {
                    if (ccNumber == confCC) {
                        if (ccValue < 64) m_UI.TriggerUIButtonEvent(ev);
                        else m_UI.TriggerUIButtonEvent(CUIButton::BtnEventRelease);
                        return true;
                    }
                    return false;
                };

                if (handleButton(m_UI.m_nMIDIPreview,      CUIButton::BtnEventPreview)) return;
                if (handleButton(m_UI.m_nMIDILeft,         CUIButton::BtnEventLeft))    return;
                if (handleButton(m_UI.m_nMIDIRight,        CUIButton::BtnEventRight))   return;
                if (handleButton(m_UI.m_nMIDIData,         CUIButton::BtnEventData))    return;
                if (handleButton(m_UI.m_nMIDIToneSelect,   CUIButton::BtnEventToneSelect)) return;
                if (handleButton(m_UI.m_nMIDIPatchPerform, CUIButton::BtnEventPatchPerform)) return;
                if (handleButton(m_UI.m_nMIDIEdit,         CUIButton::BtnEventEdit))    return;
                if (handleButton(m_UI.m_nMIDISystem,       CUIButton::BtnEventSystem))  return;
                if (handleButton(m_UI.m_nMIDIRhythm,       CUIButton::BtnEventRhythm))  return;
                if (handleButton(m_UI.m_nMIDIUtility,      CUIButton::BtnEventUtility)) return;
                if (handleButton(m_UI.m_nMIDIMute,         CUIButton::BtnEventMute))    return;
                if (handleButton(m_UI.m_nMIDIMonitor,      CUIButton::BtnEventMonitor)) return;
                if (handleButton(m_UI.m_nMIDICompare,      CUIButton::BtnEventCompare)) return;
                if (handleButton(m_UI.m_nMIDIEnter,        CUIButton::BtnEventEnter))   return;

                // Encoder
                if (ccNumber == m_UI.m_nMIDIUp && ccValue < 64) {
                    m_UI.TriggerUIButtonEvent(CUIButton::BtnEventRelease);
                    mcu.MCU_EncoderTrigger(1);
                    return;
                }
                if (ccNumber == m_UI.m_nMIDIDown && ccValue < 64) {
                    m_UI.TriggerUIButtonEvent(CUIButton::BtnEventRelease);
                    mcu.MCU_EncoderTrigger(0);
                    return;
                }
            }
        }
    }
    // add checksum for Roland sysex messages 
    if (status == 0xF0 && nLength > 6 && pData[nLength - 1] == 0xF7) {
        if (pData[1] == 0x41) {
            int chk_idx = nLength - 2;
            if (chk_idx < 5) return;

            int sum = 0;
            for (int i = 2; i < chk_idx; i++) sum += pData[i];
            sum &= 0x7F;
            uint8_t checksum = (128 - sum) & 0x7F;

            uint8_t out[256];
            memcpy(out, pData, nLength);
            out[chk_idx] = checksum;

            // Log full outgoing SysEx in HEX
            char buf[512];
            int len = 0;
            for (int i = 0; i < nLength && len < sizeof(buf) - 4; i++) {
                len += snprintf(buf + len, sizeof(buf) - len, "%02X ", out[i]);
            }
            if (len > 0) buf[len - 1] = 0; // remove trailing space
            LOGNOTE(buf);

            mcu.postMidiSC55(out, nLength);
            return;
        }
    }

    // ===== All other messages (excluding SysEx) =====
    mcu.postMidiSC55(pData, nLength);
}

void CMiniJV880::SaveNVRAMIncremental() {
    char filename[64];
    
    // Ensure the directory exists first
    FRESULT dirRes = f_mkdir("nvram");
    if (dirRes != FR_OK && dirRes != FR_EXIST) {
        LOGERR("Cannot create nvram directory, error: %d", dirRes);
        return;
    }
    
    // Find a free filename
    FIL file;
    FRESULT res;
    
    do {
        sprintf(filename, "nvram/jv880_nvram%d.bin", ++m_nNVRAMSaveCounter);
        
        // Try to open file for reading to check if it exists
        res = f_open(&file, filename, FA_READ);
        if (res == FR_OK) {
            // File exists - close and try next number
            f_close(&file);
        }
    } while (res == FR_OK); // Continue while we find existing files
    
    // Now filename contains a non-existing filename
    // Open for writing
    res = f_open(&file, filename, FA_WRITE | FA_CREATE_ALWAYS);
    if (res != FR_OK) {
        LOGERR("Cannot open file %s for writing, error: %d", filename, res);
        return;
    }
    
    // Write NVRAM data to file
    UINT bytesWritten;
    res = f_write(&file, mcu.nvram, 0x8000, &bytesWritten);
    f_close(&file);
    
    // Check if write was successful
    if (res == FR_OK && bytesWritten == 0x8000) {
        LOGNOTE("NVRAM saved to %s", filename);
    } else {
        LOGERR("Failed to save NVRAM to %s, written: %d bytes, error: %d", 
               filename, bytesWritten, res);
    }
}


void CMiniJV880::DeviceRemovedHandler(CDevice *pDevice, void *pContext) {
  LOGERR("CMiniJV880::DeviceRemovedHandler");

  CMiniJV880 *pThis = static_cast<CMiniJV880 *>(pContext);
  assert(pThis != 0);

  if (pDevice == pThis->m_pMIDIDevice)
    pThis->m_pMIDIDevice = 0;
}

void CMiniJV880::Run(unsigned nCore) {
    assert(1 <= nCore && nCore < CORES);
    //int nSamples = 0;
    

    if (nCore == 2) { // 2nd core - MCU + audio output
        const int MCU_INSTR_BURST = 64;
        //unsigned log_counter = 0;
        while (true) {
            unsigned nFrames = m_nQueueSizeFrames - m_pSoundDevice->GetQueueFramesAvail();
            if (nFrames < m_nQueueSizeFrames / 2) {
                CTimer::SimpleMsDelay(1);
                continue;
            }
            int nSamples = (int)nFrames * 2;
            if (nSamples >= (int)AUDIO_BUFFER_SIZE) nSamples = (int)AUDIO_BUFFER_SIZE - 2;
            // allocate contiguous output buffer
            int16_t *out_buf = (int16_t*)malloc(nSamples * sizeof(int16_t));
            if (!out_buf) { CTimer::SimpleMsDelay(1); continue; }
            int out_pos = 0;
            while (out_pos < nSamples) {
                // 1) try to copy available audio first
                uint64_t w = __atomic_load_n(&sample_write_idx, __ATOMIC_ACQUIRE);
                uint64_t r = __atomic_load_n(&sample_read_idx,  __ATOMIC_RELAXED);
                uint64_t avail = w - r;
                if (avail > 0) {
                    uint32_t need = (uint32_t)(nSamples - out_pos);
                    uint32_t to_copy = (avail < need) ? (uint32_t)avail : need;
                    uint32_t idx = (uint32_t)(r & AUDIO_BUFFER_MASK);
                    uint32_t first = AUDIO_BUFFER_SIZE - idx;
                    if (first > to_copy) first = to_copy;
                    memcpy(&out_buf[out_pos], &sample_buffer[idx], first * sizeof(int16_t));
                    out_pos += first;
                    r += first;
                    uint32_t rem = to_copy - first;
                    if (rem) {
                        memcpy(&out_buf[out_pos], &sample_buffer[r & AUDIO_BUFFER_MASK], rem * sizeof(int16_t));
                        out_pos += rem;
                        r += rem;
                    }
                    __atomic_store_n(&sample_read_idx, r, __ATOMIC_RELEASE);
                    continue; // loop to fill remaining samples
                }
                // 2) if no samples ready, run bounded MCU burst to allow producer to progress
                int instr = 0;
                while (instr < MCU_INSTR_BURST) {
                    if (!mcu.mcu.ex_ignore)
                        mcu.MCU_Interrupt_Handle();
                    else
                        mcu.mcu.ex_ignore = 0;

                    if (!mcu.mcu.sleep)
                        mcu.MCU_ReadInstruction();
                    mcu.mcu.cycles += n_mMCUcycles;
                    __atomic_store_n(&mcu.mcu.cycles, mcu.mcu.cycles, __ATOMIC_RELEASE);
                    mcu.TIMER_Clock(mcu.mcu.cycles);
                    mcu.MCU_UpdateUART_RX();
                    mcu.MCU_UpdateUART_TX();
                    mcu.MCU_UpdateAnalog(mcu.mcu.cycles);
                    ++instr;
                }
                // then re-check available
            } // out_pos loop

            // write to audio device
            int len = nSamples * sizeof(int16_t);
            if (m_pSoundDevice->Write(out_buf, len) != len) {
                LOGERR("Sound data dropped");
            }
            free(out_buf);
        }
    }
    else if (nCore == 3) { // 3rd core - PCM Update
        constexpr uint64_t MCU_CLOCK_HZ = 12000000ull; // if your MCU clock differs, set accordingly
        constexpr uint32_t AUDIO_RATE = 32000u;
        constexpr uint64_t CYCLES_PER_SAMPLE = MCU_CLOCK_HZ / AUDIO_RATE; // 375 typical for H8@12MHz
        //constexpr uint64_t CYCLES_PER_SAMPLE_FP = CYCLES_PER_SAMPLE << 32; // fixed-point
        const uint32_t MAX_SAMPLES_PER_ITER = 128; // bound to avoid huge bursts

        uint64_t last_generated_cycles = __atomic_load_n(&mcu.mcu.cycles, __ATOMIC_RELAXED);
        //uint64_t cycles_acc_fp = 0; // optional accumulator if needed by PCM internals

        while (true) {
            uint64_t cycles_target = __atomic_load_n(&mcu.mcu.cycles, __ATOMIC_ACQUIRE);
            if (cycles_target <= last_generated_cycles) {
                CTimer::SimpleMsDelay(0);
                continue;
            }

            uint64_t cycles_avail = cycles_target - last_generated_cycles;
            uint64_t samples_to_gen = cycles_avail / CYCLES_PER_SAMPLE;
            while (samples_to_gen > 0) {
                uint32_t gen = (uint32_t) (samples_to_gen > MAX_SAMPLES_PER_ITER ? MAX_SAMPLES_PER_ITER : samples_to_gen);
                uint64_t pcm_target = last_generated_cycles + gen * CYCLES_PER_SAMPLE;

                // PCM_Update must advance internal pcm.cycles up to pcm_target and call MCU_PostSample for samples
                mcu.pcm.PCM_Update(pcm_target);

                last_generated_cycles = pcm_target;
                samples_to_gen -= gen;

                // tiny yield to let consumer/core2 progress and reduce bursts
                CTimer::SimpleMsDelay(0);
            }
        }
    }

}


bool CMiniJV880::LoadMainRoms(uint8_t ExpRom) {
    //LOGNOTE("Loading main ROMs for synthesizer and %d exp", ExpRom);
    
    int main_rom_indices[6];
    unsigned cr;
    
    if (ExpRom == 0 || ExpRom > 19) {
        const int indices[] = {0, 1, 2, 3, 4}; // nvram, rom1, rom2, waverom1, waverom2
        memcpy(main_rom_indices, indices, sizeof(indices));
        cr = 5;
    } else {
        const int indices[] = {0, 1, 2, 3, 4, ExpRom + 6}; // nvram, rom1, rom2, waverom1, waverom2, expansion
        memcpy(main_rom_indices, indices, sizeof(indices));
        cr = 6;
    }
    
    for (unsigned i = 0; i < cr; i++) {
        int rom_index = main_rom_indices[i];
        if (!LoadRom(rom_index)) {
            LOGERR("Failed to load ROM at index %d", rom_index);
            return false;
        }
    }
    
    LOGNOTE("All main ROMs loaded successfully");
    return true;
}

bool CMiniJV880::LoadRom(uint8_t rom_index) {
    
    if (rom_index >= ROM_COUNT) {
        LOGERR("Invalid ROM index: %d", rom_index);
        return false;
    }

    RomInfo& rom = m_romInfos[rom_index];
    std::string fullPath = "roms/";
    fullPath += rom.filename;

    // Check if file exists
    if (!FileExists(fullPath.c_str())) {
        LOGERR("ROM file not found: %s", rom.filename);
        return false;
    }
    
    // Check if already loaded
    if (rom.isLoaded) {
        //LOGNOTE("ROM %s already loaded", fullPath.c_str());
        return true;
    }
    
    // Allocate memory
    rom.data = malloc(rom.size);
    if (!rom.data) {
        LOGERR("Not enough memory for %s (size: %zu)", fullPath.c_str(), rom.size);
        return false;
    }
    
    // Load file
    if (!LoadFile(fullPath.c_str(), (uint8_t*)rom.data, rom.size)) {
        LOGERR("Cannot load %s", fullPath.c_str());
        free(rom.data);
        rom.data = nullptr;
        return false;
    }
    
    //LOGNOTE("Loaded %s successfully", fullPath.c_str());
    
    // Check if descrambling is needed
    if (rom.needsUnscramble) {
        uint8_t* descrambled_data = (uint8_t*)malloc(rom.size);
        if (!descrambled_data) {
            LOGERR("Not enough memory for descrambled %s", fullPath.c_str());
            free(rom.data);
            rom.data = nullptr;
            return false;
        }
        //LOGNOTE("Descrambling %s...", rom.filename);
        UnscrambleRom((uint8_t*)rom.data, descrambled_data, rom.size);
        free(rom.data);
        rom.data = descrambled_data;
        //LOGNOTE("Descrambled %s successfully", rom.filename);
    }
    
    // Mark as loaded
    rom.isLoaded = true;
    return true;
}

// Helper function to check if file exists
bool CMiniJV880::FileExists(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (file) {
        fclose(file);
        return true;
    }
    return false;
}

bool CMiniJV880::LoadFile(const char* filename, uint8_t* buffer, size_t size) {
    FIL f;
    unsigned int nBytesRead = 0;

    if (f_open(&f, filename, FA_READ | FA_OPEN_EXISTING) != FR_OK) {
        LOGERR("Cannot open %s", filename);
        return false;
    }
    FRESULT fr = f_read(&f, buffer, size, &nBytesRead);
    f_close(&f);

    if (fr != FR_OK) {
        LOGERR("f_read error %d for %s", fr, filename);
        return false;
    }

    if (nBytesRead != size) {
        LOGERR("Unexpected file size for %s: expected %zu, read %u", filename, size, nBytesRead);
        return false;
    }

    return true;
}


void CMiniJV880::UnscrambleRom(const uint8_t *src, uint8_t *dst, int len) {
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


// additional temporary functions 
void CMiniJV880::LogPCM(uint64_t logcyc1) {
 return;
  LOGNOTE("PCM Update running | MCU cycles: %u", mcu.mcu.cycles);
 return; 
}

void CMiniJV880::LogMCU(uint64_t logcyc,uint64_t  logwriteptr,int  logsleep,int  logex) {
return;
  LOGNOTE("MCU cycles: %u | sample_write_ptr: %u | sleep: %d | ex_ignore: %d",
                                mcu.mcu.cycles, sample_write_idx,
                                mcu.mcu.sleep, mcu.mcu.ex_ignore);
 return;
}