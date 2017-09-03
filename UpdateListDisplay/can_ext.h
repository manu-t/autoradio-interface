#ifndef CAN_EXT_H
#define CAN_EXT_H

extern int canInitialize(int nBaudRate);
extern byte canTransmit(long lID, unsigned char* pData, int nDataLen);
extern byte canReceive(long* lID, unsigned char* pData, int* nDataLen);
extern byte j1939Transmit(long lPGN, byte nPriority, byte nSrcAddr, byte nDestAddr, byte* nData, int nDataLen);
extern byte j1939Receive(long* lPGN, byte* nPriority, byte* nSrcAddr, byte *nDestAddr, byte* nData, int* nDataLen);

#endif
