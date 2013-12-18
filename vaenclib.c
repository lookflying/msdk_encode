#include "vaenclib.h"
#include "msdk_encode.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define DUMP 1
msdk_encode_context ctx;
int init_encoder(int argc, char *argv[]){
	int width = atoi(argv[1]);
	int height = atoi(argv[2]);
	int bitrate;
	int fps = 25;
	int codec_id;
	int target = MFX_TARGETUSAGE_BALANCED;

	sscanf(argv[3], "fb=%d", &bitrate);
	if (bitrate == -1){
		bitrate = 2000;
	}
	bitrate = 2000;
	if (width <= 720){
		codec_id = MSDK_ENCODE_MPEG2;
	}else{
		codec_id = MSDK_ENCODE_H264;
	}
	msdk_encode_init(&ctx, width, height, bitrate, fps, codec_id, target);
	return 0;
}

#define YUV_DUMP_FILE "/dev/shm/msdk_yuv.dump"
#define ENCODED_DUMP_FILE "/dev/shm/msdk_encoded.dump"
/*return value is always 0, avc_p->length indicates the length of output data which can be zero!*/
int encode_frame(unsigned char *inputdata,
			   struct coded_buff *avc_p){
#if DUMP
	FILE * msdk_264_file;
	FILE * msdk_yuv_file;
	static int first = 1;
	if (first){
		msdk_yuv_file = fopen(YUV_DUMP_FILE,"wb");
		msdk_264_file = fopen(ENCODED_DUMP_FILE, "wb");
		first = 0;
	}else{
		msdk_yuv_file = fopen(YUV_DUMP_FILE, "ab");
		msdk_264_file = fopen(ENCODED_DUMP_FILE, "ab");
	}
	assert(msdk_264_file != NULL);
	assert(msdk_yuv_file != NULL);
#endif
	coded_buf out_buf;
	out_buf.buf = avc_p->buff;
	msdk_encode_encode_frame(&ctx, inputdata, &out_buf, 1);
	avc_p->length = out_buf.len;

#if DUMP
	fwrite(inputdata, 1, ctx.m_param.mfx.FrameInfo.CropW * ctx.m_param.mfx.FrameInfo.CropH * 3 / 2, msdk_yuv_file);
	fwrite(avc_p->buff, 1, avc_p->length, msdk_264_file);
	fclose(msdk_264_file);
	fclose(msdk_yuv_file);
#endif
	return 0;
}

int close_encoder(){
	msdk_encode_close(&ctx);
	return 0;
}
