/*************************************************************************
 *
 *  vme_list.c - Readout of C775 TDC with a pipeline TI as the trigger source
 *
 */

/* Event Buffer definitions */
#define MAX_EVENT_POOL     10
#define MAX_EVENT_LENGTH   1024*60      /* Size in Bytes */

/* Define Interrupt source and address */
#define TI_MASTER
#define TI_READOUT TI_READOUT_EXT_POLL  /* Poll for available data, external triggers */
#define TI_ADDR    (21<<19)          /* GEO slot 21 */

/* Decision on whether or not to readout the TI for each block 
   - Comment out to disable readout 
*/
#define TI_DATA_READOUT

#define FIBER_LATENCY_OFFSET 0x4A  /* measured longest fiber length */

#include "dmaBankTools.h"
#include "tiprimary_list.c" /* source required for CODA */

/* Default block level */
unsigned int BLOCKLEVEL=1;
#define BUFFERLEVEL 3

/* Redefine tsCrate according to TI_MASTER or TI_SLAVE */
#ifdef TI_SLAVE
int tsCrate=0;
#else
#ifdef TI_MASTER
/* int tsCrate=1; */
#endif
#endif

#define TDC_ID 0
#define MAX_TDC_DATA 34

/* function prototype */
void rocTrigger(int arg);

/****************************************
 *  DOWNLOAD
 ****************************************/
void
rocDownload()
{

  /* Setup Address and data modes for DMA transfers
   *   
   *  vmeDmaConfig(addrType, dataType, sstMode);
   *
   *  addrType = 0 (A16)    1 (A24)    2 (A32)
   *  dataType = 0 (D16)    1 (D32)    2 (BLK32) 3 (MBLK) 4 (2eVME) 5 (2eSST)
   *  sstMode  = 0 (SST160) 1 (SST267) 2 (SST320)
   */
  vmeDmaConfig(2,5,1); 

  /*****************
   *   TI SETUP
   *****************/
  int overall_offset=0x80;

#ifndef TI_DATA_READOUT
  /* Disable data readout */
  tiDisableDataReadout();
  /* Disable A32... where that data would have been stored on the TI */
  tiDisableA32();
#endif

  /* Set crate ID */
  tiSetCrateID(0x01); /* ROC 1 */

  tiSetTriggerSource(TI_TRIGGER_TSINPUTS);

  /* Set needed TS input bits */
  tiEnableTSInput( TI_TSINPUT_1 );

  /* Load the trigger table that associates 
     pins 21/22 | 23/24 | 25/26 : trigger1
     pins 29/30 | 31/32 | 33/34 : trigger2
  */
  tiLoadTriggerTable(0);

  tiSetTriggerHoldoff(1,10,0);
  tiSetTriggerHoldoff(2,10,0);

/*   /\* Set the sync delay width to 0x40*32 = 2.048us *\/ */
  tiSetSyncDelayWidth(0x54, 0x40, 1);

  /* Set the busy source to non-default value (no Switch Slot B busy) */
  tiSetBusySource(TI_BUSY_LOOPBACK,1);

#ifdef TI_MASTER
  /* Set number of events per block */
  tiSetBlockLevel(BLOCKLEVEL);
#endif

  tiSetEventFormat(1);

  tiSetBlockBufferLevel(BUFFERLEVEL);

  tiStatus(0);

  c775Init(0x110000,0,1,0); /* A24 = 0x11, A32 = 0x0A11 */


  printf("rocDownload: User Download Executed\n");

}

/****************************************
 *  PRESTART
 ****************************************/
void
rocPrestart()
{
  unsigned short iflag;
  int stat;
  int islot;

  tiStatus(0);

  /* Setup TDCs (no sparcification, enable berr for block reads) */
  c775Clear(TDC_ID);
  c775DisableBerr(TDC_ID);
  /*   c775EnableBerr(TDC_ID); /\* for 32bit block transfer *\/ */
  c775CommonStop(TDC_ID);
  
  c775Status(TDC_ID);

  printf("rocPrestart: User Prestart Executed\n");

}

/****************************************
 *  GO
 ****************************************/
void
rocGo()
{
  int islot;
  /* Enable modules, if needed, here */

  /* Get the current block level */
  BLOCKLEVEL = tiGetCurrentBlockLevel();
  printf("%s: Current Block Level = %d\n",
	 __FUNCTION__,BLOCKLEVEL);

  c775Enable(TDC_ID);

  /* Use this info to change block level is all modules */

}

/****************************************
 *  END
 ****************************************/
void
rocEnd()
{

  int islot;

  c775Disable(TDC_ID);

  c775Status(TDC_ID,0,0);
  tiStatus(0);

  printf("rocEnd: Ended after %d blocks\n",tiGetIntCount());
  
}

/****************************************
 *  TRIGGER
 ****************************************/
void
rocTrigger(int arg)
{
  int ii, islot, status;
  int stat, dCnt, len=0, idata;
  int ev_type = 0;
  int itimeout=0;
  
  tiSetOutputPort(1,0,0,0);

  BANKOPEN(5,BT_UI4,0);
  *dma_dabufp++ = LSWAP(tiGetIntCount());
  *dma_dabufp++ = LSWAP(0xdead);
  *dma_dabufp++ = LSWAP(0xcebaf111);
  BANKCLOSE;


#ifdef TI_DATA_READOUT
  BANKOPEN(4,BT_UI4,0);

  vmeDmaConfig(2,5,1); 
  dCnt = tiReadBlock(dma_dabufp,8+(5*BLOCKLEVEL),1);
  if(dCnt<=0)
    {
      printf("No data or error.  dCnt = %d\n",dCnt);
    }
  else
    {
      ev_type = tiDecodeTriggerType(dma_dabufp, dCnt, 1);
      if(ev_type <= 0)
	{
	  /* Could not find trigger type */
	  ev_type = 1;
	}
      
      /* CODA 2.x only allows for 4 bits of trigger type */
      ev_type &= 0xF; 

      the_event->type = ev_type;

      dma_dabufp += dCnt;
    }

  BANKCLOSE;
#endif

  while(itimeout<1000)
    {
      itimeout++;
      status = c775Dready(TDC_ID);
      if(status>0) break;
    }
  if(status > 0) 
    {
      BANKOPEN(13, BT_UI4, 0);
      dCnt = c775ReadEvent(TDC_ID,dma_dabufp);
      /* or use c775ReadBlock, if BERR was enabled */
      /* 	  dCnt = c775ReadBlock(TDC_ID,dma_dabufp,MAX_TDC_DATA); */
      if(dCnt<=0) 
	{
	  logMsg("ERROR: TDC Read Failed - Status 0x%x\n",dCnt,0,0,0,0,0);
	  *dma_dabufp++ = 0xda000bad;
	  c775Clear(TDC_ID);
	} 
      else 
	{
	  dma_dabufp += dCnt;
	}

      BANKCLOSE;
    }
  else
    {
      logMsg("ERROR: NO data in TDC  datascan = 0x%x, itimeout=%d\n",status,itimeout,0,0,0,0);
      c775Clear(TDC_ID);
    }

  tiSetOutputPort(0,0,0,0);

}

void
rocCleanup()
{

  
}
