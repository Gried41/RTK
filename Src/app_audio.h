#ifndef _APP_AUDIO_H
#define _APP_AUDIO_H

#include <stdint.h>
#include <stdbool.h>

#include "app_task.h"
#include "audio_common.h"
#include "audio_scene.h"

#include "audio_a2dp_sink.h"
#include "audio_a2dp_source.h"
#include "audio_sco.h"
#include "local_playback.h"

typedef enum {
    APP_AUDIO_EVENT_SINK_START,
    APP_AUDIO_EVENT_SINK_PLAY,
    APP_AUDIO_EVENT_SINK_STOP,
    APP_AUDIO_EVENT_SCO_START,
    APP_AUDIO_EVENT_SCO_RECV,
    APP_AUDIO_EVENT_SCO_STOP,
    APP_AUDIO_EVENT_SOURCE_START,
    APP_AUDIO_EVENT_SOURCE_STOP,
    APP_AUDIO_EVENT_TONE_PLAY,
    APP_AUDIO_EVENT_TONE_STOP,
    APP_AUDIO_EVENT_LOCAL_PLAY,
    APP_AUDIO_EVENT_LOCAL_STOP,
} app_audio_evt_type_t;

typedef struct {
    app_audio_evt_type_t event_type;
    union {
        struct {
            audio_type_t audio_type;
            uint32_t sample_rate;
        } sink_start;

        struct {
            uint8_t *buffer;
            uint32_t length;
        } sink_play;

        struct {
            audio_type_t audio_type;
            audio_sco_report_encoded_frame report_enc_cb;
            void *report_enc_arg;
        } sco_start;

        struct {
            bool valid;
            audio_type_t audio_type;
            uint8_t *buffer;
            uint32_t length;
        } sco_recv;

        struct {
            audio_type_t audio_type;
            struct sbc_encoder_param sbc_param;
            audio_a2dp_source_report_encoded_frame report_enc_cb; 
            void *report_enc_arg;
        } source_start;
    
        struct {
            audio_type_t audio_type;
            uint8_t *tone_sample; 
            uint32_t tone_len;
        } tone_play;

        struct {
            audio_type_t audio_type;
            uint8_t *local_playback_sample; 
            uint32_t local_playback_len;
        } local_play;

        struct {
            bool immediate;
        } tone_stop;
    } p;
} audio_evt_t;

/*
 * @fn          app_audio_a2dp_sink_start
 *
 * @brief       start the audio scene as an A2DP sink device.
 *
 * @param[in]   audio_type: audio_type negotiated with source, AUDIO_TYPE_SBC and AUDIO_TYPE_AAC supported currently. @ref audio_type_t.
 * @param[in]   sample_rate: the sample rate of audio flow.
 */
void app_audio_a2dp_sink_start(audio_type_t audio_type, uint32_t sample_rate);

/*
 * @fn          app_audio_a2dp_sink_stop
 *
 * @brief       stop and destroy an a2dp sink audio scene. 
 */
void app_audio_a2dp_sink_stop(void);

/*
 * @fn          app_audio_a2dp_sink_play
 *
 * @brief       play the received data from A2DP source.
 *
 * @param[in]   buffer : buffer pointer stored the received data from A2DP source.
 * @param[in]   length: the length of received audio data from A2DP source waited to be played
 */
void app_audio_a2dp_sink_play(uint8_t *buffer, uint32_t length);

/*
 * @fn          app_audio_sco_start
 *
 * @brief       start the audio scene of a HandFree phonecall , which is based on sco connection.
 *
 * @param[in]   audio_type : depends on the current CODEC set with the channel, AUDIO_TYPE_MSBC and AUDIO_TYPE_PCM supported currently. @ref audio_type_t.
 * @param[in]   report_enc_cb: callback to send encoded data to the other side during HF phonecall. 
 * @param[in]   report_enc_arg: pointer to the specific HF channal. 
 */
void app_audio_sco_start(audio_type_t audio_type, audio_sco_report_encoded_frame report_enc_cb, void *report_enc_arg);

/*
 * @fn          app_audio_sco_stop
 * 
 * @brief       stop and destroy a HF phonecall audio scene. 
 */
void app_audio_sco_stop(void);

/*
 * @fn          app_audio_sco_recv
 *
 * @brief       Receive the audio data from the other side during a HF phonecall to the sco data list waited to be process
 *
 * @param[in]   valid : to tell the audio data packet is intact or not, deciding the later decode process. TRUE: intact; FALSE: packet loss. 
 * @param[in]   audio_type : audio_type, AUDIO_TYPE_MSBC and AUDIO_TYPE_PCM supported currently.@ref audio_type_t.
 * @param[in]   buffer : pointer to the buffer stored the received data from the other side device during a HF phonecall. 
 * @param[in]   length: the length of received audio data waited to be processed 
 */
void app_audio_sco_recv(bool valid, uint8_t audio_type, uint8_t *buffer, uint32_t length);


/*
 * @fn          app_audio_a2dp_source_start
 *
 * @brief       start the audio scene as an A2DP source device.
 *
 * @param[in]   audio_type: audio type of the local audio waiting to be transfered. MP3 samples currently presented, new customized samples should be added referring to the file mp3_sample.c @ref audio_type_t.
 * @param[in]   sbc_param: parameter to config the encodedr for getting the sbc data wanted to be transfered to sink.
 * @param[in]   report_enc_cb: callback to send encoded audio data to the A2DP sink device. 
 * @param[in]   report_enc_arg: pointer to the address of A2DP sink device. 
 */
void app_audio_a2dp_source_start(audio_type_t audio_type, struct sbc_encoder_param *sbc_param, audio_a2dp_source_report_encoded_frame report_enc_cb, void *report_enc_arg);

/*
 * @fn          app_audio_a2dp_source_stop
 *
 * @brief       stop and destroy the audio scene of A2DP source device. 
 */
void app_audio_a2dp_source_stop(void);

/*
 * @fn          app_audio_tone_play
 *
 * @brief       Creat and play an audio tone.
 *
 * @param[in]   audio_type: type of the tone waiting to be played. @ref audio_type_t.
 * @param[in]   tone_sample: pointer to the sample data of an audio tone.
 * @param[in]   tone_len: the length of the sample data of the audio tone.
 */
void app_audio_tone_play(audio_type_t audio_type, uint8_t *tone_sample, uint32_t tone_len);

/*
 * @fn          app_audio_tone_stop
 *
 * @brief       stop and destroy the audio scene of tone playing, used when the sample data played completely or when the main scene be destroyed. 
 * 
 * @param[in]   immediate : stop the tone immediately or not.
 */
void app_audio_tone_stop(bool immediate);

/*
 * @fn          app_audio_local_playback_play
 *
 * @brief       play local audio.
 *
 * @param[in]   audio_type: audio type of the local audio to be played. @ref audio_type_t.
 * @param[in]   local_playback_sample: parameter to config the encodedr for getting the sbc data wanted to be transfered to sink.
 * @param[in]   report_enc_cb: callback to send encoded audio data to the A2DP sink device. 
 * @param[in]   report_enc_arg: pointer to the address of A2DP sink device. 
 */
void app_audio_local_playback_play(audio_type_t audio_type,  uint8_t *local_playback_sample, uint32_t local_playback_len);

/*
 * @fn          app_audio_local_playback_stop
 *
 * @brief       stop local play.
 */
void app_audio_local_playback_stop(void);

/*
 * @fn          app_audio_event_handler
 *
 * @brief       for internal use.
 *
 * @param[in]   event ptr: app task event. @ref struct app_task_event.
*/
void app_audio_event_handler(struct app_task_event *event);

#endif  // _APP_AUDIO_H
