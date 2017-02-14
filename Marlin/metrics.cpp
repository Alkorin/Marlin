#include "metrics.h"

#include "cardreader.h"
#include "temperature.h"

#define HAVE_BED_TEMPERATURE_SENSOR ((TEMP_SENSOR_BED == 0) ? 0 : 1)

uint32_t metrics_Layers, metrics_Duration;
uint32_t metrics_Layer, metrics_LayerDuration;
uint32_t metrics_Layer0Start, metrics_LayerStart;

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

static void spiSend16(uint16_t l) {
    spiSend(l);
    spiSend(l>>8);
}

static void spiSend32(uint32_t l) {
    spiSend(l);
    spiSend(l>>8);
    spiSend(l>>16);
    spiSend(l>>24);
}

void metrics_startPrint(long layer, long duration) {
    metrics_Layers = layer;
    metrics_Duration = duration;
}

void metrics_startLayer(long layer, long duration) {
    metrics_Layer = layer;
    metrics_LayerDuration = duration;
    metrics_LayerStart = millis();
    if (layer == 0) {
        // Start of Layer 0
        metrics_Layer0Start = millis();
    }
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

    // Write Progress (printTime, totalTime, currentLayer, totalLayer, layerTime, layerDuration)
    if(metrics_Layer0Start != 0)
    {
        spiSend(METRICS_OPCODE_PRINT_PROGRESS);
        spiSend32(millis() - metrics_Layer0Start);
        spiSend32(metrics_Duration);
        spiSend16(metrics_Layer);
        spiSend16(metrics_Layers);
        spiSend16(millis() - metrics_LayerStart);
        spiSend16(metrics_LayerDuration);
    }

    csHigh();

    lastUpdateTime = millis();
}
