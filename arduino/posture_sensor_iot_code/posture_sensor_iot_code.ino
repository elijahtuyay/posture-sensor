#include <Fuzzy.h>
#include <NewPingESP8266.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
//#include <WiFi.h>
#include <ESP8266WiFi.h>
//#include <ArduinoSort.h>
#include "QuickMedianLib.h"
#include "ThingSpeak.h"


//WIFI CONNECTION (placeholders)
const char* apiKey = "apiWrite";       //API WRITE KEY
const char* ssid = "ssid";             //WIFI SSID
const char* password = "password";     //WIFI PASSOWRD
const unsigned long chanNum = 1234567; // place older channel number
 
WiFiClient client;

Adafruit_ADS1015 ads1015;

#define TRIG  4 //2
#define ECHO  3 //0
#define MAXRANGE 80.0 //max range of ultrasonic sensor

NewPingESP8266 sonar(TRIG, ECHO, MAXRANGE);
float fsr1, fsr2, flex1, flex2, ultra;
float fsr1_mdn = 0, fsr2_mdn = 0, flex1_mdn = 0, flex2_mdn = 0, ultra_mdn = 0;
float fsr_asym, fsr_ave, flex_asym;
float skew_L_final, skew_R_final, posture_final;
int i = 0; //for counting cycles
const int cycles = 20; //interval of sending data to TS

float fsr1_A[cycles], fsr2_A[cycles], flex1_A[cycles], flex2_A[cycles], ultra_A[cycles]; //arrays


//FUZZY CONTROLLER ============================================================
Fuzzy *fuzzy1 = new Fuzzy();

//MEMBERSHIP FUNCTIONS ========================================================

//Input 1a: flex asymmetry
FuzzySet *flex_down = new FuzzySet(-20,-20,-10,-5);
FuzzySet *flex_neutral = new FuzzySet(-10,-5,5,10);
FuzzySet *flex_up = new FuzzySet(5,10,20,20);

//Input 2a: ultra distance
FuzzySet *ultra_low = new FuzzySet(0,0,10,15);
FuzzySet *ultra_med = new FuzzySet(10,15,20,25);
FuzzySet *ultra_hi = new FuzzySet(20,25,30,35);
FuzzySet *ultra_vHi = new FuzzySet(30,35,50,50);

//Input 3a: fsr average force
FuzzySet *fsr_weak = new FuzzySet(0,0,30,40);
FuzzySet *fsr_moderate = new FuzzySet(30,40,55,65);
FuzzySet *fsr_strong = new FuzzySet(55,65,90,90);

//Output 1a: slouch forward
FuzzySet *slouch_hi = new FuzzySet(0,30,30,30);
FuzzySet *slouch_med = new FuzzySet(20,50,50,80);
FuzzySet *slouch_low = new FuzzySet(70,100,100,100);

//Output 2a: lean back
FuzzySet *lean_hi = new FuzzySet(0,30,30,30);
FuzzySet *lean_med = new FuzzySet(20,50,50,80);
FuzzySet *lean_low = new FuzzySet(70,100,100,100);

//Input 3b: fsr asymmetry
FuzzySet *fsr_low = new FuzzySet(0,0,0,0);
FuzzySet *fsr_med = new FuzzySet(0,0,0,0);
FuzzySet *fsr_hi = new FuzzySet(0,0,0,0);

//Output 3: overall posture
FuzzySet *posture_low = new FuzzySet(0,0,0,0);
FuzzySet *posture_med = new FuzzySet(0,0,0,0);
FuzzySet *posture_hi = new FuzzySet(0,0,0,0);

void setup(void)
{
  //Wire.begin();
  Serial.begin(115200);
  delay(10);

  WiFi.mode(WIFI_STA); 
  ThingSpeak.begin(client); 
  
  WiFi.begin(ssid, password);
  
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
   
  WiFi.begin(ssid, password);
   
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  ads1015.begin();

  //FUZZY I/O SETUP ======================================================

  //FUZZY 1
  //Input 1a: flex asymmetry
  FuzzyInput *flex_skew = new FuzzyInput(1);
  flex_skew->addFuzzySet(flex_down);
  flex_skew->addFuzzySet(flex_neutral);
  flex_skew->addFuzzySet(flex_up);
  fuzzy1->addFuzzyInput(flex_skew);

  //Input 2a: ultra distance
  FuzzyInput *ultra_dist = new FuzzyInput(2);

  ultra_dist->addFuzzySet(ultra_low);
  ultra_dist->addFuzzySet(ultra_med);
  ultra_dist->addFuzzySet(ultra_hi);
  ultra_dist->addFuzzySet(ultra_vHi);
  fuzzy1->addFuzzyInput(ultra_dist);

  //Input 3a: fsr average force
  FuzzyInput *fsr_mean = new FuzzyInput(3);

  fsr_mean->addFuzzySet(fsr_weak);
  fsr_mean->addFuzzySet(fsr_moderate);
  fsr_mean->addFuzzySet(fsr_strong);
  fuzzy1->addFuzzyInput(fsr_mean);

  //Output 1a: slouch forward
  FuzzyOutput *slouch_out = new FuzzyOutput(1);

  slouch_out->addFuzzySet(slouch_low);
  slouch_out->addFuzzySet(slouch_med);
  slouch_out->addFuzzySet(slouch_hi);
  fuzzy1->addFuzzyOutput(slouch_out);

  //Output 2a: lean back
  FuzzyOutput *lean_out = new FuzzyOutput(2);

  lean_out->addFuzzySet(lean_low);
  lean_out->addFuzzySet(lean_med);
  lean_out->addFuzzySet(lean_hi);
  fuzzy1->addFuzzyOutput(lean_out);

  //FUZZY RULES ==========================================================
  
  //SLOUCH HIGH
  FuzzyRuleAntecedent *slouch_far = new FuzzyRuleAntecedent();
  slouch_far->joinWithAND(flex_down, ultra_hi);

  FuzzyRuleConsequent *is_slouching_far = new FuzzyRuleConsequent();
  is_slouching_far->addOutput(slouch_hi);
  is_slouching_far->addOutput(lean_low);

  FuzzyRule *slouch_rule1 = new FuzzyRule(1, slouch_far, is_slouching_far);
  fuzzy1->addFuzzyRule(slouch_rule1);
  
  
  //SLOUCH MED 1
  FuzzyRuleAntecedent *slouch_backrest = new FuzzyRuleAntecedent();
  slouch_backrest->joinWithAND(flex_down, ultra_med);

  FuzzyRuleConsequent *is_slouching_near1 = new FuzzyRuleConsequent();
  is_slouching_near1->addOutput(slouch_med);
  is_slouching_near1->addOutput(lean_low);

  FuzzyRule *slouch_rule2 = new FuzzyRule(2, slouch_backrest, is_slouching_near1);
  fuzzy1->addFuzzyRule(slouch_rule2);

  //SLOUCH MED 2
  FuzzyRuleAntecedent *slouch_noBackrest = new FuzzyRuleAntecedent();
  slouch_noBackrest->joinWithAND(flex_neutral, ultra_vHi);

  FuzzyRuleConsequent *is_slouching_near2 = new FuzzyRuleConsequent();
  is_slouching_near2->addOutput(slouch_med);
  is_slouching_near2->addOutput(lean_low);

  FuzzyRule *slouch_rule3 = new FuzzyRule(3, slouch_noBackrest, is_slouching_near2);
  fuzzy1->addFuzzyRule(slouch_rule3);

  //LEAN HIGH
  FuzzyRuleAntecedent *lean_back_far1 = new FuzzyRuleAntecedent();
  lean_back_far1->joinWithAND(flex_up, ultra_low);
  FuzzyRuleAntecedent *add_fsr_weak = new FuzzyRuleAntecedent();
  add_fsr_weak->joinSingle(fsr_weak);
  FuzzyRuleAntecedent *lean_back_far2 = new FuzzyRuleAntecedent();
  lean_back_far2->joinWithAND(lean_back_far1, add_fsr_weak);

  FuzzyRuleConsequent *is_leaning_far = new FuzzyRuleConsequent();
  is_leaning_far->addOutput(lean_hi);
  is_leaning_far->addOutput(slouch_low);

  FuzzyRule *lean_rule1 = new FuzzyRule(4, lean_back_far2, is_leaning_far);
  fuzzy1->addFuzzyRule(lean_rule1);

  //lean med
  FuzzyRuleAntecedent *lean_back_near1 = new FuzzyRuleAntecedent();
  lean_back_near1->joinWithAND(flex_up, ultra_low);
  FuzzyRuleAntecedent *add_fsr_mod = new FuzzyRuleAntecedent();
  add_fsr_mod->joinSingle(fsr_moderate);
  FuzzyRuleAntecedent *lean_back_near2 = new FuzzyRuleAntecedent();
  lean_back_near2->joinWithAND(lean_back_near1, fsr_moderate);

  FuzzyRuleConsequent *is_leaning_near = new FuzzyRuleConsequent();
  is_leaning_near->addOutput(lean_med);
  is_leaning_near->addOutput(slouch_low);

  FuzzyRule *lean_rule2 = new FuzzyRule(5, lean_back_near2, is_leaning_near);
  fuzzy1->addFuzzyRule(lean_rule2);

  //CORRECT 1
  FuzzyRuleAntecedent *correct_backrest = new FuzzyRuleAntecedent();
  correct_backrest->joinWithAND(flex_neutral, ultra_low);
  
  FuzzyRuleConsequent *correct1 = new FuzzyRuleConsequent();
  correct1->addOutput(slouch_low);
  correct1->addOutput(lean_low);

  FuzzyRule *correct_rule1 = new FuzzyRule(6, correct_backrest, correct1);
  fuzzy1->addFuzzyRule(correct_rule1);

  //CORRECT 2
  FuzzyRuleAntecedent *correct_noBackrest = new FuzzyRuleAntecedent();
  correct_noBackrest->joinWithAND(flex_neutral, ultra_hi);

  FuzzyRuleConsequent *correct2 = new FuzzyRuleConsequent();
  correct2->addOutput(slouch_low);
  correct2->addOutput(lean_low);

  FuzzyRule *correct_rule2 = new FuzzyRule(7, correct_noBackrest, correct1);
  fuzzy1->addFuzzyRule(correct_rule2);
}

void loop(void)
{
  float adc0, adc1, adc2, adc3;

  //read analog sensor values
  adc0 = ads1015.readADC_SingleEnded(0);
  adc1 = ads1015.readADC_SingleEnded(1);
  adc2 = ads1015.readADC_SingleEnded(2);
  adc3 = ads1015.readADC_SingleEnded(3);

  float Dcm = sonar.ping_cm();
  if (Dcm > (MAXRANGE - 20))
  {
    Dcm = 0;
  }

  //mapping to scale
  fsr1 = map(adc0, 0, 1640, 0, 100);
  fsr2 = map(adc1, 0, 1640, 0, 100);
  flex1 = map(adc2, 20, 80, 0, 100);
  flex2 = map(adc3, 25, 85, 0, 100);
  //ultra = map(Dcm, 0, 80, 0, 100);

  fsr1_A[i] = fsr1;
  fsr2_A[i] = fsr2;
  flex1_A[i] = flex1;
  flex2_A[i] = flex2;
  ultra_A[i] = Dcm;
  
  //Serial.print("LEFT: "); Serial.print(fsr1); Serial.print(", ");
  //Serial.print("RIGHT: "); Serial.print(fsr2); Serial.print(", ");
  //Serial.print("UP: "); Serial.print(flex1); Serial.print(", ");
  //Serial.print("DOWN: "); Serial.print(flex2); Serial.print(", ");
  //Serial.print("DISTANCE: "); Serial.print(Dcm); Serial.println("");
  
  i++;

  if(i == cycles)
  {
    //median of arrays
    
    fsr1_mdn = QuickMedian<float>::GetMedian(fsr1_A, cycles);
    fsr2_mdn = QuickMedian<float>::GetMedian(fsr2_A, cycles);
    flex1_mdn = QuickMedian<float>::GetMedian(flex1_A, cycles);
    flex2_mdn = QuickMedian<float>::GetMedian(flex2_A, cycles);
    ultra_mdn = QuickMedian<float>::GetMedian(ultra_A, cycles);
    
    //1 - left, 2 - right
    if(abs(fsr1_mdn - fsr2_mdn) > 0)
    {
      if(fsr1_mdn > fsr2_mdn)
      {
        fsr_asym = 100 - (100 * (fsr1_mdn - fsr2_mdn) / fsr1_mdn);
        skew_L_final = fsr_asym;
        skew_R_final = 100.0;
      }
      else
      {
        fsr_asym = 100 - (100 * (fsr2_mdn - fsr1_mdn) / fsr2_mdn);
        skew_L_final = 100.0;
        skew_R_final = fsr_asym;
      }
    }
    else
    {
      skew_L_final = 100.0;
      skew_R_final = 100.0;
    }

    //1 = up, 2 = down
    flex_asym = flex1_mdn - flex2_mdn;
    fsr_ave = (fsr1_mdn + fsr2_mdn) / 2;

    //slouch and lean level
    fuzzy1->setInput(1, flex_asym);
    fuzzy1->setInput(2, Dcm);
    fuzzy1->setInput(3, fsr_ave);

    fuzzy1->fuzzify();

    float slouch_final = fuzzy1->defuzzify(1);
    float lean_final = fuzzy1->defuzzify(2);

    posture_final = (min(slouch_final, lean_final) + min(skew_L_final, skew_R_final)) / 2;

    /*
    Serial.println("");
    Serial.print("Slouch: "); Serial.println(slouch_final);
    Serial.print(" | Lean: "); Serial.print(lean_final);
    Serial.println("");
    Serial.print(" | Left Skew: "); Serial.print(skew_L_final);
    Serial.print(" | Right Skew: "); Serial.print(skew_R_final);
    Serial.print(" | Right Skew: "); Serial.print(posture_final);
    Serial.println("");

    Serial.write('0');
    Serial.write(int(slouch_final));
    Serial.write(int(lean_final));
    Serial.write(int(skew_L_final));
    Serial.write(int(skew_R_final));
    Serial.write(int(posture_final));
    */
    
    //reconnect if connection is lost
    if(WiFi.status() != WL_CONNECTED)
    {
      while(WiFi.status() != WL_CONNECTED)
      {
        WiFi.begin(ssid, password);
        Serial.print(".");
        delay(100);
      }
      Serial.println("");
    }
    
    //send to thingspeak
    int fld1 = ThingSpeak.setField(1, slouch_final);
    int fld2 = ThingSpeak.setField(2, lean_final);
    int fld3 = ThingSpeak.setField(3, skew_L_final);
    int fld4 = ThingSpeak.setField(4, skew_R_final);
    int fld5 = ThingSpeak.setField(5, posture_final);

    int checkTS = ThingSpeak.writeFields(chanNum, apiKey);

    Serial.println("1: " + String(fld1) + "| 2: " + String(fld2) + "| 3: " + String(fld3) + "| 4: " + String(fld4) + "| 5: " + String(fld5));
    if(checkTS == 200){
      Serial.println("Channel update successful.");
    }
    else{
      Serial.println("Problem updating channel. HTTP error code " + String(checkTS));
    }
    
    i = 0;
  }
    
  delay(1000);
}
