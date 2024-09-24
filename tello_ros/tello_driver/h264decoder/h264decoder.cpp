extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/mem.h>
#include <libavutil/imgutils.h>  // Adicionar esta linha para usar av_image_fill_arrays
#include <libswscale/swscale.h>
}

#ifndef PIX_FMT_RGB24
#define PIX_FMT_RGB24 AV_PIX_FMT_RGB24
#endif

#include "h264decoder.hpp"
#include <utility>

typedef unsigned char ubyte;

/* Para compatibilidade com versões mais antigas da libav */
#if (LIBAVCODEC_VERSION_MAJOR <= 54)
#  define av_frame_alloc avcodec_alloc_frame
#  define av_frame_free  avcodec_free_frame
#endif

H264Decoder::H264Decoder()
{
  const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
  if (!codec)
    throw H264InitFailure("cannot find decoder");

  context = avcodec_alloc_context3(codec);
  if (!context)
    throw H264InitFailure("cannot allocate context");

  int err = avcodec_open2(context, codec, nullptr);
  if (err < 0)
    throw H264InitFailure("cannot open context");

  parser = av_parser_init(AV_CODEC_ID_H264);
  if (!parser)
    throw H264InitFailure("cannot init parser");

  frame = av_frame_alloc();
  if (!frame)
    throw H264InitFailure("cannot allocate frame");

  pkt = av_packet_alloc();
  if (!pkt)
    throw H264InitFailure("cannot allocate packet");
  av_init_packet(pkt);
}

H264Decoder::~H264Decoder()
{
  av_parser_close(parser);
  avcodec_close(context);
  av_free(context);
  av_frame_free(&frame);
  av_packet_free(&pkt);
}

ssize_t H264Decoder::parse(const ubyte* in_data, ssize_t in_size)
{
  auto nread = av_parser_parse2(parser, context, &pkt->data, &pkt->size,
    in_data, in_size,
    0, 0, AV_NOPTS_VALUE);
  return nread;
}

bool H264Decoder::is_frame_available() const
{
  return pkt->size > 0;
}

const AVFrame& H264Decoder::decode_frame()
{
  int ret;

  ret = avcodec_send_packet(context, pkt);
  if (ret < 0) {
    throw H264DecodeFailure("error sending packet for decoding\n");
  }

  ret = avcodec_receive_frame(context, frame);
  if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
    throw H264DecodeFailure("no frame available\n");
  } else if (ret < 0) {
    throw H264DecodeFailure("error during decoding\n");
  }

  return *frame;
}

ConverterRGB24::ConverterRGB24()
{
  framergb = av_frame_alloc();
  if (!framergb)
    throw H264DecodeFailure("cannot allocate frame");
  context = nullptr;
}

ConverterRGB24::~ConverterRGB24()
{
  sws_freeContext(context);
  av_frame_free(&framergb);
}

const AVFrame& ConverterRGB24::convert(const AVFrame &frame, ubyte* out_rgb)
{
  int w = frame.width;
  int h = frame.height;
  int pix_fmt = frame.format;

  context = sws_getCachedContext(context,
    w, h, (AVPixelFormat)pix_fmt,
    w, h, AV_PIX_FMT_BGR24, SWS_BILINEAR,
    nullptr, nullptr, nullptr);
  if (!context)
    throw H264DecodeFailure("cannot allocate context");

  av_image_fill_arrays(framergb->data, framergb->linesize, out_rgb, AV_PIX_FMT_BGR24, w, h, 1);

  sws_scale(context, frame.data, frame.linesize, 0, h,
    framergb->data, framergb->linesize);
  framergb->width = w;
  framergb->height = h;

  return *framergb;
}

/* Determinar o tamanho necessário do framebuffer */
int ConverterRGB24::predict_size(int w, int h)
{
  return av_image_fill_arrays(framergb->data, framergb->linesize, nullptr, AV_PIX_FMT_BGR24, w, h, 1);
}

std::pair<int, int> width_height(const AVFrame& f)
{
  return std::make_pair(f.width, f.height);
}

int row_size(const AVFrame& f)
{
  return f.linesize[0];
}

void disable_logging()
{
  av_log_set_level(AV_LOG_QUIET);
}
