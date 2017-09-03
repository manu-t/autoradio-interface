/* Test of the Renault Update List display
   The display is connected to the head unit via CAN interface
   CAN ID is 11 bits
   CAN baudrate is 500kbauds
   The display tested is from a Clio II phase 3 (2005)
   Display reference number:
*/

/*
   This code is based from this excellent work: https://translate.googleusercontent.com/translate_c?anno=2&depth=1&hl=fr&rurl=translate.google.com&sl=pl&sp=nmt4&tl=en&u=http://megane.com.pl/topic/47797-wyswietlacz-radia-update-list-protokol/page__hl__wy%25C5%259Bwietlacz%2520radia%2520update%2520list&usg=ALkJrhgGbHh_iqpb54xLhMpq9wUkCJVQuA

  Loose conclusions:
   Packages are sent on a question-answer basis
   For the "answer" package, the response has a larger ID of 0x400, eg for 0x121 it has 0x521
  There are 4 types of packages appearing at:
     Radio on / off (pair: 0x01B1 and 0x05B1)
     Pressing the key on the remote control on the steering wheel (pair: 0xA9 and 0x4A9)
     Change text to display u (pair 0x121 and 0x521)
     All time: pair 0x03CF and 0x03DF
  0xA3 bytes in packets from display and look for 8 byte full-size pushrods
   0x81 bundles in radio packages look like full 8 bytes
   It looks like the radio does not work on single bytes and on 16 bit words, it would explain so many fixed bytes (eg when handling keys)
   Radio uses normal ASCII encoding (and praise him for it: P)

  Package 0x0121:
   In Byte 4, bits 0-3 indicate the number of the key under which the radio station (1-6) is stored.
   Sending text to Display is 4 CAN packets:
  0x121 0 8: \ x10 \ x19v` \ x1 VO
  0x121 0 8:! L 2 \ x10V
  0x121 0 8: "OLUME
  0x121 0 8: # 2 \ x0 \ x81 \ x81


  Remote control keys operation:
   The display sends a message of id 0x00A9, 8 bytes in length
   - 0 byte: 0x03 - looks solid
   - 1 byte: 0x89 - same
   - 2 bytes: 0x00 - jw.
   - 3 bytes: key code: P
   - 4-7: 0xA3 claws
   The radio responds with packet ID 0x04A9, the first byte is 0x74 and the remaining 0x81

*/

#include <SerialCommand.h>

#include "mcp_can.h"
#include <SPI.h>
#include "can_ext.h"

#define AFFA2_KEY_LOAD 0x0000 / * This at the bottom of the remote;) * /
#define AFFA2_KEY_SRC_RIGHT 0x0001
#define AFFA2_KEY_SRC_LEFT 0x0002
#define AFFA2_KEY_VOLUME_UP 0x0003
#define AFFA2_KEY_VOLUME_DOWN 0x0004
#define AFFA2_KEY_PAUSE 0x0005
#define AFFA2_KEY_ROLL_UP 0x0101
#define AFFA2_KEY_ROLL_DOWN 0x0141
#define AFFA2_KEY_HOLD_MASK (0x80 | 0x40)

MCP_CAN CAN(9/*10*/);  // 9: seeedstudio; 10: sparkfun

INT8U Flag_Recv = 0;
INT8U len = 0;
INT8U buf[8];
INT32U canId = 0x000;

uint8_t test_packet[] = {
  0x10, /* Set text */
  0x1C, /* If we want to inform about them display here 0x1C and additional bytes further */
  0x7F, /* ??? */
  0x55, /* For old type: station number not set */
  0x55, /* Normal text */
  '1', '2', '3', '4', '5', '6', '7', '8', /* 8 characters for the old type */
  0x10, /* Serparator? */
  'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd', '!', '\0', /* 12 characters for new type + null byte */
};


uint8_t pingMsg[] = {
  'y', 
  0x00,
  0x81,
  0x81,
  0x81,
  0x81,
  0x81,
  0x81,
};

unsigned char msg5c1[8] = {0x74, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81};
unsigned char msg4a9[8] = {0x74, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81};

SerialCommand sCmd;     // The SerialCommand object

void setup() {
  Serial.begin(115200);

  // Setup callbacks for SerialCommand commands
  sCmd.addCommand("p", printDisp);          // print message on display
  sCmd.addCommand("t", testDisp);          // print test message on display
  sCmd.addCommand("ss", startSync);       //
  sCmd.addCommand("so", syncOK);          //
  sCmd.addCommand("sd", syncDisp);          //
  sCmd.addCommand("r", registerDisplay);  //
  sCmd.addCommand("e", enableDisplay);    // 
  sCmd.addCommand("i", initDisplay);    // 
  sCmd.addCommand("mt", messageTest);  //
  sCmd.addCommand("m", messageTextIcons);  //
  sCmd.addCommand("d", displaySong);  //
  sCmd.setDefaultHandler(unrecognized);      // Handler for command that isn't matched  (says "What?")

  // CAN 11 bits 500kbauds
  if (CAN.begin(CAN_500KBPS) == CAN_OK)  // Baud rates defined in mcp_can_dfs.h
    Serial.print("CAN Init OK.\n\r\n\r");
  else
    Serial.print("CAN Init Failed.\n\r");

  // CAN Rx interrupt declaration
  attachInterrupt(0, MCP2515_ISR, FALLING); // start interrupt

//  // set
//  unsigned char txtOnMsg[8] = {0x70, 0xA2, 0xA2, 0xA2, 0xA2, 0xA2, 0xA2, 0xA2};
////  CAN.sendMsgBuf(0x1c1, 0, 8, txtOnMsg);
////  delay(1000);
//  CAN.sendMsgBuf(0xa9, 0, 8, txtOnMsg);
//  delay(1000); // display should respond 0x4A9

  // set
//  unsigned char OnPressed1Msg[8] = {0x70, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81}; // On button push
//  CAN.sendMsgBuf(0x1B1, 0, 8, OnPressed1Msg);
//  delay(100);
//  unsigned char releasedMsg[8] = {0x74, 0xA2, 0xA2, 0xA2, 0xA2, 0xA2, 0xA2, 0xA2};
//  CAN.sendMsgBuf(0x5B1, 0, 8, releasedMsg);
//  delay(10);
//  unsigned char OnPressedMsg[8] = {0x04, 0x52, 0x02, 0xFF, 0xFF, 0x81, 0x81, 0x81}; // On button push
//  CAN.sendMsgBuf(0x1B1, 0, 8, OnPressedMsg);
//  delay(100);
  //
//  CAN.sendMsgBuf(0x5B1, 0, 8, releasedMsg);
//  delay(10);
  
//  unsigned char OnReleasedMsg[8] = {0x04, 0x52, 0x00, 0xFF, 0xFF, 0x81, 0x81, 0x81}; // On button release
//  CAN.sendMsgBuf(0x1B1, 0, 8, OnReleasedMsg);
//  delay(10);
//  //
//  CAN.sendMsgBuf(0x5B1, 0, 8, releasedMsg);
//  delay(10);

//  unsigned char msg1c1[8] = {0x02, 0x64, 0x0F, 0xA2, 0xA2, 0xA2, 0xA2, 0xA2};
//  CAN.sendMsgBuf(0x1C1, 0, 8, msg1c1);
//  delay(10);

  startSync();
  delay(1);
  syncOK();
  delay(1);
//  startSync();
//  delay(1);
  syncDisp(); // déclenche 1c1 et 0a9 côté afficheur : répondre 5C1 et 4A9
  delay(10);
  CAN.sendMsgBuf(0x5C1, NULL, 8, msg5c1);
  CAN.sendMsgBuf(0x4A9, NULL, 8, msg4a9);
  initDisplay();
  delay(1);
  registerDisplay();
  delay(1);
  enableDisplay();
  delay(50);
  delay(10);

//  send_to_display(0x121, test_packet, sizeof(test_packet));
}

void loop() {
  // receive CAN
  if(Flag_Recv) {                 // check if data was recieved
    Flag_Recv = 0;                // clear flag
    // iterate over all pending messages
    while (CAN_MSGAVAIL == CAN.checkReceive()) {      
      CAN.readMsgBuf(&len, buf);    // read data,  len: data length, buf: data buf
      canId = CAN.getCanId();
//      //Print data to the serial console 
//      Serial.print("CAN ID: ");
//      Serial.println(canId);
//      Serial.print("data len = ");Serial.println(len);
//      //This loops through each byte of data and prints it
//      for(int i = 0; i<len; i++) {   // print the data
//        Serial.print(buf[i]);Serial.print("\t");
//      }
//      Serial.println();Serial.println();
      if(canId == 0x3CF) {  // pong received  // TODO: check entire frame (not just can ID)
//        CAN.sendMsgBuf(0x3DF, NULL, 8, pingMsg);
          //Serial.println("Pion");Serial.println();
      }
//      else if(canId == 0x521) { // TODO: check entire frame (not just can ID)
//        Serial.println("TEXT ACK received"); Serial.println();
//      }
      else if(canId == 0x1C1) {
        //unsigned char msg5c1[8] = {0x74, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81};
        CAN.sendMsgBuf(0x5C1, NULL, 8, msg5c1);
//        CAN.sendMsgBuf(0x5C1, NULL, 8, msg5c1);
//        Serial.println("0x1C1");
      }
      else if(canId == 0x0A9) {
        //unsigned char msg4a9[8] = {0x74, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81};
        CAN.sendMsgBuf(0x4A9, NULL, 8, msg4a9);
//        CAN.sendMsgBuf(0x4A9, NULL, 8, msg4a9);
//        Serial.println("0x0A9");
      }
      else {
//        Serial.print("CAN ID: "); Serial.println(canId, HEX); Serial.println();
      }
    }
  }

  // serial interpreter:
  sCmd.readSerial();     // We don't do much, just process serial commands

  // TODO: periodically send sync command to display (100ms -> 1sec)
  syncOK();
  delay(200);
}

void MCP2515_ISR() {
     Flag_Recv = 1;
}

void do_send_to(uint16_t id, uint8_t * data, uint8_t datasz, uint8_t filler) {
  unsigned char packet[8] = {'\0'};
  uint8_t packetnum = 0, i, slen = datasz;

  while (slen > 0) {
    i = 0;
    if (packetnum > 0) {
      packet[0] = 0x20 + packetnum; /* Kolejny pakiet z jednego komunikatu */
      i++;
    }

    while ((i < 8) && (slen > 0)) {
      packet[i] = *data;
      data++;
      slen--;
      i++;
    }

    for (; i < 8; i++)
      packet[i] = filler;

    CAN.sendMsgBuf((unsigned long)(id), NULL, 8, packet);
    //canTransmit((unsigned long)(id), packet, 8);
//    Serial.println(packetnum);
    packetnum++;
    delay(2); /* TODO: tutaj powinniśmy poczekać na odpowiedź wyświetlacza / radia zamiast tego delay */
   if(Flag_Recv) {                 // check if data was recieved
      Flag_Recv = 0;                // clear flag
      while (CAN_MSGAVAIL == CAN.checkReceive()) {      
        CAN.readMsgBuf(&len, buf);    // read data,  len: data length, buf: data buf
//        Serial.print(CAN.getCanId(), HEX); Serial.println(" received");
      }
   }    
  }
}

void send_to_display(uint16_t id, uint8_t * data, uint8_t datasz) {
  do_send_to(id, data, datasz, 0x81);
}

void send_to_hu(uint16_t id, uint8_t * data, uint8_t datasz) {
  do_send_to(id, data, datasz, 0xA2);
}

void printDisp() {
  char *arg;
  int aNumber;

  String s = "";

//  Serial.println("Display: ");

  arg = sCmd.next();
  while (arg != NULL) {
    s += arg;
    arg = sCmd.next();
    if(arg != NULL) {  // check if space separator (if several spaces, just one is inserted)
      s += ' ';
    }
    if(s.length() > 8) {  // check if length max
      s.remove(8);    
    }
  }
//  Serial.print(s); Serial.print(" ("); Serial.print(s.length()); Serial.println(")");
//  Serial.println();

  while(s.length()<8) {
    s += ' '; // pad string with spaces to make 8 byte string
  }

  String TextCmd = "";
  TextCmd += '\x10';
  TextCmd += '\x19';
  TextCmd += '\x76';
  TextCmd += '\x60';
  TextCmd += '\x01';
  TextCmd += s;
  TextCmd += '\x10';
  TextCmd += s;
  TextCmd += "    ";  // 4 spaces to make a 12-byte string
  char charArray[28] = {'\0'};
  TextCmd.toCharArray(charArray, 27);
//  Serial.println(TextCmd);
//  for(int i = 0; i < 28; i++) {
//    Serial.print(charArray[i], HEX); Serial.print(' ');
//  }
//  Serial.println();
  send_to_display(0x121,(uint8_t *)(charArray), 27);
}

void testDisp() {
//  Serial.println("Send test packet to display");
  send_to_display(0x121, test_packet, sizeof(test_packet));
}

// This gets set as the default handler, and gets called when no other command matches.
void unrecognized(const char *command) {
  Serial.println(F("What?"));
}

void startSync() {
  unsigned char startSyncMsg[8] = {0x7A, 0x01, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81};
  CAN.sendMsgBuf(0x3DF, NULL, 8, startSyncMsg);    
}

void syncOK() {
  CAN.sendMsgBuf(0x3DF, NULL, 8, pingMsg);
}

void syncDisp() {
  unsigned char syncDispMsg[8] = {0x70, 0x1A, 0x11, 0x00, 0x00, 0x00, 0x00, 0x01};
  CAN.sendMsgBuf(0x3DF, NULL, 8, syncDispMsg);
}

void registerDisplay() {
  unsigned char registerDispMsg[8] = {0x70, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81};
  CAN.sendMsgBuf(0x1B1, NULL, 8, registerDispMsg);
}

void enableDisplay() {
  unsigned char enableDispMsg[8] = {0x04, 0x52, 0x02, 0xFF, 0xFF, 0x81, 0x81, 0x81};
  CAN.sendMsgBuf(0x1B1, NULL, 8, enableDispMsg);
}

void initDisplay() {
  unsigned char initDispMsg[8] = {0x70, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81};
  CAN.sendMsgBuf(0x121, NULL, 8, initDispMsg);
}

void messageTest() {
  unsigned char msg11[8] = {0x10, 0x19, '~', 'q', 0x01, 'E', 'U', 'R'};
  unsigned char msg12[8] = {0x21, 'O', 'P', 'E', ' ', '2', 0x10, ' '}; 
  unsigned char msg13[8] = {0x22, 'E', 'U', 'R', 'O', 'P', 'E', ' '};
  unsigned char msg14[8] = {0x23, '2', ' ', 'P', '1', 0x00, 0x81, 0x81};

  CAN.sendMsgBuf(0x121, NULL, 8, msg11);
  delay(1);
  CAN.sendMsgBuf(0x121, NULL, 8, msg12);
  delay(1);
  CAN.sendMsgBuf(0x121, NULL, 8, msg13);
  delay(1);
  CAN.sendMsgBuf(0x121, NULL, 8, msg14);
  delay(1);
}

void messageTextIcons() {
  unsigned char msg1[8] = {0x10, 0x1C, 0x7F, 0x55, 0x55, 0x3F, 0x60, 0x01};
  unsigned char msg2[8] = {0x21 ,0x46 ,0x4D ,0x20 ,0x20 ,0x20 ,0x20 ,0x20};
  unsigned char msg3[8] = {0x22 ,0x20 ,0x10 ,0x52 ,0x41 ,0x44 ,0x49 ,0x4F};
  unsigned char msg4[8] = {0x23 ,0x20 ,0x46 ,0x4D ,0x20 ,0x20 ,0x20 ,0x20};
  unsigned char msg5[8] = {0x24 ,0x00 ,0x81 ,0x81 ,0x81 ,0x81 ,0x81 ,0x81};

  CAN.sendMsgBuf(0x121, NULL, 8, msg1);
  delay(1);
  CAN.sendMsgBuf(0x121, NULL, 8, msg2);
  delay(1);
  CAN.sendMsgBuf(0x121, NULL, 8, msg3);
  delay(1);
  CAN.sendMsgBuf(0x121, NULL, 8, msg4);
  delay(1);
  CAN.sendMsgBuf(0x121, NULL, 8, msg5);
  delay(1);
}

void displaySong() {
  char *arg;
  int aNumber;

  String s = "";

//  Serial.println("Display: ");

  arg = sCmd.next();
  while (arg != NULL) {
    s += arg;
    arg = sCmd.next();
    if(arg != NULL) {  // check if space separator (if several spaces, just one is inserted)
      s += ' ';
    }
  }
  Serial.print(s); Serial.print(" ("); Serial.print(s.length()); Serial.println(")");
  Serial.println();

//  // print 8 bytes at a time
//  for(int i=0; i<s.length(); i+=8) {
//    Serial.println(i);
//    String s8 = "";  // 8 byte string
//    if( (i+8) > s.length()) {
//      // pad last section to 8 bytes
//      s8 = s.substring(i,s.length());
//      while(s8.length()<8) {
//        s8 += ' '; // pad string with spaces to make 8 byte string
//      } 
//    }
//    else {
//      s8 = s.substring(i,i+8);  
//    }
//    Serial.println(s8);
//    
//    display8ByteString(s8);
//    delay(1000);
//  }

//  while( (s.length()%8) != 0) {
//    s += ' '; // pad string with spaces to make 8 byte sections string
//  }

  Serial.print(s); Serial.print(" ("); Serial.print(s.length()); Serial.println(")");
  Serial.println();

  String s8 = s.substring(0,8);
  while( s8.length() < 8) {
    s8 += ' ';  // pad to 8 byte string  
  }
  Serial.println(s8);
  
  // display 1st 8 bytes
  display8ByteString(s8);
  syncOK(); // every 100ms to 1sec
  delay(1000);
  syncOK(); // every 100ms to 1sec
  
  // scroll after 8 bytes
  for(int i=8;i<s.length();i++) {
    Serial.println(i);
    if( (i+8) >= s.length()) {
      // pad last section to 8 bytes
      s8 = s.substring(i,s.length());
      while(s8.length()<8) {
        s8 += ' '; // pad string with spaces to make 8 byte string
      }
      i=s.length(); // stop scrolling at last section
    }
    else {
      s8 = s.substring(i,i+8);  
    }
    
    Serial.println(s8);
    display8ByteString(s8);
    delay(500);
    syncOK(); // every 100ms to 1sec
  }
}

void display8ByteString(String s) {
  if(s.length()!=8 ) {
    Serial.println("String must be 8 bytes long");
    return;
  }
  
  String TextCmd = "";
  TextCmd += '\x10';
  TextCmd += '\x19';
  TextCmd += '\x76';
  TextCmd += '\x60';
  TextCmd += '\x01';
  TextCmd += s;
  TextCmd += '\x10';
  TextCmd += s.substring(0,7);
  TextCmd += "    ";  // 4 spaces to make a 12-byte string
  char charArray[28] = {'\0'};
  TextCmd.toCharArray(charArray, 27);
//  Serial.println(TextCmd);
//  for(int i = 0; i < 28; i++) {
//    Serial.print(charArray[i], HEX); Serial.print(' ');
//  }
//  Serial.println();
  send_to_display(0x121,(uint8_t *)(charArray), 27);
}
