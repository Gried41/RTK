#include <assert.h>
#include "app_config.h"
#include "app_audio.h"
#include "audio_scene.h"
#include "co_list.h"
#include "app_task.h"

#include "mp3_sample.h"
#include "sbc_sample.h"
#include "local_playback.h"

#define APP_AUDIO_DATA_READY_THD            6
#define APP_AUDIO_DATA_BUFFER_MAX           12

enum app_audio_mode_t {
    APP_AUDIO_MODE_IDLE,
    APP_AUDIO_MODE_A2DP_SINK,
    APP_AUDIO_MODE_A2DP_SOURCE,
    APP_AUDIO_MODE_SCO,
    APP_AUDIO_MODE_TONE,
    APP_AUDIO_MODE_VOICE_RECOGNIZE,
    APP_AUDIO_MODE_LOCAL_PLAYBACK,
};

static enum app_audio_mode_t audio_mode =  APP_AUDIO_MODE_IDLE;
static enum app_audio_mode_t audio_tone_mode =  APP_AUDIO_MODE_IDLE;
static uint32_t audio_data_counter = 0;
static audio_scene_t *audio_scene;

static uint32_t tone_offset = 0;
static uint32_t tone_single_size = 128;
static uint8_t *tone_data;
static uint32_t tone_size;
static uint32_t local_playback_offset = 0;
static uint32_t local_playback_single_size = 128;
static uint8_t *local_playback_data;
static uint32_t local_playback_size;

static uint32_t mp3_rd_offset = 0;
#define A2DP_SOURCE_MP3_RAW_DATA_LENGTH         512     //the length transmit to decoder each time

static void a2dp_sink_start(audio_type_t audio_type, uint32_t sample_rate);
static void a2dp_sink_stop(void);
static void a2dp_sink_play(uint8_t *buffer, uint32_t length);
static void sco_start(audio_type_t audio_type, audio_sco_report_encoded_frame report_enc_cb, void *report_enc_arg);
static void sco_stop(void);
static void sco_recv(bool valid, uint8_t audio_type, uint8_t *buffer, uint32_t length);
static void a2dp_source_start(audio_type_t audio_type, struct sbc_encoder_param *sbc_param, audio_a2dp_source_report_encoded_frame report_enc_cb, void *report_enc_arg);
static void a2dp_source_stop(void);
static void tone_stop(bool immediate);
static void tone_play(audio_type_t audio_type, uint8_t *tone_sample, uint32_t tone_len);
static void local_playback_play(audio_type_t audio_type,  uint8_t *local_playback_sample, uint32_t local_playback_len);
static void local_playback_stop(void);

static void audio_pa_disable(void)
{
    gpio_write_pin(GPIOB, GPIO_PIN_12, GPIO_PIN_CLEAR); //3068E-C DEV1.3 PA
    gpio_write_pin(GPIOD, GPIO_PIN_9, GPIO_PIN_CLEAR); //3068E-C DEV1.3 PA

    /* deinitialize audio PSD DAC IO */
    GPIO_InitTypeDef gpio_config;

    __SYSTEM_GPIOC_CLK_ENABLE();
    gpio_config.Pin  = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8;
    gpio_config.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_config.Pull = GPIO_PULLUP;
    gpio_config.Alternate = GPIO_FUNCTION_0;
    gpio_init(GPIOC, &gpio_config);
    gpio_write_pin(GPIOC, GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8, GPIO_PIN_CLEAR);

    /* configure PD4~PD8 to PSD_DAC function */
    gpio_config.Pin  = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8;
    gpio_config.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_config.Pull = GPIO_PULLUP;
    gpio_config.Alternate = GPIO_FUNCTION_0;
    gpio_init(GPIOD, &gpio_config);
    gpio_write_pin(GPIOD, GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8, GPIO_PIN_CLEAR);

}

static void audio_pa_enable(void)
{
    gpio_write_pin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET); //3068E-C DEV1.3 PA
    gpio_write_pin(GPIOD, GPIO_PIN_9, GPIO_PIN_SET); //3068E-C DEV1.3 PA

    /* initialize audio PSD DAC IO */
    /* D4: PWMLP */
    /* D5: PWMLN */
    /* D6: PWMRP */
    /* D7: PWMRN */
    /* D8: PWMCLK */
    GPIO_InitTypeDef gpio_config;

    __SYSTEM_GPIOC_CLK_ENABLE();
    gpio_config.Pin  = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8;
    gpio_config.Mode = GPIO_MODE_AF_PP;
    gpio_config.Pull = GPIO_PULLUP;
    gpio_config.Alternate = GPIO_FUNCTION_D;
    gpio_init(GPIOC, &gpio_config);

    /* configure PD4~PD8 to PSD_DAC function */
    gpio_config.Pin  = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8;
    gpio_config.Mode = GPIO_MODE_AF_PP;
    gpio_config.Pull = GPIO_PULLUP;
    gpio_config.Alternate = GPIO_FUNCTION_D;
    gpio_init(GPIOD, &gpio_config);
}

static uint32_t request_raw_tone_cb(uint8_t *data, uint32_t length)
{
    if (tone_offset >= tone_size) {
        return 0;
    }

    if((tone_offset + length) <= tone_size){
        memcpy(data, &tone_data[tone_offset], length);
        tone_offset += length;
    }
    else{
        length = tone_size - tone_offset;
        memcpy(data, &tone_data[tone_offset], length);
        tone_offset += length;
    }
    
    if (tone_offset >= tone_size) {
        tone_stop(false);
    }
    return length;
}

static void reset_reset_dataoffset(void)
{
    tone_offset = 0;
}

static uint32_t request_local_playback_cb(uint8_t *data, uint32_t length)
{
    if (local_playback_offset >= local_playback_size) {
        return 0;
    }

    if((local_playback_offset + length) <= local_playback_size){
        memcpy(data, &local_playback_data[local_playback_offset], length);
        local_playback_offset += length;
    }
    else{
        length = local_playback_size - local_playback_offset;
        memcpy(data, &local_playback_data[local_playback_offset], length);
        local_playback_offset += length;
    }
    
    if (local_playback_offset >= local_playback_size) {
        app_audio_local_playback_stop();
        audio_pa_disable();
    }
    
    return length;
}
#if BTDM_STACK_ENABLE_A2DP_SRC
static uint32_t request_raw_mp3_cb(uint8_t *data, uint32_t length)
{
    uint32_t total_length = mp3_sample_get_size();
    
    if((mp3_rd_offset + length) <= total_length){
        memcpy(data, &mp3_sample[mp3_rd_offset], length);
        mp3_rd_offset += length;
    }
    else{
        length = total_length - mp3_rd_offset;
        memcpy(data, &mp3_sample[mp3_rd_offset], length);
        mp3_rd_offset = 0;    
    }
    
    return length;
}
#endif
void tone_destroyed_cb(void)
{
    if(audio_mode == APP_AUDIO_MODE_TONE)
    {
        audio_mode = APP_AUDIO_MODE_IDLE;
    }
    if(audio_tone_mode == APP_AUDIO_MODE_TONE)
    {
        audio_tone_mode = APP_AUDIO_MODE_IDLE;
    }
    audio_pa_disable();
}

static void app_audio_stop(void)
{
    switch(audio_mode) {
        #if BTDM_STACK_ENABLE_A2DP_SNK
        case APP_AUDIO_MODE_A2DP_SINK:
            a2dp_sink_stop();
            break;
        #endif
        #if BTDM_STACK_ENABLE_HF || BTDM_STACK_ENABLE_AG
        case APP_AUDIO_MODE_SCO:
            sco_stop();
            break;
        #endif
        #if BTDM_STACK_ENABLE_A2DP_SRC
        case APP_AUDIO_MODE_A2DP_SOURCE:
            a2dp_source_stop();
            break;
        #endif
        case APP_AUDIO_MODE_TONE:
            tone_stop(true);
            break;
        case APP_AUDIO_MODE_LOCAL_PLAYBACK:
            local_playback_stop();
            break;
        default:
            break;
    }
}
#if BTDM_STACK_ENABLE_A2DP_SNK
static void a2dp_sink_start(audio_type_t audio_type, uint32_t sample_rate)
{
    audio_a2dp_sink_param_t param;

    if ((audio_mode == APP_AUDIO_MODE_A2DP_SINK) || (audio_mode == APP_AUDIO_MODE_A2DP_SOURCE)) {
        return;
    }
    
    if (audio_mode != APP_AUDIO_MODE_IDLE) {
        app_audio_stop();
    }

    param.sample_rate = sample_rate;
    param.channels = AUDIO_CHANNELS_STEREO;
    param.decoder_type = audio_type;
    param.hw_type = AUDIO_HW_TYPE_PSD_DAC;
    param.hw_base_addr = I2S1_BASE;
    if (audio_type == AUDIO_TYPE_SBC) {
    }
    else if (audio_type == AUDIO_TYPE_AAC) {
        param.decoder_param.aac.PcmWidth = 16;
    }
    else {
        assert(0);
    }

    assert(audio_mode == APP_AUDIO_MODE_IDLE);

    audio_scene = audio_scene_create(&audio_a2dp_sink_operator, &param);
    assert(audio_scene != NULL);
    audio_mode = APP_AUDIO_MODE_A2DP_SINK;
}

static void a2dp_sink_stop(void)
{
    if (audio_mode == APP_AUDIO_MODE_A2DP_SINK) {   
        audio_scene_destroy(audio_scene);
        audio_mode = APP_AUDIO_MODE_IDLE;
        audio_pa_disable();
    }
}

static void a2dp_sink_play(uint8_t *buffer, uint32_t length)
{
    if (audio_mode == APP_AUDIO_MODE_A2DP_SINK) {
        if (audio_scene_decoder_started(audio_scene)) {
            audio_pa_enable();
        }
        audio_scene_recv_encoded_data(audio_scene, true, buffer, length);
    }
}
#endif
#if BTDM_STACK_ENABLE_HF || BTDM_STACK_ENABLE_AG
static void sco_start(audio_type_t audio_type, audio_sco_report_encoded_frame report_enc_cb, void *report_enc_arg)
{
    audio_sco_param_t param;
    
    if (audio_mode == APP_AUDIO_MODE_SCO) {
        return;
    }
    
    if (audio_mode != APP_AUDIO_MODE_IDLE) {
        app_audio_stop();
    }

    if (audio_type == AUDIO_TYPE_MSBC) {
        printf("SCO: msbc.\r\n");
        param.sample_rate = 16000;
        
        param.encoder_param.msbc.i_bitrate = 128000;
        param.encoder_param.msbc.i_samp_freq = 16000;
    }
    else {
        printf("SCO: cvsd.\r\n");
        param.sample_rate = 8000;
        
        param.decoder_param.pcm.sample_rate = 8000;
        param.decoder_param.pcm.frame_size = 120; //60
        param.decoder_param.pcm.channels = 1;
    }
    param.audio_type = audio_type;
    param.hw_type_in = AUDIO_HW_TYPE_SARADC_PDM;//AUDIO_HW_TYPE_PDM;
    param.hw_type_out = AUDIO_HW_TYPE_PSD_DAC;
    param.hw_base_addr_in = PDM0_BASE;
    param.hw_base_addr_out = NULL;
    // param.hw_type = AUDIO_HW_TYPE_PSD_DAC;
    // param.hw_base_addr = I2S0_BASE;
    param.report_enc_cb = report_enc_cb;
    param.report_enc_arg = report_enc_arg;

    assert(audio_mode == APP_AUDIO_MODE_IDLE);

    audio_scene = audio_scene_create(&audio_sco_operator, &param);
    audio_mode = APP_AUDIO_MODE_SCO;
    assert(audio_scene != NULL);
}

static void sco_stop(void)
{
    if (audio_mode == APP_AUDIO_MODE_SCO) {        
        audio_scene_destroy(audio_scene);
        audio_mode = APP_AUDIO_MODE_IDLE;
        audio_pa_disable();
    }
}

static void sco_recv(bool valid, uint8_t audio_type, uint8_t *buffer, uint32_t length)
{
    if (audio_mode == APP_AUDIO_MODE_SCO) {
        if (audio_scene_decoder_started(audio_scene)) {
            audio_pa_enable();
        }
        if (audio_type == AUDIO_TYPE_MSBC) {
            if((buffer[0] != 0x01) || ((buffer[1] & 0x08) != 0x08)){
                audio_scene_recv_encoded_data(audio_scene, 0, buffer+2, length-3);
            }
            else{
            audio_scene_recv_encoded_data(audio_scene, valid, buffer+2, length-3);
        }
        }
        else {
            audio_scene_recv_encoded_data(audio_scene, valid, buffer, length);
        }
    }
}
#endif
#if BTDM_STACK_ENABLE_A2DP_SRC
static void a2dp_source_start(audio_type_t audio_type, struct sbc_encoder_param *sbc_param, audio_a2dp_source_report_encoded_frame report_enc_cb, void *report_enc_arg)
{
    audio_a2dp_source_param_t param;

    if (audio_mode == APP_AUDIO_MODE_A2DP_SOURCE) {
        return;
    }
    
    if (audio_mode != APP_AUDIO_MODE_IDLE) {
        app_audio_stop();
    }

    param.channels = AUDIO_CHANNELS_STEREO;
    param.sample_rate = sbc_param->i_samp_freq;
    
    param.hw_type = AUDIO_HW_TYPE_I2S;
    param.hw_base_addr = I2S0_BASE;
    param.report_enc_cb = report_enc_cb;
    param.report_enc_arg = report_enc_arg;
    
    param.audio_input_type = audio_type;
    param.dec_req_raw_cb = request_raw_mp3_cb;
        
    param.audio_output_type = AUDIO_TYPE_SBC;
    param.encoder_param.sbc.i_samp_freq = param.sample_rate;
    param.encoder_param.sbc.i_num_chan = param.channels;
    param.encoder_param.sbc.i_subbands = sbc_param->i_subbands;//8;
    param.encoder_param.sbc.i_blocks = sbc_param->i_blocks;//16;
    param.encoder_param.sbc.i_bitpool = sbc_param->i_bitpool;
    param.encoder_param.sbc.i_snr = sbc_param->i_snr;
    
    assert(audio_mode == APP_AUDIO_MODE_IDLE);

    audio_scene = audio_scene_create(&audio_a2dp_source_operator, &param);
    assert(audio_scene != NULL);
    audio_mode = APP_AUDIO_MODE_A2DP_SOURCE;
}

static void a2dp_source_stop(void)
{
    if (audio_mode == APP_AUDIO_MODE_A2DP_SOURCE) {        
        audio_scene_destroy(audio_scene);
        audio_mode = APP_AUDIO_MODE_IDLE;
    }
}
#endif
static void tone_stop(bool immediate)
{
    if (immediate) {
        if (audio_mode != APP_AUDIO_MODE_IDLE) {
            audio_scene_tone_stop(true);
            if (audio_mode == APP_AUDIO_MODE_TONE) {
                audio_mode = APP_AUDIO_MODE_IDLE;
            }
        }
    }else{
        audio_scene_tone_stop(false);
    }
    audio_pa_disable();
}

static void tone_play(audio_type_t audio_type, uint8_t *tone_sample, uint32_t tone_len)
{
    audio_tone_param_t param;

    tone_stop(true);

    if(tone_sample == NULL){
       tone_data = sbc_sample;
       tone_size = sbc_sample_get_size();
    }
    else{
        tone_data = tone_sample;
        tone_size = tone_len;
    }
    
    param.audio_type = audio_type;
    if ((audio_type != AUDIO_TYPE_SBC)
            && (audio_type != AUDIO_TYPE_SBC_V2)
            && (audio_type != AUDIO_TYPE_MP3)) {
        assert(0);
    }

    param.hw_type = AUDIO_HW_TYPE_PSD_DAC;
    param.hw_base_addr = I2S1_BASE;
    param.channels = AUDIO_CHANNELS_STEREO;
    param.sample_rate = 44100;
    param.req_raw_cb = request_raw_tone_cb;
    param.tone_destroyed_cb = tone_destroyed_cb;
    
    if (audio_mode == APP_AUDIO_MODE_IDLE) {
        audio_scene = audio_scene_tone_play(&param);
        audio_mode = APP_AUDIO_MODE_TONE;
        audio_tone_mode = APP_AUDIO_MODE_TONE;
        assert(audio_scene != NULL);
        audio_pa_enable();
    }
    else{
        audio_scene_tone_play(&param);
        audio_tone_mode = APP_AUDIO_MODE_TONE;
        audio_pa_enable();
    }
}

static void local_playback_play(audio_type_t audio_type,  uint8_t *local_playback_sample, uint32_t local_playback_len)
{
    local_playback_param_t param;

    if (audio_mode == APP_AUDIO_MODE_LOCAL_PLAYBACK){
        return;
    }
    
    if (audio_mode != APP_AUDIO_MODE_IDLE) {
        app_audio_stop();
    }

    local_playback_offset = 0;
    if(local_playback_sample == NULL){
       local_playback_data = sbc_sample;
       local_playback_size = sbc_sample_get_size();
    }
    else{
        local_playback_data = local_playback_sample;
        local_playback_size = local_playback_len;
    }
    
    param.audio_type = audio_type;
    if ((audio_type != AUDIO_TYPE_SBC)
            && (audio_type != AUDIO_TYPE_SBC_V2)
            && (audio_type != AUDIO_TYPE_MP3)) {
        assert(0);
    }

    param.hw_type = AUDIO_HW_TYPE_PSD_DAC;
    param.hw_base_addr = I2S1_BASE;
    param.channels = AUDIO_CHANNELS_STEREO;
    param.sample_rate = 44100;
    param.req_raw_cb = request_local_playback_cb;
    
    assert(audio_mode == APP_AUDIO_MODE_IDLE);
    audio_scene = audio_scene_create(&audio_local_playback_operator, &param);
    audio_pa_enable();

    assert(audio_scene != NULL);
    audio_mode = APP_AUDIO_MODE_LOCAL_PLAYBACK;
}

static void local_playback_stop(void)
{
    if (audio_mode == APP_AUDIO_MODE_LOCAL_PLAYBACK) {        
        audio_scene_destroy(audio_scene);
        audio_mode = APP_AUDIO_MODE_IDLE;
        audio_pa_disable();
    }
}
#if BTDM_STACK_ENABLE_A2DP_SNK
void app_audio_a2dp_sink_start(audio_type_t audio_type, uint32_t sample_rate)
{
    audio_evt_t *evt;
    struct app_task_event *event;
     
    event = app_task_event_alloc(APP_TASK_EVENT_AUDIO, sizeof(audio_evt_t), true);
    if ( event )
    {
        evt = (void *)event->param;
        evt->event_type = APP_AUDIO_EVENT_SINK_START;
        evt->p.sink_start.audio_type = audio_type;
        evt->p.sink_start.sample_rate = sample_rate;
        //printf("1: 0x%08x, 0x%08x\r\n", event, evt);
        app_task_event_post( event, false );
    }
}

void app_audio_a2dp_sink_stop(void)
{
    audio_evt_t *evt;
    struct app_task_event *event;
     
    event = app_task_event_alloc(APP_TASK_EVENT_AUDIO, sizeof(audio_evt_t), true);
    if ( event )
    {
        evt = (void *)event->param;
        evt->event_type = APP_AUDIO_EVENT_SINK_STOP;
        //printf("2: 0x%08x, 0x%08x\r\n", event, evt);
        app_task_event_post( event, false );
    }
}

void app_audio_a2dp_sink_play(uint8_t *buffer, uint32_t length)
{
//    audio_evt_t *evt;
//    struct app_task_event *event;
//     
//    event = app_task_event_alloc(APP_TASK_EVENT_AUDIO, sizeof(audio_evt_t), true);
//    if ( event )
//    {
//        evt = (void *)event->param;
//        evt->event_type = APP_AUDIO_EVENT_SINK_PLAY;
//        evt->p.sink_play.buffer = buffer;
//        evt->p.sink_play.length = length;
//        //printf("3: 0x%08x, 0x%08x\r\n", event, evt);
//        app_task_event_post( event, false );
//    }
    a2dp_sink_play(buffer,length);
}
#endif
#if BTDM_STACK_ENABLE_HF || BTDM_STACK_ENABLE_AG
void app_audio_sco_start(audio_type_t audio_type, audio_sco_report_encoded_frame report_enc_cb, void *report_enc_arg)
{
    audio_evt_t *evt;
    struct app_task_event *event;
     
    event = app_task_event_alloc(APP_TASK_EVENT_AUDIO, sizeof(audio_evt_t), true);
    if ( event )
    {
        evt = (void *)event->param;
        evt->event_type = APP_AUDIO_EVENT_SCO_START;
        evt->p.sco_start.audio_type = audio_type;
        evt->p.sco_start.report_enc_cb = report_enc_cb;
        evt->p.sco_start.report_enc_arg = report_enc_arg;
        //printf("4: 0x%08x, 0x%08x\r\n", event, evt);
        app_task_event_post( event, false );
    }
}

void app_audio_sco_stop(void)
{
    audio_evt_t *evt;
    struct app_task_event *event;
     
    event = app_task_event_alloc(APP_TASK_EVENT_AUDIO, sizeof(audio_evt_t), true);
    if ( event )
    {
        evt = (void *)event->param;
        evt->event_type = APP_AUDIO_EVENT_SCO_STOP;
        //printf("5: 0x%08x, 0x%08x\r\n", event, evt);
        app_task_event_post( event, false );
    }
}

void app_audio_sco_recv(bool valid, uint8_t audio_type, uint8_t *buffer, uint32_t length)
{
    // audio_evt_t *evt;
    // struct app_task_event *event;
    // event = app_task_event_alloc(APP_TASK_EVENT_AUDIO, sizeof(audio_evt_t), true);
    // if ( event )
    // {
    //     evt = (void *)event->param;
    //     printf("sco_recv:0x%08x\r\n", evt);
    //     evt->event_type = APP_AUDIO_EVENT_SCO_RECV;
    //     evt->p.sco_recv.valid = valid;
    //     evt->p.sco_recv.audio_type = audio_type;
    //     evt->p.sco_recv.buffer = buffer;
    //     evt->p.sco_recv.length = length;
    //     app_task_event_post( event, false );
    // }
    sco_recv(valid, audio_type, buffer, length);
}
#endif
#if BTDM_STACK_ENABLE_A2DP_SRC
void app_audio_a2dp_source_start(audio_type_t audio_type, struct sbc_encoder_param *sbc_param, audio_a2dp_source_report_encoded_frame report_enc_cb, void *report_enc_arg)
{
    audio_evt_t *evt;
    struct app_task_event *event;
     
    event = app_task_event_alloc(APP_TASK_EVENT_AUDIO, sizeof(audio_evt_t), true);
    if ( event )
    {
        evt = (void *)event->param;
        // printf("src_start:0x%08x\r\n", evt);
        evt->event_type = APP_AUDIO_EVENT_SOURCE_START; 
        evt->p.source_start.audio_type = audio_type;
        memcpy(&evt->p.source_start.sbc_param, sbc_param, sizeof(struct sbc_encoder_param));
        evt->p.source_start.report_enc_cb = report_enc_cb;
        evt->p.source_start.report_enc_arg = report_enc_arg;
        //printf("6: 0x%08x, 0x%08x\r\n", event, evt);
        app_task_event_post( event, false );
    }
}

void app_audio_a2dp_source_stop(void)
{
    audio_evt_t *evt;
    struct app_task_event *event;
     
    event = app_task_event_alloc(APP_TASK_EVENT_AUDIO, sizeof(audio_evt_t), true);
    if ( event )
    {
        evt = (void *)event->param;
        evt->event_type = APP_AUDIO_EVENT_SOURCE_STOP;
        // printf("7: 0x%08x, 0x%08x\r\n", event, evt);
        app_task_event_post( event, false );
    }
}
#endif
void app_audio_tone_play(audio_type_t audio_type, uint8_t *tone_sample, uint32_t tone_len)
{
    audio_evt_t *evt;
    struct app_task_event *event;
     
    event = app_task_event_alloc(APP_TASK_EVENT_AUDIO, sizeof(audio_evt_t), true);
    if ( event )
    {
        evt = (void *)event->param;
        evt->event_type = APP_AUDIO_EVENT_TONE_PLAY;
        evt->p.tone_play.audio_type = audio_type;
        evt->p.tone_play.tone_sample = tone_sample;
        evt->p.tone_play.tone_len = tone_len;
        //printf("8: 0x%08x, 0x%08x\r\n", event, evt);
        app_task_event_post( event, false );
    }
}

void app_audio_tone_stop(bool immediate)
{
    audio_evt_t *evt;
    struct app_task_event *event;
     
    event = app_task_event_alloc(APP_TASK_EVENT_AUDIO, sizeof(audio_evt_t), true);
    if ( event )
    {
        evt = (void *)event->param;
        evt->event_type = APP_AUDIO_EVENT_TONE_STOP;
        //printf("9: 0x%08x, 0x%08x\r\n", event, evt);
        app_task_event_post( event, false );
    }
}

// bool app_audio_IsPlaying( void )
// {
//     return (audio_mode != APP_AUDIO_MODE_IDLE) ? true : false;
// }

void app_audio_local_playback_play(audio_type_t audio_type,  uint8_t *local_playback_sample, uint32_t local_playback_len)
{
    audio_evt_t *evt;
    struct app_task_event *event;

    event = app_task_event_alloc(APP_TASK_EVENT_AUDIO, sizeof(audio_evt_t), true);
    if ( event )
    {
        evt = (void *)event->param;
        evt->event_type = APP_AUDIO_EVENT_LOCAL_PLAY;
        evt->p.local_play.audio_type = audio_type;
        evt->p.local_play.local_playback_sample = local_playback_sample;
        evt->p.local_play.local_playback_len = local_playback_len;
        app_task_event_post( event, false );
    }
}

void app_audio_local_playback_stop(void)
{
    audio_evt_t *evt;
    struct app_task_event *event;
     
    event = app_task_event_alloc(APP_TASK_EVENT_AUDIO, sizeof(audio_evt_t), true);
    if ( event )
    {
        evt = (void *)event->param;
        evt->event_type = APP_AUDIO_EVENT_LOCAL_STOP;
        app_task_event_post( event, false );
    }
}

void app_audio_event_handler(struct app_task_event *event)
{
    audio_evt_t *evt = NULL;
    evt = (void *)&event->param[0];

#if DSP_CODE_USE_SRAM_HEAP == 1
    dsp_mem_sram_heap_init(0x20070000, 0x10000);
#endif
    switch (evt->event_type)
    {
        #if BTDM_STACK_ENABLE_A2DP_SNK
        case APP_AUDIO_EVENT_SINK_START:
            a2dp_sink_start(evt->p.sink_start.audio_type, evt->p.sink_start.sample_rate);
            break;
        case APP_AUDIO_EVENT_SINK_PLAY:
            a2dp_sink_play(evt->p.sink_play.buffer, evt->p.sink_play.length);
            break;
        case APP_AUDIO_EVENT_SINK_STOP:
            a2dp_sink_stop();
            break;
        #endif
        #if BTDM_STACK_ENABLE_HF || BTDM_STACK_ENABLE_AG
        case APP_AUDIO_EVENT_SCO_START:
            sco_start(evt->p.sco_start.audio_type, evt->p.sco_start.report_enc_cb, evt->p.sco_start.report_enc_arg);
            break;
        case APP_AUDIO_EVENT_SCO_STOP:
            sco_stop();
            break;
        case APP_AUDIO_EVENT_SCO_RECV:
            sco_recv(evt->p.sco_recv.valid, evt->p.sco_recv.audio_type, evt->p.sco_recv.buffer, evt->p.sco_recv.length);
            break;
        #endif
        #if BTDM_STACK_ENABLE_A2DP_SRC
        case APP_AUDIO_EVENT_SOURCE_START:
            a2dp_source_start(evt->p.source_start.audio_type, &evt->p.source_start.sbc_param, 
                                evt->p.source_start.report_enc_cb, evt->p.source_start.report_enc_arg);
            break;
        case APP_AUDIO_EVENT_SOURCE_STOP:
            a2dp_source_stop();
            break;
        #endif
        case APP_AUDIO_EVENT_TONE_PLAY:
            tone_play(evt->p.tone_play.audio_type, evt->p.tone_play.tone_sample, evt->p.tone_play.tone_len);
            break;
        case APP_AUDIO_EVENT_TONE_STOP:
            tone_stop(evt->p.tone_stop.immediate);
            break;
        case APP_AUDIO_EVENT_LOCAL_PLAY:
            local_playback_play(evt->p.local_play.audio_type, evt->p.local_play.local_playback_sample, 
                                                                evt->p.local_play.local_playback_len);
            break;
        case APP_AUDIO_EVENT_LOCAL_STOP:
            local_playback_stop();
            break;
        default:
            break;
    }
}
