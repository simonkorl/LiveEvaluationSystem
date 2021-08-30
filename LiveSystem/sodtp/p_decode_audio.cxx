#include "p_decode_audio.h"

/**
   Use codec_ctx and data in p_packet to decode an audio frame and writes it in
   audio_buf

   @param p_codec_ctx
   @param p_packet
   @param audio_buf: the output buffer of decoded audio data
   @param buf_size: the size of audio_buf
   @param aplayer: ptr to AudioPlayer, modify aplayer->s_audio_swr_ctx
 */
int audio_decode_frame(AVCodecContext *p_codec_ctx, AVPacket *p_packet, uint8_t *audio_buf, int buf_size, AudioPlayer* aplayer)
{
    AVFrame *p_frame = av_frame_alloc();

    int      frm_size   = 0;
    int      res        = 0;
    int      ret        = 0;
    int      nb_samples = 0;    // 重采样输出样本数
    uint8_t *p_cp_buf   = NULL;
    int      cp_len     = 0;
    bool     need_new   = false;

    res = 0;
    while (1)
    {
        need_new = false;

        // 1 接收解码器输出的数据，每次接收一个frame
        ret = avcodec_receive_frame(p_codec_ctx, p_frame);
        if (ret != 0)
        {
            if (ret == AVERROR_EOF)
            {
                printf("audio avcodec_receive_frame(): the decoder has been fully flushed\n");
                res = 0;
                goto exit;
            }
            else if (ret == AVERROR(EAGAIN))
            {
                //printf("audio avcodec_receive_frame(): output is not available in this state - "
                //       "user must try to send new input\n");
                need_new = true;
            }
            else if (ret == AVERROR(EINVAL))
            {
                printf("audio avcodec_receive_frame(): codec not opened, or it is an encoder\n");
                res = -1;
                goto exit;
            }
            else
            {
                printf("audio avcodec_receive_frame(): legitimate decoding errors\n");
                res = -1;
                goto exit;
            }
        }
        else
        {
            // s_audio_param_tgt是SDL可接受的音频帧数，是main()中取得的参数
            // 在main()函数中又有“s_audio_param_src  = s_audio_param_tgt”
            // 此处表示：如果frame中的音频参数      == s_audio_param_src == s_audio_param_tgt，那音频重采样的过程就免了(因此时s_audio_swr_ctx是NULL)
            // 　　　　　否则使用frame(源)和s_audio_param_src(目标)中的音频参数来设置s_audio_swr_ctx，并使用frame中的音频参数来赋值s_audio_param_src
            if (p_frame->format                     != aplayer->s_audio_param_src.fmt            ||
                p_frame->channel_layout             != aplayer->s_audio_param_src.channel_layout ||
                p_frame->sample_rate                != aplayer->s_audio_param_src.freq)
            {

                // 使用frame(源)和is->audio_tgt(目标)中的音频参数来设置is->swr_ctx
                aplayer->s_audio_swr_ctx      = swr_alloc_set_opts(NULL,
                                                     aplayer->s_audio_param_tgt.channel_layout,
                                                     aplayer->s_audio_param_tgt.fmt,
                                                     aplayer->s_audio_param_tgt.freq,
                                                     p_frame->channel_layout,
                                                     p_frame->format,
                                                     p_frame->sample_rate,
                                                     0,
                                                     NULL);
                if (aplayer->s_audio_swr_ctx == NULL || swr_init(aplayer->s_audio_swr_ctx) < 0)
                {
                    printf("Cannot create sample rate converter for conversion of %d Hz %s %d channels to %d Hz %s %d channels!\n",
                            p_frame->sample_rate, av_get_sample_fmt_name(p_frame->format), p_frame->channels,
                            aplayer->s_audio_param_tgt.freq, av_get_sample_fmt_name(aplayer->s_audio_param_tgt.fmt), aplayer->s_audio_param_tgt.channels);
                    swr_free(&(aplayer->s_audio_swr_ctx));
                    return -1;
                }

                // 使用frame中的参数更新s_audio_param_src，第一次更新后后面基本不用执行此if分支了，因为一个音频流中各frame通用参数一样
                aplayer->s_audio_param_src.channel_layout = p_frame->channel_layout;
                aplayer->s_audio_param_src.channels       = p_frame->channels;
                aplayer->s_audio_param_src.freq           = p_frame->sample_rate;
                aplayer->s_audio_param_src.fmt            = p_frame->format;
            }

            if (aplayer->s_audio_swr_ctx != NULL)        // 重采样
            {
                // 重采样输入参数1：输入音频样本数是p_frame->nb_samples
                // 重采样输入参数2：输入音频缓冲区
                const uint8_t **in        = (const uint8_t **)p_frame->extended_data;
                // 重采样输出参数1：输出音频缓冲区尺寸
                // 重采样输出参数2：输出音频缓冲区
                uint8_t       **out       = &(aplayer->s_resample_buf);
                // 重采样输出参数：输出音频样本数(多加了256个样本)
                int             out_count = (int64_t)p_frame->nb_samples * aplayer->s_audio_param_tgt.freq / p_frame->sample_rate + 256;
                // 重采样输出参数：输出音频缓冲区尺寸(以字节为单位)
                int out_size  = av_samples_get_buffer_size(NULL, aplayer->s_audio_param_tgt.channels, out_count, aplayer->s_audio_param_tgt.fmt, 0);
                if (out_size < 0)
                {
                    printf("av_samples_get_buffer_size() failed\n");
                    return -1;
                }

                if (aplayer->s_resample_buf == NULL)
                {
                  av_fast_malloc(&(aplayer->s_resample_buf), &(aplayer->s_resample_buf_len),
                                 out_size);
                }
                if (aplayer->s_resample_buf == NULL)
                {
                    return AVERROR(ENOMEM);
                }
                // 音频重采样：返回值是重采样后得到的音频数据中单个声道的样本数
                nb_samples = swr_convert(aplayer->s_audio_swr_ctx, out, out_count, in, p_frame->nb_samples);
                if (nb_samples < 0) {
                    printf("swr_convert() failed\n");
                    return -1;
                }
                if (nb_samples == out_count)
                {
                    printf("audio buffer is probably too small\n");
                    if (swr_init(aplayer->s_audio_swr_ctx) < 0)
                      swr_free(&(aplayer->s_audio_swr_ctx));
                }

                // 重采样返回的一帧音频数据大小(以字节为单位)
                p_cp_buf = aplayer->s_resample_buf;
                cp_len = nb_samples * aplayer->s_audio_param_tgt.channels * av_get_bytes_per_sample(aplayer->s_audio_param_tgt.fmt);
            }
            else                // 不重采样
            {
                // 根据相应音频参数，获得所需缓冲区大小
                frm_size = av_samples_get_buffer_size(
                        NULL,
                        p_codec_ctx->channels,
                        p_frame->nb_samples,
                        p_codec_ctx->sample_fmt,
                        1);

                printf("frame size %d, buffer size %d\n", frm_size, buf_size);
                assert(frm_size <= buf_size);

                p_cp_buf = p_frame->data[0];
                cp_len   = frm_size;
            }

            // 将音频帧拷贝到函数输出参数audio_buf
            memcpy(audio_buf, p_cp_buf, cp_len);

            res = cp_len;
            goto exit;
        }

        // 2 向解码器喂数据，每次喂一个packet
        if (need_new)
        {
            ret      = avcodec_send_packet(p_codec_ctx, p_packet);
            if (ret != 0)
            {
                printf("avcodec_send_packet() failed %d\n", ret);
                res = -1;
                goto exit;
            }
        }
    }

exit:
    av_frame_unref(p_frame);
    return res;
}

// 音频处理回调函数。读队列获取音频包，解码，播放
// 此函数被SDL按需调用，此函数不在用户主线程中，因此数据需要保护
// \param[in]  userdata用户在注册回调函数时指定的参数
// \param[out] stream 音频数据缓冲区地址，将解码后的音频数据填入此缓冲区
// \param[out] len    音频数据缓冲区大小，单位字节
// 回调函数返回后，stream指向的音频缓冲区将变为无效
// 双声道采样点的顺序为LRLRLR
void sdl_audio_callback(void *userdata, uint8_t *stream, int len)
{

  AudioPlayer* aplayer        = (AudioPlayer *)userdata;
  AVCodecContext *p_codec_ctx = aplayer->p_codec_ctx;
  int copy_len;           //
  int get_size;           // 获取到解码后的音频数据大小

  static uint8_t s_audio_buf[(MAX_AUDIO_FRAME_SIZE*3)/2]; // 1.5倍声音帧的大小
  static uint32_t s_audio_len = 0;    // 新取得的音频数据大小
  static uint32_t s_tx_idx = 0;       // 已发送给设备的数据量


  AVPacket *p_packet;

  int frm_size = 0;
  int ret_size = 0;
  int ret;

  while (len > 0) { // 确保stream缓冲区填满，填满后此函数返回
    if (aplayer->s_adecode_finished) {
      SDL_PauseAudio(1);
      printf("pause audio callback\n");
      return;
    }

    if (s_tx_idx >=
        s_audio_len) { // audio_buf缓冲区中数据已全部取出，则从队列中获取更多数据

      p_packet = (AVPacket *)av_malloc(sizeof(AVPacket));

      // 1. 从队列中读出一包音频数据
      if (packet_queue_pop(&(aplayer->s_audio_pkt_queue), p_packet, 1) == 0) {
        printf("audio packet buffer empty...\n");
        continue;
      }

      // 2. 解码音频包
      get_size = audio_decode_frame(p_codec_ctx, p_packet, s_audio_buf,
                                    sizeof(s_audio_buf), aplayer);
      if (get_size < 0) {
        // 出错输出一段静音
        s_audio_len = 1024; // arbitrary?
        memset(s_audio_buf, 0, s_audio_len);
        av_packet_unref(p_packet);
      } else if (get_size == 0) // 解码缓冲区被冲洗，整个解码过程完毕
      {
        aplayer->s_adecode_finished = true;
      } else {
        s_audio_len = get_size;
        av_packet_unref(p_packet);
      }
      s_tx_idx = 0;

      if (p_packet->data != NULL) {
        // av_packet_unref(p_packet);
      }
    }

    copy_len = s_audio_len - s_tx_idx;
    if (copy_len > len) {
      copy_len = len;
    }

    // 将解码后的音频帧(s_audio_buf+)写入音频设备缓冲区(stream)，播放
    memcpy(stream, (uint8_t *)s_audio_buf + s_tx_idx, copy_len);
    len -= copy_len;
    stream += copy_len;
    s_tx_idx += copy_len;
  }
}

void read_data_from_jitter(EV_P_ ev_timer *w, int revents) {
    // Print2File("========worker_cb4===============");//这里跑的
    // pJitter_Pop 少于 pJitter_Push
  Decoder *decoder = (Decoder *)w->data;
  int ret = SodtpReadPacket(decoder->pJitter, decoder->pPacket, decoder->pBlock);
    if (decoder->pJitter->state == SodtpJitter::STATE_CLOSE) {
        // timeFramePlayer.evalTimeStamp("pJitter_Pop_CLOSE","p",std::to_string(++pJitter_Pop_Count));
        // Stream is closed.
        // Thread will be closed by breaking event loop.
        ev_timer_stop(loop, w);
        fprintf(stderr, "Stream %d is closed!\n", decoder->iStream);
        return;
    }
    if (ret == SodtpJitter::STATE_NORMAL) {
        // timeFramePlayer.evalTimeStamp("pJitter_Pop","p",std::to_string(decoder->pBlock->block_id));
        // 状态正常才记录
        if(decoder->pBlock->key_block){
            timeFramePlayer.evalTimeStamp("pJitter_Pop","Audio_I_frame",std::to_string(decoder->pBlock->block_id),std::to_string(decoder->pBlock->size));
            // timeFramePlayer.evalTimeStamp("FrameType_p","p","I_frame");
        }else{
            timeFramePlayer.evalTimeStamp("pJitter_Pop","Audio_P_frame",std::to_string(decoder->pBlock->block_id),std::to_string(decoder->pBlock->size));
            // timeFramePlayer.evalTimeStamp("FrameType_p","p","P_frame");
        }
        // Receive one more block.
        decoder->iBlock++;
        // Print2File("ret == SodtpJitter::STATE_NORMAL");//这里跑的
        // printf("decoding: stream %d,\t block %d,\t size %d,\t received block count %d\n",
        //     decoder->pBlock->stream_id, decoder->pBlock->block_id,
        //     decoder->pPacket->size, decoder->iBlock);
        printf("decoding: stream %d,\t block %d,\t size %d,\t delay %d\n",
            decoder->pBlock->stream_id, decoder->pBlock->block_id,
            decoder->pPacket->size, (int)(current_mtime() - decoder->pBlock->block_ts));


        if(decoder->aplayer) {
          packet_queue_push(&(decoder->aplayer->s_audio_pkt_queue), decoder->pPacket);
        }
        // Print2File("DecodePacketPlay(decoder);");//这里断了！！！！！！！！！！！！！
    }
    else if (ret == SodtpJitter::STATE_BUFFERING) {
        // 这log会产生bug
        // timeFramePlayer.evalTimeStamp("pJitter_Pop_BUFFERING","p",std::to_string(decoder->pBlock->block_id));
        printf("decoding: buffering stream %d\n", decoder->iStream);
    }
    else if (ret == SodtpJitter::STATE_SKIP) {
        // 这log会产生bug
        // timeFramePlayer.evalTimeStamp("pJitter_Pop_SKIP","p",std::to_string(decoder->pBlock->block_id));
        printf("decoding: skip one block of stream %d\n", decoder->iStream);
    }
    else {
        // 这log会产生bug
        // timeFramePlayer.evalTimeStamp("pJitter_Pop_UNKNOWN","p",std::to_string(decoder->pBlock->block_id));
        printf("decoding: warning! unknown state of stream %d!\n", decoder->iStream);
    }
    // Free the packet that was allocated by av_read_frame
    // av_free_packet(&packet);


    // normal or skip : 40ms
    // buffering: 200ms;
    // printf("debug: %s:%s:%d\n", __FILE__, __FUNCTION__, __LINE__);
}

int open_audio_stream(CodecParWithoutExtraData *myCodecPar,
                      AudioPlayer *aplayer) {
  // assert(aplayer->p_codec == NULL);

  AVCodecContext *p_codec_ctx = aplayer->p_codec_ctx;
  SDL_AudioSpec wanted_spec;
  SDL_AudioSpec actual_spec;
  int ret;

  // 1. Prepare the decoder
  // 1.1 Read codec params from the block
  int ret2 = lhs_copy_parameters_to_context(aplayer->p_codec_ctx, myCodecPar);
  if (ret2 < 0) {
    Print2File("avcodec_parameters_to_context():" + std::to_string(ret2));
  }
  // Print2File("pVCodec = avcodec_find_decoder(pVCodecCtx->codec_id);");
  // 1.2 Find the decoder
  aplayer->p_codec = avcodec_find_decoder(aplayer->p_codec_ctx->codec_id);
  if (!aplayer->p_codec) {
    Print2File("Codec not found");
    fprintf(stderr, "Codec not found\n");
    return -1;
  }

  // Print2File("if (avcodec_open2(pVCodecCtx, pVCodec, NULL) < 0) {");
  // 1.3 Open Codec
  if (avcodec_open2(aplayer->p_codec_ctx, aplayer->p_codec, NULL) < 0) {
    Print2File("Fail to open codec!");
    fprintf(stderr, "Fail to open codec!\n");
    return -1;
  }

  // 2. 打开音频设备并创建音频处理线程
  // 2.1 打开音频设备，获取SDL设备支持的音频参数actual_spec(期望的参数是wanted_spec，实际得到actual_spec)
  // 1) SDL提供两种使音频设备取得音频数据方法：
  //    a. push，SDL以特定的频率调用回调函数，在回调函数中取得音频数据
  //    b. pull，用户程序以特定的频率调用SDL_QueueAudio()，向音频设备提供数据。此种情况wanted_spec.callback=NULL
  // 2) 音频设备打开后播放静音，不启动回调，调用SDL_PauseAudio(0)后启动回调，开始正常播放音频
  wanted_spec.freq = p_codec_ctx->sample_rate;    // 采样率
  wanted_spec.format = AUDIO_S16SYS;              // S表带符号，16是采样深度，SYS表采用系统字节序
  wanted_spec.channels = p_codec_ctx->channels;   // 声音通道数
  wanted_spec.silence = 0;                        // 静音值
  wanted_spec.samples = SDL_AUDIO_BUFFER_SIZE;    // SDL声音缓冲区尺寸，单位是单声道采样点尺寸x通道数
  wanted_spec.callback = sdl_audio_callback;      // 回调函数，若为NULL，则应使用SDL_QueueAudio()机制
  wanted_spec.userdata = aplayer;             // 提供给回调函数的参数
  if (SDL_OpenAudio(&wanted_spec, &actual_spec) < 0)
    {
      printf("SDL_OpenAudio() failed: %s\n", SDL_GetError());
      return -1;
    }

  // 2.2 根据SDL音频参数构建音频重采样参数
  // wanted_spec是期望的参数，actual_spec是实际的参数，wanted_spec和auctual_spec都是SDL中的参数。
  // 此处audio_param是FFmpeg中的参数，此参数应保证是SDL播放支持的参数，后面重采样要用到此参数
  // 音频帧解码后得到的frame中的音频格式未必被SDL支持，比如frame可能是planar格式，但SDL2.0并不支持planar格式，
  // 若将解码后的frame直接送入SDL音频缓冲区，声音将无法正常播放。所以需要先将frame重采样(转换格式)为SDL支持的模式，
  // 然后送再写入SDL音频缓冲区
  aplayer->s_audio_param_tgt.fmt = AV_SAMPLE_FMT_S16;
  aplayer->s_audio_param_tgt.freq = actual_spec.freq;
  aplayer->s_audio_param_tgt.channel_layout = av_get_default_channel_layout(actual_spec.channels);;
  aplayer->s_audio_param_tgt.channels =  actual_spec.channels;
  aplayer->s_audio_param_tgt.frame_size = av_samples_get_buffer_size(NULL, actual_spec.channels, 1, aplayer->s_audio_param_tgt.fmt, 1);
  aplayer->s_audio_param_tgt.bytes_per_sec = av_samples_get_buffer_size(NULL, actual_spec.channels, actual_spec.freq, aplayer->s_audio_param_tgt.fmt, 1);
  if (aplayer->s_audio_param_tgt.bytes_per_sec <= 0 || aplayer->s_audio_param_tgt.frame_size <= 0)
    {
      printf("av_samples_get_buffer_size failed\n");
      return -1;
    }
  aplayer->s_audio_param_src = aplayer->s_audio_param_tgt;

  // 3. 暂停/继续音频回调处理。参数1表暂停，0表继续。
  //     打开音频设备后默认未启动回调处理，通过调用SDL_PauseAudio(0)来启动回调处理。
  //     这样就可以在打开音频设备后先为回调函数安全初始化数据，一切就绪后再启动音频回调。
  //     在暂停期间，会将静音值往音频设备写。
  SDL_PauseAudio(0);

  return 0;
}

// decode and display audio
int audio_viewer(SodtpJitterPtr pJitter, AudioPlayer* aplayer) {
  // Initalizing these to NULL prevents segfaults!
  AVFormatContext* p_fmt_ctx = NULL;
  AVPacket packet;

  SodtpBlockPtr pBlock = NULL;

  CodecParWithoutExtraData *myCodecPar = NULL;
  int i = 0;
  int ret = 0;
  int res = 0;

  int WAITING_UTIME = 20000;
  int WAITING_ROUND = 500;
  int SKIPPING_ROUND = 70;
  // 0. Initialization
  av_init_packet(&packet);

  aplayer->p_codec_ctx = avcodec_alloc_context3(NULL);
  if (!aplayer->p_codec_ctx) {
    fprintf(stderr, "Could not allocate video codec context\n");
    return -1;
  }
  aplayer->p_codec_ctx->thread_count = 1;

  // Search for stream info: Read a block from jitter or wait until getting a block
  while (true) {
    ret = pJitter->front(pBlock);

    if ((ret == SodtpJitter::STATE_NORMAL) && pBlock->key_block) {
      fprintf(stdout, "sniffing audio: stream %d,\t block %d,\t size %d\n",
              pBlock->stream_id, pBlock->block_id, pBlock->size);

      // Print2File("==========================改的接口==========================");
      // pVFormatCtx = sniff_format2(pBlock->data, pBlock->size); //原本！！！！
      // TODO: currently force recognise audio stream as stream_id
      if(pBlock->stream_id == 1 && pBlock->haveFormatContext){
        // Print2File("myCodecPar = pBlock->codecParPtr;");
        // myCodecPar 除了extraData的数据
        myCodecPar = pBlock->codecParPtr;
                // Print2File("*myCodecPar->extradata = *pBlock->codecParExtradata");
        myCodecPar->extradata = pBlock->codecParExtradata;
        // Print2File("*myCodecPar->extradata = *pBlock->codecParExtradata break");
        break;
      }else{
        Print2File("pBlock->haveFormatContext false : continue");
        continue;
            }
      break;
    }
    // 弱网发生
    // Print2File("decoding: waiting for the key block of stream %u. sleep round");
    fprintf(stderr, "decoding: waiting for the key block of stream %u. sleep round %d!\n", pJitter->stream_id, i);

    if (++i > WAITING_ROUND) {
      // 弱网发生
      // Print2File("decoding: fail to read the key block of stream");
      fprintf(stderr, "decoding: fail to read the key block of stream %u.\n", pJitter->stream_id);
      break;
    }
    usleep(WAITING_UTIME);
  }
  timeMainPlayer.evalTime("p","audio viewer_while_pass");

  open_audio_stream(myCodecPar, aplayer);

  Decoder decoder; // owner: p_decode_audio.h::audio_viewer()
  decoder.pVCodecCtx  = aplayer->p_codec_ctx;
  decoder.pSwsCtx     = NULL;
  decoder.pFrame      = NULL;
  decoder.pFrameRGB   = NULL;
  decoder.pPacket     = &packet;
  decoder.pJitter     = pJitter.get();
  decoder.pBlock      = NULL;
  decoder.iStream     = pJitter->stream_id;
  decoder.iFrame      = 0;
  decoder.iBlock      = 0;
  decoder.path        = NULL;
  decoder.aplayer     = aplayer;

  ev_timer worker;
  struct ev_loop *loop = ev_loop_new(EVFLAG_AUTO);

  ev_timer_init(&worker, read_data_from_jitter, 0, 0);
  ev_timer_start(loop, &worker);
  worker.data = &decoder;

  ev_loop(loop, 0);

  // Close the codecs
  avcodec_close(aplayer->p_codec_ctx);

  // Notification for clearing the jitter.
  sem_post(pJitter->_sem);
  ev_feed_signal(SIGUSR1);

  return 0;
}
