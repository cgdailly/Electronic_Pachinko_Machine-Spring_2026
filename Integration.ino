// SLEEP MODE API https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/system/sleep_modes.html
// NON VOLATILE API https://docs.espressif.com/projects/arduino-esp32/en/latest/tutorials/preferences.html4

// LIBRARIES ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <ESP32Servo.h> // Servo
#include <Adafruit_NeoPixel.h> // LED Strip
#include <Preferences.h> // Non-Volatile
#include <WiFi.h> // WiFi
#include <Stepper.h> // Stepper
#include "esp_sleep.h" // Deep Sleep

// VALUES //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// LED STRIP VALUES

#define LIGHT_PIN 34
#define LIGHTS 90

// BRUSH MOTOR CONTROL VALUES

#define PWM_A 6
#define A1_PIN 7
#define A2_PIN 8

// SLEEP MODE VALUES

#define SLEEP_BUTTON 1
#define SLEEP_TRIGGER 9 // (TOUCH)

#define ON_LIGHT 15

// SERVO MOTOR PINS

#define BLUE_SERVO_PIN 38
#define BLACK_SERVO_PIN 39

// STEPPER MOTOR CONTROL PINS

#define STEPPER_CONTROL 42
#define IN1 5
#define IN2 4
#define IN3 3
#define IN4 2

// SENSOR PINS (ANALOG)

#define SENSOR_0 12
#define SENSOR_1 13
#define SENSOR_2 14

// VARIABLES ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// SENSOR VARIABLES, USED FOR DEBOUNCE

unsigned long last0 = 0;
unsigned long last1 = 0;
unsigned long last2 = 0;

// INITIALIZE LEDS, KEEP TRACK OF COLOR CHANGING PATTERN

Adafruit_NeoPixel lights(LIGHTS, LIGHT_PIN, NEO_GRB + NEO_KHZ800);
uint8_t color = 0;
unsigned long colorChange = 0;

// INITIALIZE SERVOS FOR FLIPPERS, SET UP FLIP BACK AND FORTH

Servo blueServo;
Servo blackServo;

unsigned long servoTiming = 0;
bool flipLeft = true;

// INITIALIZE BALL RETURN, SIGNIFY HOW MANY ARE RETURNED

Stepper ballReturn(2048, IN1, IN3, IN2, IN4);
uint8_t numReturned = 0;
bool returning = false;

// INITIALIZE DEEP SLEEP, SIGNAL WHEN INTERRUPT IS CALLED

volatile bool goToSleep = false;

// INITIALIZE NON-VOLATILE, KEEP TRACK OF SCORE

Preferences nonVol;
int highScore;
int score;

// INITIALIZE WIFI, PREPARE FOR SLOT GAME VALUES

const char* ssid = "Embedded_Pachinko";
const char* password = "4180_Project!";
bool casinoRoyale = false;
uint8_t casinoPrize;

int8_t slot1 = -1;
int8_t slot2 = -1;
int8_t slot3 = -1; 

WiFiServer jackpot(80);

// THREAD DECLARATIONS, MUTEX TO PROTECT SHARED VARIABLES

TaskHandle_t aesthetics;
TaskHandle_t gameplay;

SemaphoreHandle_t sharedVars;

// THREADING ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// CORE 0 //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*

Core 0 contains all the aesthetic components (minus the LEDs) as well as the WiFi and allows the game visuals to continue during computations during gameplay. 
If there are balls to be returned, this also returns the balls through the ball return.

  */

void Aesthetics(void *pvParameters) {
  Serial.print("Aesthetics running on core ");
  Serial.println(xPortGetCoreID());

  // TURN ON MOTORS

  analogWrite(PWM_A, 50);
  digitalWrite(A1_PIN, HIGH);
  digitalWrite(A2_PIN, LOW);

  for (;;) {

    // WiFi CONNECTING

    WiFiClient client = jackpot.available();

    if (client) {                       // CONNECTION SECURED
      Serial.println("New Client.");
      String currentLine = "";
      currentLine.reserve(128);

      unsigned long requestStart = millis();
      bool responseSent = false;

      while (client.connected() && (millis() - requestStart < 1000)) {
        while (client.available()) {
          char c = client.read();

          if (c == '\n') {
            if (currentLine.length() == 0) {
              bool localCasino = false;
              int8_t localSlot1, localSlot2, localSlot3;
              uint8_t localCasinoPrize;
              int localScore, localHighScore;

              xSemaphoreTake(sharedVars, portMAX_DELAY);        // MUTEX PROTECTING VARIABLES
              localCasino = casinoRoyale;
              casinoRoyale = false;
              xSemaphoreGive(sharedVars);

              if (localCasino) {
                runCasinoRoyale();                              // CASINO GAME RUN
              }

              xSemaphoreTake(sharedVars, portMAX_DELAY);
              localSlot1 = slot1;
              localSlot2 = slot2;
              localSlot3 = slot3;
              localCasinoPrize = casinoPrize;
              localScore = score;
              localHighScore = highScore;
              xSemaphoreGive(sharedVars);

              client.print(                                      // WEBPAGE
                "HTTP/1.1 200 OK\r\n"
                "Content-type:text/html\r\n"
                "Connection: close\r\n"
                "\r\n"
                "<!DOCTYPE html><html>"
                "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
                "<link rel=\"icon\" href=\"data:,\">"
                "<style>"
                "html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center; background: #111; color: white; }"
                "body { margin-top: 60px; }"
                ".num { font-size: 3rem; margin: 10px; color: gold; }"
                ".prize { font-size: 2rem; margin-top: 20px; color: cyan; }"
                ".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px; text-decoration: none; font-size: 24px; margin: 10px; cursor: pointer; }"
                "</style></head><body>"
              );

              client.print("<h1>");
              client.print(ssid);
              client.println("</h1>");

              client.println("<h2>Casino Royale</h2>");

              client.print("<p style='font-size:1.5rem; color:lightgreen;'>Current Score: ");
              client.print(localScore);
              client.println("</p>");

              client.print("<p style='font-size:1.5rem; color:gold;'>High Score: ");
              client.print(localHighScore);
              client.println("</p>");

              if (localSlot1 != -1 && localSlot2 != -1 && localSlot3 != -1) {
                client.print("<div class='num'>");
                client.print(localSlot1);
                client.println("</div>");

                client.print("<div class='num'>");
                client.print(localSlot2);
                client.println("</div>");

                client.print("<div class='num'>");
                client.print(localSlot3);
                client.println("</div>");

                client.print("<div class='prize'>Prize: ");
                client.print(localCasinoPrize);
                client.println("</div>");
              } else {
                client.println("<p>Waiting for jackpot trigger...</p>");
              }

              client.println("<p><a href=\"/\"><button class=\"button\">View Updates</button></a></p>");
              client.println("</body></html>");

              responseSent = true;
              break;
            } else {
              currentLine = "";
            }
          } else if (c != '\r') {
            if (currentLine.length() < 127) {
              currentLine += c;
            }
          }
        }

        if (responseSent) {
          break;
        }

        vTaskDelay(1 / portTICK_PERIOD_MS);
      }

      client.stop();
      Serial.println("Client disconnected.");
      Serial.println("");
    }

    // BALL RETURN

    uint8_t localReturn = 0;

    xSemaphoreTake(sharedVars, portMAX_DELAY);              // MUTEX

    if (returning) { 
      localReturn = numReturned;
      numReturned = 0;
      returning = false;
    }

    xSemaphoreGive(sharedVars);

    if (localReturn) {
      returnBalls(localReturn);                              // BALL RETURN
      localReturn = 0;
    }

    // SERVO MOTORS FLIP BETWEEN TWO ANGLES

    if (millis() - servoTiming >= 1000) {
      servoTiming = millis();

      if (flipLeft) {
        blueServo.write(0);
        blackServo.write(180);
      } else {
        blueServo.write(180);
        blackServo.write(0);
      }

      flipLeft = !flipLeft;
    }

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }

}

// CORE 1 //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*

Core 1 contains the gameplay aspects, including the sensor systems and the sleep mode, as well as the light updates which are quite quick.

*/

void GamePlay(void *pvParameters) {
  Serial.print("GamePlay running on core ");
  Serial.println(xPortGetCoreID());

  for (;;) {

    if (goToSleep) {                                            // Sleep mode turns everything off
      Serial.println("Entering deep sleep...");
      goToSleep = false;
      lights.clear();
      lights.show();
      delay(100);
      esp_deep_sleep_start();
    }

    sensorCheck();                                              // Checks if sensors are triggered

    // LIGHTS SWITCH COLOR

    if (millis() - colorChange >= 2000) {
      colorUpdate(); 
    } 

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// HELPERS /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void returnBalls(uint8_t num) {                                     // Runs the ball return num times
  for (int i = 0; i < num; i++) {
    ballReturn.step(512);
  }
}

void sensorCheck() {                                                // Checks if sensors are triggered with individual callibrations
    int sense0 = analogRead(SENSOR_0);
    int sense1 = analogRead(SENSOR_1);
    int sense2 = analogRead(SENSOR_2);

// USED TO CALIBRATE SENSORS
  
    // Serial.print("Sensor 0: ");
    // Serial.println(sense0);
    // if (sense0 > 70) {
    //   Serial.println("///////////////////////////////////////////////////////");
    // }
    // Serial.print("Sensor 1: ");
    // Serial.println(sense1);
    // if (sense1 > 200) {
    //   Serial.println("///////////////////////////////////////////////////////");
    // }
    // Serial.print("Sensor 2: ");
    // Serial.println(sense2);
    // if (sense2 > 95) {
    //   Serial.println("///////////////////////////////////////////////////////");
    // }

// CHECK IF LIGHTS MUST REACT
  
    bool doPlusTwo = false;
    bool doPlusThree = false;
    bool doCasino = false;

    xSemaphoreTake(sharedVars, portMAX_DELAY);                            // Mutex to protect shared variables

    if (sense0 >= 76 && sense0 < 100 && (millis() - last0 > 2000)) {
      Serial.println(sense0);
      Serial.println("Sensor 0! +2 Balls!");
      score += 2;
      numReturned = 2;
      returning = true;
      doPlusTwo = true;
      last0 = millis();
    }
    if (sense1 >= 150 && sense1 < 230 && (millis() - last1 > 2000)) {
      Serial.println(sense1);
      Serial.println("Sensor 1! Casino Royale!");
      casinoRoyale = true;
      doCasino = true;
      last1 = millis();
    }
    if (sense2 >= 95 && (millis() - last2 > 2000)) {
      Serial.println(sense2);
      Serial.println("Sensor 2! +3 Balls!");
      score += 3;
      numReturned = 3;
      returning = true;
      doPlusThree = true;
      last2 = millis();
    }

    xSemaphoreGive(sharedVars);

    if (doPlusTwo) {
      plusTwoReact();
    } else if (doPlusThree) {
      plusThreeReact();
    } else if (doCasino) {
      casinoReact();
    }

    xSemaphoreTake(sharedVars, portMAX_DELAY);

    if (score > highScore) {                                          // Updates high score
      highScore = score;
      nonVol.putInt("highScore", score);
    }

    xSemaphoreGive(sharedVars);
}

void setLights(uint8_t red, uint8_t green, uint8_t blue) {              // Sets all lights the same color on RGB scale
  for (int i = 0; i < LIGHTS; i++) {
    lights.setPixelColor(i, lights.Color(red, green, blue));
  }
  lights.show();
}

void colorUpdate() {                                                    // Changes the color according to pattern
  colorChange = millis();
      switch (color) {
        case 0:
          setLights(255, 0, 0);
          break;
        case 1:
          setLights(0, 255, 0);
          break;
        case 2:
          setLights(0, 0, 255);
          break;
        default:
          setLights(255, 0, 0);
      }

      color = (color + 1) % 3;
}

// THE REACTS BLINK DIFFERENT COLORS THREE TIMES IN REACTION TO THE SENSOR READINGS

void plusThreeReact() {                                                
  for (int i = 0; i < 3; i++) {
        setLights(40, 0, 120);
        delay(500);
        lights.clear();
        lights.show();
        delay(500);
  }
  colorUpdate();
}

void plusTwoReact() {
  for (int i = 0; i < 3; i++) {
        setLights(255, 0, 0);
        delay(500);
        lights.clear();
        lights.show();
        delay(500);
  }
  colorUpdate();
}

void casinoReact() {
  for (int i = 0; i < 3; i++) {
        setLights(0, 120, 60);
        delay(500);
        lights.clear();
        lights.show();
        delay(500);
  }
  colorUpdate();
}

void runCasinoRoyale() {                                      // Chooses three numbers between 0, 5 as the slot responses and updates them for the webpage
  uint8_t first = random(0, 5);
  Serial.print("Slot 1: ");
  Serial.println(first);
  delay(300);

  uint8_t second = random(0, 5);
  Serial.print("Slot 2: ");
  Serial.println(second);
  delay(300);

  uint8_t third = random(0, 5);
  Serial.print("Slot 3: ");
  Serial.println(third);

  uint8_t finalPrize = 0;

  if (first == second && second == third) {                                                // Calculate final score by doubling number of matching numbers
    finalPrize = 6;
  } else if (first == second || first == third || second == third) {
    finalPrize = 4;
  } else {
    finalPrize = 2;
  }

  Serial.print("Casino prize: ");
  Serial.println(finalPrize);

  xSemaphoreTake(sharedVars, portMAX_DELAY);                                                // Mutex while updating variables

  slot1 = first;
  slot2 = second;
  slot3 = third;
  casinoPrize = finalPrize;
  score += casinoPrize;

  xSemaphoreGive(sharedVars);
}

// INTERRUPTS //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ARDUINO_ISR_ATTR sleepCheck() {                                    // Triggers sleep mode
  digitalWrite(ON_LIGHT, LOW);
  goToSleep = true;
}

// SETUP ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup() {

  Serial.begin(115200);

  // GPIO PIN CONTROL

  pinMode(PWM_A, OUTPUT);
  pinMode(A1_PIN, OUTPUT);
  pinMode(A2_PIN, OUTPUT);

  pinMode(STEPPER_CONTROL, INPUT_PULLDOWN);
  pinMode(SLEEP_BUTTON, INPUT_PULLDOWN);
  pinMode(SLEEP_TRIGGER, INPUT_PULLUP);
  pinMode(ON_LIGHT, OUTPUT);

  pinMode(SENSOR_0, INPUT);
  pinMode(SENSOR_1, INPUT);
  pinMode(SENSOR_2, INPUT);

  // SERVO SETUP

  blueServo.attach(BLUE_SERVO_PIN);
  blackServo.attach(BLACK_SERVO_PIN);

  blueServo.write(0);
  blackServo.write(180);

  Serial.println("Comet Flippers Ready");
  
  // BRUSH MOTOR SETUP

  Serial.println("Rocket Spinner Ready");

  // STEPPER MOTOR SETUP

  ballReturn.setSpeed(15);
  Serial.println("Ball Return Ready");

  // DEEP SLEEP SETUP

  esp_err_t err = esp_sleep_enable_ext0_wakeup((gpio_num_t)SLEEP_BUTTON, 0);
  if (err == ESP_OK) {
    Serial.println("Deep sleep configured!");
  }

  digitalWrite(ON_LIGHT, HIGH);

  // // WiFi SETUP

  WiFi.softAP(ssid, password);

  Serial.print("AP IP address: ");

  IPAddress IP = WiFi.softAPIP();                  // IP Access for webpage
  while (!IP) {
    Serial.println("Populate and print out IPAddress");
    delay(5000);
  }
  Serial.println(IP);

  jackpot.begin();

  Serial.println("Casino Royale activated!");

  // THREADING SETUP

  sharedVars = xSemaphoreCreateMutex();

  xTaskCreatePinnedToCore(Aesthetics, "Aesthetics", 8192, NULL, 1, &aesthetics, 0);
  xTaskCreatePinnedToCore(GamePlay, "Gameplay", 4096, NULL, 1, &gameplay, 1);

  // SLEEP INTERRUPT SETUP

  attachInterrupt(SLEEP_TRIGGER, sleepCheck, FALLING);

  // NON-VOLATILE SETUP

  nonVol.begin("Pachinko", false);
  highScore = nonVol.getInt("highScore", 0);
  Serial.print("highScore is: ");
  Serial.println(highScore);

  // LED SETUP

  lights.begin();
  setLights(255, 255, 255);
  lights.setBrightness(100);
  lights.show();

  // INITIAL BALL SETUP

  returnBalls(5);

}

void loop() {
}
