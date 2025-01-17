/*************************************************************************
 *
 *  c775_linux_list.c - Readout of C775 TDC with a TIR
 *
 */

/* Event Buffer definitions */
#define MAX_EVENT_POOL     400
#define MAX_EVENT_LENGTH   1024*10      /* Size in Bytes */

/* Define Interrupt source and address */
#define TIR_SOURCE
#define TIR_ADDR 0x0ed0
#define TIR_MODE TIR_EXT_POLL

#define TDC_ID 0
#define MAX_TDC_DATA 34

#include "linuxvme_list.c"
#include "c775Lib.h"

/* function prototype */
void rocTrigger(int arg);

void
rocDownload()
{
  int dmaMode;

  /* Setup Address and data modes for DMA transfers
   *   
   *  vmeDmaConfig(addrType, dataType, sstMode);
   *
   *  addrType = 0 (A16)    1 (A24)    2 (A32)
   *  dataType = 0 (D16)    1 (D32)    2 (BLK32) 3 (MBLK) 4 (2eVME) 5 (2eSST)
   *  sstMode  = 0 (SST160) 1 (SST267) 2 (SST320)
   */
  vmeDmaConfig(1,2,0); 

  c775Init(0x110000,0,1,0); /* A24 = 0x11, A32 = 0x0A11 */

  printf("rocDownload: User Download Executed\n");

}

void
rocPrestart()
{
  unsigned short iflag;
  int stat;

  /* Program/Init VME Modules Here */
  /* Setup TDCs (no sparcification, enable berr for block reads) */
  c775Clear(TDC_ID);
  c775DisableBerr(TDC_ID);
/*   c775EnableBerr(TDC_ID); /\* for 32bit block transfer *\/ */
  c775CommonStop(TDC_ID);

  //c775Status(TDC_ID,0,0);
  c775Status(TDC_ID);
  

  printf("rocPrestart: User Prestart Executed\n");

}

void
rocGo()
{
  /* Enable modules, if needed, here */
  c775Enable(TDC_ID);

  /* Interrupts/Polling enabled after conclusion of rocGo() */
}

void
rocEnd()
{
  int status, count;
  
  //c775Status(TDC_ID,0,0);
  c775Status(TDC_ID);

  c775Disable(TDC_ID);

  printf("rocEnd: Ended after %d events\n",tirGetIntCount());
  
}

void
rocTrigger(int arg)
{
  int ii, status, dma, count;
  int nwords;
  unsigned int datascan, tirval, vme_addr;
  int length,size;
  int itimeout=0;

  *dma_dabufp++ = LSWAP(tirGetIntCount()); /* Insert Event Number */

  /* Check if an Event is available */

  while(itimeout<1000)
    {
      itimeout++;
      status = c775Dready(TDC_ID);
      if(status>0) break;
    }
  if(status > 0) 
    {
      if(tirGetIntCount() %1000==0)
	{
	  printf("itimeout = %d\n",itimeout);
	  c775PrintEvent(TDC_ID,0);
	}
      else
	{
	  nwords = c775ReadEvent(TDC_ID,dma_dabufp);
	  /* or use c775ReadBlock, if BERR was enabled */
/* 	  nwords = c775ReadBlock(TDC_ID,dma_dabufp,MAX_TDC_DATA); */
	  if(nwords<=0) 
	    {
	      logMsg("ERROR: TDC Read Failed - Status 0x%x\n",nwords,0,0,0,0,0);
	      *dma_dabufp++ = 0xda000bad;
	      c775Clear(TDC_ID);
	    } 
	  else 
	    {
	      dma_dabufp += nwords;
	    }
	}
    }
  else
    {
      logMsg("ERROR: NO data in TDC  datascan = 0x%x, itimeout=%d\n",status,itimeout,0,0,0,0);
      c775Clear(TDC_ID);
    }

  *dma_dabufp++ = LSWAP(0xda0000ff); /* Event EOB */

}
