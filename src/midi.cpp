#include "midi.h"
#include "minijv880.h"

void MidiParser::Init(CMiniJV880* synth) {
    target_ = synth;
    ResetParser();
}

void MidiParser::ResetParser() {
    status_ = 0;
    count_ = 0;
    sysex_count_ = 0;
    in_sysex_ = false;
}

void MidiParser::ParseByte(uint8_t b) {
    if (b == 0xF0) {
        in_sysex_ = true;
        sysex_count_ = 0;
        sysex_buffer_[sysex_count_++] = b;
        return;
    }

    if (b == 0xF7 && in_sysex_) {
        sysex_buffer_[sysex_count_++] = b;
        if (target_) {
            target_->HandleFullMIDIMessage(sysex_buffer_, sysex_count_);
        }
        in_sysex_ = false;
        sysex_count_ = 0;
        return;
    }

    if (in_sysex_) {
        if (sysex_count_ < sizeof(sysex_buffer_)) {
            sysex_buffer_[sysex_count_++] = b;
        }
        return;
    }

    if (b & 0x80) {  // Status byte
        status_ = b;
        count_ = 0;
        return;
    }

    if (!status_) return;

    data_[count_++] = b;
    const uint8_t needed = ((status_ & 0xF0) == 0xC0 || (status_ & 0xF0) == 0xD0) ? 1 : 2;

    if (count_ >= needed) {
        uint8_t fullMessage[4];
        fullMessage[0] = status_;
        for (uint8_t i = 0; i < count_; i++) {
            fullMessage[1 + i] = data_[i];
        }
        if (target_) {
            target_->HandleFullMIDIMessage(fullMessage, 1 + count_);
        }
        count_ = 0;
    }
}

void MidiParser::FeedSerialBytes(const uint8_t* data, unsigned len) {
    for (unsigned i = 0; i < len; ++i) {
        ParseByte(data[i]);
    }
}

void MidiParser::FeedUSBMIDIPacket(uint8_t* data, unsigned len) {
    for (unsigned i = 0; i < len; ++i) {
        ParseByte(data[i]);
    }
}

void MidiParser::ProcessMIDI() {
    if (target_) {
        int n = target_->GetSerial().Read(target_->GetMIDIBuffer(), 256);
        if (n > 0) {
            FeedSerialBytes(target_->GetMIDIBuffer(), n);
        }
    }
}