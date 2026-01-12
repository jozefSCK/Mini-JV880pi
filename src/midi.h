//
// midi.h
//
// Mini-JV880pi is a rompler-style synthesizer closely modeled JV-880 
// for bare metal Raspberry Pi
// Copyright (C) 2026  Gene J.B. (Sterr1)
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
#pragma once
#include <cstdint>

class CMiniJV880; // Forward declaration

class MidiParser {
public:
    void Init(CMiniJV880* synth);
    void ProcessMIDI();

    void FeedSerialByte(uint8_t b);
    void FeedSerialBytes(const uint8_t* data, unsigned len);  // <<<<< ДОБАВИТЬ
    void FeedUSBMIDIPacket(uint8_t* data, unsigned len);

private:
    void ResetParser();
    void ParseByte(uint8_t b);

    CMiniJV880* target_ = nullptr;

    // Parser state
    uint8_t status_ = 0;
    uint8_t data_[2];
    uint8_t count_ = 0;

    // SysEx
    uint8_t sysex_buffer_[1024];
    uint16_t sysex_count_ = 0;
    bool in_sysex_ = false;
};