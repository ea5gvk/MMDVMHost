/*
 *   Copyright (C) 2020 by Jonathan Naylor G4KLX
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "FMControl.h"

#include <string>

const uint8_t BIT_MASK_TABLE[] = { 0x80U, 0x40U, 0x20U, 0x10U, 0x08U, 0x04U, 0x02U, 0x01U };

#define WRITE_BIT(p,i,b) p[(i)>>3] = (b) ? (p[(i)>>3] | BIT_MASK_TABLE[(i)&7]) : (p[(i)>>3] & ~BIT_MASK_TABLE[(i)&7])
#define READ_BIT(p,i)    (p[(i)>>3] & BIT_MASK_TABLE[(i)&7])

CFMControl::CFMControl(CFMNetwork* network) :
m_network(network),
m_enabled(false)
{
    assert(network != NULL);
}

CFMControl::~CFMControl()
{
}

bool CFMControl::writeModem(const unsigned char* data, unsigned int length)
{
    assert(data != NULL);
    assert(length > 0U);
    assert(m_network != NULL);

    float samples[170U];
    unsigned int nSamples = 0U;

    // Unpack the serial data into float values.
    for (unsigned int i = 0U; i < length; i += 3U) {
        unsigned short sample1 = 0U;
        unsigned short sample2 = 0U;
        unsigned short MASK = 0x0001U;

        const unsigned char* base = data + i;
        for (unsigned int j = 0U; j < 12U; j++, MASK <<= 1) {
            unsigned int pos1 = j;
            unsigned int pos2 = j + 12U;

            bool b1 = READ_BIT(base, pos1) != 0U;
            bool b2 = READ_BIT(base, pos2) != 0U;

            if (b1)
                sample1 |= MASK;
            if (b2)
                sample2 |= MASK;
        }

        // Convert from unsigned short (0 - +4095) to float (-1.0 - +1.0)
        samples[nSamples++] = (float(sample1) - 2048.0F) / 2048.0F;
        samples[nSamples++] = (float(sample2) - 2048.0F) / 2048.0F;
    }

    // De-emphasise the data and any other processing needed (maybe a low-pass filter to remove the CTCSS)

    unsigned char out[350U];
    unsigned int nOut = 0U;

    // Repack the data (8-bit unsigned values containing unsigned 16-bit data)
    for (unsigned int i = 0U; i < nSamples; i++) {
        unsigned short sample = (unsigned short)((samples[i] + 1.0F) * 32767.0F + 0.5F);
        out[nOut++] = (sample >> 8) & 0xFFU;
        out[nOut++] = (sample >> 0) & 0xFFU;
    }

    return m_network->write(out, nOut);
}

unsigned int CFMControl::readModem(unsigned char* data, unsigned int space)
{
    assert(data != NULL);
    assert(space > 0U);
    assert(m_network != NULL);

    unsigned char netData[300U];
    unsigned int length = m_network->read(netData, 270U);
    if (length == 0U)
        return 0U;

    float samples[170U];
    unsigned int nSamples = 0U;

    // Convert the unsigned 16-bit data (+65535 - 0) to float (+1.0 - -1.0)
    for (unsigned int i = 0U; i < length; i += 2U) {
        unsigned short sample = (netData[i + 0U] << 8) | netData[i + 1U];
        samples[nSamples++] = (float(sample) / 32767.0F) - 1.0F;
    }

    // Pre-emphasise the data and other stuff.

    // Pack the floating point data (+1.0 to -1.0) to packed 12-bit samples (+2047 - -2048)
    unsigned int offset = 0U;
    for (unsigned int i = 0U; i < nSamples; i++) {
        unsigned short sample = (unsigned short)((samples[i] + 1.0F) * 2048.0F + 0.5F);
        unsigned short MASK = 0x0001U;
        for (unsigned int j = 0U; j < 12U; j++, MASK <<= 1) {
            bool b = (sample & MASK) != 0U;
            WRITE_BIT(data, offset, b);
            offset++;
        }
    }

    return nSamples;
}

void CFMControl::clock(unsigned int ms)
{
    // May not be needed
}

void CFMControl::enable(bool enabled)
{
    // May not be needed
}