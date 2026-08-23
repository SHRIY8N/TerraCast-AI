#include <Arduino.h>
#include <Wire.h>
#include <Arduino_LED_Matrix.h>
#include <Arduino_RouterBridge.h>

/*
  ============================================================
                    TERRACAST AI
                    FINAL CLEAN BUILD
  ============================================================

  HARDWARE
  ------------------------------------------------------------
  Soil moisture sensor:
    AO  -> A0
    VCC -> 3.3V
    GND -> GND

  OLED SSD1306 128x64 I2C:
    SDA -> SDA / D20
    SCL -> SCL / D21
    VCC -> 3.3V
    GND -> GND

  L298N:
    ENA -> D5
    IN1 -> D6
    IN2 -> D7
    GND -> UNO Q GND

  Demo button:
    D8 -> button -> GND

  SOIL CALIBRATION
  ------------------------------------------------------------
    RAW < 1000       = WET
    RAW 1000-1200    = MODERATE
    RAW > 1200       = DRY

  WATERING LOGIC
  ------------------------------------------------------------
    RAW > 1200 and rain probability < 60%
        -> pump ON

    Pump stays ON until:
        RAW <= 1000
        -> pump OFF

    Rain probability >= 60%
        -> watering blocked / pump OFF

    2-minute pump safety timeout

  DEMO BUTTON - EXACTLY 5 ACTIONS
  ------------------------------------------------------------
    1 = SUNNY
    2 = RAIN
    3 = WATERING
    4 = PLANT
    5 = LIVE

    Next press returns to 1.

  MATRIX
  ------------------------------------------------------------
    The four 12x8 reference patterns supplied by the user
    are used directly.

    LIVE uses the supplied LIVE reference pattern.

  OLED
  ------------------------------------------------------------
    The display orientation is unchanged.
    Letters use the existing working font.
    Digits use separate row-based 5x7 patterns so they
    cannot be horizontally mirrored by column-bit ordering.
*/


// ============================================================
// PINS
// ============================================================

const uint8_t SOIL_PIN = A0;

const uint8_t PUMP_ENA = D5;
const uint8_t PUMP_IN1 = D6;
const uint8_t PUMP_IN2 = D7;

const uint8_t DEMO_BUTTON_PIN = D8;


// ============================================================
// SOIL THRESHOLDS
// ============================================================

const int SOIL_WET_THRESHOLD = 1000;
const int SOIL_DRY_THRESHOLD = 1200;


// ============================================================
// IRRIGATION SETTINGS
// ============================================================

const unsigned long MAX_PUMP_RUNTIME = 120000UL;
const unsigned long WATERING_COOLDOWN = 30000UL;


// ============================================================
// OLED
// ============================================================

const uint8_t OLED_ADDRESS = 0x3C;

uint8_t oledBuffer[128 * 64 / 8];

bool oledOK = false;


// ============================================================
// LED MATRIX
// ============================================================

Arduino_LED_Matrix matrix;

uint8_t matrixFrame[104];


// ============================================================
// SENSOR / WEATHER STATE
// ============================================================

int soilRaw = 0;
int moisturePercent = 0;

bool soilOK = false;

int temperatureC = 28;
int rainProbability = 0;
int rainTenths = 0;


// ============================================================
// PUMP STATE
// ============================================================

bool pumpRunning = false;

unsigned long pumpStartTime = 0;
unsigned long lastWateringTime = 0;


// ============================================================
// DEMO MODE
// ============================================================
//
// 1 = SUNNY
// 2 = RAIN
// 3 = WATERING
// 4 = PLANT
// 5 = LIVE
//
// Start in LIVE.
// ============================================================

uint8_t demoMode = 5;


// ============================================================
// BUTTON STATE
// ============================================================

bool lastButtonState = HIGH;
unsigned long lastButtonTime = 0;


// ============================================================
// TIMERS
// ============================================================

unsigned long lastSoilRead = 0;
unsigned long lastOLEDUpdate = 0;
unsigned long lastMatrixUpdate = 0;


// ============================================================
// CITY
// ============================================================

const char CITY_NAME[] = "BENGALURU";


// ============================================================
// LETTER FONT
// 5 COLUMNS x 7 ROWS
// ============================================================

const uint8_t LETTER_FONT[26][5] = {

  // A
  {0x7E,0x11,0x11,0x11,0x7E},

  // B
  {0x7F,0x49,0x49,0x49,0x36},

  // C
  {0x3E,0x41,0x41,0x41,0x22},

  // D
  {0x7F,0x41,0x41,0x22,0x1C},

  // E
  {0x7F,0x49,0x49,0x49,0x41},

  // F
  {0x7F,0x09,0x09,0x09,0x01},

  // G
  {0x3E,0x41,0x49,0x49,0x7A},

  // H
  {0x7F,0x08,0x08,0x08,0x7F},

  // I
  {0x00,0x41,0x7F,0x41,0x00},

  // J
  {0x20,0x40,0x41,0x3F,0x01},

  // K
  {0x7F,0x08,0x14,0x22,0x41},

  // L
  {0x7F,0x40,0x40,0x40,0x40},

  // M
  {0x7F,0x02,0x0C,0x02,0x7F},

  // N
  {0x7F,0x04,0x08,0x10,0x7F},

  // O
  {0x3E,0x41,0x41,0x41,0x3E},

  // P
  {0x7F,0x09,0x09,0x09,0x06},

  // Q
  {0x3E,0x41,0x51,0x21,0x5E},

  // R
  {0x7F,0x09,0x19,0x29,0x46},

  // S
  {0x46,0x49,0x49,0x49,0x31},

  // T
  {0x01,0x01,0x7F,0x01,0x01},

  // U
  {0x3F,0x40,0x40,0x40,0x3F},

  // V
  {0x1F,0x20,0x40,0x20,0x1F},

  // W
  {0x7F,0x20,0x18,0x20,0x7F},

  // X
  {0x63,0x14,0x08,0x14,0x63},

  // Y
  {0x03,0x04,0x78,0x04,0x03},

  // Z
  {0x61,0x51,0x49,0x45,0x43}
};


// ============================================================
// DIGIT FONT
//
// These are row-based patterns.
// This completely avoids the previous digit problem.
// ============================================================

const char* DIGIT_0[7] = {
  " ### ",
  "#   #",
  "#   #",
  "#   #",
  "#   #",
  "#   #",
  " ### "
};

const char* DIGIT_1[7] = {
  "  #  ",
  " ##  ",
  "# #  ",
  "  #  ",
  "  #  ",
  "  #  ",
  " ### "
};

const char* DIGIT_2[7] = {
  " ### ",
  "#   #",
  "    #",
  "   # ",
  "  #  ",
  " #   ",
  "#####"
};

const char* DIGIT_3[7] = {
  " ### ",
  "#   #",
  "    #",
  " ### ",
  "    #",
  "#   #",
  " ### "
};

const char* DIGIT_4[7] = {
  "#  # ",
  "#  # ",
  "#  # ",
  "#####",
  "   # ",
  "   # ",
  "   # "
};

const char* DIGIT_5[7] = {
  "#####",
  "#    ",
  "#    ",
  "#### ",
  "    #",
  "#   #",
  " ### "
};

const char* DIGIT_6[7] = {
  " ### ",
  "#   #",
  "#    ",
  "#### ",
  "#   #",
  "#   #",
  " ### "
};

const char* DIGIT_7[7] = {
  "#####",
  "    #",
  "   # ",
  "  #  ",
  " #   ",
  " #   ",
  " #   "
};

const char* DIGIT_8[7] = {
  " ### ",
  "#   #",
  "#   #",
  " ### ",
  "#   #",
  "#   #",
  " ### "
};

const char* DIGIT_9[7] = {
  " ### ",
  "#   #",
  "#   #",
  " ####",
  "    #",
  "#   #",
  " ### "
};

const char* PERCENT_GLYPH[7] = {
  "#   #",
  "   # ",
  "  #  ",
  " #   ",
  "#   #",
  "     ",
  "     "
};


// ============================================================
// GET DIGIT PATTERN
// ============================================================

const char** getDigitPattern(char c)
{
  switch (c)
  {
    case '0': return DIGIT_0;
    case '1': return DIGIT_1;
    case '2': return DIGIT_2;
    case '3': return DIGIT_3;
    case '4': return DIGIT_4;
    case '5': return DIGIT_5;
    case '6': return DIGIT_6;
    case '7': return DIGIT_7;
    case '8': return DIGIT_8;
    case '9': return DIGIT_9;

    default:
      return nullptr;
  }
}


// ============================================================
// GET LETTER FONT
// ============================================================

const uint8_t* getLetterFont(char c)
{
  if (
    c >= 'A' &&
    c <= 'Z'
  )
  {
    return LETTER_FONT[
      c - 'A'
    ];
  }

  // Space
  static const uint8_t blank[5] = {
    0x00,
    0x00,
    0x00,
    0x00,
    0x00
  };

  return blank;
}


// ============================================================
// OLED COMMAND
// ============================================================

void oledCommand(
  uint8_t command
)
{
  Wire.beginTransmission(
    OLED_ADDRESS
  );

  Wire.write(0x00);
  Wire.write(command);

  Wire.endTransmission();
}


// ============================================================
// OLED DATA
// ============================================================

void oledData(
  const uint8_t* data,
  int length
)
{
  int position = 0;

  while (
    position < length
  )
  {
    Wire.beginTransmission(
      OLED_ADDRESS
    );

    Wire.write(0x40);

    int chunk =
      min(
        16,
        length - position
      );

    for (
      int i = 0;
      i < chunk;
      i++
    )
    {
      Wire.write(
        data[position + i]
      );
    }

    Wire.endTransmission();

    position += chunk;
  }
}


// ============================================================
// OLED INITIALIZATION
// ============================================================
//
// KEEP ORIGINAL ORIENTATION.
// A1 + C8 are intentionally unchanged.
//

bool initializeOLED()
{
  Wire.begin();

  Wire.setClock(
    100000
  );

  delay(200);

  Wire.beginTransmission(
    OLED_ADDRESS
  );

  if (
    Wire.endTransmission() != 0
  )
  {
    Serial.println(
      "OLED NOT FOUND"
    );

    return false;
  }

  oledCommand(0xAE);

  oledCommand(0xD5);
  oledCommand(0x80);

  oledCommand(0xA8);
  oledCommand(0x3F);

  oledCommand(0xD3);
  oledCommand(0x00);

  oledCommand(0x40);

  oledCommand(0x8D);
  oledCommand(0x14);

  oledCommand(0x20);
  oledCommand(0x00);

  // ORIGINAL ORIENTATION
  oledCommand(0xA1);
  oledCommand(0xC8);

  oledCommand(0xDA);
  oledCommand(0x12);

  oledCommand(0x81);
  oledCommand(0xCF);

  oledCommand(0xD9);
  oledCommand(0xF1);

  oledCommand(0xDB);
  oledCommand(0x40);

  oledCommand(0xA4);
  oledCommand(0xA6);

  oledCommand(0xAF);

  Serial.println(
    "OLED READY"
  );

  return true;
}


// ============================================================
// OLED PIXEL
// ============================================================

void oledPixel(
  int x,
  int y
)
{
  if (
    x < 0 ||
    x >= 128 ||
    y < 0 ||
    y >= 64
  )
  {
    return;
  }

  oledBuffer[
    (y / 8) * 128 + x
  ] |=
    (1 << (y % 8));
}


// ============================================================
// OLED CLEAR
// ============================================================

void oledClear()
{
  memset(
    oledBuffer,
    0,
    sizeof(oledBuffer)
  );
}


// ============================================================
// OLED BOX
// ============================================================

void oledBox(
  int x,
  int y,
  int width,
  int height
)
{
  for (
    int i = x;
    i < x + width;
    i++
  )
  {
    oledPixel(
      i,
      y
    );

    oledPixel(
      i,
      y + height - 1
    );
  }

  for (
    int i = y;
    i < y + height;
    i++
  )
  {
    oledPixel(
      x,
      i
    );

    oledPixel(
      x + width - 1,
      i
    );
  }
}


// ============================================================
// OLED HORIZONTAL LINE
// ============================================================

void oledLine(
  int x,
  int y,
  int width
)
{
  for (
    int i = 0;
    i < width;
    i++
  )
  {
    oledPixel(
      x + i,
      y
    );
  }
}


// ============================================================
// OLED DIGIT DRAWER
// ============================================================
//
// IMPORTANT:
// Digits are drawn ROW-BY-ROW.
// They do not use the column-based font.
// This prevents the previous mirrored/reversed digit bug.
//

void drawDigit(
  int x,
  int y,
  char digit
)
{
  const char** pattern =
    getDigitPattern(
      digit
    );

  if (
    pattern == nullptr
  )
  {
    return;
  }

  for (
    int row = 0;
    row < 7;
    row++
  )
  {
    for (
      int col = 0;
      col < 5;
      col++
    )
    {
      if (
        pattern[row][col] == '#'
      )
      {
        oledPixel(
          x + col,
          y + row
        );
      }
    }
  }
}


// ============================================================
// OLED PERCENT DRAWER
// ============================================================

void drawPercent(
  int x,
  int y
)
{
  for (
    int row = 0;
    row < 7;
    row++
  )
  {
    for (
      int col = 0;
      col < 5;
      col++
    )
    {
      if (
        PERCENT_GLYPH[row][col] == '#'
      )
      {
        oledPixel(
          x + col,
          y + row
        );
      }
    }
  }
}


// ============================================================
// OLED LETTER DRAWER
// ============================================================

void drawLetter(
  int x,
  int y,
  char letter
)
{
  const uint8_t* glyph =
    getLetterFont(
      letter
    );

  for (
    int col = 0;
    col < 5;
    col++
  )
  {
    uint8_t bits =
      glyph[col];

    for (
      int row = 0;
      row < 7;
      row++
    )
    {
      if (
        bits &
        (1 << row)
      )
      {
        oledPixel(
          x + col,
          y + row
        );
      }
    }
  }
}


// ============================================================
// OLED TEXT
// ============================================================
//
// Numbers are handled separately.
// Letters continue using the known-good font.
//

void oledText(
  int x,
  int y,
  const char* text
)
{
  while (*text)
  {
    char c = *text;

    if (
      c >= '0' &&
      c <= '9'
    )
    {
      drawDigit(
        x,
        y,
        c
      );
    }

    else if (
      c == '%'
    )
    {
      drawPercent(
        x,
        y
      );
    }

    else if (
      c >= 'A' &&
      c <= 'Z'
    )
    {
      drawLetter(
        x,
        y,
        c
      );
    }

    // One character = 6 pixels
    x += 6;

    text++;
  }
}


// ============================================================
// OLED UPDATE
// ============================================================

void updateOLEDHardware()
{
  for (
    int page = 0;
    page < 8;
    page++
  )
  {
    oledCommand(
      0xB0 + page
    );

    oledCommand(0x00);
    oledCommand(0x10);

    oledData(
      &oledBuffer[
        page * 128
      ],
      128
    );
  }
}


// ============================================================
// MODE TEXT
// ============================================================

const char* getModeText()
{
  switch (demoMode)
  {
    case 1: return "SUNNY";
    case 2: return "RAIN";
    case 3: return "WATER";
    case 4: return "PLANT";
    case 5: return "LIVE";
    default: return "LIVE";
  }
}


// ============================================================
// SOIL TEXT
// ============================================================

const char* getSoilText()
{
  if (!soilOK)
    return "ERROR";

  if (
    soilRaw <=
    SOIL_WET_THRESHOLD
  )
  {
    return "WET";
  }

  if (
    soilRaw <=
    SOIL_DRY_THRESHOLD
  )
  {
    return "MOD";
  }

  return "DRY";
}


// ============================================================
// WATER STATUS
// ============================================================

const char* getWateringText()
{
  if (
    pumpRunning
  )
  {
    return "WATER";
  }

  if (
    rainProbability >= 60
  )
  {
    return "SKIP";
  }

  if (
    soilRaw >
    SOIL_DRY_THRESHOLD
  )
  {
    return "READY";
  }

  return "OFF";
}


// ============================================================
// OLED DASHBOARD
// ============================================================

void drawOLED()
{
  if (!oledOK)
    return;

  char text[24];

  oledClear();

  oledBox(
    0,
    0,
    128,
    64
  );

  // Header
  oledText(
    4,
    3,
    CITY_NAME
  );

  oledText(
    94,
    3,
    getModeText()
  );

  oledLine(
    3,
    12,
    122
  );

  // Temperature
  snprintf(
    text,
    sizeof(text),
    "TEMP %d C",
    temperatureC
  );

  oledText(
    4,
    16,
    text
  );

  // Moisture
  snprintf(
    text,
    sizeof(text),
    "MOIST %d",
    moisturePercent
  );

  oledText(
    4,
    26,
    text
  );

  drawPercent(
    40,
    26
  );

  // RAW
  snprintf(
    text,
    sizeof(text),
    "RAW %d",
    soilRaw
  );

  oledText(
    70,
    26,
    text
  );

  // Soil state
  oledText(
    4,
    36,
    getSoilText()
  );

  // Water status
  oledText(
    70,
    36,
    getWateringText()
  );

  // Moisture bar
  oledBox(
    4,
    48,
    120,
    10
  );

  int fill =
    map(
      moisturePercent,
      0,
      100,
      0,
      116
    );

  fill =
    constrain(
      fill,
      0,
      116
    );

  for (
    int x = 6;
    x < 6 + fill;
    x++
  )
  {
    for (
      int y = 50;
      y <= 55;
      y++
    )
    {
      oledPixel(
        x,
        y
      );
    }
  }

  updateOLEDHardware();
}


// ============================================================
// SOIL SENSOR
// ============================================================
//
// <1000 = WET
// 1000-1200 = MODERATE
// >1200 = DRY
//

void readSoil()
{
  long total = 0;

  const int samples = 10;

  for (
    int i = 0;
    i < samples;
    i++
  )
  {
    total +=
      analogRead(
        SOIL_PIN
      );

    delay(2);
  }

  soilRaw =
    total / samples;

  soilOK =
    soilRaw >= 0 &&
    soilRaw <= 4095;

  if (!soilOK)
  {
    moisturePercent = 0;
    return;
  }

  // WET
  if (
    soilRaw <=
    SOIL_WET_THRESHOLD
  )
  {
    moisturePercent =
      map(
        soilRaw,
        0,
        SOIL_WET_THRESHOLD,
        100,
        70
      );
  }

  // MODERATE
  else if (
    soilRaw <=
    SOIL_DRY_THRESHOLD
  )
  {
    moisturePercent =
      map(
        soilRaw,
        SOIL_WET_THRESHOLD,
        SOIL_DRY_THRESHOLD,
        70,
        35
      );
  }

  // DRY
  else
  {
    moisturePercent =
      map(
        soilRaw,
        SOIL_DRY_THRESHOLD,
        4095,
        35,
        0
      );
  }

  moisturePercent =
    constrain(
      moisturePercent,
      0,
      100
    );

  Serial.print(
    "Soil RAW: "
  );

  Serial.print(
    soilRaw
  );

  Serial.print(
    " | Moisture: "
  );

  Serial.print(
    moisturePercent
  );

  Serial.print(
    "% | State: "
  );

  Serial.println(
    getSoilText()
  );
}


// ============================================================
// STOP PUMP
// ============================================================

void stopPump()
{
  digitalWrite(
    PUMP_ENA,
    LOW
  );

  digitalWrite(
    PUMP_IN1,
    LOW
  );

  digitalWrite(
    PUMP_IN2,
    LOW
  );

  if (
    pumpRunning
  )
  {
    Serial.println(
      "PUMP OFF"
    );

    lastWateringTime =
      millis();
  }

  pumpRunning = false;
}


// ============================================================
// START PUMP
// ============================================================

void startPump()
{
  if (pumpRunning)
    return;

  if (!soilOK)
    return;

  // Rain protection
  if (
    rainProbability >= 60
  )
  {
    Serial.println(
      "WATERING BLOCKED - RAIN"
    );

    return;
  }

  // Must actually be dry
  if (
    soilRaw <=
    SOIL_DRY_THRESHOLD
  )
  {
    return;
  }

  // Cooldown
  if (
    lastWateringTime != 0 &&
    millis() -
    lastWateringTime <
    WATERING_COOLDOWN
  )
  {
    return;
  }

  digitalWrite(
    PUMP_IN1,
    HIGH
  );

  digitalWrite(
    PUMP_IN2,
    LOW
  );

  digitalWrite(
    PUMP_ENA,
    HIGH
  );

  pumpRunning = true;

  pumpStartTime =
    millis();

  Serial.println(
    "PUMP ON - SOIL DRY"
  );
}


// ============================================================
// PUMP MONITOR
// ============================================================

void updatePump()
{
  if (!pumpRunning)
    return;

  // Primary stop condition
  if (
    soilRaw <=
    SOIL_WET_THRESHOLD
  )
  {
    stopPump();

    Serial.println(
      "PUMP OFF - SOIL WET"
    );

    return;
  }

  // Weather override
  if (
    rainProbability >= 60
  )
  {
    stopPump();

    Serial.println(
      "PUMP OFF - RAIN EXPECTED"
    );

    return;
  }

  // Safety timeout
  if (
    millis() -
    pumpStartTime >=
    MAX_PUMP_RUNTIME
  )
  {
    stopPump();

    Serial.println(
      "PUMP OFF - SAFETY TIMEOUT"
    );
  }
}


// ============================================================
// AUTOMATIC WATERING
// ============================================================

void automaticWatering()
{
  if (!soilOK)
  {
    stopPump();
    return;
  }

  // Rain expected
  if (
    rainProbability >= 60
  )
  {
    if (pumpRunning)
      stopPump();

    return;
  }

  // Wet
  if (
    soilRaw <=
    SOIL_WET_THRESHOLD
  )
  {
    stopPump();
    return;
  }

  // Dry
  if (
    soilRaw >
    SOIL_DRY_THRESHOLD
  )
  {
    startPump();
  }

  // Moderate:
  // No new watering cycle.
}


// ============================================================
// DEMO BUTTON
// ============================================================
//
// EXACT 5-ACTION CYCLE:
//
// 1 SUNNY
// 2 RAIN
// 3 WATERING
// 4 PLANT
// 5 LIVE
// back to 1
//

void checkDemoButton()
{
  bool currentState =
    digitalRead(
      DEMO_BUTTON_PIN
    );

  if (
    currentState !=
    lastButtonState &&
    millis() -
    lastButtonTime >
    80
  )
  {
    lastButtonTime =
      millis();

    lastButtonState =
      currentState;

    if (
      currentState == LOW
    )
    {
      // Stop pump when entering any demo screen.
      stopPump();

      // Cycle 1 -> 2 -> 3 -> 4 -> 5 -> 1
      if (
        demoMode >= 5
      )
      {
        demoMode = 1;
      }
      else
      {
        demoMode++;
      }

      // ------------------------------------------------------
      // 1 = SUNNY
      // ------------------------------------------------------

      if (
        demoMode == 1
      )
      {
        temperatureC = 29;
        rainProbability = 10;
        rainTenths = 0;

        Serial.println(
          "DEMO 1: SUNNY"
        );
      }

      // ------------------------------------------------------
      // 2 = RAIN
      // ------------------------------------------------------

      else if (
        demoMode == 2
      )
      {
        temperatureC = 25;
        rainProbability = 90;
        rainTenths = 80;

        Serial.println(
          "DEMO 2: RAIN"
        );
      }

      // ------------------------------------------------------
      // 3 = WATERING
      // ------------------------------------------------------

      else if (
        demoMode == 3
      )
      {
        temperatureC = 28;
        rainProbability = 5;
        rainTenths = 0;

        Serial.println(
          "DEMO 3: WATERING"
        );

        // Use real soil rule.
        if (
          soilRaw >
          SOIL_DRY_THRESHOLD
        )
        {
          startPump();
        }
        else
        {
          Serial.println(
            "DEMO WATERING: SOIL NOT DRY"
          );
        }
      }

      // ------------------------------------------------------
      // 4 = PLANT
      // ------------------------------------------------------

      else if (
        demoMode == 4
      )
      {
        temperatureC = 27;
        rainProbability = 20;
        rainTenths = 0;

        Serial.println(
          "DEMO 4: PLANT"
        );
      }

      // ------------------------------------------------------
      // 5 = LIVE
      // ------------------------------------------------------

      else
      {
        Serial.println(
          "DEMO 5: LIVE"
        );

        // Restore actual live operation.
        demoMode = 5;

        // Python will update these in LIVE.
        // Automatic watering is enabled.
      }
    }
  }
}


// ============================================================
// BRIDGE FUNCTIONS
// ============================================================

int getSoilRaw()
{
  return soilRaw;
}

int getMoisturePercent()
{
  return moisturePercent;
}

int getDemoMode()
{
  return demoMode;
}

int getPumpState()
{
  return pumpRunning
    ? 1
    : 0;
}


// ============================================================
// WEATHER INPUT FROM PYTHON
// ============================================================
//
// Numeric arguments only.
//

void setForecast(
  int temperature,
  int rainProbabilityValue,
  int rainAmountTenths
)
{
  // Weather updates are accepted only in LIVE.
  if (
    demoMode != 5
  )
  {
    return;
  }

  temperatureC =
    temperature;

  rainProbability =
    constrain(
      rainProbabilityValue,
      0,
      100
    );

  rainTenths =
    max(
      0,
      rainAmountTenths
    );
}


// ============================================================
// MATRIX
// ============================================================

void clearMatrix()
{
  memset(
    matrixFrame,
    0,
    sizeof(matrixFrame)
  );
}


// ============================================================
// SET MATRIX PIXEL
// ============================================================

void matrixPixel(
  int x,
  int y
)
{
  if (
    x < 0 ||
    x >= 13 ||
    y < 0 ||
    y >= 8
  )
  {
    return;
  }

  matrixFrame[
    y * 13 + x
  ] = 1;
}


// ============================================================
// DRAW 12x8 REFERENCE ICON
// ============================================================
//
// The supplied artwork is exactly 12 columns x 8 rows.
// UNO Q matrix is 13 columns x 8 rows.
//
// To preserve the artwork, it is placed at columns 0..11.
// The final column remains blank.
//

void drawReferenceIcon(
  const char* rows[8]
)
{
  for (
    int y = 0;
    y < 8;
    y++
  )
  {
    for (
      int x = 0;
      x < 12;
      x++
    )
    {
      if (
        rows[y][x] == '#'
      )
      {
        matrixPixel(
          x,
          y
        );
      }
    }
  }
}


// ============================================================
// SUNNY REFERENCE
// ============================================================

const char* SUNNY_ICON[8] = {
  "..#...#..#..",
  "...#.#..#...",
  "............",
  "..#..##.#...",
  "...#.##..#..",
  "............",
  "...#..#.#...",
  "..#..#...#.."
};


// ============================================================
// RAIN REFERENCE
// ============================================================

const char* RAIN_ICON[8] = {
  "............",
  "....####....",
  "...######...",
  "..########..",
  "..########..",
  "............",
  "...#.#.#.#..",
  "..#.#.#.#..."
};


// ============================================================
// WATER REFERENCE
// ============================================================

const char* WATER_ICON[8] = {
  "............",
  ".....##.....",
  "....##.#....",
  "...###..#...",
  "...####.#...",
  "...######...",
  "...######...",
  "....####...."
};


// ============================================================
// PLANT REFERENCE
// ============================================================

const char* PLANT_ICON[8] = {
  "......##....",
  "....#.#.##..",
  "....#####...",
  "..###.#.##..",
  "......#.....",
  "......#.....",
  "....#####...",
  ".....###...."
};


// ============================================================
// LIVE REFERENCE
// ============================================================
//
// Exact 12x8 pattern extracted from the supplied LIVE image.
//

const char* LIVE_ICON[8] = {
  "............",
  "#.####..####",
  "#..#.#..##..",
  "#..#.#..####",
  "#..#.#..##..",
  "#####.##.###",
  "............",
  "............"
};


// ============================================================
// UPDATE MATRIX
// ============================================================

void updateMatrix()
{
  // Keep the reference shapes stable.
  if (
    millis() -
    lastMatrixUpdate <
    150
  )
  {
    return;
  }

  lastMatrixUpdate =
    millis();

  clearMatrix();

  // ----------------------------------------------------------
  // 1 = SUNNY
  // ----------------------------------------------------------

  if (
    demoMode == 1
  )
  {
    drawReferenceIcon(
      SUNNY_ICON
    );
  }

  // ----------------------------------------------------------
  // 2 = RAIN
  // ----------------------------------------------------------

  else if (
    demoMode == 2
  )
  {
    drawReferenceIcon(
      RAIN_ICON
    );
  }

  // ----------------------------------------------------------
  // 3 = WATERING
  // ----------------------------------------------------------

  else if (
    demoMode == 3
  )
  {
    drawReferenceIcon(
      WATER_ICON
    );
  }

  // ----------------------------------------------------------
  // 4 = PLANT
  // ----------------------------------------------------------

  else if (
    demoMode == 4
  )
  {
    drawReferenceIcon(
      PLANT_ICON
    );
  }

  // ----------------------------------------------------------
  // 5 = LIVE
  // ----------------------------------------------------------

  else
  {
    drawReferenceIcon(
      LIVE_ICON
    );
  }

  matrix.draw(
    matrixFrame
  );
}


// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(
    115200
  );

  delay(1000);

  Serial.println();
  Serial.println(
    "=============================="
  );
  Serial.println(
    "        TERRACAST AI"
  );
  Serial.println(
    "        FINAL BUILD"
  );
  Serial.println(
    "=============================="
  );

  // Soil
  pinMode(
    SOIL_PIN,
    INPUT
  );

  analogReadResolution(
    12
  );

  // Pump
  pinMode(
    PUMP_ENA,
    OUTPUT
  );

  pinMode(
    PUMP_IN1,
    OUTPUT
  );

  pinMode(
    PUMP_IN2,
    OUTPUT
  );

  stopPump();

  // Button
  pinMode(
    DEMO_BUTTON_PIN,
    INPUT_PULLUP
  );

  // Matrix
  matrix.begin();

  matrix.setGrayscaleBits(
    1
  );

  clearMatrix();

  matrix.draw(
    matrixFrame
  );

  // OLED
  oledOK =
    initializeOLED();

  if (oledOK)
  {
    oledClear();

    oledBox(
      0,
      0,
      128,
      64
    );

    oledText(
      25,
      20,
      "TERRACAST"
    );

    oledText(
      40,
      34,
      "READY"
    );

    updateOLEDHardware();

    delay(1200);
  }

  // Bridge
  if (
    !Bridge.begin()
  )
  {
    Serial.println(
      "BRIDGE FAILED"
    );
  }
  else
  {
    Serial.println(
      "BRIDGE READY"
    );
  }

  Bridge.provide_safe(
    "get_soil_raw",
    getSoilRaw
  );

  Bridge.provide_safe(
    "get_moisture_percent",
    getMoisturePercent
  );

  Bridge.provide_safe(
    "get_demo_mode",
    getDemoMode
  );

  Bridge.provide_safe(
    "get_pump_state",
    getPumpState
  );

  Bridge.provide_safe(
    "set_forecast",
    setForecast
  );

  // First sensor reading
  readSoil();

  drawOLED();

  Serial.println(
    "TERRACAST AI READY"
  );
}


// ============================================================
// LOOP
// ============================================================

void loop()
{
  // Demo button
  checkDemoButton();

  // Soil update
  if (
    millis() -
    lastSoilRead >=
    1000
  )
  {
    lastSoilRead =
      millis();

    readSoil();

    // Automatic watering is active ONLY in LIVE mode.
    if (
      demoMode == 5
    )
    {
      automaticWatering();
    }
  }

  // Pump monitoring
  updatePump();

  // OLED
  if (
    millis() -
    lastOLEDUpdate >=
    700
  )
  {
    lastOLEDUpdate =
      millis();

    drawOLED();
  }

  // Matrix
  updateMatrix();

  delay(5);
}
