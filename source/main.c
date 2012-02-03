/* Now double buffered with animation with one PNG dancing.
 */ 

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include <ppu-types.h>
#include <ppu-lv2.h>

#include <rsx/rsx.h>
#include <sysutil/video.h>

#include <sysutil/sysutil.h>
#include <sysmodule/sysmodule.h>

#include <io/pad.h>

#include <pngdec/pngdec.h>

#include "rsxutil.h"
#include "commands_screen_bin.h" // png in memory

#define USE_PNG_FROM_FILE false
#define PNG_FILE "/dev_usb/psl1ght_png.bin"

#define MAX_BUFFERS 2


static int exitapp, xmbopen;
u32 module_flag;
static int currentBuffer = 0;
static inline void eventHandler(u64 status, u64 param, void * userdata)
{
	switch(status)
	{
		case SYSUTIL_EXIT_GAME:
			exitapp = 0;
			break;
		case SYSUTIL_MENU_OPEN:
			xmbopen = 1;
			break;
		case SYSUTIL_MENU_CLOSE:
			xmbopen = 0;
			break;
	}
}

static inline void unload_modules()
{
	if(module_flag & 2)
		sysModuleUnload(SYSMODULE_PNGDEC);
	if(module_flag & 1)
		sysModuleUnload(SYSMODULE_FS);
}


s32 main(s32 argc, const char* argv[])
{
	gcmContextData *context;
	void *host_addr = NULL;
	rsxBuffer buffers[MAX_BUFFERS];
	
	u16 width;
	u16 height;
	u32 bz1=2,bz2=1;
	padInfo padinfo ;
	padData paddata ;
	int i;

	atexit(unload_modules);
	if(sysModuleLoad(SYSMODULE_FS) != 0)
		return 0;
	else
		module_flag |= 1;

	if(sysModuleLoad(SYSMODULE_PNGDEC) != 0)
		return 0;
	else
		module_flag |= 2;

	/* Allocate a 1Mb buffer, alligned to a 1Mb boundary
	 * to be our shared IO memory with the RSX. */
	host_addr = memalign ( 1024*1024, HOST_SIZE ) ;
	context = initScreen ( host_addr, HOST_SIZE ) ;
	getResolution( &width, &height ) ;
	for (i = 0; i < MAX_BUFFERS; i++)
		makeBuffer( &buffers[i], width, height, i ) ;
	flip( context, MAX_BUFFERS - 1 ) ;
	setRenderTarget(context, &buffers[currentBuffer]) ;

	sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, eventHandler, NULL);

	ioPadInit(7) ;

	pngData png1;

	pngLoadFromBuffer((void *)commands_screen_bin, commands_screen_bin_size, &png1);

	exitapp = 1;
	while(exitapp)
	{
		/* Check the pads. */
		ioPadGetInfo(&padinfo);
		for(i=0; i<MAX_PADS; i++){
			if(padinfo.status[i]){
				ioPadGetData(i, &paddata);

				if(paddata.BTN_START){
					exitapp = 0;
				}
				if(paddata.BTN_UP){ //RED
					bz1=2;
					lv2syscall2(0x182,bz1, bz2);
				}
				if(paddata.BTN_LEFT){ //OFF
					bz1=0;
					lv2syscall2(0x182,bz1, bz2);
				}
				if(paddata.BTN_DOWN){ //GREEN
					bz1=1;
					lv2syscall2(0x182,bz1, bz2);
				}
				if(paddata.BTN_TRIANGLE){ //LED 1
					bz2=1;
					lv2syscall2(0x182,bz1, bz2);
				}
				if(paddata.BTN_CIRCLE){ //LED 2
					bz2=0;
					lv2syscall2(0x182,bz1, bz2);
				}
				if(paddata.BTN_CROSS){ //BLINK
					bz2=2;
					lv2syscall2(0x182,bz1, bz2);
				}
			}
		}
		waitFlip(); // Wait for the last flip to finish, so we can draw to the old buffer

		if(png1.bmp_out){
			static int x=0, y=0;
		 
			u32 *scr = (u32 *)buffers[currentBuffer].ptr;
			u32 *png= (void *)png1.bmp_out;
			int n, m;
	
			/* update x, y coordinates */
			x=(width/2)-(png1.width/2);
			y=(height/2)-(png1.height/2);
		 
			/* update screen buffer from coordinates */
			scr += y*buffers[currentBuffer].width + x;							
		 
			// draw PNG
			for(n=0;n<png1.height;n++)
			{
			  if((y+n)>=buffers[currentBuffer].height) break;
			  for(m=0;m<png1.width;m++)
			  {
				 if((x+m)>=buffers[currentBuffer].width) break;
				 scr[m]=png[m];
			  }
			  png+=png1.pitch>>2;
			  scr+=buffers[currentBuffer].width;
			}
		}
		flip(context, buffers[currentBuffer].id); // Flip buffer onto screen

		currentBuffer = !currentBuffer;
		setRenderTarget(context, &buffers[currentBuffer]) ; // change buffer

		sysUtilCheckCallback(); // check user attention span
	}

	gcmSetWaitFlip(context);
	for (i=0; i < MAX_BUFFERS; i++)
		rsxFree (buffers[i].ptr);
	rsxFinish (context, 1);
	free (host_addr);
	ioPadEnd();

	return 0;
}

