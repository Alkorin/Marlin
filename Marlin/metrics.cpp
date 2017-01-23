#include "metrics.h"

#include "cardreader.h"
#include "temperature.h"

#define HAVE_BED_TEMPERATURE_SENSOR ((TEMP_SENSOR_BED == 0) ? 0 : 1)

static void csLow() {
    // SPI Enable, Master, Fosc/4
    SPCR = _BV(SPE) | _BV(MSTR) | (0);

    digitalWrite(21, LOW);
}

static void csHigh() {
    digitalWrite(21, HIGH);
}
static void spiSend(uint8_t b) {
    SPDR = b;
    while (!TEST(SPSR, SPIF));
}

void metrics_init() {
    // Set D21 (CS/) as output and HIGH
    pinMode(21, OUTPUT);
    csHigh();
 
    // Send config
    csLow();
    spiSend(METRICS_OPCODE_CONFIG);
    spiSend(EXTRUDERS);                     // Nb of extruders
    spiSend(HAVE_BED_TEMPERATURE_SENSOR);   // Bed temperature sensor or not
    csHigh();
}

void metrics_update() {
    static unsigned long lastUpdateTime = millis();

    if(millis() - lastUpdateTime < 100) {
        // Too early, wait a little bit
        return;
    }

    csLow();

    // Write Temperatures
    for (int i = 0; i < EXTRUDERS; i++) {
        spiSend(METRICS_OPCODE_TEMP_H0 + i);
        spiSend((uint8_t)degHotend(i));
        spiSend((uint8_t)degTargetHotend(i));
    }
    
    #if HAVE_BED_TEMPERATURE_SENSOR
        spiSend(METRICS_OPCODE_TEMP_BED);
        spiSend((uint8_t)degBed());
        spiSend((uint8_t)degTargetBed());
    #endif

    // Write Status
    #if ENABLED(SDSUPPORT)
        spiSend(METRICS_OPCODE_SD_PRINTING);
        spiSend(IS_SD_PRINTING);
        if (IS_SD_PRINTING) {
            spiSend(METRICS_OPCODE_SD_PROGRESS);
            spiSend(card.percentDone());
        }
    #endif

    csHigh();

    lastUpdateTime = millis();
}
