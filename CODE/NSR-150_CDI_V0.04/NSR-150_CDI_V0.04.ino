/*
 * NSR-150 可程式 CDI
 * Version: V0.04
 * V0.04 版功能是將二期 RC 改成一期RC的騎乘體驗感
 * 接腳如下：
 *   D1 凸台 
 *   D4 RC 訊號輸出
 *   D6 點火 
 *   
 * Release Date: 2021-09-26
 * Author: 羽山秋人 (https://3wa.tw)
 * Author: @FB 田峻墉
 */
#include <Arduino.h>
#include <TM1637.h> //七段數位模組
/*
轉速   60 轉 =  每分鐘    60 轉，每秒  1    轉，1轉 = 1          秒 = 1000.000 ms = 1000000us
轉速   100 轉 = 每分鐘   100 轉，每秒  1.67 轉，1轉 = 0.598802   秒 =  598.802 ms =  598802us
轉速   200 轉 = 每分鐘   200 轉，每秒  3.3  轉，1轉 = 0.300003   秒 =  300.003 ms =  300003us
轉速   600 轉 = 每分鐘   600 轉，每秒  10   轉，1轉 = 0.1        秒 =  100.000 ms =  100000us
轉速  1500 轉 = 每分鐘  1500 轉，每秒  25   轉，1轉 = 0.04       秒 =   40.000 ms =   40000us
轉速  6000 轉 = 每分鐘  6000 轉，每秒  60   轉，1轉 = 0.01666... 秒 =   16.667 ms =   16667us
轉速 10000 轉 = 每分鐘 10000 轉，每秒 166.6 轉，1轉 = 0.00599... 秒 =    5.999 ms =    5999us
轉速 14000 轉 = 每分鐘 14000 轉，每秒 233.3 轉，1轉 = 0.0042863. 秒 =    4.286 ms =    4286us
轉速 14060 轉 = 每分鐘 14060 轉，每秒 240   轉，1轉 = 0.0041667. 秒 =    4.167 ms =    4167us
轉速 16000 轉 = 每分鐘 16000 轉，每秒 266.6 轉，1轉 = 0.0037500. 秒 =    3.750 ms =    3750us 
*/
const int ToPin = D1;  //凸台
const int RCPin = D4;  //RC訊號
const int FirePin = D6;  //點火


volatile float now_degree = 12;
//NSR維修手冊裡的圖
//volatile const float degree[16] = {12, 12, 12, 12, 17, 29, 29, 25, 23, 20, 17, 13, 10, 8, 8, 8};

//這個是羽山原本CDI量到的
volatile const float degree[16] = {10, 12, 12, 25, 26, 23, 20, 16, 13, 9, 9, 9, 8, 8, 8, 8};
volatile const float fullAdv = 65;
volatile bool isFiring = false;

//#define CLK D7
//#define DIO D8
//TM1637 tm1637(CLK, DIO);
volatile unsigned long C = micros(); //紀錄碰到凸台的時間
volatile unsigned long C_old = 0; //紀錄上次碰到凸台的時間
volatile unsigned long rpm = 0; //紀錄當前 RPM
volatile unsigned long RPM_DELAY = 0; //凸台跟上一次凸台相距時間，用來推算當前RPM多少
volatile unsigned int isShowCount = 0; //在 loop 中每轉 100次才回報一次到 Serial 觀看
bool isFirstRC_Flag = true; //當 kick start 時，RC 全開一次的旗標
volatile double CDI_DELAY = -1; //計算每一圈碰到凸台後，要延遲多久時間才點火 us
void ICACHE_RAM_ATTR countup() {  //For newest version
  //收到CDI點火，扣掉偵測到凸台RISING時間
  //只要是Rising就是Fire

  C = micros();
  // (1/(17000/60)0 *1000 * 1000 = 3529
  // 不可能有超過 17000rpm 的狀況
  RPM_DELAY = C - C_old;
  if(RPM_DELAY < 3500) {
    //超過 17000rpm 了
    return;
  }
  if(RPM_DELAY > 598802) {
    //低於 100rpm
    C_old = C;
    rpm = 0;
    return;
  }      
  rpm = 60000000UL / RPM_DELAY;
  C_old = C;
  digitalWrite(FirePin, LOW);
  if(rpm>17000)
  {
    //不可能的
    //rpm = 0;
    return;
  }
  if(rpm<80)
  {
    //太低了，熄火吧
    //rpm = 0;
    return;
  }
  if (rpm != 0)
  {
    int index = (int)rpm / 1000;
    int index_n = index + 1;
    index = (index >= 16) ? 15 : index;
    if (index_n >= 16) index_n = 15;
    long s = index * 1000;
    long e = index_n * 1000;
    long ss = degree[index] * 1000;
    long ee = degree[index_n] * 1000;
    double r = map(int(rpm), s, e, ss, ee) / 1000.0;    
    now_degree = r;

    //新的 CDI 計算方法
    //delay = adv/360*轉速一圈時間
    //rpm 一圈時間 * r * 360)
    //計算每一圈碰到凸台後，要延遲多久時間才點火 us
    CDI_DELAY = (1000000.0 / (float(rpm) / 60.0)) * ((fullAdv - r)/ 360.0);
    isFiring = true;
    //改成在 loop 裡執行
    //delayMicroseconds(long(CDI_DELAY));
    //digitalWrite(FirePin, HIGH);
    //點火持續時間
    //固定點 200us    
    //delayMicroseconds(200);
    //digitalWrite(FirePin, LOW);
  }
}



void setup() {
  // put your setup code here, to run once:
  Serial.begin(250000);
  Serial.println("Counting...");
  pinMode(ToPin, INPUT_PULLUP);
  pinMode(RCPin, OUTPUT);
  pinMode(FirePin, OUTPUT);  
  //註冊凸台中斷
  attachInterrupt(digitalPinToInterrupt(ToPin), countup, RISING); //RISING
  //tm1637.init();
  //tm1637.set(BRIGHT_TYPICAL); //BRIGHT_TYPICAL = 2,BRIGHT_DARKEST = 0,BRIGHTEST = 7;
  //playFirstTime();
  //diaplayOnLed(0);
  digitalWrite(FirePin, LOW);  
  if(isFirstRC_Flag == true)
  {
    isFirstRC_Flag=false;
    //開機後，RC全開，再關上一次
    for(int i=0;i<250;i++)
    {
      //50 duty , 10000rpm , 250 次，應該就可以全開
      //250次耗時 1.5 秒
      digitalWrite(RCPin, HIGH);
      delayMicroseconds(3000); //3ms
      digitalWrite(RCPin, LOW);
      delayMicroseconds(3000); //3ms    
    }
  }
  digitalWrite(RCPin, LOW);
}

void loop() {

  // put your main code here, to run repeatedly:
  if (isShowCount > 100)
  {
    //display_rpm();
    isShowCount = 0;    
    Serial.print(rpm);
    Serial.print(" , ");
    Serial.print(now_degree);
    Serial.print(" , ");
    Serial.print(CDI_DELAY);
    Serial.println("");
    if(micros()-C_old>=500000)
    {
      //低於 100 轉以下
      //diaplayOnLed(0);
    }
    else
    {
      //diaplayOnLed(rpm);
    }
  }
  isShowCount++;  
  
  if(isFiring == true)
  {
    isFiring = false;
    //低於5000完全不需要RC訊號
    //5000以上，收多少訊號，就給多少到RC    
    /*
轉速  6000 轉 = 每分鐘  6000 轉，每秒  60   轉，1轉 = 0.01666... 秒 =   16.667 ms =   16667us
轉速 10000 轉 = 每分鐘 10000 轉，每秒 166.6 轉，1轉 = 0.00599... 秒 =    5.999 ms =    5999us
     */
    long HALF_RPM_DELAY = RPM_DELAY/2;
    if(rpm<5000)
    {
      //原本就 LOW 了
      //digitalWrite(RCPin, LOW); 
    }
    if(rpm>=5000)
    {
      digitalWrite(RCPin, HIGH); 
    }    
    delayMicroseconds(long(CDI_DELAY));
    digitalWrite(FirePin, HIGH);
    //點火持續時間
    //固定點 200us    
    delayMicroseconds(200);
    digitalWrite(FirePin, LOW);  
    digitalWrite(RCPin, LOW);
  }   
}
/*void playFirstTime()
{
  // 0000~9999 跑二次
  for (int i = 0; i <= 9; i++)
  {
    for (int j = 0; j < 4; j++)
    {
      tm1637.display(j, i);
    }
    delay(100);
  }
}
*/
/*void display_rpm() {
  if (rpm > 30000) return;
  //if(duty>=500000) return;
  Serial.print("rpm: ");
  Serial.print(rpm);
  Serial.print(" , CDI_DELAY: ");
  Serial.println(CDI_DELAY);
  //Serial.print(" , duty: ");
  //Serial.println(duty);
}
void diaplayOnLed(int show_rpm)
{
  //將轉速，變成顯示值
  //只顯示 萬千百十
  //如果要顯示 千百十個，就不用除了
  //太多數位有點眼花
  //String rpm_str = String(show_rpm/10);
  String rpm_str = String(show_rpm);
  if (rpm_str.length() <= 3)
  {
    rpm_str = lpad(rpm_str, 4, "X"); // 變成如 "XXX0"
  }
  //Serial.print("\nAfter lpad:");
  //Serial.println(rpm_str);
  for (int i = 0; i < 4; i++)
  {
    if (rpm_str[i] == 'X')
    {
      tm1637.display(i, -1); //-1 代表 blank 一片黑
    }
    else
    {
      // Serial.println(rpm_str[i]);
      // 腦包直接轉回 String 再把單字轉 int
      // From : https://www.arduino.cc/en/Tutorial.StringToIntExample
      tm1637.display(i, String(rpm_str[i]).toInt());
    }
  }
}
String lpad(String temp , byte L , String theword) {
  //用來補LED左邊的空白
  byte mylen = temp.length();
  if (mylen > (L - 1))return temp.substring(0, L - 1);
  for (byte i = 0; i < (L - mylen); i++)
    temp = theword + temp;
  return temp;
}
*/
