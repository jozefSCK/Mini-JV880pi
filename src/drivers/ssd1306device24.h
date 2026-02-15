//
// ssd1306device24.h
//
// Mini-JV880pi - Roland JV880 synthesizer for bare metal Raspberry Pi
// Copyright (C) 2026  Gene J.B. (Sterr1)
//
// Much of this driver is based on code from:
//    mt32-pi - A baremetal MIDI synthesizer for Raspberry Pi
//    Copyright (C) 2020-2022 Dale Whinham <daleyo@gmail.com>
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2018-2022  R. Stange <rsta2@o2online.de>
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
#ifndef _display_ssd1306device24_h
#define _display_ssd1306device24_h

#include <circle/device.h>
#include <circle/i2cmaster.h>
#include <circle/spinlock.h>
#include <circle/types.h>
#include <display/chardevice.h>

// Hard-codes how the text maps to the graphics display
#define SSD1306_COLUMNS24	25	// 128/5 = 25.6 (25 characters)
#define SSD1306_ROWS24	4	// 64/8 = 8

class CSSD1306Device24 : public CCharDevice	/// LCD dot-matrix display driver (using SSD1306 controller)
{
public:
	/// \param nWidth Display size in pixels (128 only)
	/// \param nHeight Display size in pixels (32 or 64)
	/// \param pI2CMaster I2C master to be used
	/// \param nAddress I2C slave address of the display controller
	/// \param rotated Display rotated?
	/// \param mirrored Display mirrored?
	CSSD1306Device24 (unsigned nWidth, unsigned nHeight,
			CI2CMaster *pI2CMaster, u8 nAddress,
			bool rotated=false, bool mirrored=false);
	~CSSD1306Device24 (void);

	/// \return Operation successful?
	boolean Initialize24 (void);

private:
	// Virtual methods that must override base class methods (no suffix to maintain override compatibility)
	void DevClearCursor (void) override;
	void DevSetCursor (unsigned nCursorX, unsigned nCursorY) override;
	void DevSetCursorMode (boolean bVisible) override;
	void DevSetChar (unsigned nPosX, unsigned nPosY, char chChar) override;
	void DevUpdateDisplay (void) override;

	// Character functions with 24 suffix
	void Clear24(bool bImmediate = false);
	void Print24(const char* pText, u8 nCursorX, u8 nCursorY, bool bClearLine = false, bool bImmediate = false);

	// Graphics functions with 24 suffix
	void DrawChar24(char chChar, u8 nCursorX, u8 nCursorY, bool bInverted = false, bool bDoubleWidth = false);
	void Flip24();

	void SetBacklightState24(bool bEnabled);

private:
	unsigned m_nWidth;
	unsigned m_nHeight;
	CI2CMaster *m_pI2CMaster;
	u8 m_nAddress;
	bool m_bBacklightEnabled;
    bool m_bRotated;
    bool m_bMirrored;

	// Frame buffer structure with 24 suffix
	struct TFrameBufferUpdatePacket24
	{
		u8 DataControlByte;
		u8 FrameBuffer[128 * 64 / 8];
	}
	PACKED;

	// Helper methods with 24 suffix
	void WriteCommand24(u8 nCommand) const;
	void WriteFrameBuffer24(bool bForceFullUpdate = false) const;
	void SwapFrameBuffers24();

	// Double framebuffers
	TFrameBufferUpdatePacket24 m_FrameBuffers[2];
	u8 m_nCurrentFrameBuffer;
};

#endif