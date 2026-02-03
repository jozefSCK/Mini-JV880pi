//
// udpmididevice.h
//

#ifndef _udpmididevice_h
#define _udpmididevice_h

#include "midi.h"
#include <circle/net/socket.h>
#include <circle/net/ipaddress.h>
#include <circle/types.h>

class CMiniJV880;
class CConfig;

class IUDPMIDIReceiveHandler {
public:
    virtual void OnUDPMIDIDataReceived(const u8* pData, size_t nSize) = 0;
};

class CUDPMIDIReceiver;

class CUDPMIDIDevice : public IUDPMIDIReceiveHandler
{
public:
    CUDPMIDIDevice(CMiniJV880* pSynthesizer, CConfig* pConfig);
    ~CUDPMIDIDevice();

    boolean Initialize();
    void Send(const u8* pMessage, size_t nLength);

    // IUDPMIDIReceiveHandler
    void OnUDPMIDIDataReceived(const u8* pData, size_t nSize) override;

private:
    CMiniJV880* m_pSynthesizer;
    CConfig* m_pConfig;
    MidiParser m_Parser;

    // Raw UDP MIDI only
    CUDPMIDIReceiver* m_pUDPMIDIReceiver;
    CSocket* m_pUDPSendSocket;
    CIPAddress m_UDPDestAddress;
    u16 m_UDPDestPort;
};

#endif