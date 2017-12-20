/* 20/12/2017: new bluetooth module: RN-52 (instead of Seedstudio bluetooth serial)
   tested with Mikroelektronika's BT audio click MIKROE-2399 : https://www.mikroe.com/bt-audio-click
*/ 

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

#define DATA_CMD_PIN  3  // change RN52 mode : Data or command
enum RN52MODE {
  DATA_MODE,  // 0
  CMD_MODE   // 1
};

#include <SerialCommand.h>

#include "mcp_can.h"
#include <SPI.h>
#include "can_ext.h"

#include <RN52.h>
RN52 rn52(7,8);  //set RX to pin 7 and TX to pin 8 on Arduino (other way round on RN52)

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

String Txt = "";      // text to display

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

  rn52.begin(9600);

  // CAN 11 bits 500kbauds
  if (CAN.begin(CAN_500KBPS) == CAN_OK)  // Baud rates defined in mcp_can_dfs.h
    Serial.print("CAN Init OK.\n\r\n\r");
  else
    Serial.print("CAN Init Failed.\n\r");

  // CAN Rx interrupt declaration
  attachInterrupt(0, MCP2515_ISR, FALLING); // start interrupt

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

  pinMode(DATA_CMD_PIN, OUTPUT);
  digitalWrite(DATA_CMD_PIN, CMD_MODE);
  rn52.reconnectLast();

}

void loop() {
  // receive CAN
  if(Flag_Recv) {                 // check if data was recieved
    Flag_Recv = 0;                // clear flag
    // iterate over all pending messages
    while (CAN_MSGAVAIL == CAN.checkReceive()) {      
      CAN.readMsgBuf(&len, buf);    // read data,  len: data length, buf: data buf
      canId = CAN.getCanId();
      if(canId == 0x3CF) {  // pong received  // TODO: check entire frame (not just can ID)
//        CAN.sendMsgBuf(0x3DF, NULL, 8, pingMsg);
          //Serial.println("Pion");Serial.println();
      }
//      else if(canId == 0x521) { // TODO: check entire frame (not just can ID)
//        Serial.println("TEXT ACK received"); Serial.println();
//      }
      else if(canId == 0x1C1) {
        CAN.sendMsgBuf(0x5C1, NULL, 8, msg5c1);
      }
      else if(canId == 0x0A9) {
        CAN.sendMsgBuf(0x4A9, NULL, 8, msg4a9);
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

  Txt = "";

  String artistStr = rn52.trackTitle();
  Serial.print("Artist: "); Serial.println(artistStr);    //ouifm : track = artist !
  if(artistStr != "") {
    Txt += artistStr;
  }
  String titleStr = rn52.album();
  Serial.print("Title: "); Serial.println(titleStr); //ouifm : album = title !
  if(titleStr != "") {
    Txt += " - "; // separator
    Txt += titleStr;
  }
  
  // pre-filter known bad strings:
  Txt.replace("Album=", "");
  Txt.replace("Title=", "");
  Txt.replace("OÜI FM Alternatif", "OUIFM AL");
  Txt.replace("OÜI FM Rock 2000", "OUIFM 2K");
  Txt.replace("OÜI FM Rock 90's", "OUIFM 90");
  Txt.replace("OÜI FM Rock 80's", "OUIFM 80");
  Txt.replace("OÜI FM Rock 70's", "OUIFM 70");
  Txt.replace("OÜI FM Rock 60's", "OUIFM 60");
  Txt.replace("OÜI FM", "OUI FM");

  Serial.println(Txt);
  Serial.println(Txt.length()-1); // doesn't count ending char

//  static int count = 0;
//  if(Txt == "") { // empty string at startup
//    display8ByteString("HELLO   ");
//    if(count++ > 25) {
//      Txt == "";
//      display8ByteString("        ");
//    }
//  }
  
  String teststr = "ZACK DE LA ROCHA - WE WANT IT ALL";
  //scrollDisplay(teststr/*Txt*/);
  //wordScroll(/*teststr*/Txt);
  //semiScroll(/*teststr*/Txt);
  delay(500);
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

  scrollDisplay(s);
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

void scrollDisplay(String s) {
  if(s.length() == 0) {
    return;  
  }

  s.trim(); // remove \r\n at the end of string
  
  //Serial.print(s); Serial.print(" ("); Serial.print(s.length()); Serial.println(")");
  //Serial.println();

  String s8 = s.substring(0,8);
  while( s8.length() < 8) {
    s8 += ' ';  // pad to 8 byte string  
  }
  //Serial.println(s8);
  
  // display 1st 8 bytes
  display8ByteString(s8);
  syncOK(); // every 100ms to 1sec
  delay(1000);
  syncOK(); // every 100ms to 1sec
  
  // scroll after 8 bytes
  for(int i=8;i<s.length();i++) {
    //Serial.println(i);
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
    
    //Serial.println(s8);
    display8ByteString(s8);
    delay(500);
    syncOK(); // every 100ms to 1sec
    delay(500);
  }
}

void wordScroll(String s) {
  if(s.length() == 0) {
    return;  
  }

  s.trim(); // remove \r\n at the end of string
  s += '/';
  s += ' '; // add space at the end for tokenization
  //Serial.println(s);

  // display entire word (if more than 8 char, split)
  int wordIdx = 0;
  int start = 0;
  while(wordIdx < s.length()) {
    wordIdx = s.indexOf(' ',start);
    if(wordIdx == -1) {
      exit;  
    }
    //Serial.println(wordIdx);
    String subs = s.substring(start,wordIdx);
    //Serial.println(subs);
    String s8 = "";  // 8 byte string
    for(int i=0; i<subs.length(); i+=8) {
      if( (i+8) > subs.length()) {
        // pad last section to 8 bytes
        s8 = subs.substring(i,subs.length());
        while(s8.length()<8) {
          s8 += ' '; // pad string with spaces to make 8 byte string
        }
      }
      else {
        s8 = subs.substring(i,i+8);  
      }
      //Serial.println(s8);
      display8ByteString(s8);
      syncOK(); // every 100ms to 1sec
      delay(1000);
    }
    start=wordIdx+1;
  }
}

void semiScroll(String s)
{
  if(s.length() == 0) {
    return;  
  }

  s.trim(); // remove \r\n at the end of string

  String saveds = s;
  int Idx = 0;  // find the " - " index
  Idx = s.indexOf(" - ");
  if(Idx == -1) {
      exit;  
    }
  //Serial.println(Idx);
  s = saveds.substring(0,Idx);
  //Serial.println(s);

  // Scroll artist string (until " - " token) 
  String s8 = s.substring(0,8);
  while( s8.length() < 8) {
    s8 += ' ';  // pad to 8 byte string  
  }
  //Serial.println(s8);
  
  // display 1st 8 bytes
  display8ByteString(s8);
  syncOK(); // every 100ms to 1sec
  delay(1000);
  syncOK(); // every 100ms to 1sec
  
  // scroll after 8 bytes
  if(s.length() > 8) {
    for(int i=1;i<s.length();i++) {
      //Serial.println(i);
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
      
      //Serial.println(s8);
      display8ByteString(s8);
      delay(500);
      syncOK(); // every 100ms to 1sec
      delay(500);
      syncOK(); // every 100ms to 1sec
    }  
  }
  
  // print separator
  display8ByteString("--------");
  syncOK(); // every 100ms to 1sec
  delay(500);

  // Scroll song name
  s = saveds.substring(Idx+3,saveds.length());
  //Serial.println(s);
  s8 = s.substring(0,8);
  while( s8.length() < 8) {
    s8 += ' ';  // pad to 8 byte string  
  }
  //Serial.println(s8);
  
  // display 1st 8 bytes
  display8ByteString(s8);
  syncOK(); // every 100ms to 1sec
  delay(1000);
  syncOK(); // every 100ms to 1sec

  if(s.length() > 8) {
    // scroll after 8 bytes
    for(int i=1;i<s.length();i++) {
      //Serial.println(i);
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
      
      //Serial.println(s8);
      display8ByteString(s8);
      delay(500);
      syncOK(); // every 100ms to 1sec
      delay(500);
      syncOK(); // every 100ms to 1sec
    }
  }

  // print end separator
  display8ByteString("////////");
  syncOK(); // every 100ms to 1sec
  delay(500);
}
