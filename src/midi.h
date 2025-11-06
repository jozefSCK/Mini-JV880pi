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