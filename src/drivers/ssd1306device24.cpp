//
// ssd1306device24.cpp
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
#include "ssd1306device24.h"
#include <circle/timer.h>
#include <circle/types.h>
#include <circle/util.h>
#include <assert.h>
#include "font5x8.h"

enum TSSD1306Command24 : u8
{
	SetMemoryAddressingMode24    = 0x20,
	SetColumnAddress24           = 0x21,
	SetPageAddress24             = 0x22,
	SetStartLine24               = 0x40,
	SetContrast24                = 0x81,
	SetChargePump24              = 0x8D,
	EntireDisplayOnResume24      = 0xA4,
	SetNormalDisplay24           = 0xA6,
	SetMultiplexRatio24          = 0xA8,
	SetDisplayOff24              = 0xAE,
	SetDisplayOn24               = 0xAF,
	SetDisplayOffset24           = 0xD3,
	SetDisplayClockDivideRatio24 = 0xD5,
	SetPrechargePeriod24         = 0xD9,
	SetCOMPins24                 = 0xDA,
	SetVCOMHDeselectLevel24      = 0xDB,
};

// Compile-time (constexpr) font/graphics conversion functions.
// The SSD1306 stores pixel data in columns, but our source data is stored as rows.
// These templated functions generate column-wise versions of our graphics at compile-time.
namespace
{
	using CharData = u8[8];

	// Iterate through each row of the character data and collect bits for the nth column
	static constexpr u8 SingleColumn24(const CharData& CharData, u8 nColumn)
	{
		u8 bit = 4 - nColumn;  // 5-1-nColumn for 5-width font
		u8 column = 0;

		for (u8 i = 0; i < 8; ++i)
			column |= (CharData[i] >> bit & 1) << i;

		return column;
	}

	// Double the height of the character by duplicating column bits into a 16-bit value
	static constexpr u16 DoubleColumn24(const CharData& CharData, u8 nColumn)
	{
		u8 singleColumn = SingleColumn24(CharData, nColumn);
		u16 column = 0;

		for (u8 i = 0; i < 8; ++i)
		{
			bool bit = singleColumn >> i & 1;
			column |= bit << i * 2 | bit << (i * 2 + 1);
		}

		return column;
	}

	// Templated array-like structure with precomputed font data
	template<size_t N, class F>
	class Font24
	{
	public:
		// Result type of conversion function should determine array type, but fixed here to u16
		using Column = u16;
		using ColumnData = Column[5];  // 5 columns for 5x8 font

		constexpr Font24(const CharData(&CharData)[N], F Function) : mCharData{ {0} }
		{
			for (size_t i = 0; i < N; ++i)
				for (u8 j = 0; j < 5; ++j)  // 5 columns
					mCharData[i][j] = Function(CharData[i], j);
		}

		const ColumnData& operator[](size_t nIndex) const { return mCharData[nIndex]; }

	private:
		ColumnData mCharData[N];
	};
}

// Single and double-height versions of the font
constexpr auto FontDouble24 = Font24<FONT_SIZE, decltype(DoubleColumn24)>(Font5x8, DoubleColumn24);

CSSD1306Device24::CSSD1306Device24 (unsigned nWidth, unsigned nHeight,
				CI2CMaster *pI2CMaster, u8 nAddress,
			    bool rotated, bool mirrored)
:	CCharDevice (SSD1306_COLUMNS24, SSD1306_ROWS24),
	m_nWidth (nWidth),
	m_nHeight (nHeight),
	m_pI2CMaster (pI2CMaster),
	m_nAddress (nAddress),
	m_bRotated (rotated),
	m_bMirrored (mirrored),
	m_FrameBuffers{{0x40, {0}}, {0x40, {0}}},
	m_nCurrentFrameBuffer(0)
{
}

CSSD1306Device24::~CSSD1306Device24 (void)
{
}

boolean CSSD1306Device24::Initialize24 (void)
{
	assert(m_pI2CMaster != nullptr);

	// Validate dimensions - only 128x32 and 128x64 supported for now
	if (!(m_nHeight == 32 || m_nHeight == 64) || !(m_nWidth == 128 || m_nWidth == 132))
		return false;

	const bool bIsSSD1305 = m_nWidth == 132;
	const u8 nMultiplexRatio24  = m_nHeight - 1;
	const u8 nCOMPins24         = (m_nHeight == 32 && !bIsSSD1305) ? 0x02 : 0x12;
	const u8 nColumnAddrRange24 = m_nWidth - 1;
	const u8 nPageAddrRange24   = m_nHeight / 8 - 1;
	// https://www.buydisplay.com/download/ic/SSD1312_Datasheet.pdf   Pg. 51 Section 2.1.19
	//            normal    inverted
	// normal     A1 C8       A0 C0
	// mirrored   A0 C8       A1 C0
	const u8 nSegRemap24        = (m_bRotated && !m_bMirrored) || (!m_bRotated && m_bMirrored) ? 0xA0 : 0xA1;
	const u8 nCOMScanDir24      = m_bRotated ? 0xC0 : 0xC8;

	const u8 InitSequence24[] =
	{
		SetDisplayOff24,
		SetDisplayClockDivideRatio24,	0x80,				// Default value
		SetMultiplexRatio24,		nMultiplexRatio24,		// Screen height - 1
		SetDisplayOffset24,		0x00,				// None
		SetStartLine24 | 0x00,						// Set start line
		SetChargePump24,			0x14,				// Enable charge pump
		SetMemoryAddressingMode24,	0x00,				// 00 = horizontal
		nSegRemap24,
		nCOMScanDir24,							// COM output scan direction
		SetCOMPins24,			nCOMPins24,			// Alternate COM config and disable COM left/right
		SetContrast24,			0x7F,				// 00-FF, default to half
		SetPrechargePeriod24,		0x22,				// Default value
		SetVCOMHDeselectLevel24,		0x20,				// Default value
		EntireDisplayOnResume24,						// Resume to RAM content display
		SetNormalDisplay24,
		SetDisplayOn24,
		SetColumnAddress24,		0x00,	nColumnAddrRange24,
		SetPageAddress24,			0x00,	nPageAddrRange24,
	};

	for (u8 nCommand24 : InitSequence24)
		WriteCommand24(nCommand24);

	return CCharDevice::Initialize();
}

void CSSD1306Device24::DevClearCursor24 (void)
{
	// Just clear the display
	Clear24(TRUE);
}

void CSSD1306Device24::DevSetCursorMode24 (boolean bVisible)
{
}

void CSSD1306Device24::DevSetChar24 (unsigned nPosX, unsigned nPosY, char chChar)
{
	// Draw a non-inverted, non-double width character
	DrawChar24 (chChar, nPosX, nPosY, FALSE, FALSE);
}

void CSSD1306Device24::DevSetCursor24 (unsigned nCursorX, unsigned nCursorY)
{
}

void CSSD1306Device24::DevUpdateDisplay24 (void) {
	WriteFrameBuffer24(true);
}

void CSSD1306Device24::WriteCommand24(u8 nCommand24) const
{
	const u8 Buffer24[] = { 0x80, nCommand24 };
	m_pI2CMaster->Write(m_nAddress, Buffer24, sizeof(Buffer24));
}

void CSSD1306Device24::WriteFrameBuffer24(bool bForceFullUpdate) const
{
	// Reset start line
	WriteCommand24(SetStartLine24 | 0x00);

	// Compare two framebuffers
	const size_t nFrameBufferSize = m_nWidth * m_nHeight / 8;
	const bool bNeedsUpdate = bForceFullUpdate || memcmp(m_FrameBuffers[0].FrameBuffer, m_FrameBuffers[1].FrameBuffer, nFrameBufferSize) != 0;

	// Copy entire framebuffer
	if (bNeedsUpdate)
		m_pI2CMaster->Write(m_nAddress, &m_FrameBuffers[m_nCurrentFrameBuffer], sizeof(TFrameBufferUpdatePacket24::DataControlByte) + nFrameBufferSize);
}

void CSSD1306Device24::SwapFrameBuffers24()
{
	// Make other framebuffer current
	m_nCurrentFrameBuffer = (m_nCurrentFrameBuffer + 1) % 2;
}

void CSSD1306Device24::DrawChar24(char chChar, u8 nCursorX, u8 nCursorY, bool bInverted, bool bDoubleWidth)
{
	const size_t nRowOffset    = nCursorY * m_nWidth;
	const size_t nColumnOffset = nCursorX * (bDoubleWidth ? 10 : 5);  // 5 or 10 pixels per character
	u8* pFrameBuffer           = m_FrameBuffers[m_nCurrentFrameBuffer].FrameBuffer;

	// Handle character range
	if (chChar < ' ')
		chChar = ' ';

	size_t charIndex = static_cast<u8>(chChar - ' ');
	if (charIndex >= FONT_SIZE)
		charIndex = 0;  // Use first character for out of range

	for (u8 i = 0; i < 5; ++i)  // 5 columns for 5x8 font
	{
		u16 nFontColumn = FontDouble24[charIndex][i];

		// Don't invert the leftmost column or last two rows
		if (i > 0 && bInverted)
			nFontColumn ^= 0x3FFF;

		// Shift down by 2 pixels (as in original)
		nFontColumn <<= 2;

		// Upper half of font
		const size_t nOffset = nRowOffset + nColumnOffset + (bDoubleWidth ? i * 2 : i);

		if (nOffset < sizeof(m_FrameBuffers[0].FrameBuffer))
			pFrameBuffer[nOffset] = nFontColumn & 0xFF;
		if (nOffset + m_nWidth < sizeof(m_FrameBuffers[0].FrameBuffer))
			pFrameBuffer[nOffset + m_nWidth] = (nFontColumn >> 8) & 0xFF;
			
		if (bDoubleWidth)
		{
			if (nOffset + 1 < sizeof(m_FrameBuffers[0].FrameBuffer))
				pFrameBuffer[nOffset + 1] = pFrameBuffer[nOffset];
			if (nOffset + m_nWidth + 1 < sizeof(m_FrameBuffers[0].FrameBuffer))
				pFrameBuffer[nOffset + m_nWidth + 1] = pFrameBuffer[nOffset + m_nWidth];
		}
	}
}

void CSSD1306Device24::Flip24()
{
	WriteFrameBuffer24();
	SwapFrameBuffers24();
}

void CSSD1306Device24::Print24(const char* pText, u8 nCursorX, u8 nCursorY, bool bClearLine, bool bImmediate)
{
	if (bClearLine)
	{
		for (u8 nChar = 0; nChar < nCursorX; ++nChar)
			DrawChar24(' ', nChar, nCursorY);
	}

	u8 currentX = nCursorX;
	while (*pText && currentX < SSD1306_COLUMNS24)
	{
		DrawChar24(*pText++, currentX, nCursorY);
		++currentX;
	}

	if (bClearLine)
	{
		while (currentX < SSD1306_COLUMNS24)
			DrawChar24(' ', currentX++, nCursorY);
	}

	if (bImmediate)
		WriteFrameBuffer24(true);
}

void CSSD1306Device24::Clear24(bool bImmediate)
{
	u8* pFrameBuffer = m_FrameBuffers[m_nCurrentFrameBuffer].FrameBuffer;
	memset(pFrameBuffer, 0, m_nWidth * m_nHeight / 8);

	if (bImmediate)
		WriteFrameBuffer24(true);
}

void CSSD1306Device24::SetBacklightState24(bool bEnabled)
{
	m_bBacklightEnabled = bEnabled;

	// Power on/off display
	WriteCommand24(bEnabled ? SetDisplayOn24 : SetDisplayOff24);
}