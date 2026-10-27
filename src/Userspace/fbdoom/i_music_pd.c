//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	Music for PureDarwin: a real MUS-format parser (the lump format
//	DOOM's music actually ships in) driving a simple square-wave
//	synthesizer per MIDI channel, mixed into the same AudioQueue stream
//	i_sound_pd.c's sfx mixer feeds. This is not trying to emulate the
//	OPL2/AdLib timbre real DOOM used (that's DBOPL-scale work - see
//	fbDOOM's own unused i_oplmusic.c for the real thing); it plays the
//	actual notes/timing/volume from the song at the right pitches, just
//	with a plain tone instead of an FM instrument patch.
//
//	Called from i_sound_pd.c's I_UpdateSound() once per output sample so
//	MUS event timing (in its own native tic rate, independent of DOOM's
//	35Hz game tic) stays sample-accurate.
//
//-----------------------------------------------------------------------------

#include "doomtype.h"
#include "i_sound.h"
#include <string.h>

#define MUS_MAGIC "MUS\x1a"
#define MUS_NUM_CHANNELS 16
#define MUS_PERCUSSION_CHANNEL 15
#define MUS_TICRATE 140 /* DMX's native MUS clock */
#define OUT_RATE 48000

struct mus_header {
    char id[4];
    unsigned short score_len;
    unsigned short score_start;
    unsigned short channels;
    unsigned short sec_channels;
    unsigned short num_instruments;
};

enum {
    MUS_EV_RELEASE = 0,
    MUS_EV_PLAY = 1,
    MUS_EV_PITCHBEND = 2,
    MUS_EV_SYSTEM = 3,
    MUS_EV_CONTROLLER = 4,
    MUS_EV_UNUSED = 5,
    MUS_EV_SCOREEND = 6,
};

/* MUS (like the MIDI it derives from) is polyphonic PER CHANNEL: E1M1
 * keeps the low E2 drone ringing on channel 1 while the melody notes play
 * over it on the same channel, and each RELEASE names the specific note it
 * ends. A single note slot per channel cannot represent that - the drone's
 * trailing RELEASE was cutting off whichever melody note had since started
 * (truncating every melody note to a ~20ms blip, heard as "beep boop dots"
 * instead of sustained music). So notes live in a shared voice pool, and
 * releases only end the voice matching their (channel, note) pair. */
#define MUS_MAX_VOICES 24

struct mus_voice {
    int channel; /* owning MUS channel, -1 = free */
    int note;
    int volume;  /* 0-127, snapshotted at note-on */
    boolean held;   /* note-on still active (not yet released) */
    boolean noise;  /* percussion: LFSR noise burst instead of a pitched wave */
    unsigned phase; /* wave phase; full 32-bit range = one cycle */
    unsigned step;  /* phase increment per output sample (freq/OUT_RATE * 2^32) */
    int cur_amp; /* smoothed amplitude, eases toward the note's target amp */
    unsigned decay_q16; /* pluck envelope, Q16: 65536 at note-on, decays
                         * toward a sustain floor (0 for percussion) */
};

struct mus_channel {
    int volume; /* last note volume used on this channel (MUS: PLAY events
                 * without a volume byte reuse the previous one) */
    int bend;   /* MUS pitch wheel, 0-255, 128 = center, +/-2 semitones */
};

/* Max amplitude change per output sample for cur_amp; caps ramp time to
 * ~2.7ms at 48kHz (3000/128*48000000 samples), short enough to be
 * inaudible as a ramp but long enough to kill the on/off click. */
#define MUS_AMP_RAMP_STEP 128

static const unsigned char *mus_data;
static unsigned mus_len;
static unsigned mus_pos;     /* byte offset into mus_data, from score_start */
static unsigned mus_start;
static boolean mus_playing;
static boolean mus_looping;
static int mus_volume = 127; /* 0-127, set by I_SetMusicVolume */

static struct mus_channel mus_channels[MUS_NUM_CHANNELS];
static struct mus_voice mus_voices[MUS_MAX_VOICES];

static unsigned mus_sample_countdown; /* output samples until next event batch */
static unsigned mus_tic_accum_milli;  /* fractional output-samples-per-mus-tic carry */

/* MIDI note number -> phase step. The mixer's phase accumulator wraps at
 * 2^32 = one full wave cycle, so step = freq/OUT_RATE * 2^32.
 *
 * Computed from an exact integer table (equal-tempered octave 4 in mHz,
 * A4=440Hz at MIDI note 69) shifted by octave, deliberately NOT via
 * pow(2.0, (note-69)/12.0): PureDarwin's hand-rolled libm pow() is
 * inaccurate for fractional exponents, which detuned every melody note
 * audibly (low bass octaves, nearer integer exponents, survived). */
static const unsigned note_mhz_octave4[12] = {
    261626, /* C4  */
    277183, /* C#4 */
    293665, /* D4  */
    311127, /* D#4 */
    329628, /* E4  */
    349228, /* F4  */
    369994, /* F#4 */
    391995, /* G4  */
    415305, /* G#4 */
    440000, /* A4  */
    466164, /* A#4 */
    493883, /* B4  */
};

static unsigned
note_step(int note)
{
    if (note < 0)
        note = 0;
    if (note > 127)
        note = 127;

    /* MIDI 60 = C4; octave 5 in note/12 terms. */
    int oct = note / 12;
    unsigned long long freq_mhz = note_mhz_octave4[note % 12];
    if (oct >= 5)
        freq_mhz <<= (oct - 5);
    else
        freq_mhz >>= (5 - oct);

    unsigned long long step = (freq_mhz << 32) / ((unsigned long long)OUT_RATE * 1000ull);
    if (step > 4294967295ull)
        step = 4294967295ull;
    return (unsigned)step;
}

/* Phase step for a note under a MUS pitch wheel value (0-255, 128 = center,
 * full range +/-2 semitones - same mapping mus2mid uses for the MIDI wheel).
 * Interpolates linearly between the two neighboring semitone steps; within
 * one semitone that is accurate to <0.2%, far below what's audible in a
 * bend. E1M1's lead guitar slides constantly - discarding bends is a big
 * part of why the melody sounded like a flat childlike MIDI recreation. */
static unsigned
note_step_bend(int note, int bend)
{
    /* semitone offset in Q8: (bend-128)/64 semitones -> *4 for Q8 */
    int semis_q8 = (bend - 128) * 4;
    int whole = semis_q8 >> 8;          /* floor toward -inf */
    unsigned frac = (unsigned)(semis_q8 - (whole << 8)); /* 0-255 */

    unsigned lo = note_step(note + whole);
    unsigned hi = note_step(note + whole + 1);
    return lo + (unsigned)(((unsigned long long)(hi - lo) * frac) >> 8);
}

static unsigned char
mus_read_byte(void)
{
    if (mus_pos >= mus_len)
        return 0xff; /* treat as score-end-ish if we run off the end */
    return mus_data[mus_pos++];
}

/* MUS variable-length time delta: 7 bits per byte, MSB=continue. */
static unsigned
mus_read_vlq(void)
{
    unsigned val = 0;
    unsigned char b;
    do {
        b = mus_read_byte();
        val = (val << 7) | (b & 0x7f);
    } while (b & 0x80);
    return val;
}

static void
mus_stop_all_notes(void)
{
    for (int i = 0; i < MUS_NUM_CHANNELS; i++) {
        mus_channels[i].volume = 0;
        mus_channels[i].bend = 128;
    }
    for (int i = 0; i < MUS_MAX_VOICES; i++)
        mus_voices[i].held = false; /* fade out via the amp ramp */
}

static void
mus_note_on(int channel, int note, int vol, boolean noise)
{
    struct mus_voice *v = NULL;

    /* Retrigger if this exact note is already sounding on this channel,
     * else grab a free voice, else steal the quietest one. */
    for (int i = 0; i < MUS_MAX_VOICES; i++) {
        if (mus_voices[i].channel == channel && mus_voices[i].note == note) {
            v = &mus_voices[i];
            break;
        }
    }
    if (v == NULL) {
        for (int i = 0; i < MUS_MAX_VOICES; i++) {
            if (mus_voices[i].channel < 0) {
                v = &mus_voices[i];
                break;
            }
        }
    }
    if (v == NULL) {
        v = &mus_voices[0];
        for (int i = 1; i < MUS_MAX_VOICES; i++)
            if (mus_voices[i].cur_amp < v->cur_amp)
                v = &mus_voices[i];
    }

    if (v->channel != channel || v->note != note)
        v->phase = 0;
    v->channel = channel;
    v->note = note;
    v->volume = vol;
    v->held = true;
    v->noise = noise;
    v->decay_q16 = 65536; /* fresh pluck: restart the decay envelope */
    v->step = note_step_bend(note, mus_channels[channel].bend);
}

static void
mus_note_off(int channel, int note)
{
    for (int i = 0; i < MUS_MAX_VOICES; i++) {
        if (mus_voices[i].channel == channel && mus_voices[i].note == note)
            mus_voices[i].held = false;
    }
}

static void
mus_restart(void)
{
    mus_pos = mus_start;
    mus_sample_countdown = 0;
    mus_tic_accum_milli = 0;
    mus_stop_all_notes();
}

/* Process every event at the current score position until we hit one
 * followed by a time delta, then schedule the next batch. */
static void
mus_process_events(void)
{
    for (;;) {
        if (mus_pos >= mus_len) {
            mus_playing = false;
            return;
        }

        unsigned char evbyte = mus_read_byte();
        int channel = evbyte & 0x0f;
        int evtype = (evbyte >> 4) & 0x07;
        boolean has_delay = (evbyte & 0x80) != 0;

        switch (evtype) {
        case MUS_EV_RELEASE: {
            unsigned char note = mus_read_byte() & 0x7f;
            if (channel < MUS_NUM_CHANNELS)
                mus_note_off(channel, note);
            break;
        }
        case MUS_EV_PLAY: {
            unsigned char b = mus_read_byte();
            int note = b & 0x7f;
            boolean has_vol = (b & 0x80) != 0;
            int vol = channel < MUS_NUM_CHANNELS ? mus_channels[channel].volume : 100;
            if (has_vol)
                vol = mus_read_byte() & 0x7f;
            if (channel < MUS_NUM_CHANNELS) {
                mus_channels[channel].volume = vol;
                /* Percussion channel: a pitchless noise burst (kick/snare/
                 * hat all get the same treatment - crude, but the drums
                 * drive the whole track and total silence there is far
                 * worse than an untyped hit). */
                mus_note_on(channel, note, vol,
                            channel == MUS_PERCUSSION_CHANNEL);
            }
            break;
        }
        case MUS_EV_PITCHBEND: {
            int bend = mus_read_byte();
            if (channel < MUS_NUM_CHANNELS) {
                mus_channels[channel].bend = bend;
                /* Retune this channel's live voices - bends happen DURING
                 * notes (that's their whole point). */
                for (int i = 0; i < MUS_MAX_VOICES; i++) {
                    if (mus_voices[i].channel == channel && !mus_voices[i].noise)
                        mus_voices[i].step = note_step_bend(mus_voices[i].note, bend);
                }
            }
            break;
        }
        case MUS_EV_SYSTEM:
            mus_read_byte();
            break;
        case MUS_EV_CONTROLLER: {
            unsigned char ctrl = mus_read_byte();
            unsigned char val = mus_read_byte();
            if (ctrl == 3 && channel < MUS_NUM_CHANNELS) /* volume controller */
                mus_channels[channel].volume = val & 0x7f;
            break;
        }
        case MUS_EV_SCOREEND:
            if (mus_looping) {
                mus_restart();
            } else {
                mus_playing = false;
            }
            return;
        default:
            break;
        }

        if (has_delay) {
            unsigned delay_tics = mus_read_vlq();
            /* Convert MUS tics to output samples, Q1000 fixed-point carry
             * so 48000/140 (not integer) doesn't drift. */
            unsigned long long milli = (unsigned long long)delay_tics * OUT_RATE * 1000ull;
            mus_tic_accum_milli += (unsigned)(milli / MUS_TICRATE);
            mus_sample_countdown = mus_tic_accum_milli / 1000u;
            mus_tic_accum_milli -= mus_sample_countdown * 1000u;
            return;
        }
        /* No delay: more events belong to this same instant, keep going. */
    }
}

/* Called once per output sample from i_sound_pd.c's mixer. Returns a signed
 * sample (already volume-scaled, summed across channels) to add into the
 * output mix. */
int
I_PD_MixMusicSample(void)
{
    if (!mus_playing)
        return 0;

    if (mus_sample_countdown == 0)
        mus_process_events();

    if (mus_sample_countdown > 0)
        mus_sample_countdown--;

    static unsigned lfsr = 0xACE1u; /* percussion noise source */

    int mix = 0;
    for (int i = 0; i < MUS_MAX_VOICES; i++) {
        struct mus_voice *v = &mus_voices[i];
        if (v->channel < 0)
            continue;

        boolean active = v->held && v->volume > 0;

        /* Pluck envelope: a struck/plucked instrument decays after the
         * attack instead of holding organ-flat forever . Pitched notes decay
         * toward a ~35% sustain floor (time constant ~0.34s); percussion
         * decays all the way to zero fast (~85ms burst). */
        if (v->noise) {
            v->decay_q16 -= v->decay_q16 >> 11; /* ~43ms time constant */
            if (v->decay_q16 <= 1024) {
                v->decay_q16 = 0;
                v->held = false;
            }
        } else if (v->decay_q16 > 23000) { /* ~35% sustain floor */
            unsigned diff = v->decay_q16 - 23000;
            unsigned dec = diff >> 14; /* ~0.34s time constant */
            v->decay_q16 -= dec ? dec : 1;
        }

        /* Higher notes slightly quieter (real instrument spectra tilt
         * down with pitch; equal-amplitude trebles pierce). Percussion
         * sits well below the pitched voices - raw noise reads much
         * louder to the ear than a tone at the same peak. */
        int tilt;
        if (v->noise) {
            tilt = 1400;
        } else {
            tilt = 3600 - v->note * 12;
            if (tilt < 1500)
                tilt = 1500;
        }

        /* Target peak amplitude, scaled to a moderate per-voice ceiling
         * (not the full int16 range) since several voices can be active
         * at once and this gets summed with sfx afterward. */
        int target_amp = active
            ? (int)(((long long)v->volume * mus_volume * tilt * v->decay_q16) / (127LL * 127 * 65536))
            : 0;

        /* Ease cur_amp toward target_amp instead of snapping instantly:
         * gating a wave on/off (or changing its volume) at full amplitude
         * in a single sample is an audible click - every note on/off
         * popped like a row of "dots" instead of a smooth tone. */
        if (v->cur_amp < target_amp) {
            v->cur_amp += MUS_AMP_RAMP_STEP;
            if (v->cur_amp > target_amp)
                v->cur_amp = target_amp;
        } else if (v->cur_amp > target_amp) {
            v->cur_amp -= MUS_AMP_RAMP_STEP;
            if (v->cur_amp < target_amp)
                v->cur_amp = target_amp;
        }

        if (v->cur_amp == 0 && !active) {
            v->channel = -1; /* fully faded out: free the voice */
            continue;
        }

        if (v->noise) {
            /* 16-bit Galois LFSR white noise through a one-pole lowpass
             * (~2kHz): raw white noise is all treble hiss; filtered it
             * lands closer to a drum thump. */
            static int noise_lp;
            lfsr = (lfsr >> 1) ^ (unsigned)(-(int)(lfsr & 1u) & 0xB400u);
            int raw = (int)(lfsr & 0xffffu) - 32768;
            noise_lp += (raw - noise_lp) >> 2;
            mix += (int)(((long long)noise_lp * v->cur_amp) >> 15);
            continue;
        }

        /* Triangle wave instead of a hard square: a square wave flips
         * polarity instantaneously twice per cycle, which for short fast
         * notes reads to the ear as a click rather than a pitch. A
         * triangle ramps continuously with no discontinuity, so even a
         * single short cycle still sounds like a tone. */
        unsigned p = v->phase;
        int tri;
        if (p < 0x80000000u)
            tri = (int)(((long long)p * 2 * v->cur_amp) >> 31) - v->cur_amp;
        else {
            unsigned q = p - 0x80000000u;
            tri = v->cur_amp - (int)(((long long)q * 2 * v->cur_amp) >> 31);
        }
        mix += tri;
        v->phase += v->step;
    }
    return mix;
}

void I_PD_InitMusicPlayer(void)
{
    for (int i = 0; i < MUS_MAX_VOICES; i++) {
        mus_voices[i].channel = -1;
        mus_voices[i].cur_amp = 0;
        mus_voices[i].held = false;
    }
    mus_stop_all_notes();
}

void I_InitMusic(void)
{
    I_PD_InitMusicPlayer();
}

void I_ShutdownMusic(void)
{
    mus_playing = false;
}

void I_SetMusicVolume(int volume)
{
    mus_volume = volume;
}

void I_PauseSong(void)
{
    mus_playing = false;
}

void I_ResumeSong(void)
{
    if (mus_data)
        mus_playing = true;
}

void *I_RegisterSong(void *data, int len)
{
    const struct mus_header *hdr = (const struct mus_header *)data;

    if (len < (int)sizeof(struct mus_header) || memcmp(hdr->id, MUS_MAGIC, 4) != 0)
        return NULL;

    mus_data = (const unsigned char *)data;
    mus_len = (unsigned)len;
    mus_start = hdr->score_start;
    mus_playing = false;
    return (void *)data;
}

void I_UnRegisterSong(void *handle)
{
    if ((const void *)handle == (const void *)mus_data) {
        mus_playing = false;
        mus_data = NULL;
    }
}

void I_PlaySong(void *handle, boolean looping)
{
    if ((const void *)handle != (const void *)mus_data)
        return;
    mus_looping = looping;
    mus_restart();
    mus_playing = true;
}

void I_StopSong(void)
{
    mus_playing = false;
    mus_stop_all_notes();
}

boolean I_MusicIsPlaying(void)
{
    return mus_playing;
}
