//
// udpmididevice.h
//
// Mini-JV880pi UDP MIDI Device (with RTP-MIDI support)
// Based on MiniDexed implementation
// Copyright (C) 2026  Gene J.B. (Sterr1)
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