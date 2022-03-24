//Adafruit MCP23017 Arduino Library を導入してください
//Arduino Micro または Leonard を使用してください

//簡単な説明
//マスコン5段、ブレーキ8段+EBです。それ以外は使用不可です。(今のところ)
//ノッチの移動量に応じて、各キーボードコマンドを打ち込んでいます。
//MC53側は真理値に基づいてノッチ(N,P1～P5)を指示します。レバーサも対応します。
//ME38側はポテンショの値を一旦角度換算し、ブレーキノッチ(N,B1～EB)を指示します。

//BVEゲーム開始時は、一旦ブレーキハンドルをN→B8、マスコンノッチはN、レバーさハンドルをB→N→Fと動かす等してリセットします。

#include <Adafruit_MCP23X17.h>
#include <Keyboard.h>
#include <EEPROM.h>

//マスコン入力ピンアサイン
#define PIN_MC_1 0
#define PIN_MC_2 1
#define PIN_MC_3 2
#define PIN_MC_4 3
#define PIN_MC_5 4
#define PIN_MC_DEC 5

//レバーサ入力ピンアサイン
#define PIN_MC_DIR_F 6
#define PIN_MC_DIR_B 7

//ホーン入力　※不要な場合はコメントアウト
#define PIN_HORN_1 8
#define PIN_HORN_2 9

#define SS2 4
#define CS_PIN SS


//↓デバッグのコメント(//)を解除するとシリアルモニタでデバッグできます
//
#define DEBUG

Adafruit_MCP23X17 mcp;
SPISettings settings = SPISettings( 1000000 , MSBFIRST , SPI_MODE0 );

int mcBit = 0;
int mcBit_latch = 0;
int notch_mc = 0;
int notch_mc_latch = 0;
int notch_mc_H = 0;
int notch_mc_H_latch = 0;
String notch_name = "";
int notch_brk = 0;
int notch_brk_latch = 0;
String notch_brk_name = "";
int iDir = 0;
int iDir_latch = 0;
String strDir = "";
bool Horn_1 = 0;
bool Horn_1_latch = 0;
bool Horn_2 = 0;
bool Horn_2_latch = 0;
float adj_N = 0.0;
float adj_EB = 0.0;

int POT_N = 0;
int POT_EB = 0;

unsigned long iniMillis_N = 0;
unsigned long iniMillis_EB = 0;
int setMode_N = 0;
int setMode_EB = 0;


void setup() {
  pinMode(SS2, OUTPUT);
  Serial.begin(115200);
  Keyboard.begin();

  if (!mcp.begin_SPI(CS_PIN)) {
    Serial.println("Error.");
    while (1);
  }

  // マスコンスイッチを全てプルアップ
  /*
  mcp.pinMode(PIN_MC_1, INPUT_PULLUP);
  mcp.pinMode(PIN_MC_2, INPUT_PULLUP);
  mcp.pinMode(PIN_MC_3, INPUT_PULLUP);
  mcp.pinMode(PIN_MC_4, INPUT_PULLUP);
  mcp.pinMode(PIN_MC_5, INPUT_PULLUP);
  mcp.pinMode(PIN_MC_DEC, INPUT_PULLUP);
  mcp.pinMode(PIN_MC_DIR_F, INPUT_PULLUP);
  mcp.pinMode(PIN_MC_DIR_B, INPUT_PULLUP);
  mcp.pinMode(PIN_HORN_1, INPUT_PULLUP);
  mcp.pinMode(PIN_HORN_2, INPUT_PULLUP);
*/
  EEPROM.get(0, POT_N);
  EEPROM.get(2, POT_EB);

}

void loop() {
  // LOW = pressed, HIGH = not pressed

  //シリアルモニタが止まるのを防止するおまじない
  if (Serial.available()) {
    Serial.readStringUntil('\n');
  }

  //read_MC();            //マスコンノッチ読込ルーチン
  //read_Dir();           //マスコンレバーサ読込ルーチン
  read_Break();//ブレーキハンドル読込ルーチン
  read_Break_Setting(); //ブレーキハンドル読込ルーチン(未実装)
  //read_Horn();
  keyboard_control();   //キーボード(HID)アウトプットルーチン

#ifdef DEBUG
  Serial.print(" ");
  Serial.print(notch_name);
  Serial.print(" ");
  Serial.print(notch_brk_name);
  Serial.print(" Dir:");
  Serial.print(strDir);
  Serial.println();
#endif

  delay(50);
}

//MCP3008ADコンバータ読取
int adcRead(byte ch) { // 0 .. 7
  byte channelData = (ch + 8 ) << 4;
  // Serial.println(String(channelData,BIN));
  SPI.beginTransaction(settings);
  digitalWrite(SS2, LOW);
  SPI.transfer(0b00000001); // Start bit 1
  byte highByte = SPI.transfer(channelData); // singleEnd
  byte lowByte = SPI.transfer(0x00); // dummy
  digitalWrite(SS2, HIGH);
  SPI.endTransaction();
  return ((highByte & 0x03) << 8) + lowByte ;
}

//MCP23S17マスコンノッチ状態読込 (MC53抑速ブレーキ対応)
void read_MC(void) {
  mcBit = mcp.readGPIOA();
  if (mcBit != mcBit_latch) {

#ifdef DEBUG
    Serial.print(!mcp.digitalRead(PIN_MC_1), DEC);
    Serial.print(!mcp.digitalRead(PIN_MC_2), DEC);
    Serial.print(!mcp.digitalRead(PIN_MC_3), DEC);
    Serial.print(!mcp.digitalRead(PIN_MC_4), DEC);
    Serial.print(!mcp.digitalRead(PIN_MC_5), DEC);
    Serial.print(!mcp.digitalRead(PIN_MC_DEC), DEC);
    Serial.print(!mcp.digitalRead(PIN_MC_DIR_F), DEC);
    Serial.print(!mcp.digitalRead(PIN_MC_DIR_B), DEC);
#endif

    if (mcp.digitalRead(PIN_MC_DEC)) {
      if (!mcp.digitalRead(PIN_MC_5)) {
        notch_mc = 14;
        notch_name = "P5";
      } else if (!mcp.digitalRead(PIN_MC_4)) {
        notch_mc = 13;
        notch_name = "P4";
      } else if (!mcp.digitalRead(PIN_MC_3)) {
        notch_mc = 12;
        notch_name = "P3";
      } else if (!mcp.digitalRead(PIN_MC_2)) {
        notch_mc = 11;
        notch_name = "P2";
      } else if (!mcp.digitalRead(PIN_MC_1)) {
        notch_mc = 10;
        notch_name = "P1";
      } else {
        notch_mc = 9;
        notch_mc_H = 20;
        notch_name = "N ";
      }
    } else {
      if (!mcp.digitalRead(PIN_MC_5)) {
        notch_mc_H = 21;
        notch_name = "H1";
      } else if (!mcp.digitalRead(PIN_MC_3) && mcp.digitalRead(PIN_MC_4)) {
        notch_mc_H = 22;
        notch_name = "H2";
      } else if (!mcp.digitalRead(PIN_MC_2) && mcp.digitalRead(PIN_MC_4)) {
        notch_mc_H = 23;
        notch_name = "H3";
      } else if (!mcp.digitalRead(PIN_MC_2) && !mcp.digitalRead(PIN_MC_4)) {
        notch_mc_H = 24;
        notch_name = "H4";
      } else if (!mcp.digitalRead(PIN_MC_3) && !mcp.digitalRead(PIN_MC_4)) {
        notch_mc_H = 25;
        notch_name = "H5";
      }
    }
#ifdef DEBUG
    Serial.print(" mc:");
    Serial.print(notch_mc);
    Serial.print(" mc_latch:");
    Serial.println(notch_mc_latch);
    Serial.print(" mc_H:");
    Serial.print(notch_mc_H);
    Serial.print(" mc_H_latch:");
    Serial.println(notch_mc_H_latch);
#endif
  }
  mcBit_latch = mcBit;
}

//マスコンレバーサ読取
void read_Dir(void) {
  if (!mcp.digitalRead(PIN_MC_DIR_F)) {
    iDir = 1;
    strDir = "F";
  } else if (!mcp.digitalRead(PIN_MC_DIR_B)) {
    iDir = -1;
    strDir = "B";
  } else {
    iDir = 0;
    strDir = "N ";
  }
}

//ブレーキ角度読取ZZ
void read_Break(void) {

  int deg = map(adcRead(0), POT_N , POT_EB , 0, 165);
#ifdef DEBUG
  Serial.print(" Pot1:");
  Serial.print(10000 + adcRead(0));

  Serial.print(" Deg:");
  Serial.print(1000 + deg);
#endif

  if (deg < 5) {
    notch_brk = 9;
    notch_brk_name = "N ";
  } else if ( deg < 15 ) {
    notch_brk = 8;
    notch_brk_name = "B1";
  } else if ( deg < 25 ) {
    notch_brk = 7;
    notch_brk_name = "B2";
  } else if ( deg < 35 ) {
    notch_brk = 6;
    notch_brk_name = "B3";
  } else if ( deg < 45 ) {
    notch_brk = 5;
    notch_brk_name = "B4";
  } else if ( deg < 55 ) {
    notch_brk = 4;
    notch_brk_name = "B5";
  } else if ( deg < 65 ) {
    notch_brk = 3;
    notch_brk_name = "B6";
  } else if ( deg < 75 ) {
    notch_brk = 2;
    notch_brk_name = "B7";
  } else if ( deg < 150 ) {
    notch_brk = 1;
    notch_brk_name = "B8";
  } else {
    notch_brk = 0;
    notch_brk_name = "EB";
  }

#ifdef DEBUG
  bool sw = 0;
  if (adcRead(1) < 512)sw = 0; else sw = 1;
  Serial.print(" SW1:");
  Serial.print(sw);

  if (adcRead(2) < 512)sw = 0; else sw = 1;
  Serial.print(" SW2:");
  Serial.print(sw);

  if (adcRead(3) < 512)sw = 0; else sw = 1;
  Serial.print(" SW3:");
  Serial.print(sw);

  if (adcRead(4) < 512)sw = 0; else sw = 1;
  Serial.print(" SW4:");
  Serial.print(sw);
#endif
}

//キーボード(HID)出力
void keyboard_control(void) {
  //マスコンノッチが前回と異なるとき
  if (notch_mc != notch_mc_latch ) {
    int d = abs(notch_mc - notch_mc_latch);
#ifdef DEBUG
    Serial.print(" notch_mc:");
    Serial.print(notch_mc);
    Serial.print(" notch_mc-notch_mc_latch:");
    Serial.print(d);
    Serial.print(" Key:");
#endif
    //力行ノッチ
    if ( notch_mc >= 9 && notch_mc_latch >= 9 && notch_mc <= 14 && notch_mc_latch <= 14 ) {
      //進段
      if ((notch_mc - notch_mc_latch) > 0) {
        for (int i = 0; i < d ; i ++) {
          Keyboard.write(0x5A);//"/"
#ifdef DEBUG
          Serial.println("Z");
#endif
        }
      }
      //戻し
      if ((notch_mc - notch_mc_latch) < 0) {
        for (int i = 0; i < d ; i ++) {
          Keyboard.write(0x41);//"/"
#ifdef DEBUG
          Serial.println("A");
#endif
        }
      }
    }
  }

  //抑速ノッチが前回と異なるとき
  if (notch_mc_H != notch_mc_H_latch ) {
    int d = abs(notch_mc_H - notch_mc_H_latch);
    //抑速ノッチ
    if ( notch_mc_H >= 20 && notch_mc_H_latch >= 20 && notch_mc_H <= 25 && notch_mc_H_latch <= 25 ) {
      //進段
      if ((notch_mc_H - notch_mc_H_latch) > 0) {
        for (int i = 0; i < d ; i ++) {
          Keyboard.write(0x51);//"/"
#ifdef DEBUG
          Serial.println("Q");
#endif
        }
      }
      //戻し
      if ((notch_mc_H - notch_mc_H_latch) < 0) {
        for (int i = 0; i < d ; i ++) {
          Keyboard.write(0x41);//"/"
#ifdef DEBUG
          Serial.println("A");
#endif
        }
      }
    }
  }


  //ブレーキノッチ(角度)が前回と異なるとき
  if (notch_brk != notch_brk_latch) {
    int d = abs(notch_brk - notch_brk_latch);
#ifdef DEBUG
    Serial.print(" notch_brk:");
    Serial.print(notch_brk);
    Serial.print(" notch_brk-notch_brk_latch:");
    Serial.print(d);
    Serial.print(" Key:");
#endif
    //ブレーキノッチ
    if ( notch_brk <= 9 && notch_brk_latch <= 9 && notch_brk > 0) {
      //戻し
      if ((notch_brk - notch_brk_latch) > 0) {
        for (int i = 0; i < d ; i ++) {
          Keyboard.write(0x2C);//","
#ifdef DEBUG
          Serial.println(",");
#endif
        }
      }
      //ブレーキ
      if ((notch_brk - notch_brk_latch) < 0) {
        for (int i = 0; i < d ; i ++) {
          Keyboard.write(0x2E);//"."
#ifdef DEBUG
          Serial.println(".");
#endif
        }
      }
    }
    if (notch_brk == 0) {
      Keyboard.write(0x2F);//"/"
#ifdef DEBUG
      Serial.println("/");
#endif
    }
  }

  //レバーサが前回と異なるとき
  if (iDir != iDir_latch) {
    int d = abs(iDir - iDir_latch);
#ifdef DEBUG
    Serial.print(" iDir:");
    Serial.print(iDir);
#endif
    //前進
    if ((iDir - iDir_latch) > 0) {
      for (int i = 0; i < d ; i ++) {
        Keyboard.write(0xDA);//"↑"
#ifdef DEBUG
        Serial.println("↑");
#endif
      }
    }
    //後退
    if ((iDir - iDir_latch) < 0) {
      for (int i = 0; i < d ; i ++) {
        Keyboard.write(0xD9);//"↓"
#ifdef DEBUG
        Serial.println("↓");
#endif
      }
    }
  }

  notch_mc_latch = notch_mc;
  notch_mc_H_latch = notch_mc_H;
  notch_brk_latch = notch_brk;
  iDir_latch = iDir;
}

//ブレーキ角度調整
void read_Break_Setting(void) {
  int value = 0;
  bool n = (adcRead(5) < 512);
  bool eb = (adcRead(6) < 512);
  if (n) {
    if (setMode_N == 0) {
      adj_N = adcRead(0);
      setMode_N = 1;
      iniMillis_N = millis();
      Serial.print("POT_N=");
      Serial.print(EEPROM.get(0, value));
      Serial.print(" ADC=");
      Serial.println(adj_N);
    } else if (setMode_N == 1) {
      if (millis() - iniMillis_N > 3000) {
        setMode_N = 2;
        //Serial.println("Mode_1");
      }
      adj_N = adj_N * 0.9 + adcRead(0) * 0.1;
      //Serial.println("Mode_1");
    } else if (setMode_N == 2) {
      setMode_N = 0;
      POT_N = (int) adj_N;
      EEPROM.put(0, POT_N );
      Serial.print("NEW POT_N=");
      Serial.println(POT_N);
    }
  } else {
    setMode_N = 0;
  }

  if (eb) {
    if (setMode_EB == 0) {
      adj_EB = adcRead(0);
      setMode_EB = 1;
      iniMillis_EB = millis();
      Serial.print("POT_EB=");
      Serial.print(EEPROM.get(2, value));
      Serial.print(" ADC=");
      Serial.println(adj_EB);
    } else if (setMode_EB == 1) {
      if (millis() - iniMillis_EB > 3000) {
        setMode_EB = 2;
        //Serial.println("Mode_1");
      }
      adj_EB = adj_EB * 0.9 + adcRead(0) * 0.1;
      //Serial.println("Mode_1");
    } else if (setMode_EB == 2) {
      setMode_EB = 0;
      POT_EB = (int) adj_EB;
      EEPROM.put(2, POT_EB);
      Serial.print("NEW POT_EB=");
      Serial.println(POT_EB);
    }
  } else {
    setMode_EB = 0;
  }
}

void read_Horn(void) {
  Horn_1 = !mcp.digitalRead(PIN_HORN_1);
  if ( Horn_1 != Horn_1_latch )
  {
    if (Horn_1) {
      Keyboard.press(0xB0);//"Enter"
    } else {
      Keyboard.release(0xB0);
    }
  }
  Horn_1_latch = Horn_1;

  Horn_2 = !mcp.digitalRead(PIN_HORN_2);
  if ( Horn_2 != Horn_2_latch )
  {
    if (Horn_2 ) {
      Keyboard.press(0xDF);//"Enter"
    } else {
      Keyboard.release(0xDF);
    }
  }
  Horn_2_latch = Horn_2;
}
