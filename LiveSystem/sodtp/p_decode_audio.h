//! 1. When it receives the first packet with stream info, it setup the decoder and start the decoing thread
//! 2. It receives data from a jitter buffer and then feed the decoder
#ifndef P_DECODE_AUDIO_H
#define P_DECODE_AUDIO_H

#include <assert.h>
#include <cstdint>
#include <cstdio>

extern "C"{
#include <SDL2/SDL.h>
#include <SDL2/SDL_mutex.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include <ev.h>

#include "bounded_buffer.h"
#include "p_sodtp_jitter.h"
#include "../util/util_log.h"
#include "./p_decode_video.h"
#include <thread>

#define SDL_USEREVENT_REFRESH (SDL_USEREVENT + 1)

#define SDL_AUDIO_BUFFER_SIZE 1024
#define MAX_AUDIO_FRAME_SIZE  192000

typedef struct packet_queue_t {
  AVPacketList *first_pkt = NULL;
  AVPacketList *last_pkt = NULL;
  int           nb_packets;     // 队列中AVPacket的个数
  int           size;           // 队列中AVPacket总的大小(字节数)
  SDL_mutex    *mutex = NULL;
  SDL_cond     *cond = NULL;
} packet_queue_t;

void packet_queue_init(packet_queue_t *q)
{
    memset(q, 0, sizeof(packet_queue_t));
    q->mutex = SDL_CreateMutex();
    q->cond  = SDL_CreateCond();
}

int packet_queue_num(packet_queue_t *q)
{
    return q->nb_packets;
}

// 写队列尾部。pkt是一包还未解码的音视频数据
int packet_queue_push(packet_queue_t *q, AVPacket *pkt)
{
    AVPacketList *pkt_list;

    if ((pkt != NULL) && (pkt->data != NULL) && (av_packet_make_refcounted(pkt) < 0))
    {
        return -1;
    }

    pkt_list = av_malloc(sizeof(AVPacketList));
    if (!pkt_list)
    {
        return -1;
    }

    pkt_list->pkt  = *pkt;
    pkt_list->next = NULL;

    SDL_LockMutex(q->mutex);

    if (!q->last_pkt)           // 队列为空
    {
        q->first_pkt = pkt_list;
    }
    else
    {
        q->last_pkt->next = pkt_list;
    }
    q->last_pkt  = pkt_list;
    q->nb_packets++;
    q->size     += pkt_list->pkt.size;
    // 发个条件变量的信号：重启等待q->cond条件变量的一个线程
    SDL_CondSignal(q->cond);

    SDL_UnlockMutex(q->mutex);
    return 0;
}

// 读队列头部。
int packet_queue_pop(packet_queue_t *q, AVPacket *pkt, int block) {
  AVPacketList                                 *p_pkt_node;
  int ret;
  SDL_LockMutex(q->mutex);

  while (1)
      {
      p_pkt_node = q->first_pkt;
        if (p_pkt_node)         // 队列非空，取一个出来
        {
            q->first_pkt = p_pkt_node->next;
            if (!q->first_pkt)
            {
                q->last_pkt = NULL;
            }
            q->nb_packets--;
            q->size -= p_pkt_node->pkt.size;
            *pkt     = p_pkt_node->pkt;
            av_free(p_pkt_node);
            ret      = 1;
            break;
        }
        else if (!block)        // 队列空且阻塞标志无效，则立即退出
        {
            ret = 0;
            break;
        }
        else                    // 队列空且阻塞标志有效，则等待
        {
          SDL_CondWait(q->cond, q->mutex);
        }
    }
    SDL_UnlockMutex(q->mutex);
    return ret;
}

// 读队列头部。
int packet_queue_front(packet_queue_t *q, AVPacket *pkt, bool block) {
  AVPacketList                        *p_pkt_node;
  int                                  ret = 0;

  SDL_LockMutex(q->mutex);

  p_pkt_node = q->first_pkt;

  while(true) {
    if (p_pkt_node) {
      *pkt = p_pkt_node->pkt;
      return 1;
    } else if(!block){
      return 0;
    } else {
      SDL_CondWait(q->cond, q->mutex);
    }
  }

  SDL_UnlockMutex(q->mutex);
  return ret;
}

typedef struct AudioParams {
  int                 freq;
  int                 channels;
  int64_t             channel_layout;
  enum AVSampleFormat fmt;
  int                 frame_size;
  int                 bytes_per_sec;
} FF_AudioParams;

class AudioPlayer {
public:
  AudioPlayer() {
    packet_queue_init(&(this->s_audio_pkt_queue));
  }
  AVCodec *p_codec            = NULL;
  AVCodecContext *p_codec_ctx = NULL;

  FF_AudioParams  s_audio_param_src;
  FF_AudioParams  s_audio_param_tgt;
  SwrContext     *s_audio_swr_ctx = NULL;
  uint8_t        *s_resample_buf  = NULL; // 重采样输出缓冲区
  uint32_t        s_resample_buf_len = 0;

  bool s_adecode_finished = false;

  packet_queue_t s_audio_pkt_queue;
};

int audio_decode_frame(AVCodecContext *p_codec_ctx, AVPacket *p_packet,
                       uint8_t *audio_buf, int buf_size, AudioPlayer *aplayer);

void sdl_audio_callback(void *userdata, uint8_t *stream, int len);

void read_data_from_jitter(Decoder* decoder);

int open_audio_stream(CodecParWithoutExtraData *myCodecPar,
                      AudioPlayer *aplayer);

int audio_viewer(SodtpJitterPtr pJitter, AudioPlayer *aplayer);

#endif // P_DECODE_AUDIO_H
