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
	int target = MFX_TARGETUSAGE_BALANCED;

	sscanf(argv[3], "fb=%d", &bitrate);
	if (bitrate == -1){
		bitrate = 2000;
	}
	bitrate = 2000;
	msdk_encode_init(&ctx, width, height, bitrate, fps, target);
	return 0;
}

/*return value is always 0, avc_p->length indicates the length of output data which can be zero!*/
int encode_frame(unsigned char *inputdata,
			   struct coded_buff *avc_p){
#if DUMP
	FILE * msdk_264_file;
	FILE * msdk_yuv_file;
	static int first = 1;
	if (first){
		msdk_yuv_file = fopen("/dev/shm/msdk_yuv.dump", "wb");
		msdk_264_file = fopen("/dev/shm/msdk_264.dump", "wb");
		first = 0;
	}else{
		msdk_yuv_file = fopen("/dev/shm/msdk_yuv.dump", "ab");
		msdk_264_file = fopen("/dev/shm/msdk_264.dump", "ab");
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
