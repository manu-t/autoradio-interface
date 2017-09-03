// ------------------------------------------------------------------------
// J1939 CAN Connection
// ------------------------------------------------------------------------
#include "mcp_can.h"

#define CS_PIN                                 9     // Use pin 10 for Seeed Studio CAN Shield up to version 1.0
                                                     // Use pin 9 for Seeed Studion CAN shiled version 1.1 and higher
                                                     // Use pin 10 for SK Pang CAN shield
MCP_CAN CAN0(CS_PIN);

// ------------------------------------------------------------------------
// CAN message ring buffer setup
// ------------------------------------------------------------------------
#define CANMSGBUFFERSIZE 10
struct CANMsg
{
  long lID;
  unsigned char pData[8];
  int nDataLen;
};
CANMsg CANMsgBuffer[CANMSGBUFFERSIZE];
int nWritePointer;
int nReadPointer;

// ------------------------------------------------------------------------
// Initialize the CAN controller
// ------------------------------------------------------------------------
int canInitialize(int nBaudRate)
{
  // Default settings
  nReadPointer = 0;
  nWritePointer = 0;
  
  // Initialize the CAN controller
  if(CAN0.begin(nBaudRate) == 0)
    return 0;
  else return 1;

}// end canInitialize

// ------------------------------------------------------------------------
// Transmit CAN message
// ------------------------------------------------------------------------
byte canTransmit(long lID, unsigned char* pData, int nDataLen)
{
  
  if(CAN0.sendMsgBuf(lID, CAN_STDID, nDataLen, pData) == 0)
    return 0;
  else
    return 1;
  
}// end canTransmit

// ------------------------------------------------------------------------
// Receive CAN message
// ------------------------------------------------------------------------
byte canReceive(long* lID, unsigned char* pData, int* nDataLen)
{
  // Declarations
  byte nCnt;

  // In case there is a message, put it into the buffer
  while(CAN0.checkReceive() == CAN_MSGAVAIL)
  {
    // Read the message buffer
    CAN0.readMsgBuf(&nCnt, &CANMsgBuffer[nWritePointer].pData[0]);
    CANMsgBuffer[nWritePointer].nDataLen = (int)nCnt;
    CANMsgBuffer[nWritePointer].lID = CAN0.getCanId();    
    
    if(++nWritePointer == CANMSGBUFFERSIZE)
      nWritePointer = 0;
    
  }// end while

  // Check ring buffer for a message
  if(nReadPointer != nWritePointer)
  {
    // Read the next message buffer entry
    *nDataLen = CANMsgBuffer[nReadPointer].nDataLen;
    *lID = CANMsgBuffer[nReadPointer].lID;

    for(int nIdx = 0; nIdx < *nDataLen; nIdx++)
      pData[nIdx] = CANMsgBuffer[nReadPointer].pData[nIdx];

    if(++nReadPointer == CANMSGBUFFERSIZE)
      nReadPointer = 0;
    
    return 0;
    
  }// end if
  else return 1;

}// end canReceive

// ------------------------------------------------------------------------
// J1939 Check Peer-to-Peer
// ------------------------------------------------------------------------
bool j1939PeerToPeer(long lPGN)
{
  // Check the PGN 
  if(lPGN > 0 && lPGN <= 0xEFFF)
    return true;

  return false;

}// end j1939PeerToPeer

// ------------------------------------------------------------------------
// J1939 Transmit
// ------------------------------------------------------------------------
byte j1939Transmit(long lPGN, byte nPriority, byte nSrcAddr, byte nDestAddr, byte* nData, int nDataLen)
{
  // Declarations
  long lID = ((long)nPriority << 26) + (lPGN << 8) + (long)nSrcAddr;
  
  // If PGN represents a peer-to-peer, add destination address to the ID
  if(j1939PeerToPeer(lPGN) == true)
  {
    lID = lID & 0xFFFF00FF;
    lID = lID | ((long)nDestAddr << 8);
    
  }// end if
  
  // Transmit the message
  return canTransmit(lID, nData, nDataLen);
  
}// end j1939Transmit

// ------------------------------------------------------------------------
// J1939 Receive
// ------------------------------------------------------------------------
byte j1939Receive(long* lPGN, byte* nPriority, byte* nSrcAddr, byte *nDestAddr, byte* nData, int* nDataLen)
{
  // Declarations
  byte nRetCode = 1;
  long lID;
  
  // Default settings
  *nSrcAddr = 255;
  *nDestAddr = 255;
  
  if(canReceive(&lID, nData, nDataLen) == 0)
  {
    long lPriority = lID & 0x1C000000;
    *nPriority = (byte)(lPriority >> 26);
    
    *lPGN = lID & 0x00FFFF00;
    *lPGN = *lPGN >> 8;

    lID = lID & 0x000000FF;
    *nSrcAddr = (byte)lID;
    
    if(j1939PeerToPeer(*lPGN) == true)
    {
      *nDestAddr = (byte)(*lPGN & 0xFF);
      *lPGN = *lPGN & 0x01FF00;
    }

    nRetCode = 0;
    
  }// end if
  
  return nRetCode;
  
}// end j1939Receive

