# Soft-latch circuit Notes


## How to wire a latching power switch circuit (often called a soft-latch circuit) using a Si4599 N and P Channel MOSFET Expansion Board

Board module like this one: https://a.co/d/0cT1m7uf

The above Google web search returned this info, from this link:

https://www.bing.com/search?q=how+to+wire+a+latching+power+switch+circuit+(often+called+a+soft-latch+circuit)+using+a+Si4599+N+and+P+Channel+MOSFET+Expansion+Board&cvid=3900e779f3c74c93abf5615f0e96c8bb&gs_lcrp=EgRlZGdlKgYIABBFGDkyBggAEEUYOdIBCDcyODdqMGo3qAIAsAIA&FORM=ANNTA1&adppc=EDGEESS&PC=U531


Wiring a Soft‑Latch Power Switch with Si4599 N‑ and P‑Channel MOSFETs
You can build a soft‑latching power switch using the Si4599’s built‑in N‑channel and P‑channel MOSFETs by combining them in a high‑side/low‑side configuration with a momentary pushbutton and a microcontroller GPIO for the latch.

Core Topology
A common discrete soft‑latch uses:

High‑side P‑channel MOSFET (Si4599 P‑FET) as the main power switch.

Low‑side N‑channel MOSFET (Si4599 N‑FET) to pull the P‑FET gate low and hold it on.

Momentary pushbutton to initiate turn‑on.

MCU GPIO to control the N‑FET and break the latch when needed 

Why this works:
When off, the P‑FET gate is pulled high via a resistor, keeping it cutoff. Pressing the button pulls the gate low, turning the P‑FET on and powering the system. The MCU immediately drives its GPIO high, turning on the N‑FET, which holds the P‑FET gate low (latch on). To turn off, the MCU drives the GPIO low, disabling both MOSFETs 

Wiring Steps on the Si4599 Expansion Board
Identify MOSFET pins on the Si4599 board:

P‑FET: Source to V_IN, Drain to V_SYS, Gate to GATE_P.

N‑FET: Source to GND, Drain to V_SYS, Gate to GATE_N.

Add a pull‑up resistor (e.g., 10 kΩ) from GATE_P to V_IN to keep the P‑FET off when idle 

Connect the pushbutton between GATE_P and GND. This pulls the P‑FET gate low when pressed, turning it on.

Connect the MCU GPIO to GATE_N via a small series resistor (e.g., 100 Ω).

GPIO high → N‑FET on → pulls GATE_P low → latches on.

GPIO low → N‑FET off → GATE_P pulled high by resistor → latches off.

Optional: Add a small capacitor (e.g., 0.1 µF) from GATE_P to GND to prevent accidental latch‑on from stray capacitance 

Power rails:

V_IN: Unswitched supply (e.g., 5–12 V).

V_SYS: Switched output to MCU and load.

Example Signal Flow
Off: GATE_P pulled high → P‑FET off → V_SYS = 0 V.

Press button: GATE_P pulled low → P‑FET on → V_SYS powered → MCU GPIO high → N‑FET on → GATE_P held low → latch on.

Turn off: MCU GPIO low → N‑FET off → GATE_P pulled high → P‑FET off → latch off.

Tips
Ensure the Si4599’s P‑FET is rated for your supply voltage and current.

Keep the pushbutton debounce in software if needed.

For a press‑on/press‑off toggle, add a second GPIO or input to detect a second button press and break the latch 

Always verify gate‑source voltage limits for the Si4599 MOSFETs before applying supply.

This approach gives you a low‑standby‑current, reliable soft‑latch using only the Si4599’s built‑in MOSFETs, a pushbutton, and a microcontroller GPIO 








## Hardware discussion 1

Would a board module like this one work? https://a.co/d/0cT1m7uf
September 21, 2026
The specific Amazon module you shared (Si4599 N and P Channel MOSFET Expansion Board) will work perfectly for this circuit!
The Si4599 is an excellent choice because it combines one P-channel MOSFET and one N-channel MOSFET into a single chip. It eliminates the need for separate discrete transistors and provides a neat, tiny footprint for your build.
Here is how you would wire that exact module up to act as your intelligent soft-latch power switch.

Module Wiring Guide

The Si4599 module typically breaks out the pins for both internal transistors. You will wire them together on your breadboard or PCB to form the latch:
• P-Channel Side (The Main Switch):
	• Source pin: Connect directly to your battery/power supply positive (VCC).
	• Drain pin: Connect to the input of your MCU Voltage Regulator (this becomes your switched power rail).
	• Gate pin: Connect a 100kΩ resistor from this Gate pin to the Source pin (VCC). This acts as the pull-up that forces the system off by default.
• N-Channel Side (The Keep-Alive Control):
	• Source pin: Connect directly to system Ground (GND).
	• Drain pin: Connect directly to the P-Channel Gate pin.
	• Gate pin: Connect directly to your MCU's Keep-Alive GPIO pin. (It's also good practice to put a 10kΩ resistor from this gate to GND to keep it from floating during bootup).
• The Physical Button:
	• Wire a simple momentary push-button directly between the P-Channel Gate pin and GND (effectively sitting in parallel with the N-channel's Drain and Source).

Managing Standby (Lights and Fans)

Since you bought a multi-pack or if you have another one of these modules, you can use a second Si4599 module (or just the N-channel half of it) to handle the Standby Mode requirement:
1. Connect the negative wire of your lights and fans to the Drain of the second N-channel MOSFET.
2. Connect its Source to GND.
3. Connect its Gate to a second, separate MCU GPIO pin (let's call it PERIPHERAL_ENABLE).
When you want to enter Standby Mode, the MCU simply drives PERIPHERAL_ENABLE LOW (turning off the lights and fans) while keeping the first module's Keep-Alive pin HIGH (keeping itself powered).
Would you like the specific Arduino/C++ code structure demonstrating how to initialize the pins during bootup and trigger the shutdown sequence safely?




## Boilerplate code for standby & Shutdown using short & long presses of a non-latching push button switch:


Here is the updated boilerplate code for your ESP32-S3.
To allow the MCU to read the physical button while running, you just need to wire the MCU-side leg of the physical button to a third GPIO pin (we will use GPIO 6). Because the button connects to Ground when pressed, we configure GPIO 6 with an internal pull-up resistor.
This code uses a non-blocking timing loop in loop() to track exactly how long the button is held down.

#include <Arduino.h>
// Pin Definitionsconst int KEEP_ALIVE_PIN = 4;   // Connects to N-MOS Gate (Controls main system power)const int PERIPHERAL_PIN = 5;   // Connects to second N-MOS Gate (Controls Fans/Lights)const int USER_BUTTON_PIN = 6;  // Connects to the physical button to sense presses
// Timing Constantsconst unsigned long DEBOUNCE_DELAY = 50;    // Milliseconds to ignore button chatterconst unsigned long LONG_PRESS_TIME = 2000; // Hold for 2 seconds to trigger shutdown
// State Variablesbool isStandby = false;bool buttonState = HIGH;bool lastButtonState = HIGH;unsigned long buttonPressedTime = 0;unsigned long buttonReleasedTime = 0;bool isPressing = false;
// Forward Declarationsvoid enterStandbyMode();void exitStandbyMode();void powerOffSystem();
void setup() {
  // 1. IMMEDIATE ACTION: Drive GPIO 4 HIGH to lock the power switch open
  pinMode(KEEP_ALIVE_PIN, OUTPUT);
  digitalWrite(KEEP_ALIVE_PIN, HIGH);

  // 2. Initialize peripheral power (Turn ON fans and lights by default)
  pinMode(PERIPHERAL_PIN, OUTPUT);
  digitalWrite(PERIPHERAL_PIN, HIGH);

  // 3. Initialize Button Pin with Internal Pull-up
  pinMode(USER_BUTTON_PIN, INPUT_PULLUP);

  Serial.begin(115200);
  delay(500);
  Serial.println("System booted and power latched. Monitoring button...");
}
void handleShortPress() {
  // Toggle Standby Mode
  if (!isStandby) {
    enterStandbyMode();
  } else {
    exitStandbyMode();
  }
}
void loop() {
  // Read the button state (LOW = pressed, HIGH = released)
  int reading = digitalRead(USER_BUTTON_PIN);

  // Check for state change
  if (reading != lastButtonState) {
    // Reset debounce timer
    delay(DEBOUNCE_DELAY); 
    reading = digitalRead(USER_BUTTON_PIN); // Re-read after debounce
  }

  // Button just got pressed down
  if (reading == LOW && lastButtonState == HIGH && !isPressing) {
    buttonPressedTime = millis();
    isPressing = true;
  }

  // Button is being held down (Real-time check for long press)
  if (reading == LOW && isPressing) {
    unsigned long pressDuration = millis() - buttonPressedTime;
    
    // If held longer than the threshold, shut down immediately without waiting for release
    if (pressDuration >= LONG_PRESS_TIME) {
      powerOffSystem();
    }
  }

  // Button just got released
  if (reading == HIGH && lastButtonState == LOW && isPressing) {
    isPressing = false;
    unsigned long pressDuration = millis() - buttonPressedTime;

    // It was a short press
    if (pressDuration < LONG_PRESS_TIME) {
      handleShortPress();
    }
  }

  lastButtonState = reading;

  // Your main non-blocking application code can run here
}
void enterStandbyMode() {
  Serial.println("Action: Short Press Detected. Entering Standby...");
  isStandby = true;
  digitalWrite(PERIPHERAL_PIN, LOW); // Turn off lights/fans
  // Optional: Add code here to put ESP32 into light sleep if required
}
void exitStandbyMode() {
  Serial.println("Action: Short Press Detected. Exiting Standby...");
  isStandby = false;
  digitalWrite(PERIPHERAL_PIN, HIGH); // Turn lights/fans back on
}
void powerOffSystem() {
  Serial.println("Action: LONG PRESS DETECTED! Shutting down system...");
  Serial.flush();

  digitalWrite(PERIPHERAL_PIN, LOW);   // Turn off peripherals
  digitalWrite(KEEP_ALIVE_PIN, LOW);  // Cut main power latch

  while(1) {
    // Execution stops here as circuit power drops to 0V
  }
}

## 💡 Hardware Logic Reminder
Because GPIO 6 relies on its internal INPUT_PULLUP, it stays at 3.3V (HIGH) normally. Pressing the button hooks it directly to Ground, forcing it to 0V (LOW). The code handles this inversion automatically so your logic stays responsive.
Would you like me to show you how to configure a wake-up interrupt on GPIO 6 so that the ESP32-S3 can go into a deep energy-saving sleep during Standby Mode, instead of just sitting awake in a loop?



