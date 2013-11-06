/*
 * H.26L/H.264/AVC/JVT/14496-10/... sei decoding
 * Copyright (c) 2003 Michael Niedermayer <michaelni@gmx.at>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * H.264 / AVC / MPEG4 part10 sei decoding.
 * @author Michael Niedermayer <michaelni@gmx.at>
 */

#include "internal.h"
#include "avcodec.h"
#include "h264.h"
#include "golomb.h"

//#undef NDEBUG
#include <assert.h>

static const uint8_t sei_num_clock_ts_table[9]={
    1,  1,  1,  2,  2,  3,  3,  2,  3
};

void ff_h264_reset_sei(H264Context *h) {
    h->sei_recovery_frame_cnt       = -1;
    h->sei_dpb_output_delay         =  0;
    h->sei_cpb_removal_delay        = -1;
    h->sei_buffering_period_present =  0;
}

static int decode_picture_timing(H264Context *h){
    if(h->sps.nal_hrd_parameters_present_flag || h->sps.vcl_hrd_parameters_present_flag){
        h->sei_cpb_removal_delay = get_bits_long(&h->gb, h->sps.cpb_removal_delay_length);
        h->sei_dpb_output_delay = get_bits_long(&h->gb, h->sps.dpb_output_delay_length);
    }
    if(h->sps.pic_struct_present_flag){
        unsigned int i, num_clock_ts;
        h->sei_pic_struct = get_bits(&h->gb, 4);
        h->sei_ct_type    = 0;

        if (h->sei_pic_struct > SEI_PIC_STRUCT_FRAME_TRIPLING)
            return -1;

        num_clock_ts = sei_num_clock_ts_table[h->sei_pic_struct];

        for (i = 0 ; i < num_clock_ts ; i++){
            if(get_bits(&h->gb, 1)){                  /* clock_timestamp_flag */
                unsigned int full_timestamp_flag;
                h->sei_ct_type |= 1<<get_bits(&h->gb, 2);
                skip_bits(&h->gb, 1);                 /* nuit_field_based_flag */
                skip_bits(&h->gb, 5);                 /* counting_type */
                full_timestamp_flag = get_bits(&h->gb, 1);
                skip_bits(&h->gb, 1);                 /* discontinuity_flag */
                skip_bits(&h->gb, 1);                 /* cnt_dropped_flag */
                skip_bits(&h->gb, 8);                 /* n_frames */
                if(full_timestamp_flag){
                    skip_bits(&h->gb, 6);             /* seconds_value 0..59 */
                    skip_bits(&h->gb, 6);             /* minutes_value 0..59 */
                    skip_bits(&h->gb, 5);             /* hours_value 0..23 */
                }else{
                    if(get_bits(&h->gb, 1)){          /* seconds_flag */
                        skip_bits(&h->gb, 6);         /* seconds_value range 0..59 */
                        if(get_bits(&h->gb, 1)){      /* minutes_flag */
                            skip_bits(&h->gb, 6);     /* minutes_value 0..59 */
                            if(get_bits(&h->gb, 1))   /* hours_flag */
                                skip_bits(&h->gb, 5); /* hours_value 0..23 */
                        }
                    }
                }
                if(h->sps.time_offset_length > 0)
                    skip_bits(&h->gb, h->sps.time_offset_length); /* time_offset */
            }
        }

        if(h->avctx->debug & FF_DEBUG_PICT_INFO)
            av_log(h->avctx, AV_LOG_DEBUG, "ct_type:%X pic_struct:%d\n", h->sei_ct_type, h->sei_pic_struct);
    }
    return 0;
}

static int decode_user_data_itu_t_t35(H264Context *s, int buf_size) {

    const uint8_t *p = NULL, *buf_end = NULL;

    //Jump index bytes to start of user data
    //Starts with 0xB5 and 0x0031
    p = s->gb.buffer + (s->gb.index >> 3);

    /*Ignore first 3 bytes
    p[0] = 0xB5 ITU-T Country Code
    p[1-2] = 0x0031 ITU-T Provider code */
    p += 3; //ignore 0xB50031
    buf_end = p + buf_size-3;

    /*we parse the DTG active format information */
    if (buf_end - p >= 5 &&
            p[0] == 'D' && p[1] == 'T' && p[2] == 'G' && p[3] == '1') {
        int flags = p[4];
        p += 5;
        if (flags & 0x80) { //skip event id
            p += 2;
        }
        if (flags & 0x40) {
            if (buf_end - p < 1)
                return;
            s->avctx->dtg_active_format = p[0] & 0x0f;
        }
    } else if (buf_end - p >= 6 &&
            p[0] == 0x43 && p[1] == 0x43 && p[2] == 0x01 && p[3] == 0xf8 &&
            p[4] == 0x9e) {
#undef fprintf
        int atsc_cnt_loc = s->tmp_atsc_cc_len;
        uint8_t real_count = 0;
        unsigned int i;

        s->tmp_atsc_cc_buf[s->tmp_atsc_cc_len++] = 0x40 | (0x1f&real_count);
        s->tmp_atsc_cc_buf[s->tmp_atsc_cc_len++] = 0x00; // em_data

        for (i=5; i < (buf_end - p - 2) &&
        (s->tmp_atsc_cc_len + 3) < ATSC_CC_BUF_SIZE; i++)
        {
            if ((p[i]&0xfe) == 0xfe) // CC1&CC2 || CC3&CC4
            {
                uint8_t type = (p[i] & 0x01) ^ 0x01;
                uint8_t cc_data_1 = p[++i];
                uint8_t cc_data_2 = p[++i];
                uint8_t valid = 1;
                uint8_t cc608_hdr = 0xf8 | (valid ? 0x04 : 0x00) | type;
                real_count++;
                s->tmp_atsc_cc_buf[s->tmp_atsc_cc_len++] = cc608_hdr;
                s->tmp_atsc_cc_buf[s->tmp_atsc_cc_len++] = cc_data_1;
                s->tmp_atsc_cc_buf[s->tmp_atsc_cc_len++] = cc_data_2;
                continue;
            }
            break;
        }
        if (!real_count)
        {
            s->tmp_atsc_cc_len = atsc_cnt_loc;
        }
        else
        {
            s->tmp_atsc_cc_buf[atsc_cnt_loc] = 0x40 | (0x1f&real_count);
            s->tmp_atsc_cc_len = atsc_cnt_loc + 2 + 3 * real_count;
        }
    } else if (buf_end - p >= 6 &&
            p[0] == 'G' && p[1] == 'A' && p[2] == '9' && p[3] == '4') {
        /* Parse CEA-708/608 Closed Captions in ATSC user data */
        int user_data_type_code = p[4];
        if (user_data_type_code == 0x03) { // caption data
            int cccnt = p[5] & 0x1f;
            int cclen = 3 * cccnt + 2;
            int proc = (p[5] >> 6) & 1;
            int blen = s->tmp_atsc_cc_len;

            p += 5;

            if ((cclen <= buf_end - p) && ((cclen + blen) < ATSC_CC_BUF_SIZE)) {
                uint8_t *dst = s->tmp_atsc_cc_buf + s->tmp_atsc_cc_len;
                memcpy(dst, p, cclen);
                s->tmp_atsc_cc_len += cclen;
            }
        }
        else if (user_data_type_code == 0x04) {
            // additional CEA-608 data, as per SCTE 21
        }
        else if (user_data_type_code == 0x05) {
            // luma PAM data, as per SCTE 21
        }
        else if (user_data_type_code == 0x06) {
            // bar data (letterboxing info)
        }
    } else if (buf_end - p >= 3 && p[0] == 0x03 && ((p[1]&0x7f) == 0x01)) {
        // SCTE 20 encoding of CEA-608
        unsigned int cc_count = p[2]>>3;
        unsigned int cc_bits = cc_count * 26;
        unsigned int cc_bytes = (cc_bits + 7 - 3) / 8;
        if (buf_end - p >= (2+cc_bytes) && (s->tmp_scte_cc_len + 2 + 3*cc_count) < SCTE_CC_BUF_SIZE) {
            int scte_cnt_loc = s->tmp_scte_cc_len;
            uint8_t real_count = 0, marker = 1, i;
            GetBitContext gb;
            init_get_bits(&gb, p+2, (buf_end-p-2) * sizeof(uint8_t));
            get_bits(&gb, 5); // swallow cc_count
            s->tmp_scte_cc_buf[s->tmp_scte_cc_len++] = 0x40 | (0x1f&cc_count);
            s->tmp_scte_cc_buf[s->tmp_scte_cc_len++] = 0x00; // em_data
            for (i = 0; i < cc_count; i++) {
                uint8_t valid, cc608_hdr;
                uint8_t priority = get_bits(&gb, 2);
                uint8_t field_no = get_bits(&gb, 2);
                uint8_t line_offset = get_bits(&gb, 5);
                uint8_t cc_data_1 = av_reverse[get_bits(&gb, 8)];
                uint8_t cc_data_2 = av_reverse[get_bits(&gb, 8)];
                uint8_t type = (1 == field_no) ? 0x00 : 0x01;
                (void) priority; // we use all the data, don't need priority
                marker &= get_bits(&gb, 1);
                // dump if marker bit missing
                valid = marker;
                // ignore forbidden and repeated (3:2 pulldown) field numbers
                valid = valid && (1 == field_no || 2 == field_no);
                // ignore content not in line 21
                valid = valid && (11 == line_offset);
                if (!valid)
                    continue;
                cc608_hdr = 0xf8 | (valid ? 0x04 : 0x00) | type;
                real_count++;
                s->tmp_scte_cc_buf[s->tmp_scte_cc_len++] = cc608_hdr;
                s->tmp_scte_cc_buf[s->tmp_scte_cc_len++] = cc_data_1;
                s->tmp_scte_cc_buf[s->tmp_scte_cc_len++] = cc_data_2;
            }
            if (!real_count)
            {
                s->tmp_scte_cc_len = scte_cnt_loc;
            }
            else
            {
                s->tmp_scte_cc_buf[scte_cnt_loc] = 0x40 | (0x1f&real_count);
                s->tmp_scte_cc_len = scte_cnt_loc + 2 + 3 * real_count;
            }
        }
    } else if (buf_end - p >= 11 &&
            p[0] == 0x05 && p[1] == 0x02) {
        /* parse EIA-608 captions embedded in a DVB stream. */
        uint8_t dvb_cc_type = p[7];
        p += 8;

        /* Predictive frame tag, but MythTV reorders predictive
         * frames for us along with the CC data, so we ignore it.
         */
        if (dvb_cc_type == 0x05) {
            dvb_cc_type = p[6];
            p += 7;
        }

        if (dvb_cc_type == 0x02) { /* 2-byte caption, can be repeated */
            int type = 0x00; // line 21 field 1 == 0x00, field 2 == 0x01
            uint8_t cc608_hdr = 0xf8 | 0x04/*valid*/ | type;
            uint8_t hi = p[1] & 0xFF;
            uint8_t lo = p[2] & 0xFF;

            dvb_cc_type = p[3];

            if ((2 <= buf_end - p) && ((3 + s->tmp_atsc_cc_len) < ATSC_CC_BUF_SIZE)) {
                s->tmp_atsc_cc_buf[s->tmp_atsc_cc_len++] = 0x40 | (0x1f&1/*cc_count*/);
                s->tmp_atsc_cc_buf[s->tmp_atsc_cc_len++] = 0x00; // em_data
                s->tmp_atsc_cc_buf[s->tmp_atsc_cc_len++] = cc608_hdr;
                s->tmp_atsc_cc_buf[s->tmp_atsc_cc_len++] = hi;
                s->tmp_atsc_cc_buf[s->tmp_atsc_cc_len++] = lo;

                /* Only repeat characters when the next type flag
                 * is 0x04 and the characters are repeatable (i.e., less than
                 * 32 with the parity stripped).
                 */
                if (dvb_cc_type == 0x04 && (hi & 0x7f) < 32) {
                    if ((2 <= buf_end - p) && ((3 + s->tmp_atsc_cc_len) < ATSC_CC_BUF_SIZE)) {
                        s->tmp_atsc_cc_buf[s->tmp_atsc_cc_len++] = 0x40 | (0x1f&1/*cc_count*/);
                        s->tmp_atsc_cc_buf[s->tmp_atsc_cc_len++] = 0x00; // em_data
                        s->tmp_atsc_cc_buf[s->tmp_atsc_cc_len++] = cc608_hdr;
                        s->tmp_atsc_cc_buf[s->tmp_atsc_cc_len++] = hi;
                        s->tmp_atsc_cc_buf[s->tmp_atsc_cc_len++] = lo;
                    }
                }
            }

            p += 6;
        } else if (dvb_cc_type == 0x04) { /* 4-byte caption, not repeated */
            if ((4 <= buf_end - p) &&
                    ((6 + s->tmp_atsc_cc_len) < ATSC_CC_BUF_SIZE)) {
                int type = 0x00; // line 21 field 1 == 0x00, field 2 == 0x01
                uint8_t cc608_hdr = 0xf8 | 0x04/*valid*/ | type;

                s->tmp_atsc_cc_buf[s->tmp_atsc_cc_len++] = 0x40 | (0x1f&2/*cc_count*/);
                s->tmp_atsc_cc_buf[s->tmp_atsc_cc_len++] = 0x00; // em_data
                s->tmp_atsc_cc_buf[s->tmp_atsc_cc_len++] = cc608_hdr;
                s->tmp_atsc_cc_buf[s->tmp_atsc_cc_len++] = p[1] & 0xFF;
                s->tmp_atsc_cc_buf[s->tmp_atsc_cc_len++] = p[2] & 0xFF;
                s->tmp_atsc_cc_buf[s->tmp_atsc_cc_len++] = cc608_hdr;
                s->tmp_atsc_cc_buf[s->tmp_atsc_cc_len++] = p[3] & 0xFF;
                s->tmp_atsc_cc_buf[s->tmp_atsc_cc_len++] = p[4] & 0xFF;
            }

            p += 9;
        }
    }
    // For other CEA-608 embedding options see:
    /* SCTE 21 */
    /* ETSI EN 301 775 */
    return 0;
}

static int decode_unregistered_user_data(H264Context *h, int size){
    uint8_t user_data[16+256];
    int e, build, i;

    if(size<16)
        return -1;

    for(i=0; i<sizeof(user_data)-1 && i<size; i++){
        user_data[i]= get_bits(&h->gb, 8);
    }

    user_data[i]= 0;
    e= sscanf(user_data+16, "x264 - core %d"/*%s - H.264/MPEG-4 AVC codec - Copyleft 2005 - http://www.videolan.org/x264.html*/, &build);
    if(e==1 && build>0)
        h->x264_build= build;
    if(e==1 && build==1 && !strncmp(user_data+16, "x264 - core 0000", 16))
        h->x264_build = 67;

    if(h->avctx->debug & FF_DEBUG_BUGS)
        av_log(h->avctx, AV_LOG_DEBUG, "user data:\"%s\"\n", user_data+16);

    for(; i<size; i++)
        skip_bits(&h->gb, 8);

    return 0;
}

static int decode_recovery_point(H264Context *h){
    h->sei_recovery_frame_cnt = get_ue_golomb(&h->gb);
    skip_bits(&h->gb, 4);       /* 1b exact_match_flag, 1b broken_link_flag, 2b changing_slice_group_idc */

    return 0;
}

static int decode_buffering_period(H264Context *h){
    unsigned int sps_id;
    int sched_sel_idx;
    SPS *sps;

    sps_id = get_ue_golomb_31(&h->gb);
    if(sps_id > 31 || !h->sps_buffers[sps_id]) {
        av_log(h->avctx, AV_LOG_ERROR, "non-existing SPS %d referenced in buffering period\n", sps_id);
        return -1;
    }
    sps = h->sps_buffers[sps_id];

    // NOTE: This is really so duplicated in the standard... See H.264, D.1.1
    if (sps->nal_hrd_parameters_present_flag) {
        for (sched_sel_idx = 0; sched_sel_idx < sps->cpb_cnt; sched_sel_idx++) {
            h->initial_cpb_removal_delay[sched_sel_idx] = get_bits_long(&h->gb, sps->initial_cpb_removal_delay_length);
            skip_bits(&h->gb, sps->initial_cpb_removal_delay_length); // initial_cpb_removal_delay_offset
        }
    }
    if (sps->vcl_hrd_parameters_present_flag) {
        for (sched_sel_idx = 0; sched_sel_idx < sps->cpb_cnt; sched_sel_idx++) {
            h->initial_cpb_removal_delay[sched_sel_idx] = get_bits_long(&h->gb, sps->initial_cpb_removal_delay_length);
            skip_bits(&h->gb, sps->initial_cpb_removal_delay_length); // initial_cpb_removal_delay_offset
        }
    }

    h->sei_buffering_period_present = 1;
    return 0;
}

int ff_h264_decode_sei(H264Context *h){
    while (get_bits_left(&h->gb) > 16) {
        int size, type;

        type=0;
        do{
            if (get_bits_left(&h->gb) < 8)
                return -1;
            type+= show_bits(&h->gb, 8);
        }while(get_bits(&h->gb, 8) == 255);

        size=0;
        do{
            if (get_bits_left(&h->gb) < 8)
                return -1;
            size+= show_bits(&h->gb, 8);
        }while(get_bits(&h->gb, 8) == 255);

        if(h->avctx->debug&FF_DEBUG_STARTCODE)
            av_log(h->avctx, AV_LOG_DEBUG, "SEI %d len:%d\n", type, size);

        switch(type){
        case SEI_TYPE_PIC_TIMING: // Picture timing SEI
            if(decode_picture_timing(h) < 0)
                return -1;
            break;
        case SEI_TYPE_USER_DATA_ITU_T_T35:
            if(decode_user_data_itu_t_t35(h, size) < 0)
                return -1;
            skip_bits(&h->gb, 8*size);
            break;
        case SEI_TYPE_USER_DATA_UNREGISTERED:
            if(decode_unregistered_user_data(h, size) < 0)
                return -1;
            break;
        case SEI_TYPE_RECOVERY_POINT:
            if(decode_recovery_point(h) < 0)
                return -1;
            break;
        case SEI_BUFFERING_PERIOD:
            if(decode_buffering_period(h) < 0)
                return -1;
            break;
        default:
            skip_bits(&h->gb, 8*size);
        }

        //FIXME check bits here
        align_get_bits(&h->gb);
    }

    return 0;
}
