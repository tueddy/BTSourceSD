#include "Arduino.h"
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


Audio audio;
BluetoothA2DPSource a2dp_source;
RingbufHandle_t audioRingBuffer;


// feed the A2DP source with audio data
int32_t get_data_channels(Frame *frame, int32_t channel_len) {
    if (channel_len < 0 || frame == NULL)
        return 0;

    // Receive data from ring buffer
    size_t len{};
    vRingbufferGetInfo(audioRingBuffer, nullptr, nullptr, nullptr, nullptr, &len);
    if (len < (channel_len * 4)) {
        Serial.println("not enough data");
        return 0;
    };

    size_t item_size = 0;
    int16_t *item;
    item = (int16_t *)xRingbufferReceiveUpTo(audioRingBuffer, &item_size, 0, channel_len * 2);
    if (item != NULL) {
        if (channel_len  != (item_size / 2)) {
   	        printf( "a2dp_input_data_callback itemSize != dataLength (%u != %u)\n", item_size, channel_len );
        };
        // fill the channel data
        for (int sample = 0; sample < (item_size / 2); ++sample) {
            frame[sample].channel1 = item[sample * 2];
            frame[sample].channel2 = item[sample * 2 + 1];
        }
        vRingbufferReturnItem(audioRingBuffer, (void *)item);
    };
    return item_size / 2;   
}

/*
//int a2dp_input_data_callback( unsigned char* data, int dataLength ){
int32_t get_data_channels(Frame *frame, int32_t channel_len) { 
   if (channel_len < 0 || frame == NULL)return 0;  
   size_t itemSize = 0;
   Frame* ringBufferItem = (Frame*)xRingbufferReceiveUpTo( audioRingBuffer, &itemSize, 0, channel_len); 
   if( ringBufferItem == NULL ){
   	printf("ringBufferItem is NULL\n");
   	return 0;
   }
   if( itemSize != channel_len ){
   	printf( "a2dp_input_data_callback itemSize != dataLength (%u != %u)\n", itemSize, channel_len );
   	
   	vRingbufferReturnItem( audioRingBuffer, ringBufferItem );
   	return 0;
   }
   
   memcpy( frame, ringBufferItem, itemSize );
   
   vRingbufferReturnItem( audioRingBuffer, ringBufferItem );
      
   return itemSize;
}
*/

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
    // create audio buffer
    audioRingBuffer =  xRingbufferCreate(8192, RINGBUF_TYPE_BYTEBUF);
    //  setuo BT source
    a2dp_source.set_on_connection_state_changed(connection_state_changed);
    a2dp_source.set_on_audio_state_changed(audio_state_changed);
    a2dp_source.start("MI Portable Bluetooth Speaker", get_data_channels);  
    a2dp_source.set_volume(18);
    //  start SD
    pinMode(2, INPUT_PULLUP);
    SPI.begin(14, 2, 15);
    if(!SD.begin(13)){
        Serial.println("Card Mount Failed");
        return;
    }


    // setup audio
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(20); // 0...21
    // start  audio
 //   audio.connecttoFS(SD, "/test.mp3");
}

void loop()
{
    audio.loop();
    if ((!audio.isRunning()) && (a2dp_source.is_connected())) {
        // start audio playback
        audio.connecttoFS(SD, "/test.mp3");
   }
}

// optional
void audio_info(const char *info){
    //if (startsWith((char *)info, "SampleRate")) {
        Serial.print("info        "); Serial.println(info);
    //}
}



void audio_process_extern(int16_t* buff, uint16_t len, bool *continueI2S){
    *continueI2S = true;
    if (len) {
        // fill ringbuffer with audio data
        xRingbufferSend(audioRingBuffer, buff, len * sizeof(int16_t), 0);
    }    
}
