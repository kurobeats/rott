/*
Copyright (C) 1994-1995 Apogee Software, Ltd.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#ifdef DOS
#include <malloc.h>
#include <dos.h>
#include <conio.h>
#include <io.h>
#endif

#include <stdlib.h>
#include <sys/stat.h>
#include "modexlib.h"
//MED
#include "memcheck.h"
#include "rt_util.h"

void    SchrinkMemPicture();
void StrechMemPicture ();
void StrechFlipMemPicture ();
// GLOBAL VARIABLES

boolean StrechScreen=true;//bn�++
extern char *tmpPICbuf;
char *sdl_surfacePTR;
extern int iG_aimCross;
extern int iG_X_center;
extern int iG_Y_center;
char 	   *iG_buf_center;
  
int    linewidth;
//int    ylookup[MAXSCREENHEIGHT];
int    ylookup[600];//just set to max res
byte  *page1start;
byte  *page2start;
byte  *page3start;
int    screensize;
byte  *bufferofs;
byte  *displayofs;
boolean graphicsmode=false;
char        *bufofsTopLimit;
char        *bufofsBottomLimit;

void DrawCenterAim ();

#ifdef DOS

/*
====================
=
= GraphicsMode
=
====================
*/
void GraphicsMode ( void )
{
union REGS regs;

regs.w.ax = 0x13;
int386(0x10,&regs,&regs);
graphicsmode=true;
}

/*
====================
=
= SetTextMode
=
====================
*/
void SetTextMode ( void )
{

union REGS regs;

regs.w.ax = 0x03;
int386(0x10,&regs,&regs);
graphicsmode=false;

}

/*
====================
=
= TurnOffTextCursor
=
====================
*/
void TurnOffTextCursor ( void )
{

union REGS regs;

regs.w.ax = 0x0100;
regs.w.cx = 0x2000;
int386(0x10,&regs,&regs);

}

#if 0
/*
====================
=
= TurnOnTextCursor
=
====================
*/
void TurnOnTextCursor ( void )
{

union REGS regs;

regs.w.ax = 0x03;
int386(0x10,&regs,&regs);

}
#endif

/*
====================
=
= WaitVBL
=
====================
*/
void WaitVBL( void )
{
   unsigned char i;

   i=inp(0x03da);
   while ((i&8)==0)
      i=inp(0x03da);
   while ((i&8)==1)
      i=inp(0x03da);
}


/*
====================
=
= VL_SetLineWidth
=
= Line witdh is in WORDS, 40 words is normal width for vgaplanegr
=
====================
*/

void VL_SetLineWidth (unsigned width)
{
   int i,offset;

//
// set wide virtual screen
//
   outpw (CRTC_INDEX,CRTC_OFFSET+width*256);

//
// set up lookup tables
//
   linewidth = width*2;

   offset = 0;

   for (i=0;i<iGLOBAL_SCREENHEIGHT;i++)
      {
      ylookup[i]=offset;
      offset += linewidth;
      }
}

/*
=======================
=
= VL_SetVGAPlaneMode
=
=======================
*/

void VL_SetVGAPlaneMode ( void )
{
    GraphicsMode();
    VL_DePlaneVGA ();
    VL_SetLineWidth (48);
    screensize=208*iGLOBAL_SCREENBWIDE*2;//bna++ *2
    page1start=0xa0200;
    page2start=0xa0200+screensize;
    page3start=0xa0200+(2u*screensize);
    displayofs = page1start;
    bufferofs = page2start;
    XFlipPage ();
}

/*
=======================
=
= VL_CopyPlanarPage
=
=======================
*/
void VL_CopyPlanarPage ( byte * src, byte * dest )
{
   int plane;

   for (plane=0;plane<4;plane++)
      {
      VGAREADMAP(plane);
      VGAWRITEMAP(plane);
      memcpy(dest,src,screensize);
      }
}

/*
=======================
=
= VL_CopyPlanarPageToMemory
=
=======================
*/
void VL_CopyPlanarPageToMemory ( byte * src, byte * dest )
{
   byte * ptr;
   int plane,a,b;

   for (plane=0;plane<4;plane++)
      {
      ptr=dest+plane;
      VGAREADMAP(plane);
      for (a=0;a<200;a++)
         for (b=0;b<80;b++,ptr+=4)
            *(ptr)=*(src+(a*linewidth)+b);
      }
}

/*
=======================
=
= VL_CopyBufferToAll
=
=======================
*/
void VL_CopyBufferToAll ( byte *buffer )
{
   int plane;

   for (plane=0;plane<4;plane++)
      {
      VGAREADMAP(plane);
      VGAWRITEMAP(plane);
      if (page1start!=buffer)
         memcpy((byte *)page1start,(byte *)buffer,screensize);
      if (page2start!=buffer)
         memcpy((byte *)page2start,(byte *)buffer,screensize);
      if (page3start!=buffer)
         memcpy((byte *)page3start,(byte *)buffer,screensize);
      }
}

/*
=======================
=
= VL_CopyDisplayToHidden
=
=======================
*/
void VL_CopyDisplayToHidden ( void )
{
   VL_CopyBufferToAll ( displayofs );
}

/*
=================
=
= VL_ClearBuffer
=
= Fill the entire video buffer with a given color
=
=================
*/

void VL_ClearBuffer (unsigned buf, byte color)
{
  VGAMAPMASK(15);
  memset((byte *)buf,color,screensize);
}

/*
=================
=
= VL_ClearVideo
=
= Fill the entire video buffer with a given color
=
=================
*/

void VL_ClearVideo (byte color)
{
  VGAMAPMASK(15);
  memset((byte *)(0xa000<<4),color,0x10000);
}

/*
=================
=
= VL_DePlaneVGA
=
=================
*/

void VL_DePlaneVGA (void)
{

//
// change CPU addressing to non linear mode
//

//
// turn off chain 4 and odd/even
//
        outp (SC_INDEX,SC_MEMMODE);
        outp (SC_DATA,(inp(SC_DATA)&~8)|4);

        outp (SC_INDEX,SC_MAPMASK);         // leave this set throughout

//
// turn off odd/even and set write mode 0
//
        outp (GC_INDEX,GC_MODE);
        outp (GC_DATA,inp(GC_DATA)&~0x13);

//
// turn off chain
//
        outp (GC_INDEX,GC_MISCELLANEOUS);
        outp (GC_DATA,inp(GC_DATA)&~2);

//
// clear the entire buffer space, because int 10h only did 16 k / plane
//
        VL_ClearVideo (0);

//
// change CRTC scanning from doubleword to byte mode, allowing >64k scans
//
        outp (CRTC_INDEX,CRTC_UNDERLINE);
        outp (CRTC_DATA,inp(CRTC_DATA)&~0x40);

        outp (CRTC_INDEX,CRTC_MODE);
        outp (CRTC_DATA,inp(CRTC_DATA)|0x40);
}


/*
=================
=
= XFlipPage
=
=================
*/

void XFlipPage ( void )
{
   displayofs=bufferofs;

//   _disable();

   outp(CRTC_INDEX,CRTC_STARTHIGH);
   outp(CRTC_DATA,((displayofs&0x0000ffff)>>8));

//   _enable();

   bufferofs += screensize;
   if (bufferofs > page3start)
      bufferofs = page1start;
}

#else

#include "SDL.h"

#ifndef STUB_FUNCTION

/* rt_def.h isn't included, so I just put this here... */
#if !defined(ANSIESC)
#define STUB_FUNCTION fprintf(stderr,"STUB: %s at " __FILE__ ", line %d, thread %d\n",__FUNCTION__,__LINE__,getpid())
#else
#define STUB_FUNCTION
#endif

#endif

/*
====================
=
= GraphicsMode
=
====================
*/
static SDL_Surface *sdl_surface = NULL;
static SDL_Surface *sdl_backbuf = NULL;

void GraphicsMode ( void )
{
    Uint32 flags = 0;

	if (SDL_InitSubSystem (SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
	{
	    Error ("Could not initialize SDL\n");
	}

    #if defined(PLATFORM_WIN32) || defined(PLATFORM_MACOSX)
        // FIXME: remove this.  --ryan.
        flags = SDL_FULLSCREEN;
        SDL_WM_GrabInput(SDL_GRAB_ON);
    #endif

    SDL_WM_SetCaption ("Rise of the Triad", "ROTT");
    SDL_ShowCursor (0);
//    sdl_surface = SDL_SetVideoMode (320, 200, 8, flags);
    sdl_surface = SDL_SetVideoMode (iGLOBAL_SCREENWIDTH, iGLOBAL_SCREENHEIGHT, 8, flags);    
	if (sdl_surface == NULL)
	{
		Error ("Could not set video mode\n");
	} 
}

/*
====================
=
= SetTextMode
=
====================
*/
void SetTextMode ( void )
{
	if (SDL_WasInit(SDL_INIT_VIDEO) == SDL_INIT_VIDEO) {
		if (sdl_surface != NULL) {
			SDL_FreeSurface(sdl_surface);
	
			sdl_surface = NULL;
		}
		
		SDL_QuitSubSystem (SDL_INIT_VIDEO);
	}
}

/*
====================
=
= TurnOffTextCursor
=
====================
*/
void TurnOffTextCursor ( void )
{
}

/*
====================
=
= WaitVBL
=
====================
*/
void WaitVBL( void )
{
	SDL_Delay (16667/1000);
}

/*
=======================
=
= VL_SetVGAPlaneMode
=
=======================
*/

void VL_SetVGAPlaneMode ( void )
{
   int i,offset;

    GraphicsMode();

//
// set up lookup tables
//
//bna--   linewidth = 320;
   linewidth = iGLOBAL_SCREENWIDTH;

   offset = 0;

   for (i=0;i<iGLOBAL_SCREENHEIGHT;i++)
      {
      ylookup[i]=offset;
      offset += linewidth;
      }

//    screensize=MAXSCREENHEIGHT*MAXSCREENWIDTH;
    screensize=iGLOBAL_SCREENHEIGHT*iGLOBAL_SCREENWIDTH;



    page1start=sdl_surface->pixels;
    page2start=sdl_surface->pixels;
    page3start=sdl_surface->pixels;
    displayofs = page1start;
    bufferofs = page2start;

	iG_X_center = iGLOBAL_SCREENWIDTH / 2;
	iG_Y_center = (iGLOBAL_SCREENHEIGHT / 2)+10 ;//+10 = move aim down a bit

	iG_buf_center = bufferofs + (screensize/2);//(iG_Y_center*iGLOBAL_SCREENWIDTH);//+iG_X_center;

	bufofsTopLimit =  bufferofs + screensize - iGLOBAL_SCREENWIDTH;
	bufofsBottomLimit = bufferofs + iGLOBAL_SCREENWIDTH;

    XFlipPage ();
}

/*
=======================
=
= VL_CopyPlanarPage
=
=======================
*/
void VL_CopyPlanarPage ( byte * src, byte * dest )
{
#ifdef DOS
   int plane;

   for (plane=0;plane<4;plane++)
      {
      VGAREADMAP(plane);
      VGAWRITEMAP(plane);
      memcpy(dest,src,screensize);
      }
#else
      memcpy(dest,src,screensize);
#endif
}

/*
=======================
=
= VL_CopyPlanarPageToMemory
=
=======================
*/
void VL_CopyPlanarPageToMemory ( byte * src, byte * dest )
{
#ifdef DOS
   byte * ptr;
   int plane,a,b;

   for (plane=0;plane<4;plane++)
      {
      ptr=dest+plane;
      VGAREADMAP(plane);
      for (a=0;a<200;a++)
         for (b=0;b<80;b++,ptr+=4)
            *(ptr)=*(src+(a*linewidth)+b);
      }
#else
      memcpy(dest,src,screensize);
#endif
}

/*
=======================
=
= VL_CopyBufferToAll
=
=======================
*/
void VL_CopyBufferToAll ( byte *buffer )
{
#ifdef DOS
   int plane;

   for (plane=0;plane<4;plane++)
      {
      VGAREADMAP(plane);
      VGAWRITEMAP(plane);
      if (page1start!=buffer)
         memcpy((byte *)page1start,(byte *)buffer,screensize);
      if (page2start!=buffer)
         memcpy((byte *)page2start,(byte *)buffer,screensize);
      if (page3start!=buffer)
         memcpy((byte *)page3start,(byte *)buffer,screensize);
      }
#endif
}

/*
=======================
=
= VL_CopyDisplayToHidden
=
=======================
*/
void VL_CopyDisplayToHidden ( void )
{
   VL_CopyBufferToAll ( displayofs );
}

/*
=================
=
= VL_ClearBuffer
=
= Fill the entire video buffer with a given color
=
=================
*/

void VL_ClearBuffer (byte *buf, byte color)
{
#ifdef DOS
  VGAMAPMASK(15);
  memset((byte *)buf,color,screensize);
#else
  memset((byte *)buf,color,screensize);
#endif
}

/*
=================
=
= VL_ClearVideo
=
= Fill the entire video buffer with a given color
=
=================
*/

void VL_ClearVideo (byte color)
{
#ifdef DOS
  VGAMAPMASK(15);
  memset((byte *)(0xa000<<4),color,0x10000);
#else
  memset (sdl_surface->pixels, color, iGLOBAL_SCREENWIDTH*iGLOBAL_SCREENHEIGHT);
#endif
}

/*
=================
=
= VL_DePlaneVGA
=
=================
*/

void VL_DePlaneVGA (void)
{
}


/* C version of rt_vh_a.asm */

void VH_UpdateScreen (void)
{ 	

	if ((StrechScreen==true)&&(iGLOBAL_SCREENWIDTH > 320)){//bna++
		StrechMemPicture ();
	}else{
		DrawCenterAim ();
	}
	SDL_UpdateRect (SDL_GetVideoSurface (), 0, 0, 0, 0);
}


/*
=================
=
= XFlipPage
=
=================
*/

void XFlipPage ( void )
{
#ifdef DOS
   displayofs=bufferofs;

//   _disable();

   outp(CRTC_INDEX,CRTC_STARTHIGH);
   outp(CRTC_DATA,((displayofs&0x0000ffff)>>8));

//   _enable();

   bufferofs += screensize;
   if (bufferofs > page3start)
      bufferofs = page1start;
#else
 	if ((StrechScreen==true)&&(iGLOBAL_SCREENWIDTH > 320)){//bna++
		StrechMemPicture ();
	}else{
		DrawCenterAim ();
	}
   SDL_UpdateRect (sdl_surface, 0, 0, 0, 0);
 
#endif
}

#endif






// bna section -------------------------------------------
void StrechMemPicture ()
{

		//strech mem //	   SetTextMode (  );
   		byte *source,*target,*tmp,*tmp2;
		int x,y,x1,y1;
		int cnt,NbOfLines;
		float Yratio,Xratio,old;

		//strech pixels in X direction
		source = ( byte * )( sdl_surface->pixels);//store screen in tmp pic mem
		sdl_surfacePTR = source;
		memcpy( tmpPICbuf, source, (200*iGLOBAL_SCREENWIDTH) );

		source = tmpPICbuf;
		target = ( byte * )( sdl_surface->pixels);//screen buffer

	    Xratio = iGLOBAL_SCREENWIDTH * 10/ 320;
		Xratio = (Xratio/10);
		cnt = (int)Xratio; 
		Xratio = (Xratio - cnt)/2; 
		old = 0;

		for (y=0;y<200;y++){
			tmp = source;
			tmp2 = target;
			//write pixel x and x-1 in line 1
			for (x=0;x<320;x++){
				for (x1=0;x1<cnt;x1++){
					//copy one pixel ----------------------
					*(target++) = *(source) ;
					old += Xratio;
					//-----------------------------------
					if (old > 1) {
						//copy extra pixel
						*(target++) = *(source) ;				
						old -= 1;
					}
				}
				source++;
			}
			source = tmp + iGLOBAL_SCREENWIDTH;
			target = tmp2 + iGLOBAL_SCREENWIDTH;
		}

		//strech lines in Y direction
		source = ( byte * )( sdl_surface->pixels);//store screen in tmp pic mem
		memcpy( tmpPICbuf, source, (200*iGLOBAL_SCREENWIDTH) );

		source = tmpPICbuf;
		target = ( byte * )( sdl_surface->pixels);//screen buffer

		Yratio = iGLOBAL_SCREENHEIGHT * 10/ 200;//we shall strech 200 lines to 480/600
		Yratio = (Yratio/10);
		cnt = (int)Yratio; //2
		Yratio = (Yratio - cnt)/2; //.2
		NbOfLines=0;//make sure we dont exeed iGLOBAL_SCREENHEIGHT or we get a crash
		old = 0;

		for (y=0;y<200;y++){
			for (y1=0;y1<cnt;y1++){
				//copy one line ----------------------
				memcpy(target, source,iGLOBAL_SCREENWIDTH);
				if (NbOfLines++ >= iGLOBAL_SCREENHEIGHT-1){goto stopx;}
				target += (iGLOBAL_SCREENWIDTH);
				old += Yratio;
				//-----------------------------------
				if (old > 1) {
					//copy extra line
					memcpy(target, source,iGLOBAL_SCREENWIDTH);				
					if (NbOfLines++ >= iGLOBAL_SCREENHEIGHT-1){goto stopx;}
					target += (iGLOBAL_SCREENWIDTH);
					old -= 1;
				}
			}
			source += iGLOBAL_SCREENWIDTH;
		}
stopx:;




}


void SchrinkMemPicture( byte * source)
{ 
	//schrink mem picure and plce it in tmpPICbuf
   	byte *target,*tmp,*tmp2;
	int x,y;
	int cnt,NbOfLines;
	float Yratio,Xratio,old;

	target = tmpPICbuf;

    Xratio = iGLOBAL_SCREENWIDTH * 10/ 320;
	Xratio = (Xratio/10);
	cnt = (int)Xratio; //2
	Xratio = (Xratio - cnt)/2; //.2
	old = 0;	

	// delte redunted pixels
	for (y=0;y<iGLOBAL_SCREENHEIGHT;y++){
		tmp = source;
		tmp2 = target;
		for (x=0;x<iGLOBAL_SCREENWIDTH;x++){
			//copy 1 pixel
			*(target++) = *(source) ;
			source += cnt;
			old += Xratio;
			if (old >= 1) {
				source++;
				old -= 1;
			}
		}
		source = tmp + iGLOBAL_SCREENWIDTH;
		target = tmp2 + iGLOBAL_SCREENWIDTH;
	}
	//delete every redunted lines
	source = tmpPICbuf;//+(1* iGLOBAL_SCREENWIDTH);
	target = tmpPICbuf;

	Yratio = iGLOBAL_SCREENHEIGHT * 10 / 200;//we shall schrink 480/600 lines to 200  
	Yratio = (Yratio/10);
	cnt = (int)Yratio; //2
	Yratio = (Yratio - cnt); //.2
	NbOfLines=0;//make sure we dont exeed iGLOBAL_SCREENHEIGHT or we get a crash
	old = 0;
//SetTextMode (  );
	for (y=0;y<200;y++){
		//if (source > (tmpPICbuf+(iGLOBAL_SCREENWIDTH*iGLOBAL_SCREENHEIGHT))){goto stopy;}
		memcpy(target, source, iGLOBAL_SCREENWIDTH);	
		source += (cnt * iGLOBAL_SCREENWIDTH);
		old += Yratio;
		if (old > 1) {
			//delte extra line
			source += iGLOBAL_SCREENWIDTH;
			old -= 1;
		}
		target += iGLOBAL_SCREENWIDTH;
	}

//stopy:;	
}

// bna function added start
extern	boolean ingame;
int		iG_playerTilt;

void DrawCenterAim ()
{
	int x;
	if (iG_aimCross > 0){
		if (( ingame == true )&&(iGLOBAL_SCREENWIDTH>320)){
			  if ((iG_playerTilt <0 )||(iG_playerTilt >iGLOBAL_SCREENHEIGHT/2)){
					iG_playerTilt = -(2048 - iG_playerTilt);
			  }
			  if (iGLOBAL_SCREENWIDTH == 640){ x = iG_playerTilt;iG_playerTilt=x/2; }
			  iG_buf_center = bufferofs + ((iG_Y_center-iG_playerTilt)*iGLOBAL_SCREENWIDTH);//+iG_X_center;

			  for (x=iG_X_center-10;x<=iG_X_center-4;x++){
				  if ((iG_buf_center+x < bufofsTopLimit)&&(iG_buf_center+x > bufofsBottomLimit)){
					 *(iG_buf_center+x) = 75;
				  }
			  }
			  for (x=iG_X_center+4;x<=iG_X_center+10;x++){
				  if ((iG_buf_center+x < bufofsTopLimit)&&(iG_buf_center+x > bufofsBottomLimit)){
					 *(iG_buf_center+x) = 75;
				  }
			  }
			  for (x=10;x>=4;x--){
				  if (((iG_buf_center-(x*iGLOBAL_SCREENWIDTH)+iG_X_center) < bufofsTopLimit)&&((iG_buf_center-(x*iGLOBAL_SCREENWIDTH)+iG_X_center) > bufofsBottomLimit)){
					 *(iG_buf_center-(x*iGLOBAL_SCREENWIDTH)+iG_X_center) = 75;
				  }
			  }
			  for (x=4;x<=10;x++){
				  if (((iG_buf_center-(x*iGLOBAL_SCREENWIDTH)+iG_X_center) < bufofsTopLimit)&&((iG_buf_center-(x*iGLOBAL_SCREENWIDTH)+iG_X_center) > bufofsBottomLimit)){
					 *(iG_buf_center+(x*iGLOBAL_SCREENWIDTH)+iG_X_center) = 75;
				  }
			  }
		}
	}
}
// bna function added end




// bna section -------------------------------------------



