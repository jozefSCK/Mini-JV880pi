//
// udpmididevice.cpp
//
// Mini-JV880pi UDP MIDI Device
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

LOGMODULE("udpmididevice");

CUDPMIDIDevice::CUDPMIDIDevice(CMiniJV880* pSynthesizer, CConfig* pConfig)
:   m_pSynthesizer(pSynthesizer),
    m_pConfig(pConfig),
    m_pUDPMIDIReceiver(nullptr),
    m_pUDPSendSocket(nullptr),
    m_UDPDestPort(1999)
{
    m_Parser.Init(m_pSynthesizer);
}

CUDPMIDIDevice::~CUDPMIDIDevice()
{
    // Только для CUDPMIDIReceiver (без RTP-MIDI)
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
    if (!m_pConfig->GetUDPMIDIEnabled()) {
        LOGNOTE("UDP MIDI disabled in config");
        return true;
    }

    // Raw UDP MIDI receiver
    m_pUDPMIDIReceiver = new CUDPMIDIReceiver(this);
    if (!m_pUDPMIDIReceiver->Initialize()) {
        LOGERR("Failed to init UDP MIDI receiver");
        delete m_pUDPMIDIReceiver;
        m_pUDPMIDIReceiver = nullptr;
        return false;
    }
    LOGNOTE("UDP MIDI receiver initialized on port 1999");

    // UDP sender setup
    m_UDPDestAddress.Set(0xFFFFFFFF); // Broadcast
    
    if (m_pConfig->GetUDPMIDIIPAddress().IsSet()) {
        m_UDPDestAddress.Set(m_pConfig->GetUDPMIDIIPAddress());
    }

    CString IPAddressString;
    m_UDPDestAddress.Format(&IPAddressString);

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
    }

    return true;
}

void CUDPMIDIDevice::OnUDPMIDIDataReceived(const u8* pData, size_t nSize)
{
    // Используем FeedSerialBytes вместо FeedUDPMIDIPacket
    m_Parser.FeedSerialBytes(pData, nSize);
}

void CUDPMIDIDevice::Send(const u8* pMessage, size_t nLength)
{
    if (m_pUDPSendSocket) {
        int res = m_pUDPSendSocket->SendTo(
            pMessage, 
            nLength, 
            0, 
            m_UDPDestAddress, 
            m_UDPDestPort
        );
        
        if (res < 0) {
            LOGERR("Failed to send %u bytes via UDP MIDI", (unsigned)nLength);
        }
    }
}