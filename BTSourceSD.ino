#include <Arduino.h>
#include "Audio.h"
#include "SPI.h"
#include "SD.h"
#include "FS.h"
#include <BluetoothA2DPSource.h>
#include "freertos/ringbuf.h"

// Digital I/O used
#define I2S_DOUT      25
#define I2S_BCLK      27
#define I2S_LRC       26

#define BT_NAME "My POGS Wireless Headphone"
//#define BT_NAME "MI Portable Bluetooth Speaker"

Audio audio;
BluetoothA2DPSource a2dp_source;
RingbufHandle_t audioRingBuffer;

// feed the A2DP source with audio data
int32_t get_data_channels(Frame *frame, int32_t channel_len) {
    if (channel_len < 0 || frame == NULL)
        return 0;
    // Receive data from ring buffer
    size_t len{};
    #if ESP_ARDUINO_VERSION_MAJOR >= 2
        vRingbufferGetInfo(audioRingBuffer, nullptr, nullptr, nullptr, nullptr, &len);
    #else
        vRingbufferGetInfo(audioRingBuffer, nullptr, nullptr, nullptr, &len);
    #endif    
    if (len < (channel_len * 4)) {
        //Serial.println("not enough data");
        return 0;
    };
    size_t sampleSize = 0;
    uint8_t* sampleBuff;
    sampleBuff = (uint8_t *)xRingbufferReceiveUpTo(audioRingBuffer, &sampleSize, (portTickType)portMAX_DELAY, channel_len * 4);
    if (sampleBuff != NULL) {
        // fill the channel data
        for (int sample = 0; sample < (channel_len); ++sample) {
            frame[sample].channel1 = (sampleBuff[sample * 4 + 3] << 8) | sampleBuff[sample * 4 + 2];
            frame[sample].channel2 = (sampleBuff[sample * 4 + 1] << 8) | sampleBuff[sample * 4];
        };
        vRingbufferReturnItem(audioRingBuffer, (void *)sampleBuff);
    };
    return channel_len;   
};


// Bluetooth connection state changed
void connection_state_changed(esp_a2d_connection_state_t state, void *ptr){
  Serial.print("Bluetooth connection state: ");
  Serial.println(a2dp_source.to_str(state));
}

// Bluetooth audio state changed
void audio_state_changed(esp_a2d_audio_state_t state, void *ptr){
  Serial.print("Bluetooth audio state: " );
  Serial.println(a2dp_source.to_str(state));
}


void setup() {
    Serial.begin(115200);
    Serial.println("");
    Serial.println("BTSource demo");
    Serial.println("This demo streams audio data to bluetooth headset");
    Serial.println("");
    Serial.print("ESP-IDF version: ");
    Serial.println(String(ESP.getSdkVersion()));
    // create audio buffer
    audioRingBuffer =  xRingbufferCreate(8192, RINGBUF_TYPE_BYTEBUF);
    //  setuo BT source
    a2dp_source.set_on_connection_state_changed(connection_state_changed);
    a2dp_source.set_on_audio_state_changed(audio_state_changed);
    a2dp_source.set_auto_reconnect(false);
    a2dp_source.start(BT_NAME, get_data_channels);  
    // a2dp_source.set_task_core(11); /
    a2dp_source.set_volume(64); // BT-source volume
    //  start SD-card (SPI mode)
    pinMode(12, OUTPUT);
    pinMode(14, OUTPUT);
    digitalWrite(12, HIGH);
    digitalWrite(14, HIGH);
    if (!SD.begin(4)) {   
        Serial.println("Card Mount Failed");
        return;
    }
    Serial.println("SD mounted");
    // setup audio
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
}

void loop()
{
    audio.loop();
    if (a2dp_source.is_connected()) {
        if (!audio.isRunning()) {
            // start audio playback
            Serial.println("connecttoFS");
            audio.connecttoFS(SD, "/test.mp3");  
//            audio.connecttoFS(SD, "/Häschen_Hüpf.mp3");  
//            audio.connecttoFS(SD, "/Rabe.mp3");  
//            audio.connecttoFS(SD, "stereo-test.mp3");
        } else {
            ;
        }
   } 
}

// optional
void audio_info(const char *info){
    Serial.print("info        "); 
    Serial.println(info);
}


void audio_process_i2s(uint32_t* sample, bool *continueI2S){
    if (a2dp_source.is_connected()) {
        // send audio sample to ringbuffer
        *continueI2S = (pdTRUE != xRingbufferSend(audioRingBuffer, sample, sizeof(uint32_t), (portTickType)portMAX_DELAY));
    } else {
        *continueI2S = true;
    }
}

