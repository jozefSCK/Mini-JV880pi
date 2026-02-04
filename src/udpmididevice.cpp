//
// udpmididevice.cpp
//
// Mini-JV880pi UDP MIDI Device (with RTP-MIDI support)
// Based on MiniDexed implementation
// Copyright (C) 2026  Gene J.B. (Sterr1)
//

#include "udpmididevice.h"
#include "minijv880.h"
#include "config.h"
#include <circle/logger.h>
#include <circle/net/netsubsystem.h>
#include <circle/net/in.h>
#include <cstring>
#include <assert.h>

#define VIRTUALCABLE 0

LOGMODULE("udpmididevice");

CUDPMIDIDevice::CUDPMIDIDevice(CMiniJV880* pSynthesizer, CConfig* pConfig)
:   m_pSynthesizer(pSynthesizer),
    m_pConfig(pConfig),
    m_pAppleMIDIParticipant(nullptr),
    m_bIsAppleMIDIConnected(false),
    m_pUDPMIDIReceiver(nullptr),
    m_pUDPSendSocket(nullptr),
    m_UDPDestPort(1999)
{
    m_Parser.Init(m_pSynthesizer);
}

CUDPMIDIDevice::~CUDPMIDIDevice()
{
    if (m_pAppleMIDIParticipant) {
        delete m_pAppleMIDIParticipant;
        m_pAppleMIDIParticipant = nullptr;
    }

    if (m_pUDPMIDIReceiver) {
        delete m_pUDPMIDIReceiver;
        m_pUDPMIDIReceiver = nullptr;
    }

    if (m_pUDPSendSocket) {
        delete m_pUDPSendSocket;
        m_pUDPSendSocket = nullptr;
    }
}

boolean CUDPMIDIDevice::Initialize()
{
    LOGNOTE("=== UDP MIDI Device Initialization ===");

    // === Initialize RTP-MIDI (Apple MIDI) ===
    // This is used by rtpMIDI on Windows and macOS
    m_pAppleMIDIParticipant = new CAppleMIDIParticipant(
        &m_Random, 
        this, 
        m_pConfig->GetNetworkHostname()
    );
    
    if (!m_pAppleMIDIParticipant->Initialize()) {
        LOGERR("Failed to initialize RTP-MIDI listener");
        delete m_pAppleMIDIParticipant;
        m_pAppleMIDIParticipant = nullptr;
    } else {
        LOGNOTE("RTP-MIDI (Apple MIDI) listener initialized");
        LOGNOTE("  -> Compatible with rtpMIDI (Windows/Mac)");
    }

    // === Initialize Simple UDP MIDI ===
    // This is used by ipMIDI and custom tools
    if (m_pConfig->GetUDPMIDIEnabled()) {
        m_pUDPMIDIReceiver = new CUDPMIDIReceiver(this);
        if (!m_pUDPMIDIReceiver->Initialize()) {
            LOGERR("Failed to initialize simple UDP MIDI receiver");
            delete m_pUDPMIDIReceiver;
            m_pUDPMIDIReceiver = nullptr;
        } else {
            LOGNOTE("Simple UDP MIDI receiver initialized on port 1999");
            LOGNOTE("  -> Compatible with ipMIDI and custom tools");
        }

        // Setup UDP sender (for MIDI output/thru if needed)
        m_UDPDestAddress.Set(0xFFFFFFFF); // Broadcast by default
        
        if (m_pConfig->GetUDPMIDIIPAddress().IsSet()) {
            m_UDPDestAddress.Set(m_pConfig->GetUDPMIDIIPAddress());
        }

        CString IPAddressString;
        m_UDPDestAddress.Format(&IPAddressString);

        // Address 0.0.0.0 disables transmit
        if (!m_UDPDestAddress.IsNull()) {
            CNetSubSystem* pNet = CNetSubSystem::Get();
            m_pUDPSendSocket = new CSocket(pNet, IPPROTO_UDP);
            
            if (m_pUDPSendSocket->Connect(m_UDPDestAddress, m_UDPDestPort) >= 0) {
                m_pUDPSendSocket->SetOptionBroadcast(TRUE);
                LOGNOTE("UDP MIDI sender initialized. Target: %s:%u",
                        (const char*)IPAddressString, m_UDPDestPort);
            } else {
                LOGERR("Failed to connect UDP send socket");
                delete m_pUDPSendSocket;
                m_pUDPSendSocket = nullptr;
            }
        } else {
            LOGNOTE("UDP MIDI sender disabled (target 0.0.0.0)");
        }
    } else {
        LOGNOTE("Simple UDP MIDI is disabled in configuration");
    }

    LOGNOTE("=== UDP MIDI Device Ready ===");
    return true;
}

// ============================================
// RTP-MIDI (Apple MIDI) Callbacks
// ============================================

void CUDPMIDIDevice::OnAppleMIDIDataReceived(const u8* pData, size_t nSize)
{
    // RTP-MIDI library already extracted pure MIDI from RTP packets
    LOGDBG("RTP-MIDI RX: %u bytes", (unsigned)nSize);
    m_Parser.FeedSerialBytes(pData, nSize);
}

void CUDPMIDIDevice::OnAppleMIDIConnect(const CIPAddress* pIPAddress, const char* pName)
{
    m_bIsAppleMIDIConnected = true;
    
    CString IPString;
    pIPAddress->Format(&IPString);
    LOGNOTE("RTP-MIDI device connected: %s (%s)", 
            pName ? pName : "Unknown", 
            (const char*)IPString);
}

void CUDPMIDIDevice::OnAppleMIDIDisconnect(const CIPAddress* pIPAddress, const char* pName)
{
    m_bIsAppleMIDIConnected = false;
    
    CString IPString;
    pIPAddress->Format(&IPString);
    LOGNOTE("RTP-MIDI device disconnected: %s (%s)", 
            pName ? pName : "Unknown", 
            (const char*)IPString);
}

// ============================================
// Simple UDP MIDI Callback
// ============================================

void CUDPMIDIDevice::OnUDPMIDIDataReceived(const u8* pData, size_t nSize)
{
    // Simple UDP MIDI - raw MIDI bytes
    LOGDBG("UDP-MIDI RX: %u bytes", (unsigned)nSize);
    m_Parser.FeedSerialBytes(pData, nSize);
}