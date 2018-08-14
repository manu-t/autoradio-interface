// dpad C&K dpad & Renault 8200058695 steering wheel remote control
// + RN42 (HID)
// tested on trinket pro 5V 16MHz

#include <SoftwareSerial.h>

#include <Key.h>
#include <Keypad.h>

#define WITH_LCD 0

#include <ClickEncoder.h>
#include <TimerOne.h>

#include "OneButton.h"

//KEYBOARD MODEFIER CODES
#define BP_MOD_RIGHT_GUI  (1<<7)
#define BP_MOD_RIGHT_ALT  (1<<6)
#define BP_MOD_RIGHT_SHIFT  (1<<5)
#define BP_MOD_RIGHT_CTRL (1<<4)
#define BP_MOD_LEFT_GUI   (1<<3)
#define BP_MOD_LEFT_ALT   (1<<2)
#define BP_MOD_LEFT_SHIFT (1<<1)
#define BP_MOD_LEFT_CTRL  (1<<0)
#define BP_MOD_NOMOD    0x00

SoftwareSerial mySerial(9,10);

ClickEncoder *encoder;  // dpad encoder for VOL+ and VOL- smartphone
int16_t last, value;

void timerIsr() {
  encoder->service();
}

OneButton button1(6, true); // DPAD RIGHT
OneButton button2(4, true); // DPAD DOWN
OneButton button3(3, true); // DPAD LEFT
OneButton button4(5, true); // DPAD UP

// matrix keypad Renault 820058695
const byte rows = 3; //three rows
const byte cols = 3; //three columns
char keys[rows][cols] = {
  {'B','A','*'},
  {'F','G','H'},
  {'E','D','C'},
};
byte rowPins[rows] = {19, 18, 17}; //connect to the row pinouts of the keypad
byte colPins[cols] = {16, 15, 14}; //connect to the column pinouts of the keypad
Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, rows, cols );

boolean flagALT = false;
String msg = "";
boolean flagVolminus = false;
boolean flagVolplus = false;

void setup() {  
  Serial.begin(115200);  // uart de debug
  mySerial.begin(115200);  // uart de communication avec le module bt hid

  encoder = new ClickEncoder(/*7, 8, 2*//*11,12,13*/11,12,8);

  Timer1.initialize(1000);
  Timer1.attachInterrupt(timerIsr); 
  
  last = -1;

  // link the button 1 functions.
  button1.attachClick(click1);
  //button1.attachDoubleClick(doubleclick1);
  button1.attachLongPressStart(longPressStart1);
  button1.attachLongPressStop(longPressStop1);
  button1.attachDuringLongPress(longPress1);

  // link the button 2 functions.
  button2.attachClick(click2);
  //button2.attachDoubleClick(doubleclick2);
  button2.attachLongPressStart(longPressStart2);
  button2.attachLongPressStop(longPressStop2);
  button2.attachDuringLongPress(longPress2);

  // link the button 3 functions.
  button3.attachClick(click3);
  //button3.attachDoubleClick(doubleclick3);
  button3.attachLongPressStart(longPressStart3);
  button3.attachLongPressStop(longPressStop3);
  button3.attachDuringLongPress(longPress3);

  // link the button 4 functions.
  button4.attachClick(click4);
  //button4.attachDoubleClick(doubleclick4);
  button4.attachLongPressStart(longPressStart4);
  button4.attachLongPressStop(longPressStop4);
  button4.attachDuringLongPress(longPress4);

//  keypad.addEventListener(keypadEvent); //add an event listener for this keypad
  
  Serial.println("Telecommande BT HID \nAppuyer sur les boutons-poussoirs...");
}

void loop() {
  value = encoder->getDir();
  if(value == 1) {
    Serial.println("VOL+");
    consumerKeyPress(0x10);
    consumerRelease();
  }
  if(value == -1) {
    Serial.println("VOL-");
    consumerKeyPress(0x20);
    consumerRelease();
  }
  
  ClickEncoder::Button b = encoder->getButton();
  if (b != ClickEncoder::Open) {
    #define VERBOSECASE(label) case label: Serial.println(#label); break;
    switch (b) {
      VERBOSECASE(ClickEncoder::Pressed);
      //VERBOSECASE(ClickEncoder::Held)
      VERBOSECASE(ClickEncoder::Released)
      case ClickEncoder::Clicked: Serial.println("ENTER"); mySerial.write(13); break;
      case ClickEncoder::DoubleClicked:
          Serial.println("ClickEncoder::DoubleClicked");
          encoder->setAccelerationEnabled(!encoder->getAccelerationEnabled());
          Serial.print("  Acceleration is ");
          Serial.println((encoder->getAccelerationEnabled()) ? "enabled" : "disabled");
        break;
    }
  }

  // get matrix keypad Renault
  if (keypad.getKeys())
    {
        for (int i=0; i<LIST_MAX; i++)   // Scan the whole key list.
        {
            if ( keypad.key[i].stateChanged )   // Only find keys that have changed state.
            {
                switch (keypad.key[i].kstate) {  // Report active key state : IDLE, PRESSED, HOLD, or RELEASED
                    case PRESSED:
                    if(keypad.key[i].kchar == 'E') {
                      msg = " ALT pressed";
                      flagALT = true;
                    }
                    else if(keypad.key[i].kchar == 'B') {
                      sourcePlus();
                    }
                    else if(keypad.key[i].kchar == 'A') {
                      sourceMinus();
                    }
                    else if(keypad.key[i].kchar == 'D') {
                      flagVolminus = true;
                    }
                    else if(keypad.key[i].kchar == 'C') {
                      flagVolplus = true;
                    }
                    else {
                      msg = " PRESSED.";
                    }

                    if( flagVolplus && flagVolminus ) {
                      muteSmartphone();  
                      break;
                    }

                    if(flagVolplus) {
                      Serial.println("VOL+");
                      consumerKeyPress(0x10);
                      consumerRelease();
                      break;
                    }

                    if(flagVolminus) {
                      Serial.println("VOL-");
                      consumerKeyPress(0x20);
                      consumerRelease();
                      break;
                    }
                break;
                    case HOLD:
                    msg = " HOLD.";
                break;
                    case RELEASED:
                    if(keypad.key[i].kchar == 'E') {
                      msg = " ALT released";
                      flagALT = false;
                      keyboardReleaseAll();
                    }
                    else if(keypad.key[i].kchar == 'D') {
                      flagVolminus = false;
                    }
                    else if(keypad.key[i].kchar == 'C') {
                      flagVolplus = false;
                    }
                    else {
                      msg = " RELEASED.";
                    }
                break;
                    case IDLE:
                    msg = " IDLE.";
                }
                Serial.print("Key ");
                Serial.print(keypad.key[i].kchar);
                Serial.println(msg);
            }
        }
}

  // keep watching the push buttons:
  button1.tick();
  button2.tick();
  button3.tick();
  button4.tick();
}

void keyboardPress(byte BP_KEY,byte BP_MOD){
  mySerial.write((byte)0xFD); //Start HID Report
  mySerial.write((byte)0x09); //Length byte
  mySerial.write((byte)0x01); //Descriptor byte
  mySerial.write(BP_MOD); //Modifier byte
  mySerial.write((byte)0x00); //-
  mySerial.write(BP_KEY); //Send KEY
  for(byte i = 0;i<5;i++){ //Send five zero bytes
    mySerial.write((byte)0x00);
  }
}

void keyboardReleaseAll(){
  keyboardPress((byte)0x00,BP_MOD_NOMOD);
}

void consumerKeyPress(byte key) {
    mySerial.write((byte)0xFD); //Start HID Report
    mySerial.write((byte)0x03); //Length byte
    mySerial.write((byte)0x03);
    mySerial.write(key); //Send KEY
    mySerial.write((byte)0x00);
}

void consumerRelease() {
    mySerial.write((byte)0xFD); //Start HID Report
    mySerial.write((byte)0x03); //Length byte
    mySerial.write((byte)0x03);
    mySerial.write((byte)0x00);
    mySerial.write((byte)0x00);
}

// ----- button 1 callback functions

// This function will be called when the button1 was pressed 1 time (and no 2. button press followed).
void click1() {
  Serial.println("RIGHT ARROW");
  mySerial.write(7);
} // click1


// This function will be called when the button1 was pressed 2 times in a short timeframe.
void doubleclick1() {
  Serial.println("Button 1 doubleclick.");
} // doubleclick1


// This function will be called once, when the button1 is pressed for a long time.
void longPressStart1() {
  Serial.println("Button 1 longPress start");
} // longPressStart1


// This function will be called often, while the button1 is pressed for a long time.
void longPress1() {
  Serial.println("Button 1 longPress...");
} // longPress1


// This function will be called once, when the button1 is released after beeing pressed for a long time.
void longPressStop1() {
  Serial.println("Button 1 longPress stop");
} // longPressStop1


// ... and the same for button 2:

void click2() {
  Serial.println("DOWN ARROW");
  mySerial.write(12);
} // click2


void doubleclick2() {
  Serial.println("Button 2 doubleclick.");
} // doubleclick2


void longPressStart2() {
  Serial.println("Button 2 longPress start");
} // longPressStart2


void longPress2() {
  Serial.println("Button 2 longPress...");
} // longPress2

void longPressStop2() {
  Serial.println("Button 2 longPress stop");
} // longPressStop2

// ... and the same for button 3:

void click3() {
  Serial.println("LEFT ARROW");
  mySerial.write(11);
} // click3


void doubleclick3() {
  Serial.println("Button 3 doubleclick.");
} // doubleclick3


void longPressStart3() {
  Serial.println("Button 3 longPress start");
} // longPressStart3


void longPress3() {
  Serial.println("Button 3 longPress...");
} // longPress3

void longPressStop3() {
  Serial.println("Button 3 longPress stop");
} // longPressStop3

// ... and the same for button 4:

void click4() {
  Serial.println("UP ARROW");
  mySerial.write(14);
} // click4


void doubleclick4() {
  Serial.println("Button 4 doubleclick.");
} // doubleclick4


void longPressStart4() {
  Serial.println("Button 4 longPress start");
} // longPressStart4


void longPress4() {
  Serial.println("Button 4 longPress...");
} // longPress4

void longPressStop4() {
  Serial.println("Button 4 longPress stop");
} // longPressStop4

void sourcePlus() {
  if(flagALT) {
    Serial.println("ALT-TAB");
    keyboardPress(0x2B,BP_MOD_LEFT_ALT);
    keyboardPress(0x00,BP_MOD_LEFT_ALT); // release tab key but keep alt modifier until released
  }
  else {
    Serial.println("AC HOME");
    consumerKeyPress(0x01);
    consumerRelease();    
    
  }  
}

void sourceMinus() {
  if(flagALT) {
    Serial.println("ALT-SHIFT-TAB");
    keyboardPress(0x2B,BP_MOD_LEFT_SHIFT|BP_MOD_LEFT_ALT);
    keyboardPress(0x00,BP_MOD_LEFT_ALT); // release tab key but keep alt modifier until released
  }
  else {
    Serial.println("ESC");
    mySerial.write(27);
  }  
}

void muteSmartphone() {
  Serial.println("MUTE (smartphone)");
  consumerKeyPress(0x40);
  consumerRelease();    
}

