#ifndef __atheros_stereo__
#define __atheros_stereo__

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
union _stereo_sample_cnt {
    struct __stereo_sample_cnt {
#ifdef __BIG_ENDIAN
            volatile unsigned short ch1;
            volatile unsigned short ch0;
#else/*--- #ifdef __BIG_ENDIAN ---*/
#endif/*--- #else ---*//*--- #ifdef __BIG_ENDIAN ---*/
        } Bits;
        volatile unsigned int Register;
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
struct _ath_stereo_base {
    union _stereo_config {
        struct __stereo_config {
#ifdef __BIG_ENDIAN
            unsigned int reserved:                          8;
            volatile unsigned int spdif_enable:             1;
            volatile unsigned int refclk_sel:               1; /*--- enable stereo choose from external clock ---*/
            volatile unsigned int enable:                   1;
            volatile unsigned int mic_reset:                1;
            volatile unsigned int reset:                    1; /*--- automatically cleared by hardware ---*/
            volatile unsigned int i2s_delay:                1;  
            volatile unsigned int pcm_swap:                 1; /*--- swap byte order ---*/
            volatile unsigned int mic_wordsize:             1; /*--- 0: 16 Bit 1: 32 Bit ---*/
            volatile unsigned int stereo_mono:              2; /*--- 0: Stereo 1: Mono from Channel 0 2: Mono from Channel 1 ---*/
            volatile unsigned int data_word_size:           2; /*--- 8/16/24/32 Bit /word ---*/
            volatile unsigned int i2s_wordsize:             1; /*--- 16/32 Bits/I²S word ---*/
            volatile unsigned int mck_sel:                  1; /*--- 0: raw master clock is divided audio PLL clock, 1: raw master clock is MCLK_IN ---*/
            volatile unsigned int sample_cnt_clear_type:    1; /*--- 1: software read from sample_cnt clears counter registers ---*/
            volatile unsigned int master:                   1; /*--- 0: external DAC is master ---*/
            /*--------------------------------------------------------------------------------*\
             * I2S_SCK = MCLK / DIV
             * DIV = MCLK / (SAMPLER_RAT * I2S_WORD_SIZE *2 channels)
             * SPDIF_SCK  = MCLK/POSEDGE
            \*--------------------------------------------------------------------------------*/
            volatile unsigned int posedge:                  8; /*--- count in unit of MCLK ---*/
#else/*--- #ifdef __BIG_ENDIAN ---*/
#endif/*--- #else ---*//*--- #ifdef __BIG_ENDIAN ---*/
        } Bits;
        volatile unsigned int Register;
    } stereo_config;
    union _stereo_volume {
        struct __stereo_volume {
#ifdef __BIG_ENDIAN
            unsigned int reserved2:                        19;
            volatile unsigned int channel1:                 5; /*--- -6 dB-steps: 11111:-90 dB 11110: -84 dB 10001 -6 dB 00000/10000: 0 dB 00001 +6 dB  00111: +42 dB (max)  ---*/
            unsigned int reserved:                          3;
            volatile unsigned int channel0:                 5; /*--- -6 dB-steps: 11111:-90 dB 11110: -84 dB 10001 -6 dB 00000/10000: 0 dB 00001 +6 dB  00111: +42 dB (max)  ---*/
#else/*--- #ifdef __BIG_ENDIAN ---*/
#endif/*--- #else ---*//*--- #ifdef __BIG_ENDIAN ---*/
        } Bits;
        volatile unsigned int Register;
    } stereo_volume;
    union _stereo_master_clock {
        struct __stereo_master_clock {
#ifdef __BIG_ENDIAN
            unsigned int reserved:                         31;
            volatile unsigned int mck_sel:                 1; /*---master clock select ---*/
#else/*--- #ifdef __BIG_ENDIAN ---*/
#endif/*--- #else ---*//*--- #ifdef __BIG_ENDIAN ---*/
        } Bits;
        volatile unsigned int Register;
    } stereo_master_clock;
    union _stereo_sample_cnt stereo_tx_sample_cnt_lsb;
    union _stereo_sample_cnt stereo_tx_sample_cnt_msb;
    union _stereo_sample_cnt stereo_rx_sample_cnt_lsb;
    union _stereo_sample_cnt stereo_rx_sample_cnt_msb;
};

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
struct _ath_audio_pll_base {
    union _audio_pll_config {
        struct __audio_pll_config {
#ifdef __BIG_ENDIAN
            unsigned int reserved:                          17;
            volatile unsigned int ext_div:                  3;
            volatile unsigned int reserved2:                2;
            volatile unsigned int postpllpwd:               3; /*--- rw post power up control ---*/
            volatile unsigned int reserved3:                1;
            volatile unsigned int pllpwd:                   1; /*--- write zero to power up---*/
            volatile unsigned int bypass:                   1;  
            volatile unsigned int refdiv:                   4; /*--- refclk divider ---*/
#else/*--- #ifdef __BIG_ENDIAN ---*/
#endif/*--- #else ---*//*--- #ifdef __BIG_ENDIAN ---*/
        } Bits;
        volatile unsigned int Register;
    } audio_pll_config;
    union _audio_pll_modulation {
        struct __audio_pll_modulation {
#ifdef __BIG_ENDIAN
            unsigned int reserved:                          3;
            volatile unsigned int tgt_div_frac:            18;
            volatile unsigned int reserved2:                4;
            volatile unsigned int tgt_div_int:              6;
            volatile unsigned int start:                    1; /*--- start the audio-modulation ---*/
#else/*--- #ifdef __BIG_ENDIAN ---*/
#endif/*--- #else ---*//*--- #ifdef __BIG_ENDIAN ---*/
        } Bits;
        volatile unsigned int Register;
    } audio_pll_modulation;
    union _audio_pll_mod_step {
        struct __audio_pll_mod_step {
#ifdef __BIG_ENDIAN
            volatile unsigned int frac:                    18;
            volatile unsigned int reserved:                10;
            volatile unsigned int update_cnt:               4;
#else/*--- #ifdef __BIG_ENDIAN ---*/
#endif/*--- #else ---*//*--- #ifdef __BIG_ENDIAN ---*/
        } Bits;
        volatile unsigned int Register;
    } audio_pll_mod_step;
    union _audio_pll_current_modulation {
        struct __audio_pll_current_modulation {
#ifdef __BIG_ENDIAN
            unsigned int reserved:                          4;
            volatile unsigned int frac:                    18;
            volatile unsigned int reserved2:                3;
            volatile unsigned int tgt_div_int:              6;
            volatile unsigned int reserved3:                1;
#else/*--- #ifdef __BIG_ENDIAN ---*/
#endif/*--- #else ---*//*--- #ifdef __BIG_ENDIAN ---*/
        } Bits;
        volatile unsigned int Register;
    } audio_pll_current_modulation;
};
#define AUDIO_PLL_REFCLK      (40 * 1000 * 1000)
/*--------------------------------------------------------------------------------*\
 * pllfreq = REFCLK/REFDIV  * (div_frac/ 2^18 + div_int)  / 2^postplldiv
 * mclk = pllfreq / extdiv
\*--------------------------------------------------------------------------------*/
static void dump_audio_pll(const char *prefix) {
    struct _ath_audio_pll_base *apll = (struct _ath_audio_pll_base *)KSEG1ADDR(ATH_AUDIO_PLL_CONFIG);
    unsigned int pllfreq = AUDIO_PLL_REFCLK;
    unsigned int mclk = 0;

    if(apll->audio_pll_config.Bits.refdiv && !apll->audio_pll_config.Bits.bypass) {
        long long    refclk_div = AUDIO_PLL_REFCLK /  apll->audio_pll_config.Bits.refdiv;
        unsigned int int_frac   = (apll->audio_pll_modulation.Bits.tgt_div_int << 18) + apll->audio_pll_modulation.Bits.tgt_div_frac;
        int_frac              >>=  apll->audio_pll_config.Bits.postpllpwd;

        pllfreq = (unsigned int)(refclk_div * (long long)int_frac >> 18);
    }
    if(apll->audio_pll_config.Bits.ext_div) {
         mclk= pllfreq / apll->audio_pll_config.Bits.ext_div;
    }
    printk(KERN_INFO"%s\n"
           "%p AUDIO_PLL_CONFIG         0x%08x EXTDIV=%x POSTPLLPWD=%x PLLPWD=%x BYPASS=%x REFDIV=%u\n"
           "%p AUDIO_PLL_MODULATION     0x%08x TGT_DIV_FRAC=%u TGT_DIV_INT=%u START=%x\n"
           "%p AUDIO_PLL_MOD_STEP       0x%08x FRAC=%u UPDATE_CNT=%u\n"
           "%p CUR_AUDIO_PLL_MODULATION 0x%08x FRAC=%u INT=%u\n"
           "pll=%u mclk=%u\n",
            prefix,
            &apll->audio_pll_config.Register, apll->audio_pll_config.Register, 
                                              apll->audio_pll_config.Bits.ext_div,
                                              apll->audio_pll_config.Bits.postpllpwd, 
                                              apll->audio_pll_config.Bits.pllpwd, 
                                              apll->audio_pll_config.Bits.bypass, 
                                              apll->audio_pll_config.Bits.refdiv, 
            &apll->audio_pll_modulation.Register, apll->audio_pll_modulation.Register, 
                                              apll->audio_pll_modulation.Bits.tgt_div_frac, 
                                              apll->audio_pll_modulation.Bits.tgt_div_int, 
                                              apll->audio_pll_modulation.Bits.start,
            &apll->audio_pll_mod_step.Register, apll->audio_pll_mod_step.Register, 
                                              apll->audio_pll_mod_step.Bits.frac, 
                                              apll->audio_pll_mod_step.Bits.update_cnt, 
            &apll->audio_pll_current_modulation.Register, apll->audio_pll_current_modulation.Register, 
                                              apll->audio_pll_current_modulation.Bits.frac, 
                                              apll->audio_pll_current_modulation.Bits.tgt_div_int,
               pllfreq, mclk
          );
}
#endif/*--- #ifndef __atheros_stereo__ ---*/
