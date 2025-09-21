int sensorValue;

void setup() {
  Serial.begin(9600);
}

void loop() {
  sensorValue = analogRead(15);
  Serial.println(sensorValue);
  delay(100);
}

// Upright is 4095.
// Full bent is around 2020.