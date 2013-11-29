#ifndef MSDK_ENCODE_MUXER_H
#define MSDK_ENCODE_MUXER_H
#include <libavformat/avformat.h>
#include "mfxdefs.h"
#include "mfxvideo.h"
#include "msdk_encode.h"
typedef struct msdk_encode_muxer_context_t{
	AVFormatContext * m_av_ctx;
	AVOutputFormat * m_av_fmt;
	AVStream * m_vidoe_st;
}msdk_encode_muxer_context;

void msdk_encode_init_muxer(msdk_encode_muxer_context *muxer_ctx, char* file_name);
void msdk_encode_mux(msdk_encode_muxer_context *muxer_ctx, mfxBitstream bitstream, coded_buf muxed_buf);
void msdk_encode_close_muxer(msdk_encode_muxer_context *muxer_ctx);
#endif
