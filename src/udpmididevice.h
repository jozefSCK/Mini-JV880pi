//
// udpmididevice.h
//
// Mini-JV880pi UDP MIDI Device (with RTP-MIDI support)
// Based on MiniDexed implementation
// Copyright (C) 2022  The MiniDexed Team
// Copyright (C) 2026  Plamikcho, Giulioz, Gene J.B. (Sterr1)
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
#ifndef _udpmididevice_h
#define _udpmididevice_h

#include "midi.h"
#include "net/udpmidi.h"
#include "net/applemidi.h"
#include <circle/net/socket.h>
#include <circle/net/ipaddress.h>
#include <circle/types.h>

class CMiniJV880;
class CConfig;

// Handler for both UDP MIDI and Apple MIDI (RTP-MIDI)
class CUDPMIDIDevice : public CUDPMIDIHandler,
                        public CAppleMIDIHandler
{
public:
    CUDPMIDIDevice(CMiniJV880* pSynthesizer, CConfig* pConfig);
    virtual ~CUDPMIDIDevice();

    boolean Initialize();

    // CUDPMIDIHandler interface (simple UDP MIDI)
    void OnUDPMIDIDataReceived(const u8* pData, size_t nSize) override;

    // CAppleMIDIHandler interface (RTP-MIDI)
    void OnAppleMIDIDataReceived(const u8* pData, size_t nSize) override;
    void OnAppleMIDIConnect(const CIPAddress* pIPAddress, const char* pName) override;
    void OnAppleMIDIDisconnect(const CIPAddress* pIPAddress, const char* pName) override;

private:
    CMiniJV880* m_pSynthesizer;
    CConfig* m_pConfig;
    MidiParser m_Parser;

    // RTP-MIDI (Apple MIDI) participant
    CAppleMIDIParticipant* m_pAppleMIDIParticipant;
    boolean m_bIsAppleMIDIConnected;
    CBcmRandomNumberGenerator m_Random; // For RTP-MIDI session IDs

    // Simple UDP MIDI receiver
    CUDPMIDIReceiver* m_pUDPMIDIReceiver;

    // UDP sender (for MIDI Thru/Echo if needed)
    CSocket* m_pUDPSendSocket;
    CIPAddress m_UDPDestAddress;
    u16 m_UDPDestPort;
};

#endif