#include "msdk_encode.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <va/va_drm.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#define DEBUG 0
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

void msdk_encode_set_param(msdk_encode_context *ctx, int width, int height, int bitrate, int fps,int codec_id, int target){
	switch (codec_id){
		case MSDK_ENCODE_MPEG2:
			ctx->m_param.mfx.CodecId = MFX_CODEC_MPEG2;
			break;	
		case MSDK_ENCODE_H264:
		default:
			ctx->m_param.mfx.CodecId = MFX_CODEC_AVC;
			break;
	}
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

	ctx->m_param.mfx.GopPicSize = 30;
	ctx->m_param.mfx.GopRefDist = 1;//Distance between I- or P- key frames (1 means no B-frames)
	ctx->m_param.AsyncDepth = 1;
	ctx->m_param.mfx.NumRefFrame = 5;

	ctx->m_ext_param = (mfxExtBuffer**)malloc(sizeof(mfxExtBuffer*) * 2);
	ctx->m_param.NumExtParam = 0;

	//	ctx->m_coding_option.MaxDecFrameBuffering = 1;
	//	ctx->m_coding_option.RefPicMarkRep = MFX_CODINGOPTION_ON; //repeat sei feature

	//	ctx->m_coding_option.PicTimingSEI = MFX_CODINGOPTION_ON;
	ctx->m_ext_param[0] = (mfxExtBuffer*)&ctx->m_coding_option;
	ctx->m_param.NumExtParam++;
	//do nothing with picture_timing_sei , hope to be useful
	//	ctx->m_ext_param[1] = (mfxExtBuffer*)&ctx->m_picture_timing_sei;

	ctx->m_param.ExtParam = ctx->m_ext_param;

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
	memset(&ctx->m_param, 0, sizeof(ctx->m_param));
	ctx->m_va_dpy = NULL;
	ctx->m_fd = -1;
	ctx->m_nAsyncDepth = 1;
	ctx->m_surface = NULL;
	ctx->m_surface_num = 0;
	ctx->m_last_surface = 0;

	memset(&ctx->m_coding_option, 0, sizeof(ctx->m_coding_option));
	ctx->m_coding_option.Header.BufferId = MFX_EXTBUFF_CODING_OPTION;
	ctx->m_coding_option.Header.BufferSz = sizeof(ctx->m_coding_option);

	memset(&ctx->m_coding_option2, 0, sizeof(ctx->m_coding_option2));
	ctx->m_coding_option2.Header.BufferId = MFX_EXTBUFF_CODING_OPTION2;
	ctx->m_coding_option2.Header.BufferSz = sizeof(ctx->m_coding_option2);



	memset(&ctx->m_picture_timing_sei, 0, sizeof(ctx->m_picture_timing_sei));
	ctx->m_picture_timing_sei.Header.BufferId = MFX_EXTBUFF_PICTURE_TIMING_SEI;
	ctx->m_picture_timing_sei.Header.BufferSz = sizeof(ctx->m_picture_timing_sei);

	ctx->m_timestamp = 0;

	ctx->m_task = NULL;//not used in this demo	
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

void msdk_encode_init(msdk_encode_context *ctx, int width, int height, int bitrate, int fps, int codec_id, int target){
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


	}else{
		assert(0);
	}
	msdk_encode_set_param(ctx, width, height, bitrate, fps, codec_id, target);
	ctx->m_nAsyncDepth = ctx->m_param.AsyncDepth;

	ctx->m_status = MFXVideoENCODE_QueryIOSurf(ctx->m_session, &ctx->m_param, &ctx->m_request);
	if (MFX_WRN_PARTIAL_ACCELERATION == ctx->m_status)
	{
		printf("WARNING: partial acceleration\n");
		MSDK_IGNORE_MFX_STS(ctx->m_status, MFX_WRN_PARTIAL_ACCELERATION);
	}
	//	printf("Min = %u, Suggested = %u\n", ctx->m_request.NumFrameMin, ctx->m_request.NumFrameSuggested);

	ctx->m_surface_num = ctx->m_request.NumFrameSuggested + ctx->m_nAsyncDepth - 1;
	ctx->m_last_surface = ctx->m_surface_num - 1;


	//may be partial acceleration
	if (MFX_WRN_PARTIAL_ACCELERATION == ctx->m_status)
	{
		printf("WARNING: partial acceleration\n");
		MSDK_IGNORE_MFX_STS(ctx->m_status, MFX_WRN_PARTIAL_ACCELERATION);
	}
	CHECK_NO_ERROR(ctx->m_status);

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



}

void msdk_encode_copy_to_surface(mfxFrameSurface1* surface, unsigned char * yuv_buf){
	mfxU32 size = surface->Info.CropW * surface->Info.CropH; 
	mfxU32 width, height, width2, height2, pitch;
	width = surface->Info.CropW;
	height = surface->Info.CropH;
	width2 = surface->Info.CropW / 2;
	height2 = surface->Info.CropH / 2;
	pitch = surface->Data.Pitch;
	unsigned int i, j;
	unsigned char *y_ptr = surface->Data.Y;
	unsigned char *y_src = yuv_buf;
	for (i = 0; i < height; ++i){
		memcpy(y_ptr, y_src, width);
		y_ptr += pitch;
		y_src += width;
	}
	unsigned char *u_ptr = surface->Data.UV;
	unsigned char *v_ptr = surface->Data.UV + 1;
	unsigned char *u_src = yuv_buf + size ;
	unsigned char *v_src = yuv_buf + size + size / 4;
	for (i = 0; i < height2; ++i){
		for (j = 0; j < width2; ++j){
			u_ptr[j * 2] = u_src[j];
			v_ptr[j * 2] = v_src[j];
		}
		u_ptr += pitch;
		u_src += width2;
		v_ptr += pitch;
		v_src += width2;
	}
}

int msdk_encode_encode_frame(msdk_encode_context *ctx, unsigned char *yuv_buf, coded_buf *out_buf, int buf_allocated){
	mfxU32 buf_size = ctx->m_param.mfx.FrameInfo.CropW * ctx->m_param.mfx.FrameInfo.CropH * 4;
	mfxFrameSurface1* surface = NULL;
	mfxU16 surface_idx = 0;
	msdk_encode_task task;
	memset(&task.m_bitstream, 0, sizeof(mfxBitstream));
	msdk_encode_reset_bitstream(&task.m_bitstream, buf_size);
	task.m_syncpoint = NULL;

	surface_idx = (ctx->m_last_surface + 1) % ctx->m_surface_num;
	mfxU16 i = 0;
	for (i = 0; i < ctx->m_surface_num; ++i){
		surface = &ctx->m_surface[surface_idx];
		if (0 == surface->Data.Locked){
			ctx->m_last_surface = surface_idx;
			goto got_free_surface;
		}	
		surface_idx = (surface_idx + 1) % ctx->m_surface_num;
	}
	assert(0);
got_free_surface:
	msdk_encode_copy_to_surface(surface, yuv_buf);
	surface->Data.TimeStamp = ctx->m_timestamp;

	for(;;){
#if DEBUG
		static int count;
#endif
		ctx->m_status = MFXVideoENCODE_EncodeFrameAsync(
				ctx->m_session,
				NULL,
				surface,
				&task.m_bitstream,
				&task.m_syncpoint);

		ctx->m_timestamp++;

#if DEBUG
		printf("count = %d, sync_point == 0x%X\n", count++, (unsigned int)task.m_syncpoint);
#endif
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
	if (task.m_syncpoint){
		ctx->m_status = MFXVideoCORE_SyncOperation(ctx->m_session, task.m_syncpoint, 250000);
#if DEBUG
		//		printf("sync_point == 0x%X\n", (unsigned int)task.m_syncpoint);
		if (MFX_WRN_IN_EXECUTION == ctx->m_status){
			printf("in execution\n");
		}else if (MFX_ERR_ABORTED == ctx->m_status){
			printf("aborted\n");
		}
		fflush(stdout);
#endif
		CHECK_NO_ERROR(ctx->m_status);
	}
#if DEBUG
	printf("offset = %u, len = %u, type = %X\n", task.m_bitstream.DataOffset, task.m_bitstream.DataLength, task.m_bitstream.FrameType);
#endif
	if (!buf_allocated){
		out_buf->buf = (unsigned char*) malloc(task.m_bitstream.DataLength);//should be freed after used outside
	}
	out_buf->len = task.m_bitstream.DataLength;
	memcpy(out_buf->buf, task.m_bitstream.Data + task.m_bitstream.DataOffset, out_buf->len);
	msdk_encode_reset_bitstream(&task.m_bitstream, 0);
	return 0;
}

void msdk_encode_close(msdk_encode_context *ctx){
	mfxU16 i;	
	vaTerminate(ctx->m_va_dpy);
	close(ctx->m_fd);
	mfxFrameSurface1 *surf_ptr = ctx->m_surface;
	for (i = 0; i < ctx->m_surface_num; ++i){
		free(surf_ptr->Data.Y);
		++surf_ptr;
	}
	free(ctx->m_surface);
	MFXVideoENCODE_Close(ctx->m_session);
	MFXClose(ctx->m_session);	
}
