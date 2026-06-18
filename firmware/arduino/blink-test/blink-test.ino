void setup() {
  pinMode(8, OUTPUT);   // onboard LED
  pinMode(1, OUTPUT);   // power relay
  pinMode(2, OUTPUT);   // reset relay

  // start with relays off (active-low)
  digitalWrite(1, HIGH);
  digitalWrite(2, HIGH);
}

void loop() {
  // LED + relays ON
  digitalWrite(8, LOW);
  digitalWrite(1, LOW);
  digitalWrite(2, LOW);
  delay(1000);

  // LED + relays OFF
  digitalWrite(8, HIGH);
  digitalWrite(1, HIGH);
  digitalWrite(2, HIGH);
  delay(1000);
}
