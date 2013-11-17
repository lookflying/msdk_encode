#include "msdk_encode.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <va/va_drm.h>
#include <fcntl.h>
#define CHECK_NO_ERROR(err) assert(err == MFX_ERR_NONE)
#define MSDK_IGNORE_MFX_STS(P, X)                {if ((X) == (P)) {P = MFX_ERR_NONE;}}

mfxStatus va_to_mfx_status(VAStatus va_res)
{
	mfxStatus mfxRes = MFX_ERR_NONE;

	switch (va_res)
	{
		case VA_STATUS_SUCCESS:
			mfxRes = MFX_ERR_NONE;
			break;
		case VA_STATUS_ERROR_ALLOCATION_FAILED:
			mfxRes = MFX_ERR_MEMORY_ALLOC;
			break;
		case VA_STATUS_ERROR_ATTR_NOT_SUPPORTED:
		case VA_STATUS_ERROR_UNSUPPORTED_PROFILE:
		case VA_STATUS_ERROR_UNSUPPORTED_ENTRYPOINT:
		case VA_STATUS_ERROR_UNSUPPORTED_RT_FORMAT:
		case VA_STATUS_ERROR_UNSUPPORTED_BUFFERTYPE:
		case VA_STATUS_ERROR_FLAG_NOT_SUPPORTED:
		case VA_STATUS_ERROR_RESOLUTION_NOT_SUPPORTED:
			mfxRes = MFX_ERR_UNSUPPORTED;
			break;
		case VA_STATUS_ERROR_INVALID_DISPLAY:
		case VA_STATUS_ERROR_INVALID_CONFIG:
		case VA_STATUS_ERROR_INVALID_CONTEXT:
		case VA_STATUS_ERROR_INVALID_SURFACE:
		case VA_STATUS_ERROR_INVALID_BUFFER:
		case VA_STATUS_ERROR_INVALID_IMAGE:
		case VA_STATUS_ERROR_INVALID_SUBPICTURE:
			mfxRes = MFX_ERR_NOT_INITIALIZED;
			break;
		case VA_STATUS_ERROR_INVALID_PARAMETER:
			mfxRes = MFX_ERR_INVALID_VIDEO_PARAM;
		default:
			mfxRes = MFX_ERR_UNKNOWN;
			break;
	}
	return mfxRes;
}

mfxStatus ConvertFrameRate(mfxF64 dFrameRate, mfxU32* pnFrameRateExtN, mfxU32* pnFrameRateExtD)
{
	//MSDK_CHECK_POINTER(pnFrameRateExtN, MFX_ERR_NULL_PTR);
	//MSDK_CHECK_POINTER(pnFrameRateExtD, MFX_ERR_NULL_PTR);

	mfxU32 fr;

	fr = (mfxU32)(dFrameRate + .5);

	if (fabs(fr - dFrameRate) < 0.0001)
	{
		*pnFrameRateExtN = fr;
		*pnFrameRateExtD = 1;
		return MFX_ERR_NONE;
	}

	fr = (mfxU32)(dFrameRate * 1.001 + .5);

	if (fabs(fr * 1000 - dFrameRate * 1001) < 10)
	{
		*pnFrameRateExtN = fr * 1000;
		*pnFrameRateExtD = 1001;
		return MFX_ERR_NONE;
	}

	*pnFrameRateExtN = (mfxU32)(dFrameRate * 10000 + .5);
	*pnFrameRateExtD = 10000;

	return MFX_ERR_NONE;
}

void msdk_encode_set_param(msdk_encode_context *ctx, int width, int height, int bitrate, int fps, int target){
	ctx->m_param.mfx.CodecId = MFX_CODEC_AVC;
	ctx->m_param.mfx.TargetUsage = (mfxU16) target;
	ctx->m_param.mfx.TargetKbps = (mfxU16) bitrate;
	ctx->m_param.mfx.RateControlMethod = (mfxU16) MFX_RATECONTROL_CBR;
	ConvertFrameRate(fps, 
			&ctx->m_param.mfx.FrameInfo.FrameRateExtN,
			&ctx->m_param.mfx.FrameInfo.FrameRateExtD);
	ctx->m_param.mfx.EncodedOrder = 0;
	ctx->m_param.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
	ctx->m_param.mfx.FrameInfo.FourCC = MFX_FOURCC_YV12;
	ctx->m_param.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
	ctx->m_param.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
	ctx->m_param.mfx.FrameInfo.Width = MSDK_ALIGN16(width);
	ctx->m_param.mfx.FrameInfo.Height = MSDK_ALIGN16(height);

	ctx->m_param.mfx.FrameInfo.CropX = 0;
	ctx->m_param.mfx.FrameInfo.CropY = 0;
	ctx->m_param.mfx.FrameInfo.CropW = width;
	ctx->m_param.mfx.FrameInfo.CropH = height;
}

mfxStatus msdk_encode_open_va_drm(msdk_encode_context *ctx){
	VAStatus va_res = VA_STATUS_SUCCESS;
	mfxStatus sts = MFX_ERR_NONE;
	int major_version = 0, minor_version = 0;
	ctx->m_fd = open("/dev/dri/card0", O_RDWR);

	if (ctx->m_fd < 0) sts = MFX_ERR_NOT_INITIALIZED;
	if (MFX_ERR_NONE == sts){
		ctx->m_va_dpy = vaGetDisplayDRM(ctx->m_fd);
		if (!ctx->m_va_dpy){
			close(ctx->m_fd);
			sts = MFX_ERR_NULL_PTR;
		}
	}
	if (MFX_ERR_NONE == sts){
		va_res = vaInitialize(ctx->m_va_dpy, &major_version, &minor_version);
		sts = va_to_mfx_status(va_res);
		if (MFX_ERR_NONE != sts){
			close(ctx->m_fd);
			ctx->m_fd = -1;
		}
	}
	return sts;
}

void msdk_encode_init(msdk_encode_context *ctx, int width, int height, int bitrate, int fps, int target){
	//ctx->m_version = {1, 1};
	mfxVersion version = {1, 1};
	ctx->m_status = MFXInit(MFX_IMPL_HARDWARE_ANY, &version, &ctx->m_session);
	printf("%s\n", ctx->m_status == MFX_ERR_UNSUPPORTED?"unsupported":"no error");
	fflush(stdout);
	CHECK_NO_ERROR(ctx->m_status);
	mfxIMPL impl;
	MFXQueryIMPL(ctx->m_session, &impl);
	if (MFX_IMPL_HARDWARE == MFX_IMPL_BASETYPE(impl)){
		printf("using hardware\n");
		ctx->m_status =  msdk_encode_open_va_drm(ctx);
		CHECK_NO_ERROR(ctx->m_status);
		MFXVideoCORE_SetHandle(ctx->m_session, MFX_HANDLE_VA_DISPLAY, ctx->m_va_dpy);
		/* In case of system memory we demonstrate "no external allocator" usage model.
			 We don't call SetAllocator, Media SDK uses internal allocator.
			 We use system memory allocator simply as a memory manager for application*/
		//TODO m_pBufferAllocator SysMemBufferAllocator m_pmfxAllocatorParams


	}else{
		assert(0);
	}
	msdk_encode_set_param(ctx, width, height, bitrate, fps, target);
	ctx->m_nAsyncDepth = 4;

	MFXVideoENCODE_QueryIOSurf(ctx->m_session, &ctx->m_param, &ctx->m_request);
	printf("Min = %u, Suggested = %u\n", ctx->m_request.NumFrameMin, ctx->m_request.NumFrameSuggested);
	mfxU16 n_frames = ctx->m_request.NumFrameSuggested + ctx->m_nAsyncDepth - 1;


	//TODO allocate memory according to suggested frames + nAsyncDepth -1
	//TODO must init frames which are stored as struct surface1
	MFXVideoENCODE_Init(ctx->m_session, &ctx->m_param);

	//may be partial acceleration
	if (MFX_WRN_PARTIAL_ACCELERATION == ctx->m_status)
	{
		printf("WARNING: partial acceleration\n");
		MSDK_IGNORE_MFX_STS(ctx->m_status, MFX_WRN_PARTIAL_ACCELERATION);
	}
	CHECK_NO_ERROR(ctx->m_status);
	
	//should be ok
}


void msdk_encode_encode_frame(msdk_encode_context *ctx, unsigned char *yuv_buf, coded_buf out_buf){

}

void msdk_encode_close(msdk_encode_context *ctx){

}
