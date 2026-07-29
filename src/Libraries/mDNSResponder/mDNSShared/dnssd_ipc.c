/*
 * Copyright (c) 2003-2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1.  Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of its
 *     contributors may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "dnssd_ipc.h"
#if APPLE_OSX_mDNSResponder
#include "mdns_tlv.h"
#endif

#if defined(_WIN32)

char *win32_strerror(int inErrorCode)
{
    static char buffer[1024];
    DWORD n;
    memset(buffer, 0, sizeof(buffer));
    n = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        (DWORD) inErrorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        buffer,
        sizeof(buffer),
        NULL);
    if (n > 0)
    {
        // Remove any trailing CR's or LF's since some messages have them.
        while ((n > 0) && isspace(((unsigned char *) buffer)[n - 1]))
            buffer[--n] = '\0';
    }
    return buffer;
}

#endif

void put_uint32(const uint32_t l, char **ptr)
{
    (*ptr)[0] = (char)((l >> 24) &  0xFF);
    (*ptr)[1] = (char)((l >> 16) &  0xFF);
    (*ptr)[2] = (char)((l >>  8) &  0xFF);
    (*ptr)[3] = (char)((l      ) &  0xFF);
    *ptr += sizeof(uint32_t);
}

uint32_t get_uint32(const char **ptr, const char *end)
{
    if (!*ptr || *ptr + sizeof(uint32_t) > end)
    {
        *ptr = NULL;
        return(0);
    }
    else
    {
        uint8_t *p = (uint8_t*) *ptr;
        *ptr += sizeof(uint32_t);
        return((uint32_t) ((uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]));
    }
}

void put_uint16(uint16_t s, char **ptr)
{
    (*ptr)[0] = (char)((s >>  8) &  0xFF);
    (*ptr)[1] = (char)((s      ) &  0xFF);
    *ptr += sizeof(uint16_t);
}

uint16_t get_uint16(const char **ptr, const char *end)
{
    if (!*ptr || *ptr + sizeof(uint16_t) > end)
    {
        *ptr = NULL;
        return(0);
    }
    else
    {
        uint8_t *p = (uint8_t*) *ptr;
        *ptr += sizeof(uint16_t);
        return((uint16_t) ((uint16_t)p[0] << 8 | p[1]));
    }
}

int put_string(const char *str, char **ptr)
{
    size_t len;
    if (!str) str = "";
    len = strlen(str) + 1;
    memcpy(*ptr, str, len);
    *ptr += len;
    return 0;
}

int get_string(const char **ptr, const char *const end, char *buffer, int buflen)
{
    if (!*ptr)
    {
        *buffer = 0;
        return(-1);
    }
    else
    {
        char *lim = buffer + buflen;    // Calculate limit
        while (*ptr < end && buffer < lim)
        {
            char c = *buffer++ = *(*ptr)++;
            if (c == 0) return(0);      // Success
        }
        if (buffer == lim) buffer--;
        *buffer = 0;                    // Failed, so terminate string,
        *ptr = NULL;                    // clear pointer,
        return(-1);                     // and return failure indication
    }
}

void put_rdata(const int rdlen, const unsigned char *rdata, char **ptr)
{
    memcpy(*ptr, rdata, rdlen);
    *ptr += rdlen;
}

const char *get_rdata(const char **ptr, const char *end, int rdlen)
{
    if (!*ptr || *ptr + rdlen > end)
    {
        *ptr = NULL;
        return(0);
    }
    else
    {
        const char *rd = *ptr;
        *ptr += rdlen;
        return rd;
    }
}

#if APPLE_OSX_mDNSResponder
size_t get_required_tlv16_length(const uint16_t valuelen)
{
    return mdns_tlv16_get_required_length(valuelen);
}

void put_tlv16(const uint16_t type, const uint16_t length, const uint8_t *value, char **ptr)
{
	uint8_t *dst = (uint8_t *)*ptr;
	mdns_tlv16_set(dst, NULL, type, length, value, &dst);
    *ptr = (char *)dst;
}
#endif

void ConvertHeaderBytes(ipc_msg_hdr *hdr)
{
    hdr->version   = htonl(hdr->version);
    hdr->datalen   = htonl(hdr->datalen);
    hdr->ipc_flags = htonl(hdr->ipc_flags);
    hdr->op        = htonl(hdr->op );
    hdr->reg_index = htonl(hdr->reg_index);
}
