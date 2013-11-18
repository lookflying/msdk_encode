#include "msdk_encode.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <va/va_drm.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
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
	ctx->m_param.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
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

void msdk_encode_init_context(msdk_encode_context *ctx){
	ctx->m_session = NULL;
	ctx->m_status = MFX_ERR_NONE;
	memset(&ctx->m_param, 0, sizeof(mfxVideoParam));
	ctx->m_va_dpy = NULL;
	ctx->m_fd = -1;
	ctx->m_nAsyncDepth = 1;
	ctx->m_surface = NULL;
	ctx->m_surface_num = 0;
	ctx->m_last_surface = 0;
	ctx->m_bitstream = NULL;
	ctx->m_bitstream_num = 0;
	/*	memset(&ctx->m_surface, 0, sizeof(mfxFrameSurface1));
			memset(&ctx->m_bitstream, 0, sizeof(mfxBitstream));*/
	ctx->m_syncpoint = NULL;
}

mfxStatus msdk_encode_reset_bitstream(mfxBitstream* bitstream, unsigned int size){
	if (size == 0){
		if (bitstream->Data != NULL){
			free(bitstream->Data);
		}
		memset(bitstream, 0, sizeof(mfxBitstream));
		return MFX_ERR_NONE;
	}else{
		if (size < bitstream->MaxLength){
			return MFX_ERR_UNSUPPORTED;
		}else{
			unsigned int aligned_size = MSDK_ALIGN32(size);
			mfxU8* new_ptr = malloc(aligned_size);
			MSDK_CHECK_POINTER(new_ptr, MFX_ERR_MEMORY_ALLOC);
			if (bitstream->Data != NULL){
				memmove(new_ptr, bitstream->Data + bitstream->DataOffset, bitstream->DataLength);
				free(bitstream->Data);
			}
			bitstream->Data = new_ptr;
			bitstream->DataOffset = 0;
			bitstream->MaxLength = aligned_size;
			return MFX_ERR_NONE;
		}
	}
	assert(0);
	return MFX_ERR_NONE;
}

void msdk_encode_init(msdk_encode_context *ctx, int width, int height, int bitrate, int fps, int target){
	msdk_encode_init_context(ctx);
	mfxVersion version = {{1, 1}};
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
		MFXVideoCORE_SetHandle(ctx->m_session, MFX_HANDLE_VA_DISPLAY, (mfxHDL)ctx->m_va_dpy);
		/* In case of system memory we demonstrate "no external allocator" usage model.
			 We don't call SetAllocator, Media SDK uses internal allocator.
			 We use system memory allocator simply as a memory manager for application*/
		//FIXME (seem to be no need) m_pBufferAllocator SysMemBufferAllocator m_pmfxAllocatorParams


	}else{
		assert(0);
	}
	msdk_encode_set_param(ctx, width, height, bitrate, fps, target);
	ctx->m_nAsyncDepth = 1;

	ctx->m_status = MFXVideoENCODE_QueryIOSurf(ctx->m_session, &ctx->m_param, &ctx->m_request);
	if (MFX_WRN_PARTIAL_ACCELERATION == ctx->m_status)
	{
		printf("WARNING: partial acceleration\n");
		MSDK_IGNORE_MFX_STS(ctx->m_status, MFX_WRN_PARTIAL_ACCELERATION);
	}
	printf("Min = %u, Suggested = %u\n", ctx->m_request.NumFrameMin, ctx->m_request.NumFrameSuggested);
	//FIXME (maybe one surface is enough)	
	ctx->m_surface_num = ctx->m_request.NumFrameSuggested + ctx->m_nAsyncDepth - 1;
	ctx->m_last_surface = ctx->m_surface_num - 1;


	//may be partial acceleration
	if (MFX_WRN_PARTIAL_ACCELERATION == ctx->m_status)
	{
		printf("WARNING: partial acceleration\n");
		MSDK_IGNORE_MFX_STS(ctx->m_status, MFX_WRN_PARTIAL_ACCELERATION);
	}
	CHECK_NO_ERROR(ctx->m_status);

	//FIXME (memory can just use input buf) allocate memory according to suggested frames + nAsyncDepth -1
	//must init frames which are stored as struct surface1
	ctx->m_status =	MFXVideoENCODE_Init(ctx->m_session, &ctx->m_param);
	if (MFX_WRN_PARTIAL_ACCELERATION == ctx->m_status)
	{
		printf("WARNING: partial acceleration\n");
		MSDK_IGNORE_MFX_STS(ctx->m_status, MFX_WRN_PARTIAL_ACCELERATION);
	}
	CHECK_NO_ERROR(ctx->m_status);

	ctx->m_surface = (mfxFrameSurface1*)malloc(ctx->m_surface_num * sizeof(mfxFrameSurface1));
	assert(ctx->m_surface != NULL);
	memset(ctx->m_surface, 0, ctx->m_surface_num * sizeof(mfxFrameSurface1));

	mfxFrameSurface1 *surf_ptr = ctx->m_surface;
	mfxU16 i;	
	for (i = 0; i < ctx->m_surface_num; ++i){//msdk only support NV12; assume no crop
		memcpy(&surf_ptr->Info, &ctx->m_param.mfx.FrameInfo, sizeof(mfxInfoMFX));
		mfxU32 aligned_width = MSDK_ALIGN32(width);
		mfxU32 aligned_height = MSDK_ALIGN32(height);
		mfxU32 aligned_size = aligned_width * aligned_height * 3 / 2;
		surf_ptr->Data.B = surf_ptr->Data.Y = (mfxU8*)malloc(aligned_size);
		assert(surf_ptr->Data.Y != NULL);
		surf_ptr->Data.UV = surf_ptr->Data.Y + aligned_width * aligned_height;
		surf_ptr->Data.Pitch = aligned_width;
		++surf_ptr;
	}


	ctx->m_bitstream_num = ctx->m_surface_num;//FIXME
	ctx->m_bitstream = (mfxBitstream*)malloc(ctx->m_bitstream_num * sizeof(mfxBitstream));
	assert(ctx->m_bitstream != NULL);
	memset(ctx->m_bitstream, 0, ctx->m_bitstream_num * sizeof(mfxBitstream));
	mfxBitstream * bs_ptr = ctx->m_bitstream;
	mfxVideoParam param;
	MFXVideoENCODE_GetVideoParam(ctx->m_session, &param);
	for (i = 0; i < ctx->m_bitstream_num; ++i){
		mfxU32 aligned_width = MSDK_ALIGN32(width);
		mfxU32 aligned_height = MSDK_ALIGN32(height);
		//FIXME why size always 0;
		//	msdk_encode_reset_bitstream(bs_ptr, param.mfx.BufferSizeInKB * 1024);
		msdk_encode_reset_bitstream(bs_ptr, aligned_width * aligned_height * 4);
		++bs_ptr;
	}


	//should be ok
}

void msdk_encode_copy_to_surface(mfxFrameSurface1* surface, unsigned char * yuv_buf){
	mfxU32 size = surface->Info.CropW * surface->Info.CropH; 
	mfxU32 width2, height2, pitch;
	width2 = surface->Info.CropW / 2;
	height2 = surface->Info.CropH / 2;
	pitch = surface->Data.Pitch;
	memcpy(surface->Data.Y, yuv_buf, size); 
	unsigned int i, j;
	unsigned char *u_ptr = yuv_buf + size;
	unsigned char *v_ptr = yuv_buf + size + size / 4;
	for (i = 0; i < height2; ++i){
		for (j = 0; j < width2; ++j){
			surface->Data.UV[i * pitch + j * 2] = u_ptr[i * width2 + j];
			surface->Data.UV[i * pitch + j * 2 + 1] = v_ptr[i * width2 + j];
		}
	}
}

int msdk_encode_encode_frame(msdk_encode_context *ctx, unsigned char *yuv_buf, coded_buf *out_buf){
	mfxU32 buf_size = ctx->m_param.mfx.FrameInfo.CropW * ctx->m_param.mfx.FrameInfo.CropH * 4;
	mfxFrameSurface1* surface = NULL;
	mfxU16 surface_idx = 0;
	msdk_encode_task task;
	memset(&task.m_bitstream, 0, sizeof(mfxBitstream));
	msdk_encode_reset_bitstream(&task.m_bitstream, buf_size);
	task.m_syncpoint = NULL;
	mfxBitstream* bitstream = NULL;
	mfxU16 bitstream_idx = 0;

	surface_idx = (ctx->m_last_surface + 1) % ctx->m_surface_num;
	surface = &ctx->m_surface[surface_idx];
	mfxU16 i = 0;
	for (i = 0; i < ctx->m_surface_num; ++i){
		if (0 == surface->Data.Locked){
			ctx->m_last_surface = (surface_idx + i) % ctx->m_surface_num;
			goto got_free_surface;
		}
	}
	assert(0);
got_free_surface:
	msdk_encode_copy_to_surface(surface, yuv_buf);


	for(;;){
		ctx->m_status = MFXVideoENCODE_EncodeFrameAsync(ctx->m_session,
				NULL,
				surface,
				&task.m_bitstream,
				&task.m_syncpoint);

		if (MFX_ERR_NONE < ctx->m_status && !task.m_syncpoint){
			if (MFX_WRN_DEVICE_BUSY == ctx->m_status)
				MSDK_SLEEP(1);
		}else if (MFX_ERR_NONE < ctx->m_status && task.m_syncpoint){
			ctx->m_status = MFX_ERR_NONE;
			break;
		}else if (MFX_ERR_NOT_ENOUGH_BUFFER == ctx->m_status){
			printf("not enough buffer");
			buf_size *= 2;
			msdk_encode_reset_bitstream(&task.m_bitstream, buf_size);
		}else{
			MSDK_IGNORE_MFX_STS(ctx->m_status, MFX_ERR_MORE_BITSTREAM);
			break;
		}
	}
	ctx->m_status = MFXVideoCORE_SyncOperation(ctx->m_session, task.m_syncpoint, 250000);
	CHECK_NO_ERROR(ctx->m_status);
	return 0;
}

void msdk_encode_close(msdk_encode_context *ctx){

}
