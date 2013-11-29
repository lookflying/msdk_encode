#include "msdk_encode_muxer.h"
#include <stdio.h>
void msdk_encode_init_muxer(msdk_encode_muxer_context *muxer_ctx, char* file_name){
	avformat_alloc_output_context2(&muxer_ctx->m_av_ctx, NULL, NULL, file_name);
	if (!muxer_ctx->m_av_ctx){
		printf("fail to guess the format!\n");
		return;
	}
	muxer_ctx->m_av_fmt = muxer_ctx->m_av_ctx->oformat;
	muxer_ctx->m_vidoe_st = NULL;
	if (muxer_ctx->m_av_fmt != AV_CODEC_ID_NONE){
	}


}

void msdk_encode_mux(msdk_encode_muxer_context *muxer_ctx, mfxBitstream bitstream, coded_buf muxed_buf){

}

void msdk_encode_close_muxer(msdk_encode_muxer_context *muxer_ctx){

}
