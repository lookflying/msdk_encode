#include "msdk_encode.h"
#include "stdlib.h"
#include "stdio.h"
#include "assert.h"

FILE* infile;
FILE* outfile;

int main(int argc, char** argv){
	msdk_encode_context ctx;
	//	ctx.m_session = (mfxSession)malloc(sizeof(_mfxSession));
	msdk_encode_init(&ctx, 704, 576, 3000, 25, MFX_TARGETUSAGE_BALANCED);
	//	unsigned char* buf = (unsigned char*)malloc(1280 * 720 * 3 / 2);

	infile = fopen("/home/lookflying/yuv/channel7.yuv", "rb");
	outfile = fopen("/home/lookflying/msdk/channel7.264", "wb");
	fseek(infile, 0, SEEK_END);
	unsigned int size = ftell(infile);
	rewind(infile);
	assert(size % (704 * 576 * 3 / 2) == 0);
	assert(infile != NULL && outfile != NULL);
	unsigned char* yuv_buf = (unsigned char*) malloc(704 * 576 * 3 / 2);
	coded_buf outbuf;
	assert(yuv_buf != NULL);
	while(fread(yuv_buf, 1, 704*576*3/2, infile) == 704*576*3/2){
		msdk_encode_encode_frame(&ctx, yuv_buf, &outbuf);
		fwrite(outbuf.buf, 1, outbuf.len, outfile);
		free(outbuf.buf);
	}
	free(yuv_buf);
	fclose(infile);
	fclose(outfile);
	msdk_encode_close(&ctx);
	return 0;	
}
