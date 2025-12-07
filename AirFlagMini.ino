#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

// Dev kit pins
#define BUTTON_SELECT_PIN 12
#define BUTTON_UP_PIN 14
#define BUTTON_DOWN_PIN 27
#define BUTTON_LEFT_PIN 26
#define BUTTON_RIGHT_PIN 25
#define VIBE 13
#define RED_LED 2

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

struct ButtonState {
    uint8_t pin;
    bool lastReading;
    unsigned long lastChangeMs;
    unsigned long pressedAt;
    bool holdReported;
};

ButtonState selectButton{BUTTON_SELECT_PIN, HIGH, 0, 0, false};
ButtonState upButton{BUTTON_UP_PIN, HIGH, 0, 0, false};
ButtonState downButton{BUTTON_DOWN_PIN, HIGH, 0, 0, false};
ButtonState leftButton{BUTTON_LEFT_PIN, HIGH, 0, 0, false};
ButtonState rightButton{BUTTON_RIGHT_PIN, HIGH, 0, 0, false};

constexpr uint16_t debounceMs = 60;
constexpr uint16_t holdMs = 900;

enum ScreenState { SCREEN_PING, SCREEN_DEVICES, SCREEN_ACTIONS, SCREEN_GPIO, SCREEN_BLE, SCREEN_DELAY };
ScreenState currentScreen = SCREEN_PING;
ScreenState deviceReturnScreen = SCREEN_PING;

const char *devices[] = {"Gateway A", "Sensor Pod", "Test Beacon", "Field Unit", "ATS-A1111", "ATS-B2222"};
constexpr int deviceCount = sizeof(devices) / sizeof(devices[0]);
int selectedDevice = 0; // Last slot is "Back"

int lastPingCount = 0;
unsigned long lastPingStamp = 0;

int selectedAction = 0; // 0 = GPIO, 1 = BLE

int gpioSelection = 0; // 0 = OFF, 1 = ON, 2 = Back
int gpioState = 0;

enum BLEMode { BLE_OFF, BLE_ON, BLE_ON_DELAY };
BLEMode bleMode = BLE_OFF;
int bleSelection = 0; // 0=OFF,1=ON,2=Delay,3=Back

int delayHours = 0;
int delayMinutes = 5;
int delayField = 0; // 0=tens hour,1=ones hour,2=tens min,3=ones min,4=OK,5=Back

unsigned long vibeUntil = 0;

bool checkPress(ButtonState &btn) {
    bool reading = digitalRead(btn.pin) == LOW;
    unsigned long now = millis();
    if (reading != btn.lastReading && (now - btn.lastChangeMs) > debounceMs) {
        btn.lastChangeMs = now;
        btn.lastReading = reading;
        if (reading) {
            btn.pressedAt = now;
            btn.holdReported = false;
            return true;
        }
    }
    return false;
}

bool checkHold(ButtonState &btn) {
    bool reading = digitalRead(btn.pin) == LOW;
    unsigned long now = millis();
    if (reading && btn.pressedAt != 0 && !btn.holdReported && (now - btn.pressedAt) >= holdMs) {
        btn.holdReported = true;
        return true;
    }
    if (!reading) {
        btn.holdReported = false;
        btn.pressedAt = 0;
    }
    return false;
}

int wrapUp(int value, int maxExclusive) {
    value--;
    if (value < 0) {
        value = maxExclusive - 1;
    }
    return value;
}

int wrapDown(int value, int maxExclusive) {
    value++;
    if (value >= maxExclusive) {
        value = 0;
    }
    return value;
}

void drawTitle(const char *text) {
    display.setTextSize(1);
    display.setTextColor(WHITE, BLACK);
    display.setCursor(0, 0);
    display.println(text);
}

void drawFooter(const char *text) {
    display.setTextSize(1);
    display.setTextColor(WHITE, BLACK);
    display.setCursor(0, 54);
    display.println(text);
}

void drawPingScreen() {
    drawTitle("Ping");
    display.setTextSize(1);
    display.setTextColor(WHITE, BLACK);
    display.setCursor(0, 14);
    display.println("Devices");
    display.setTextSize(1);
    display.setTextColor(WHITE, BLACK);
    display.setCursor(0, 26);
    display.println("Select = Ping");
    if (lastPingStamp > 0) {
        display.setCursor(0, 38);
        display.print(lastPingCount);
        display.print(" found ");
        display.print((millis() - lastPingStamp) / 1000);
        display.print("s ago");
    }
}

void drawDeviceScreen() {
    drawTitle("Devices");
    display.setTextSize(1);
    display.setTextColor(WHITE, BLACK);
    display.setCursor(0, 12);
    display.println("Pick a device");

    int y = 22;
    for (int i = 0; i < deviceCount + 1; i++) {
        bool isBack = (i == deviceCount);
        if (i == selectedDevice) {
            display.setTextColor(BLACK, WHITE);
            display.setCursor(0, y);
            display.print("> ");
            display.println(isBack ? "[ Back ]" : devices[i]);
            display.setTextColor(WHITE, BLACK);
        } else {
            display.setCursor(0, y);
            display.print("  ");
            display.println(isBack ? "[ Back ]" : devices[i]);
        }
        y += 8;
    }
}

void drawActionScreen() {
    if (selectedDevice >= deviceCount) {
        Serial.println("drawActionScreen invalid selectedDevice");
        return;
    }

    drawTitle("Actions");
    display.setTextSize(1);
    display.setTextColor(WHITE, BLACK);
    display.setCursor(0, 14);
    display.print("Device: ");
    display.println(devices[selectedDevice]);

    const char *options[3] = {"GPIO", "BLE", "[ Back ]"};
    int y = 26;
    for (int i = 0; i < 3; i++) {
        if (i == selectedAction) {
            display.setTextColor(BLACK, WHITE);
            display.setCursor(0, y);
            display.println(options[i]);
            display.setTextColor(WHITE, BLACK);
        } else {
            display.setCursor(0, y);
            display.println(options[i]);
        }
        y += 10;
    }
}

void drawGpioScreen() {
    if (selectedDevice >= deviceCount) {
        Serial.println("drawGpioScreen invalid selectedDevice");
        return;
    }

    drawTitle("GPIO");
    display.setTextSize(1);
    display.setTextColor(WHITE, BLACK);
    display.setCursor(0, 14);
    display.print("Device: ");
    display.println(devices[selectedDevice]);
    display.setCursor(0, 22);
    display.print("State: ");
    display.println(gpioState == 1 ? "ON" : "OFF");

    const char *options[3] = {"OFF", "ON", "[ Back ]"};
    int y = 32;
    for (int i = 0; i < 3; i++) {
        if (i == gpioSelection) {
            display.setTextColor(BLACK, WHITE);
            display.setCursor(0, y);
            display.println(options[i]);
            display.setTextColor(WHITE, BLACK);
        } else {
            display.setCursor(0, y);
            display.println(options[i]);
        }
        y += 8;
    }
}

void drawBleScreen() {
    if (selectedDevice >= deviceCount) {
        Serial.println("drawBleScreen invalid selectedDevice");
        return;
    }

    drawTitle("BLE");
    display.setTextSize(1);
    display.setTextColor(WHITE, BLACK);
    display.setCursor(0, 12);
    display.print("Delay ");
    if (delayHours < 10) display.print("0");
    display.print(delayHours);
    display.print(":");
    if (delayMinutes < 10) display.print("0");
    display.println(delayMinutes);

    display.setCursor(0, 22);
    display.print("Device: ");
    display.println(devices[selectedDevice]);

    const char *options[4] = {"OFF", "ON", "ON w/Delay", "[ Back ]"};
    int y = 32;
    for (int i = 0; i < 4; i++) {
        if (i == bleSelection) {
            display.setTextColor(BLACK, WHITE);
            display.setCursor(0, y);
            display.println(options[i]);
            display.setTextColor(WHITE, BLACK);
        } else {
            display.setCursor(0, y);
            display.println(options[i]);
        }
        y += 8;
    }
}

void drawDelayScreen() {
    drawTitle("Delay");
    display.setTextSize(1);
    display.setTextColor(WHITE, BLACK);
    display.setCursor(0, 12);
    display.println("Pick digit, Up/Down +/-");

    int tensH = delayHours / 10;
    int onesH = delayHours % 10;
    int tensM = delayMinutes / 10;
    int onesM = delayMinutes % 10;

    display.setTextSize(2);
    display.setCursor(8, 26);
    display.setTextColor(delayField == 0 ? BLACK : WHITE, delayField == 0 ? WHITE : BLACK);
    display.print(tensH);

    display.setTextColor(delayField == 1 ? BLACK : WHITE, delayField == 1 ? WHITE : BLACK);
    display.setCursor(24, 26);
    display.print(onesH);

    display.setTextColor(WHITE, BLACK);
    display.setCursor(40, 26);
    display.print(":");

    display.setTextColor(delayField == 2 ? BLACK : WHITE, delayField == 2 ? WHITE : BLACK);
    display.setCursor(50, 26);
    display.print(tensM);

    display.setTextColor(delayField == 3 ? BLACK : WHITE, delayField == 3 ? WHITE : BLACK);
    display.setCursor(66, 26);
    display.print(onesM);

    display.setTextSize(1);
    display.setCursor(0, 44);
    display.setTextColor(delayField == 4 ? BLACK : WHITE, delayField == 4 ? WHITE : BLACK);
    display.println("   [ OK ]");
    display.setCursor(0, 52);
    display.setTextColor(delayField == 5 ? BLACK : WHITE, delayField == 5 ? WHITE : BLACK);
    display.println("   [ Back ]");
}

void renderScreen() {
    display.clearDisplay();
    display.setTextColor(WHITE, BLACK);
    switch (currentScreen) {
        case SCREEN_PING:
            drawPingScreen();
            break;
        case SCREEN_DEVICES:
            drawDeviceScreen();
            break;
        case SCREEN_ACTIONS:
            drawActionScreen();
            break;
        case SCREEN_GPIO:
            drawGpioScreen();
            break;
        case SCREEN_BLE:
            drawBleScreen();
            break;
        case SCREEN_DELAY:
            drawDelayScreen();
            break;
    }
    display.display();
}

void goBackOneLevel() {
    switch (currentScreen) {
        case SCREEN_DELAY:
            currentScreen = SCREEN_BLE;
            delayField = 0;
            break;
        case SCREEN_GPIO:
        case SCREEN_BLE:
        case SCREEN_ACTIONS:
            currentScreen = SCREEN_DEVICES;
            deviceReturnScreen = SCREEN_ACTIONS;
            selectedDevice = 0;
            break;
        case SCREEN_DEVICES:
            currentScreen = SCREEN_PING;
            selectedDevice = 0;
            deviceReturnScreen = SCREEN_PING;
            break;
        default:
            break;
    }
}

void handleSelect() {
    switch (currentScreen) {
        case SCREEN_PING:    
            if (deviceCount > 0)
                lastPingCount = (lastPingCount % deviceCount) + 1;
            else
                lastPingCount = 0;
            lastPingStamp = millis();
            digitalWrite(VIBE, HIGH);
            vibeUntil = millis() + 300;
            deviceReturnScreen = SCREEN_PING;
            selectedDevice = 0;
            currentScreen = SCREEN_DEVICES;
            break;
        case SCREEN_DEVICES:
            if (selectedDevice == deviceCount) {
                currentScreen = deviceReturnScreen;
                if (currentScreen == SCREEN_PING) {
                    selectedDevice = 0;
                    deviceReturnScreen = SCREEN_PING;
                } else if (currentScreen == SCREEN_ACTIONS) {
                    selectedAction = 0;
                }
            } else {
                currentScreen = SCREEN_ACTIONS;
                deviceReturnScreen = SCREEN_ACTIONS;
                selectedAction = 0;
            }
            break;
        case SCREEN_ACTIONS:
            if (selectedAction == 2) {
                goBackOneLevel();
            } else {
                currentScreen = selectedAction == 0 ? SCREEN_GPIO : SCREEN_BLE;
            }
            break;
        case SCREEN_GPIO:
            if (gpioSelection == 2) {
                goBackOneLevel();
            } else {
                gpioState = gpioSelection;
                digitalWrite(VIBE, HIGH);
                vibeUntil = millis() + 150;
                currentScreen = SCREEN_ACTIONS;
            }
            break;
        case SCREEN_BLE:
            if (bleSelection == 3) {
                goBackOneLevel();
            } else if (bleSelection == 2) {
                currentScreen = SCREEN_DELAY;
                delayField = 0;
            } else {
                bleMode = static_cast<BLEMode>(bleSelection);
                digitalWrite(VIBE, HIGH);
                vibeUntil = millis() + 150;
                currentScreen = SCREEN_ACTIONS;
            }
            break;
        case SCREEN_DELAY:
            if (delayField == 4) {
                bleMode = BLE_ON_DELAY;
                bleSelection = 2;
                digitalWrite(VIBE, HIGH);
                vibeUntil = millis() + 150;
                currentScreen = SCREEN_BLE;
                delayField = 0;
            } else if (delayField == 5) {
                goBackOneLevel();
            }
            break;
    }
}

void handleLeft() {
    switch (currentScreen) {
        case SCREEN_DEVICES:
            selectedDevice = wrapUp(selectedDevice, deviceCount + 1);
            break;
        case SCREEN_ACTIONS:
            selectedAction = wrapUp(selectedAction, 3);
            break;
        case SCREEN_GPIO:
            gpioSelection = wrapUp(gpioSelection, 3);
            break;
        case SCREEN_BLE:
            bleSelection = wrapUp(bleSelection, 4);
            break;
        case SCREEN_DELAY:
            if (delayField > 0) {
                delayField--;
            }
            break;
        default:
            break;
    }
}

void handleRight() {
    switch (currentScreen) {
        case SCREEN_DEVICES:
            selectedDevice = wrapDown(selectedDevice, deviceCount + 1);
            break;
        case SCREEN_ACTIONS:
            selectedAction = wrapDown(selectedAction, 3);
            break;
        case SCREEN_GPIO:
            gpioSelection = wrapDown(gpioSelection, 3);
            break;
        case SCREEN_BLE:
            bleSelection = wrapDown(bleSelection, 4);
            break;
        case SCREEN_DELAY:
            if (delayField < 5) {
                delayField++;
            }
            break;
        case SCREEN_PING:
            handleSelect();
            break;
        default:
            break;
    }
}

int modWrap(int value, int delta, int mod) {
    int n = (value + delta) % mod;
    if (n < 0) n += mod;
    return n;
}

void adjustDelayDigit(int field, int delta) {
    int tensH = delayHours / 10;
    int onesH = delayHours % 10;
    int tensM = delayMinutes / 10;
    int onesM = delayMinutes % 10;

    switch (field) {
        case 0: {
            tensH = modWrap(tensH, delta, 3); // 0-2
            if (tensH == 2 && onesH > 3) onesH = 3;
            break;
        }
        case 1: {
            int maxOnesH = (tensH == 2) ? 3 : 9;
            onesH = modWrap(onesH, delta, maxOnesH + 1);
            break;
        }
        case 2: {
            tensM = modWrap(tensM, delta, 6); // 0-5
            break;
        }
        case 3: {
            onesM = modWrap(onesM, delta, 10); // 0-9
            break;
        }
        default:
            break;
    }

    delayHours = tensH * 10 + onesH;
    if (delayHours > 23) delayHours = 23;
    delayMinutes = tensM * 10 + onesM;
    if (delayMinutes > 59) delayMinutes = 59;
}

void handleUp() {
    switch (currentScreen) {
        case SCREEN_DEVICES:
            selectedDevice = wrapUp(selectedDevice, deviceCount + 1);
            break;
        case SCREEN_ACTIONS:
            selectedAction = wrapUp(selectedAction, 3);
            break;
        case SCREEN_GPIO:
            gpioSelection = wrapUp(gpioSelection, 3);
            break;
        case SCREEN_BLE:
            bleSelection = wrapUp(bleSelection, 4);
            break;
        case SCREEN_DELAY:
            if (delayField < 4) {
                adjustDelayDigit(delayField, 1);
            }
            break;
        default:
            break;
    }
}

void handleDown() {
    switch (currentScreen) {
        case SCREEN_DEVICES:
            selectedDevice = wrapDown(selectedDevice, deviceCount + 1);
            break;
        case SCREEN_ACTIONS:
            selectedAction = wrapDown(selectedAction, 3);
            break;
        case SCREEN_GPIO:
            gpioSelection = wrapDown(gpioSelection, 3);
            break;
        case SCREEN_BLE:
            bleSelection = wrapDown(bleSelection, 4);
            break;
        case SCREEN_DELAY:
            if (delayField < 4) {
                adjustDelayDigit(delayField, -1);
            }
            break;
        default:
            break;
    }
}

void pollButtons() {
    bool selectPressed = checkPress(selectButton);
    bool upPressed = checkPress(upButton);
    bool downPressed = checkPress(downButton);
    bool leftPressed = checkPress(leftButton);
    bool rightPressed = checkPress(rightButton);
    bool selectHeld = checkHold(selectButton);

    if (selectHeld) {
        goBackOneLevel();
        return;
    }
    if (leftPressed) {
        handleLeft();
    }
    if (rightPressed) {
        handleRight();
    }
    if (selectPressed) {
        handleSelect();
    }
    if (upPressed) {
        handleUp();
    }
    if (downPressed) {
        handleDown();
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(BUTTON_SELECT_PIN, INPUT_PULLUP);
    pinMode(BUTTON_UP_PIN, INPUT_PULLUP);
    pinMode(BUTTON_DOWN_PIN, INPUT_PULLUP);
    pinMode(BUTTON_LEFT_PIN, INPUT_PULLUP);
    pinMode(BUTTON_RIGHT_PIN, INPUT_PULLUP);
    pinMode(VIBE, OUTPUT);
    pinMode(RED_LED, OUTPUT);
    digitalWrite(VIBE, LOW);
    digitalWrite(RED_LED, LOW);

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        while (true) {
            delay(10);
        }
    }
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE, BLACK);
    display.setCursor(0, 12);
    display.println("AirSentry");
    display.setTextSize(1);
    display.setCursor(0, 36);
    display.println("Dummy UI (DEV)");
    display.display();
    delay(1200);
}

void loop() {
    pollButtons();
    if (vibeUntil && millis() > vibeUntil) {
        digitalWrite(VIBE, LOW);
        vibeUntil = 0;
    }
    renderScreen();
    delay(20);
}