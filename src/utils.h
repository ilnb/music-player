#ifndef _UTILS_H
#define _UTILS_H

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <stdio.h>
#include <stdlib.h>

#define SI static inline

#define ARR(name, size) name = calloc(size, sizeof *name);

#define freeArrs(...)                                                                              \
  __freeArrs((void *[]){__VA_ARGS__}, sizeof((void *[]){__VA_ARGS__}) / sizeof(void *));

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

SI int getImage(const char *input_file, const char *output_file) {
  AVFormatContext *fmt_ctx = NULL;
  if (avformat_open_input(&fmt_ctx, input_file, NULL, NULL) < 0) {
    fprintf(stderr, "Could not open input file: %s\n", input_file);
    return -1;
  }

  if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
    fprintf(stderr, "Could not find stream info\n");
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
    fprintf(stderr, "No video stream found\n");
    avformat_close_input(&fmt_ctx);
    return -1;
  }

  AVCodecParameters *codecpar = fmt_ctx->streams[video_stream_index]->codecpar;
  const AVCodec *codec = avcodec_find_decoder(codecpar->codec_id);
  if (!codec) {
    fprintf(stderr, "Unsupported codec\n");
    avformat_close_input(&fmt_ctx);
    return -1;
  }

  AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
  avcodec_parameters_to_context(codec_ctx, codecpar);
  if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
    fprintf(stderr, "Failed to open codec\n");
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&fmt_ctx);
    return -1;
  }

  AVPacket *packet = av_packet_alloc();
  AVFrame *frame = av_frame_alloc();
  AVFrame *rgb_frame = av_frame_alloc();

  int width = codec_ctx->width;
  int height = codec_ctx->height;

  int num_bytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, width, height, 1);
  uint8_t *buffer = (uint8_t *)av_malloc(num_bytes * sizeof(uint8_t));
  av_image_fill_arrays(rgb_frame->data, rgb_frame->linesize, buffer, AV_PIX_FMT_RGB24, width,
                       height, 1);

  struct SwsContext *sws_ctx = sws_getContext(width, height, codec_ctx->pix_fmt, width, height,
                                              AV_PIX_FMT_RGB24, SWS_BILINEAR, NULL, NULL, NULL);

  int got_frame = 0;
  while (av_read_frame(fmt_ctx, packet) >= 0) {
    if (packet->stream_index == video_stream_index) {
      if (avcodec_send_packet(codec_ctx, packet) == 0) {
        if (avcodec_receive_frame(codec_ctx, frame) == 0) {
          sws_scale(sws_ctx, (const uint8_t *const *)frame->data, frame->linesize, 0, height,
                    rgb_frame->data, rgb_frame->linesize);
          got_frame = 1;
          av_packet_unref(packet);
          break;
        }
      }
    }
    av_packet_unref(packet);
  }

  int ret = -1;
  if (got_frame) {
    const AVCodec *jpeg_codec = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
    AVCodecContext *jpeg_ctx = avcodec_alloc_context3(jpeg_codec);
    jpeg_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    jpeg_ctx->height = height;
    jpeg_ctx->width = width;
    jpeg_ctx->time_base = (AVRational){1, 25};
    jpeg_ctx->strict_std_compliance = FF_COMPLIANCE_UNOFFICIAL;

    if (avcodec_open2(jpeg_ctx, jpeg_codec, NULL) >= 0) {
      AVPacket *jpeg_packet = av_packet_alloc();
      AVFrame *jpeg_frame = av_frame_alloc();
      jpeg_frame->format = AV_PIX_FMT_YUV420P;
      jpeg_frame->width = width;
      jpeg_frame->height = height;
      av_image_alloc(jpeg_frame->data, jpeg_frame->linesize, width, height, AV_PIX_FMT_YUV420P, 1);

      struct SwsContext *rgb2yuv =
          sws_getContext(width, height, AV_PIX_FMT_RGB24, width, height, AV_PIX_FMT_YUV420P,
                         SWS_BILINEAR, NULL, NULL, NULL);
      sws_scale(rgb2yuv, (const uint8_t *const *)rgb_frame->data, rgb_frame->linesize, 0, height,
                jpeg_frame->data, jpeg_frame->linesize);

      if (avcodec_send_frame(jpeg_ctx, jpeg_frame) == 0 &&
          avcodec_receive_packet(jpeg_ctx, jpeg_packet) == 0) {
        FILE *f = fopen(output_file, "wb");
        fwrite(jpeg_packet->data, 1, jpeg_packet->size, f);
        fclose(f);
        ret = 0;
      }

      av_packet_free(&jpeg_packet);
      av_frame_free(&jpeg_frame);
      sws_freeContext(rgb2yuv);
    }
    avcodec_free_context(&jpeg_ctx);
  }

  av_packet_free(&packet);
  av_frame_free(&frame);
  av_frame_free(&rgb_frame);
  av_free(buffer);
  sws_freeContext(sws_ctx);
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

#endif
