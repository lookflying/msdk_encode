#include "vaenclib.h"
#include "msdk_encode.h"
#include <stdio.h>
#include <stdlib.h>


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
	msdk_encode_init(&ctx, width, height, bitrate, fps, target);
	return 0;
}

int encode_frame(unsigned char *inputdata,
			   struct coded_buff *avc_p){
	msdk_encode_encode_frame(&ctx, inputdata, (coded_buf*)avc_p);	
}

int close_encoder(){
	msdk_encode_close(&ctx);

}
