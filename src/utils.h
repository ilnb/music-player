#ifndef _UTILS_H
#define _UTILS_H

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>

#define SI static inline

#define ARR(name, size) name = calloc(size, sizeof *name);

#define freeArrs(...) __freeArrs((void *[]){__VA_ARGS__}, sizeof((void *[]){__VA_ARGS__}) / sizeof(void *));

SI void __freeArrs(void *ptrs[], int count) {
  for (int i = 0; i < count; ++i)
    free(ptrs[i]);
}

SI float getAudioDuration(const char *file_path) {
  AVFormatContext *fmt_ctx = NULL;

  if (avformat_open_input(&fmt_ctx, file_path, NULL, NULL) < 0)
    return -1.0f;

  if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
    avformat_close_input(&fmt_ctx);
    return -1.0f;
  }

  float duration = (float)fmt_ctx->duration / AV_TIME_BASE;

  avformat_close_input(&fmt_ctx);
  return duration;
}

SI int getImage(const char *in_file, const char *out_file) {
  AVFormatContext *fmt_ctx = NULL;
  if (avformat_open_input(&fmt_ctx, in_file, NULL, NULL) < 0) {
    return -1;
  }

  if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
    avformat_close_input(&fmt_ctx);
    return -1;
  }

  int video_stream_index = -1;
  for (unsigned i = 0; i < fmt_ctx->nb_streams; i++) {
    if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
      video_stream_index = i;
      break;
    }
  }

  if (video_stream_index == -1) {
    avformat_close_input(&fmt_ctx);
    return -1;
  }

  AVCodecParameters *codecpar = fmt_ctx->streams[video_stream_index]->codecpar;
  const AVCodec *codec = avcodec_find_decoder(codecpar->codec_id);
  AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
  avcodec_parameters_to_context(codec_ctx, codecpar);
  avcodec_open2(codec_ctx, codec, NULL);

  AVPacket *packet = av_packet_alloc();
  AVFrame *frame = av_frame_alloc();
  int ret = -1;

  while (av_read_frame(fmt_ctx, packet) >= 0) {
    if (packet->stream_index == video_stream_index) {
      if (avcodec_send_packet(codec_ctx, packet) >= 0) {
        if (avcodec_receive_frame(codec_ctx, frame) >= 0) {
          struct SwsContext *sws_ctx =
              sws_getContext(frame->width, frame->height, codec_ctx->pix_fmt, frame->width, frame->height, AV_PIX_FMT_RGBA, SWS_BILINEAR, NULL, NULL, NULL);

          uint8_t *rgb_data = malloc(frame->width * frame->height * 4);
          uint8_t *dest[4] = {rgb_data, NULL, NULL, NULL};
          int dest_linesize[4] = {frame->width * 4, 0, 0, 0};
          sws_scale(sws_ctx, (const uint8_t *const *)frame->data, frame->linesize, 0, frame->height, dest, dest_linesize);
          sws_freeContext(sws_ctx);

          Image img = {.data = rgb_data, .width = frame->width, .height = frame->height, .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, .mipmaps = 1};
          ExportImage(img, out_file);
          free(rgb_data);
          ret = 0;
          av_packet_unref(packet);
          break;
        }
      }
    }
    av_packet_unref(packet);
  }

  av_frame_free(&frame);
  av_packet_free(&packet);
  avcodec_free_context(&codec_ctx);
  avformat_close_input(&fmt_ctx);
  return ret;
}

typedef struct {
  int h, m, s;
} Time;

SI Time getTime(float _t) {
  Time t;
  t.s = (int)_t % 60;
  _t /= 60;
  t.m = (int)_t % 60;
  _t /= 60;
  t.h = _t;
  return t;
}

SI float getSec(Time t) { return t.h * 3600 + t.m * 60 + t.s; }

SI char *getFilename(char *path) {
  char *s = path + strlen(path);
  while (*s != '/' && s != path)
    --s;
  return *s == '/' ? s + 1 : s;
}

SI int init_fifo(AVAudioFifo **fifo, AVCodecContext *out_codec_ctx) {
  if (!(*fifo = av_audio_fifo_alloc(out_codec_ctx->sample_fmt, out_codec_ctx->ch_layout.nb_channels, 1))) {
    fprintf(stderr, "Could not allocate FIFO\n");
    return -1;
  }
  return 0;
}

SI int add_samples_to_fifo(AVAudioFifo *fifo, uint8_t **converted_input_samples, const int frame_size) {
  if (av_audio_fifo_realloc(fifo, av_audio_fifo_size(fifo) + frame_size) < 0)
    return -1;
  if (av_audio_fifo_write(fifo, (void **)converted_input_samples, frame_size) < frame_size)
    return -1;
  return 0;
}

SI int encode_audio_frame(AVFrame *frame, AVFormatContext *out_format_ctx, AVCodecContext *out_codec_ctx) {
  AVPacket *out_pkt = av_packet_alloc();
  int ret;
  if ((ret = avcodec_send_frame(out_codec_ctx, frame)) < 0) {
    av_packet_free(&out_pkt);
    return ret;
  }
  while (ret >= 0) {
    ret = avcodec_receive_packet(out_codec_ctx, out_pkt);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      av_packet_free(&out_pkt);
      return 0;
    } else if (ret < 0) {
      av_packet_free(&out_pkt);
      return ret;
    }
    if ((ret = av_write_frame(out_format_ctx, out_pkt)) < 0) {
      av_packet_free(&out_pkt);
      return ret;
    }
    av_packet_unref(out_pkt);
  }
  av_packet_free(&out_pkt);
  return 0;
}

SI int transcodeToMp3(const char *input_file, const char *output_file) {
  AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
  AVCodecContext *dec_ctx = NULL, *enc_ctx = NULL;
  SwrContext *swr_ctx = NULL;
  AVAudioFifo *fifo = NULL;
  int ret = -1;
  int audio_stream_idx = -1;
  AVPacket *in_pkt = NULL;
  AVFrame *in_frame = NULL;

  if (avformat_open_input(&ifmt_ctx, input_file, NULL, NULL) < 0)
    goto cleanup;
  if (avformat_find_stream_info(ifmt_ctx, NULL) < 0)
    goto cleanup;

  for (unsigned i = 0; i < ifmt_ctx->nb_streams; i++) {
    if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
      audio_stream_idx = i;
      break;
    }
  }
  if (audio_stream_idx == -1)
    goto cleanup;

  const AVCodec *decoder = avcodec_find_decoder(ifmt_ctx->streams[audio_stream_idx]->codecpar->codec_id);
  if (!decoder)
    goto cleanup;
  dec_ctx = avcodec_alloc_context3(decoder);
  if (!dec_ctx)
    goto cleanup;
  if (avcodec_parameters_to_context(dec_ctx, ifmt_ctx->streams[audio_stream_idx]->codecpar) < 0)
    goto cleanup;
  if (avcodec_open2(dec_ctx, decoder, NULL) < 0)
    goto cleanup;

  if (avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, output_file) < 0)
    goto cleanup;
  const AVCodec *encoder = avcodec_find_encoder(AV_CODEC_ID_MP3);
  if (!encoder)
    goto cleanup;
  enc_ctx = avcodec_alloc_context3(encoder);
  if (!enc_ctx)
    goto cleanup;

  enc_ctx->sample_rate = 48000;
  av_channel_layout_default(&enc_ctx->ch_layout, 2);
  enc_ctx->sample_fmt = AV_SAMPLE_FMT_FLTP;
  enc_ctx->bit_rate = 192000;
  enc_ctx->time_base = (AVRational){1, enc_ctx->sample_rate};

  AVStream *out_stream = avformat_new_stream(ofmt_ctx, NULL);
  if (!out_stream)
    goto cleanup;
  if (avcodec_open2(enc_ctx, encoder, NULL) < 0)
    goto cleanup;
  if (avcodec_parameters_from_context(out_stream->codecpar, enc_ctx) < 0)
    goto cleanup;
  out_stream->time_base = enc_ctx->time_base;

  if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
    if (avio_open(&ofmt_ctx->pb, output_file, AVIO_FLAG_WRITE) < 0)
      goto cleanup;
  }
  if (avformat_write_header(ofmt_ctx, NULL) < 0)
    goto cleanup;

  if (swr_alloc_set_opts2(&swr_ctx, &enc_ctx->ch_layout, enc_ctx->sample_fmt, enc_ctx->sample_rate, &dec_ctx->ch_layout, dec_ctx->sample_fmt, dec_ctx->sample_rate, 0, NULL) < 0)
    goto cleanup;
  if (swr_init(swr_ctx) < 0)
    goto cleanup;
  if (init_fifo(&fifo, enc_ctx) < 0)
    goto cleanup;

  in_pkt = av_packet_alloc();
  in_frame = av_frame_alloc();
  if (!in_pkt || !in_frame)
    goto cleanup;

  int64_t pts = 0;

  while (av_read_frame(ifmt_ctx, in_pkt) >= 0) {
    if (in_pkt->stream_index == audio_stream_idx) {
      if (avcodec_send_packet(dec_ctx, in_pkt) >= 0) {
        while (avcodec_receive_frame(dec_ctx, in_frame) >= 0) {
          uint8_t **converted_samples = NULL;
          if (av_samples_alloc_array_and_samples(&converted_samples, NULL, enc_ctx->ch_layout.nb_channels, in_frame->nb_samples, enc_ctx->sample_fmt, 0) < 0)
            break;
          swr_convert(swr_ctx, converted_samples, in_frame->nb_samples, (const uint8_t **)in_frame->extended_data, in_frame->nb_samples);
          add_samples_to_fifo(fifo, converted_samples, in_frame->nb_samples);
          av_freep(&converted_samples[0]);
          free(converted_samples);

          while (av_audio_fifo_size(fifo) >= enc_ctx->frame_size) {
            AVFrame *out_frame = av_frame_alloc();
            out_frame->nb_samples = enc_ctx->frame_size;
            av_channel_layout_copy(&out_frame->ch_layout, &enc_ctx->ch_layout);
            out_frame->format = enc_ctx->sample_fmt;
            out_frame->sample_rate = enc_ctx->sample_rate;
            av_frame_get_buffer(out_frame, 0);
            av_audio_fifo_read(fifo, (void **)out_frame->data, enc_ctx->frame_size);
            out_frame->pts = pts;
            pts += out_frame->nb_samples;
            encode_audio_frame(out_frame, ofmt_ctx, enc_ctx);
            av_frame_free(&out_frame);
          }
        }
      }
    }
    av_packet_unref(in_pkt);
  }

  // Flush decoder
  avcodec_send_packet(dec_ctx, NULL);
  while (avcodec_receive_frame(dec_ctx, in_frame) >= 0) {
    uint8_t **converted_samples = NULL;
    if (av_samples_alloc_array_and_samples(&converted_samples, NULL, enc_ctx->ch_layout.nb_channels, in_frame->nb_samples, enc_ctx->sample_fmt, 0) < 0)
      break;
    swr_convert(swr_ctx, converted_samples, in_frame->nb_samples, (const uint8_t **)in_frame->extended_data, in_frame->nb_samples);
    add_samples_to_fifo(fifo, converted_samples, in_frame->nb_samples);
    av_freep(&converted_samples[0]);
    free(converted_samples);
    while (av_audio_fifo_size(fifo) >= enc_ctx->frame_size) {
      AVFrame *out_frame = av_frame_alloc();
      out_frame->nb_samples = enc_ctx->frame_size;
      av_channel_layout_copy(&out_frame->ch_layout, &enc_ctx->ch_layout);
      out_frame->format = enc_ctx->sample_fmt;
      out_frame->sample_rate = enc_ctx->sample_rate;
      av_frame_get_buffer(out_frame, 0);
      av_audio_fifo_read(fifo, (void **)out_frame->data, enc_ctx->frame_size);
      out_frame->pts = pts;
      pts += out_frame->nb_samples;
      encode_audio_frame(out_frame, ofmt_ctx, enc_ctx);
      av_frame_free(&out_frame);
    }
  }

  // Flush swr_ctx
  int out_samples = swr_get_delay(swr_ctx, enc_ctx->sample_rate) + 3;
  if (out_samples > 0) {
    uint8_t **converted_samples = NULL;
    if (av_samples_alloc_array_and_samples(&converted_samples, NULL, enc_ctx->ch_layout.nb_channels, out_samples, enc_ctx->sample_fmt, 0) >= 0) {
      int ret = swr_convert(swr_ctx, converted_samples, out_samples, NULL, 0);
      if (ret > 0)
        add_samples_to_fifo(fifo, converted_samples, ret);
      av_freep(&converted_samples[0]);
      free(converted_samples);
    }
  }

  // Flush remaining FIFO
  while (av_audio_fifo_size(fifo) > 0) {
    int frame_size = FFMIN(av_audio_fifo_size(fifo), enc_ctx->frame_size);
    AVFrame *out_frame = av_frame_alloc();
    out_frame->nb_samples = frame_size;
    av_channel_layout_copy(&out_frame->ch_layout, &enc_ctx->ch_layout);
    out_frame->format = enc_ctx->sample_fmt;
    out_frame->sample_rate = enc_ctx->sample_rate;
    av_frame_get_buffer(out_frame, 0);
    av_audio_fifo_read(fifo, (void **)out_frame->data, frame_size);
    out_frame->pts = pts;
    pts += out_frame->nb_samples;
    encode_audio_frame(out_frame, ofmt_ctx, enc_ctx);
    av_frame_free(&out_frame);
  }

  encode_audio_frame(NULL, ofmt_ctx, enc_ctx);
  av_write_trailer(ofmt_ctx);
  ret = 0;

cleanup:
  if (in_pkt)
    av_packet_free(&in_pkt);
  if (in_frame)
    av_frame_free(&in_frame);
  if (fifo)
    av_audio_fifo_free(fifo);
  if (swr_ctx)
    swr_free(&swr_ctx);
  if (enc_ctx)
    avcodec_free_context(&enc_ctx);
  if (dec_ctx)
    avcodec_free_context(&dec_ctx);
  if (ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
    avio_closep(&ofmt_ctx->pb);
  }
  if (ofmt_ctx)
    avformat_free_context(ofmt_ctx);
  if (ifmt_ctx)
    avformat_close_input(&ifmt_ctx);
  return ret;
}

#endif
