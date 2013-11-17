#include "msdk_encode.h"
#include "stdlib.h"
int main(int argc, char** argv){
	msdk_encode_context ctx;
//	ctx.m_session = (mfxSession)malloc(sizeof(_mfxSession));
	msdk_encode_init(&ctx, 1280, 720, 3000, 25, MFX_TARGETUSAGE_BALANCED);
	
	return 0;	
}
