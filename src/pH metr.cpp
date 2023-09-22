#include <Arduino.h>

int pH_Value; 
float Voltage;
float pH;
 
void setup() 
{ 
  Serial.begin(9600);
  pinMode(pH_Value, INPUT); 
} 
 
void loop() 
{ 
  pH_Value = analogRead(A0); 
  Voltage = 0;
  for (int i = 0; i<=100; i++)
  {
    Voltage = Voltage + (pH_Value * (3.3 / 4095));
    delay(1);
  }
  Voltage = Voltage / 100;
  pH = Voltage * 2.8;

  Serial.print("Napięcie: "); 
  Serial.print(Voltage); 
  Serial.print("  ");
  Serial.print("pH: "); 
  Serial.print(pH);
  Serial.println();
  delay(100); 
}