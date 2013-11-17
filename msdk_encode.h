#ifndef MSDK_ENCODE_H
#define MSDK_ENCODE_H
#include "mfxvideo.h"
#include "mfxdefs.h"
#include <va/va.h>
typedef struct coded_buf_t{
	unsigned char * buf;
	unsigned int len;
} coded_buf;

typedef struct msdk_encode_context_t{
	mfxSession m_session;
//	mfxVersion m_version;
	mfxStatus m_status;
	mfxVideoParam m_param;
	mfxFrameAllocRequest m_request;
	VADisplay m_va_dpy;
	int m_fd;
	mfxU16 m_nAsyncDepth;
} msdk_encode_context;

#define MSDK_ALIGN16(value)                      (((value + 15) >> 4) << 4) // round up to a multiple of 16
#define MSDK_ALIGN32(value)                      (((value + 31) >> 5) << 5) // round up to a multiple of 32


void msdk_encode_init(msdk_encode_context *ctx, int width, int height, int bitrate, int fps, int target);
void msdk_encode_encode_frame(msdk_encode_context *ctx, unsigned char *yuv_buf, coded_buf out_buf);
void msdk_encode_close(msdk_encode_context *ctx);

void msdk_encode_set_param(msdk_encode_context *ctx, int width, int height, int bitrate, int fps, int target);
mfxStatus ConvertFrameRate(mfxF64 dFrameRate, mfxU32* pnFrameRateExtN, mfxU32* pnFrameRateExtD);

#endif
