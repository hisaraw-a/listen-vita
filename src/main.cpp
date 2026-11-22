/*


  Fetch 2
  curl 'https://listen.moe/graphql' \
  --compressed \
  -X POST \
  -H 'User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:155.0) Gecko/20100101 Firefox/155.0' \
  -H 'Accept: application/graphql-response+json, application/json' \
  -H 'Accept-Language: en-US,en;q=0.9' \
  -H 'Accept-Encoding: gzip, deflate, br, zstd' \
  -H 'Content-Type: application/json' \
  -H 'Referer: https://listen.moe/' \
  -H 'Origin: https://listen.moe' \
  -H 'Sec-GPC: 1' \
  -H 'Sec-Fetch-Dest: empty' \
  -H 'Sec-Fetch-Mode: cors' \
  -H 'Sec-Fetch-Site: same-origin' \
  -H 'Connection: keep-alive' \
  -H 'Priority: u=0' \
  -H 'TE: trailers' \
  --data-raw $'{"query":"\\n  mutation login($username: String\041, $password: String\041) {\\n    login(username: $username, password: $password) {\\n      user {\\n        uuid username displayName avatarImage bannerImage bio\\n        roles { name slug color songRequests }\\n        settings { mfa }\\n        additionalSongRequests\\n        uploadLimit\\n      }\\n      token\\n      mfa\\n    }\\n  }\\n","variables":{"username":"test","password":"test"},"operationName":"login"}'
  Response: {"data":{"login":{"user":{"uuid":"b399d324-bf45-432b-ac81-527ec7df977c","username":"test","displayName":"test","avatarImage":null,"bannerImage":null,"bio":null,"roles":[],"settings":{"mfa":false},"additionalSongRequests":0,"uploadLimit":30},"token":"","mfa":false}}}
  We only need the token. 
curl 'https://listen.moe/api/v1/search' \
  --compressed \
  -X POST \
  -H 'User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:155.0) Gecko/20100101 Firefox/155.0' \
  -H 'Accept: * *' \
  -H 'Accept-Language: en-US,en;q=0.9' \
  -H 'Accept-Encoding: gzip, deflate, br, zstd' \
  -H 'Content-Type: application/json' \
  -H 'Referer: https://listen.moe/music?q=pop' \
  -H 'Origin: https://listen.moe' \
  -H 'Sec-GPC: 1' \
  -H 'Sec-Fetch-Dest: empty' \
  -H 'Sec-Fetch-Mode: cors' \
  -H 'Sec-Fetch-Site: same-origin' \
  -H 'Authorization: Bearer TOKEN HERE' \
  -H 'Connection: keep-alive' \
  -H 'Cookie: token= TOKEN HERE' \
  -H 'Priority: u=4' \
  -H 'TE: trailers' \
  --data-raw '{"search_text":"pop","limit":50,"result_types":["songs"]}'
  This'll be needed, as this will have to be the only way to request songs.
*/

#define VITASDK
// Proprietary libraries here
#include <psp2/io/stat.h>
#include <psp2/sysmodule.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/display.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/net/http.h>
#include <psp2/io/fcntl.h>
#include <psp2/json.h>
#include "psp2/ctrl.h"
#include "psp2/rtc.h"
// C-style libraries here
#include <stdarg.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>
#include <string>
#include "math.h"
#include "vector"
#include "cstring"
#include "mpg123.h"

// Curl and SSL
#include "openssl/opensslv.h"
#include <openssl/ssl.h>
#include <openssl/crypto.h>
#include <curl/curl.h>

// SDL libraries 
#include "SDL3/SDL.h" 
#include "SDL3_ttf/SDL_ttf.h"

#define RING_BUFFER_SIZE (1024 * 1024) // 1 MB fixed buffer

// Configuration

const char *stream_url = "http://listen.moe:8080/fallback";
const char *output_file = "ux0:data/curltest/stream.mp3";
const char *output_folder = "ux0:data/curltest";
const char *filetype = ".mp3";
const char *existing_file ="old.mp3";

 const char *meta_url = "https://listen.moe/metadata";
const char* FONT0 = "app0:files/ns.ttf"; 
const char* FONT1 = "app0:files/console.ttf"; 


enum {
    SCREEN_WIDTH  = 960,
    SCREEN_HEIGHT = 544
};
struct Leaf{
  uint16_t x;
  uint16_t y;
  int age;
};
typedef struct {
    uint8_t buffer[RING_BUFFER_SIZE];
    size_t read_pos;
    size_t write_pos;
    size_t count;
    SDL_Mutex *mutex;
    SDL_Condition *cond;
    bool is_closed;
} AudioRingBuffer;

struct Configurations {
    int8_t CONFIG_OPT;
    bool IsOnConfig;
    bool Debug; 
    bool RecordFile;
    bool GetStreamTime;
    bool DoRequestBETA;
    bool RenameFile2title;
    bool IsPaused;
    
    

};

struct AppStruct {
    
    SDL_Window* window;
    SDL_Renderer* renderer;
    uint64_t StreamedBytes; 
    uint64_t StreamOLDBytes;
    TTF_Font* FONT00;
    TTF_Font* FONT01;
    SceIoStat* StreamStat;
    FILE* StreamFile;
    const char* streamFileName;
    AudioRingBuffer* buffer;
    char* title;
    char* oldTitle;
    bool MetaTrigger;
    bool MetaReady;
    bool AudioInit;
    uint64_t TransferSpeed; 
    bool LockDownload;
    char* MetadataOut;
    size_t MoSize;
    
};

// libmpg123 handle
mpg123_handle *mh = nullptr;
SceCtrlData ctrl;
SceCtrlData oldctrl;
SDL_AudioStream* g_audio_stream = nullptr;
int leafAmount = 0;
#define leafMAX 8
float  leafRate =  0.2;


#define MetadataSize 2048
#define MaxDebugPrints 16
int8_t maxOpt = 5;
uint64_t Timeinterval = 16;
Leaf* leafTable[leafMAX] = {nullptr};
char* debugScreen[MaxDebugPrints] = {NULL}; 
AppStruct* MainObject;
Configurations Config;


char* c_strdup(const char* src) {
    if (!src) return nullptr;
    char* dest = new char[strlen(src) + 1];
    strcpy(dest, src);
    return dest;
}
#define strdup c_strdup

void CustomDebug(const char* text) {
    if (Config.Debug == true){
    if (!text) return;
    for (int i = 0; i < MaxDebugPrints; i++) {
        if (debugScreen[i] == NULL) {
            debugScreen[i] = strdup(text);
            break; 
        }
    }
    }
}

void CustomDebugFmt(const char* text, void* arg) {
    //NOTE:  config.debug == true is needed to avoid perfomance loss
    if (Config.Debug == true){
    char buffer[1024];
    int res = snprintf(buffer, sizeof(buffer), text, arg);
    if (res >= 0) {
        CustomDebug(buffer);
    } else {
        CustomDebug("Got error in one of debug text calls!");
    }
   }
}

AudioRingBuffer* ring_buffer_create() {
    AudioRingBuffer *rb = (AudioRingBuffer*)malloc(sizeof(AudioRingBuffer));
    rb->read_pos = 0;
    rb->write_pos = 0;
    rb->count = 0;
    rb->mutex = SDL_CreateMutex();
    rb->cond = SDL_CreateCondition();
    rb->is_closed = false;
    return rb;
}

void ring_buffer_write(AudioRingBuffer *rb, const uint8_t *data, size_t size) {
    SDL_LockMutex(rb->mutex);
    for (size_t i = 0; i < size; i++) {
        while (rb->count == RING_BUFFER_SIZE && !rb->is_closed) {
            SDL_WaitCondition(rb->cond, rb->mutex);
        }
        if (rb->is_closed) break;
        rb->buffer[rb->write_pos] = data[i];
        rb->write_pos = (rb->write_pos + 1) % RING_BUFFER_SIZE;
        rb->count++;
    }
    SDL_SignalCondition(rb->cond);
    SDL_UnlockMutex(rb->mutex);
}

void ring_buffer_close(AudioRingBuffer *rb) {
    SDL_LockMutex(rb->mutex);
    rb->is_closed = true;
    SDL_SignalCondition(rb->cond);
    SDL_UnlockMutex(rb->mutex);
}

// Helper to safely read bytes from the ring buffer for mpg123_feed
size_t ring_buffer_read_bytes(AudioRingBuffer *rb, uint8_t *dest, size_t max_size) {
    SDL_LockMutex(rb->mutex);
    size_t bytes_read = 0;
    while (bytes_read < max_size && rb->count > 0) {
        dest[bytes_read++] = rb->buffer[rb->read_pos];
        rb->read_pos = (rb->read_pos + 1) % RING_BUFFER_SIZE;
        rb->count--;
    }
    SDL_SignalCondition(rb->cond);
    SDL_UnlockMutex(rb->mutex);
    return bytes_read;
}

void swapFile(const char *new_path) {
    MainObject->LockDownload = true;
    CustomDebug("swapFile was called!");
    if (MainObject->StreamFile) {
        fflush(MainObject->StreamFile);
        fclose(MainObject->StreamFile);
    }
    MainObject->streamFileName = new_path;
    char buf[128];
    snprintf(buf,127,"%s/%s%s",output_folder,new_path,filetype);
    FILE* f = fopen(buf,"wb");
    if (f !=NULL){
        fclose(f);
    MainObject->StreamFile = fopen(buf,"wb");
    MainObject->StreamedBytes = 0;
    MainObject->LockDownload = false;
    
    CustomDebugFmt("Mainobject->StreamFile is now reading into %s",(void*)buf);
    }else{
     CustomDebugFmt("could not fopen into %s",(void*)buf);
    }
}
void checkSongChange(){
    CustomDebug("checkSongChange was called!");
    //Does the user allow this to happen in the first place?
    if (Config.RenameFile2title == true && MainObject->oldTitle !=nullptr && MainObject->title !=nullptr && Config.RecordFile == true){
    CustomDebug("Passed config check #1");
    if (strcmp(MainObject->title,MainObject->oldTitle) != 0){  
            //You see, the metadata is updated slowly, so this check is necessary to avoid locking downloads too much 
    swapFile(MainObject->oldTitle);
          CustomDebugFmt("File swap! Now writing into %s",(void*)MainObject->title);
        }
  
         
            
   }else{
    CustomDebug("Did not commit song change, same names");
   }
}
    


void parseTitle() {
    //Welcome to IF statement hell!
  //  CustomDebug("parseTitle was called!");
  if (MainObject->MetaReady == true && MainObject->MetadataOut !=nullptr &&strlen(MainObject->MetadataOut) >64){
  //  CustomDebug("Metadata is able to be used!");
     free(MainObject->oldTitle);
     MainObject->oldTitle = nullptr;
     const char* i1 = strstr(MainObject->MetadataOut,"JPOP Stream");
        
     if (i1 !=nullptr){
      //  CustomDebug("Passed IF check 1");
     const char* i2 = strstr(i1, "\"title\":");
     if (i2 !=nullptr){
        //CustomDebug("Passed IF Check 2");
     i2 +=9; //Let's avoid that double quote, yes?
     const char* i3 = strchr(i2,'"');
     if (i3 !=nullptr){
      //  CustomDebug("Passed IF Check 3");
     //Now, it's in the title, all we need to do is extract it!
   
     int len = (i3-i2)+1;
      //Does the title have .mp3? Needed due to listen.moe applying .mp3 sometimes to the title.
     if (strstr(i2,".mp3") == NULL){ 
        //Nope, it doesn't! Wonderful!
     char* out = (char*)malloc(len);
     strncpy(out,i2,len-1);
     out[len-1] = '\0';
     CustomDebugFmt("Title should be %s",(void*)out);
     MainObject->oldTitle = MainObject->title;
     MainObject->title = out;
     }else{
    // Yes, it sadly has. Since .mp3 is 4 characters long, 
     char* out = (char*)malloc(len-4);
     strncpy(out,i2,len-5);
     out[len-4] = '\0';
      MainObject->oldTitle = MainObject->title;
     MainObject->title = out;
     }
     
     }
     }
     }
  }

}

int DrawText(const char* text, int x, int y, SDL_Color* color, TTF_Font* font) {
    if (!font || !text || text[0] == '\0') return EXIT_FAILURE;
    if (text == nullptr) return EXIT_FAILURE;
    SDL_Surface *rawSurface = TTF_RenderText_Blended(font, text, 0, *color); 
    if (!rawSurface) return EXIT_FAILURE;

    SDL_Surface *optimizedSurface = SDL_ConvertSurface(rawSurface, SDL_PIXELFORMAT_RGBA8888);
    SDL_DestroySurface(rawSurface); 
    
    if (!optimizedSurface) return EXIT_FAILURE;

    SDL_Texture *textTexture = SDL_CreateTextureFromSurface(MainObject->renderer, optimizedSurface);
    if (!textTexture) {
        SDL_DestroySurface(optimizedSurface);
        return EXIT_FAILURE;
    }

    SDL_SetTextureBlendMode(textTexture, SDL_BLENDMODE_BLEND);
    SDL_FRect textRect = { (float)x, (float)y, (float)optimizedSurface->w, (float)optimizedSurface->h };

    SDL_RenderTexture(MainObject->renderer, textTexture, NULL, &textRect);
    SDL_DestroySurface(optimizedSurface);
    SDL_DestroyTexture(textTexture);

    return EXIT_SUCCESS;
}

size_t write_data_to_disk(void *ptr, size_t size, size_t nmemb, void *stream) {
      while (MainObject->LockDownload == true) {
        sceKernelDelayThread(1);
    }
    size_t total_bytes = size * nmemb;
    if (Config.RecordFile == true){
    FILE *fp = (FILE *)stream;
    if (!fp || total_bytes == 0) return 0;
    size_t written = fwrite(ptr, 1, total_bytes, fp);
    // Push data into the ring buffer for the decoder thread
    ring_buffer_write(MainObject->buffer, (uint8_t*)ptr, total_bytes);
    fflush(fp);
       MainObject->StreamedBytes = MainObject->StreamedBytes + written;
       return written; 
    }else{
    if ( total_bytes == 0) return 0;
     ring_buffer_write(MainObject->buffer,(uint8_t*)ptr,total_bytes);
       MainObject->StreamedBytes = MainObject->StreamedBytes + total_bytes;
     return total_bytes;
    }
 
    

    
}

static size_t write_sse_metadata_callback(void *ptr, size_t size, size_t nmemb, void *stream) {
   size_t total_bytes = size * nmemb;
    size_t new_size = MainObject->MoSize + total_bytes;
    char* memA = (char*)realloc(MainObject->MetadataOut, new_size + 1);
    if (memA == NULL) {
        CustomDebug("reallocation error on metadata!");
        return 0; 
    }
    memcpy(memA + MainObject->MoSize, ptr, total_bytes);
    MainObject->MetadataOut = memA;
    MainObject->MoSize = new_size;
    MainObject->MetadataOut[MainObject->MoSize] = '\0';
    return total_bytes;
}


int download_metadata_stream() {
    
  
    CURL *curl = curl_easy_init();
    if (!curl) {
        
        return -2;
    } 
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_URL, meta_url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_sse_metadata_callback);
     curl_easy_setopt(curl, CURLOPT_WRITEDATA, MainObject->MetadataOut);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
    curl_easy_setopt(curl, CURLOPT_SSL_ENABLE_ALPN, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L); 
    curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2); 
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Connection: keep-alive");
    headers = curl_slist_append(headers, "Accept: text/event-stream");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT,10L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 5L);
    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || http_code != 200) return res;
    return 0; 
}

int download_audio_stream(const char *url, const char *output_path) {
    FILE *fp = MainObject->StreamFile;
    if (!fp) {
        fp = fopen(output_path, "wb");
        if (!fp) return -1;
        MainObject->StreamFile = fp;
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        fclose(fp);
        MainObject->StreamFile = NULL;
        return -2;
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data_to_disk);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
    curl_easy_setopt(curl, CURLOPT_SSL_ENABLE_ALPN, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L); 
    curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2); 
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Connection: keep-alive");
    headers = curl_slist_append(headers, "Accept: audio/mpeg, */*");
    headers = curl_slist_append(headers, "Icy-MetaData: 0"); 

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 900L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    fclose(fp);
    ring_buffer_close(MainObject->buffer);
    MainObject->StreamFile = NULL;
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || http_code != 200) return res;
    return 0; 
}

void netInit() {
    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    SceNetInitParam netInitParam;
    int size = 4 * 1024 * 1024;
    netInitParam.memory = malloc(size);
    netInitParam.size = size;
    netInitParam.flags = 0;
    sceNetInit(&netInitParam);
    sceNetCtlInit();
}

void netTerm() {
    sceNetCtlTerm();
    sceNetTerm();
    sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
}

void httpInit() {
    sceSysmoduleLoadModule(SCE_SYSMODULE_HTTP);
    sceHttpInit(4 * 1024 * 1024); 
}

void httpTerm() {
    sceHttpTerm();
    sceSysmoduleUnloadModule(SCE_SYSMODULE_HTTP);
}

int MetadataThread(unsigned int args, void* argp) {
    CustomDebug("Metadata initiated");
    int res = 0;
restart:
    sceKernelDelayThread(2 * 100000);
    while (res == 0 || res == CURLE_OPERATION_TIMEDOUT) {
        if (MainObject->MetaTrigger == true) {
            MainObject->MetaReady = false;
            checkSongChange();
            res = download_metadata_stream();
            MainObject->MetaReady = true;
                parseTitle();
             CustomDebug("Metadata stream completed, repeating!");
             
            //Only uncomment on emergency situations, it'll use 90% of the CPU!
             CustomDebugFmt("MetadataOut = %s",(void*)MainObject->MetadataOut);
             MainObject->MoSize = 0;
             free(MainObject->MetadataOut);
     MainObject->MetadataOut = nullptr;
        }
    }
    CustomDebugFmt("MetadataThread catch error! cURL error %d", (void*)(intptr_t)res);
    CustomDebugFmt("curl_easy_strerror(res) returns %s",(void*)curl_easy_strerror((CURLcode)res));
    goto restart;
    return res;
}

int streamThread(SceSize args, void *argp) {
    CustomDebug("Download stream initiated");
    sceKernelDelayThread(5 * 100000);

retry:
    int res = download_audio_stream(stream_url, output_file);
    if (res != 0) {
        sceKernelDelayThread(5 * 100000);
        CustomDebugFmt("Audio download returned %d", (void*)res);
        goto retry;
    }
    return 0;
}

void DrawConsole() {
    //Saves a little bit of clock cycles, but it's probably not gonna be noticed anyways. 
    if (Config.Debug == true){
    SDL_Color color = {255, 255, 255, 255};
    for (int i = 0; i < MaxDebugPrints; i++) {
        if (debugScreen[i] != NULL) {
            DrawText(debugScreen[i], 50, 50 + (i * 16), &color, MainObject->FONT01);
        }
    }
    if (debugScreen[MaxDebugPrints-1] != NULL){
        for (int i = 0; i < MaxDebugPrints; i++){
            free(debugScreen[i]);
            debugScreen[i] = NULL;
        }
    }
}
}

// Initialize libmpg123 
bool InitAudio() {
    int err = MPG123_OK;
    mh = mpg123_new(NULL, &err);
    if (!mh) {
        CustomDebugFmt("mpg123_new failed: %s", (void*)mpg123_plain_strerror(err));
        return false;
    }

    if (mpg123_open_feed(mh) != MPG123_OK) {
        CustomDebug("mpg123_open_feed failed");
        mpg123_delete(mh);
        mh = nullptr;
        return false;
    }

    long rate = 44100;
    int channels = 2;
    mpg123_format_none(mh);
    mpg123_format(mh, rate, channels, MPG123_ENC_SIGNED_16);

    SDL_AudioSpec spec;
    spec.freq = rate;
    spec.format = SDL_AUDIO_S16;
    spec.channels = channels;

    g_audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (!g_audio_stream) {
        CustomDebugFmt("Failed to create SDL audio stream: %s", (void*)SDL_GetError());
        mpg123_close(mh);
        mpg123_delete(mh);
        mh = nullptr;
        return false;
    }

    SDL_ResumeAudioStreamDevice(g_audio_stream);
    MainObject->AudioInit = true;
    return true;
}

int audioDecoderThread(SceSize argc, void* argp) {
    uint8_t input_chunk[4096];
    unsigned char pcm_buffer[4096];
    size_t done = 0;
   bool Buffering = false;
    while (!MainObject->AudioInit && MainObject->buffer && !MainObject->buffer->is_closed) {
        sceKernelDelayThread(10000); 
    }

    while (MainObject->AudioInit && g_audio_stream && mh) {
        // Pull the bytes from the buffer, and feed it straight into mpg123!
        if (Config.IsPaused == false){
            if (Buffering == true){
                sceKernelDelayThread(10);
            };
        size_t bytes_read = ring_buffer_read_bytes(MainObject->buffer, input_chunk, sizeof(input_chunk));
        if (bytes_read > 0) {
            mpg123_feed(mh, input_chunk, bytes_read);
        }
        int queued =   SDL_GetAudioStreamQueued(g_audio_stream);
        
        int ret = mpg123_read(mh, pcm_buffer, sizeof(pcm_buffer), &done);
    
        if (ret == MPG123_OK && done > 0 && Config.IsPaused == false) {
          
            SDL_PutAudioStreamData(g_audio_stream, pcm_buffer, (int)done);
            
        } 
    
        else if (ret == MPG123_NEED_MORE) {
            //This is normal; usually mpg123 is so fast, that it outreads the buffer.
            //Also, the user won't notice such a small slowdown anyways.
            sceKernelDelayThread(1);
            //Is the amount of queued audio bytes less than 1KiB, and config says the stream isn't paused?
            //Slow the thread down, so we allow the queue to normalize!
            if (queued <1024 && Config.IsPaused == false){
              Buffering = true;
             if (queued <512 && queued >64)
            CustomDebugFmt("Buffering! DownloadSpeed is %d bytes/s approx",(void*)MainObject->TransferSpeed);
            }else{
                Buffering = false;
            }
        }
        else if (ret == MPG123_DONE) {
            if (MainObject->buffer->is_closed){ 
            sceKernelDelayThread(15000);
            break;
            }
        }
    }
    }
    return 0;
}
bool* getOptByConfig(int8_t CurrentOpt){
   switch (CurrentOpt)
   {
   case 0:
    return &Config.Debug;
   case 1:
    return &Config.RecordFile;
   case 2:
    return &Config.GetStreamTime;
    case 3:
    return &Config.DoRequestBETA;
    case 4:
    return &Config.RenameFile2title;
   default:
    return nullptr;
    break;
   }
}
const char* GetNameByConfigOpt(int8_t CurrentOpt){
    switch (CurrentOpt)
   {
   case 0:
    return "Debug mode";
   case 1:
    return "Download stream";
   case 2:
    return "Show stream length";
    case 3:
    return "Send song requests";
    case 4:
    return "Set downloaded stream to title";
   default:
   return nullptr;
    break;
   }
}
void UpdateMetadata() {
    if (MainObject->MetaReady == true) {
        MainObject->MetaTrigger = true; 
    }
    

    SceIoStat* stat = (SceIoStat*)malloc(sizeof(SceIoStat));
    if (sceIoGetstat(output_file, stat) >= 0) {
        if (stat->st_size > 32 * 1024 && MainObject->AudioInit == false) {
            InitAudio();
        }

        MainObject->StreamedBytes = stat->st_size;
        MainObject->StreamStat = stat;
        
    }
    free(stat);
}
bool CheckBtn(unsigned int button) {
    // Current frame has it down, but old frame did NOT have it down
    bool isPressedNow = (ctrl.buttons & button);
    bool wasPressedBefore = (oldctrl.buttons & button);
    
    return isPressedNow && !wasPressedBefore;
}
void InputLoop(int Framecount){

    oldctrl = ctrl;
    int res = sceCtrlReadBufferPositive(0,&ctrl,1);
    if (res <0){
        CustomDebugFmt("sceCtrlReadBufferPositive returned %d",(void*)res);
        return;
    }
    //Was gonna be L1+UP, but turns out the bit masking is against us!
    if (Config.IsOnConfig == true){
        if (CheckBtn(SCE_CTRL_TRIANGLE) == true){
        Config.IsOnConfig = !Config.IsOnConfig;
    } else if (CheckBtn(SCE_CTRL_CROSS) == true){
      bool* opt = getOptByConfig(Config.CONFIG_OPT);
      //Turns out, that declaring the dereferenced variable just creates a copy, and i was modifying the copy.
      *opt = !(*opt);
      if (Config.CONFIG_OPT == 0){
        if (*opt == true){
        CustomDebug("Debug mode activated!");
        }else{
            //Must do this, so when it's reactivated, the FPS doesn't drop to 30-ish FPS.
          for (int i = 0; i < MaxDebugPrints; i++){
            free(debugScreen[i]);
            debugScreen[i] = NULL;
        }
        }
      }else{
        if (*opt == true && Config.Debug == true){
            
          CustomDebugFmt("%s is now activated!",(void*)GetNameByConfigOpt(Config.CONFIG_OPT)); 
        }else if (*opt == false && Config.Debug == true){
        CustomDebugFmt("%s is now disabled!",(void*)GetNameByConfigOpt(Config.CONFIG_OPT));
        }
      }
      
    }else if (CheckBtn(SCE_CTRL_UP) == true){
      if (Config.CONFIG_OPT >0){
        Config.CONFIG_OPT--;
      }else{
        Config.CONFIG_OPT = maxOpt-1;
      }
    }else if (CheckBtn(SCE_CTRL_DOWN) == true){
        if (Config.CONFIG_OPT <maxOpt-1){
            Config.CONFIG_OPT++;
        }else{
            Config.CONFIG_OPT = 0;
        }
    }
    }else{
    if (CheckBtn(SCE_CTRL_TRIANGLE) == true){
        Config.IsOnConfig = !Config.IsOnConfig;
    }else if (CheckBtn(SCE_CTRL_CROSS) == true){
        Config.IsPaused = false;
    }else if (CheckBtn(SCE_CTRL_CIRCLE) == true){
        Config.IsPaused = true;
    }
}
}
void DrawMenus(){

    if (Config.IsOnConfig == true){
         SDL_FRect b = { SCREEN_WIDTH-SCREEN_WIDTH/3, 0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT };
          SDL_SetRenderDrawColor(MainObject->renderer, 25, 25, 25, 255);
          SDL_RenderFillRect(MainObject->renderer, &b);
              SDL_FRect b2 = { SCREEN_WIDTH-SCREEN_WIDTH/3, 0.0f, (float)SCREEN_WIDTH, 20.0f};
          SDL_SetRenderDrawColor(MainObject->renderer, 200, 0, 0, 255);
          SDL_RenderFillRect(MainObject->renderer, &b2);
          SDL_Color c = {255,0,0,SDL_ALPHA_OPAQUE};
          DrawText("Configuration",SCREEN_WIDTH-SCREEN_WIDTH/3,20,&c,MainObject->FONT00);
        SDL_Color color;
       for (int i = 0; i<maxOpt; i++){

        if (Config.CONFIG_OPT == i){
          color = {255, 0, 0, SDL_ALPHA_OPAQUE};
        }else if (*getOptByConfig(i) == true){
            //This means that the option is activated.
          color = {0, 255, 0, SDL_ALPHA_OPAQUE};
        }else{
         color = {255, 255, 255, SDL_ALPHA_OPAQUE};
        }
        //Draw the text, and the text will have the configuration option from the iteration. 
        DrawText(GetNameByConfigOpt(i),(SCREEN_WIDTH-SCREEN_WIDTH/3)+20,100+(i*50),&color,MainObject->FONT00);
       }
    }else{
        return;
    }
}
void DrawBG(){
    SDL_FRect bg = {0.0f,0.0f,SCREEN_WIDTH,SCREEN_HEIGHT};
  
    SDL_SetRenderDrawColor(MainObject->renderer,32,32,32,255);
    SDL_RenderFillRect(MainObject->renderer,&bg);

   for (int i = 0; i<leafMAX; i++){
    Leaf* l = leafTable[i];
    if (l !=nullptr){
        if (l->x >SCREEN_WIDTH || l->y >SCREEN_HEIGHT){
            free(l);
            leafTable[i] = nullptr;
        }
      l->x = l->x + ((sin(l->x/100) *50) +1);
      uint8_t SCR = SCREEN_HEIGHT/(l->y*SCREEN_HEIGHT);
      if (l->y !=0){
      l->y = l->y- SCR;
      }
      l->age = l->age+1;
      SDL_SetRenderDrawColor(MainObject->renderer,255,255,255,(255-l->age) & 255);
      SDL_RenderPoint(MainObject->renderer,l->x,l->y);
    }else{
    if ((rand()/RAND_MAX) < leafRate){
       Leaf* nl = (Leaf*)malloc(sizeof(Leaf));
       memset(nl,0,sizeof(Leaf));
       nl->x = (uint16_t)((rand()/RAND_MAX)*SCREEN_WIDTH);
       nl->y = SCREEN_HEIGHT;
    }
    }

}
}
void UILoop(int Framecount,uint64_t tick) {
    if (Framecount >60){
    UpdateMetadata();
    }
    InputLoop(Framecount);
    SDL_RenderClear(MainObject->renderer);
    DrawBG();
     SDL_Color color = {255, 0, 0, SDL_ALPHA_OPAQUE};

    /* //UNUSED
    char buf[128];
    if (MainObject->StreamedBytes < 1024 * 1024){
        snprintf(buf, 128, "MainObject.streambytes = %d", MainObject->StreamedBytes);
    } else {
        
        unsigned long convert = MainObject->StreamedBytes / (1024 * 1024);
        snprintf(buf, 128, "MainObject.streambytes = %lu MB", convert); 
    }
    DrawText(buf, 50 + Framecount*2, 100, &color, MainObject->FONT01);
    */ 
    if (MainObject->title !=nullptr) {
        DrawText(MainObject->title, 50, 200, &color, MainObject->FONT00);
    }
       DrawText(MainObject->streamFileName,50,300,&color,MainObject->FONT00);
    SDL_SetRenderDrawColor(MainObject->renderer, 255, 255, 255, 128);
    
    DrawMenus();
    DrawConsole();
    SDL_RenderPresent(MainObject->renderer);
}

int main(int argc, char *argv[]) {
    mpg123_init();
    MainObject = (AppStruct*)malloc(sizeof(AppStruct));
    SceDateTime timer;
    sceRtcGetCurrentClock(&timer,0);

    srand((timer.second+timer.microsecond) & UINT32_MAX-1);
  
    memset(MainObject, 0, sizeof(AppStruct));
  
    if (SDL_Init(SDL_INIT_VIDEO) < 0) return -1;
    if (SDL_Init(SDL_INIT_AUDIO) < 0) return -1;
    if (TTF_Init() < 0) { 
        SDL_Quit(); 
        return -1; 
    }
     sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    MainObject->MetaReady = true;
    MainObject->buffer = ring_buffer_create();
    //I don't care if this throws up a error, it's just renaming the existing file. Some people will like this function.
    sceIoRename(output_file, existing_file);
    
    SceUID thid = sceKernelCreateThread("radio_stream_thread", streamThread, 0x10000100, 0x40000, 0, 0, NULL);
    if (thid >= 0) {
        sceKernelStartThread(thid, 0, NULL);
    }
    
    SceUID athid = sceKernelCreateThread("audio_decoder_thread", audioDecoderThread, 0x10000100, 0x8000, 0, 0, NULL);
    if (athid >= 0) {
        sceKernelStartThread(athid, 0, NULL);
    }
    
    SceUID thrid = sceKernelCreateThread("Metadata_thread", MetadataThread, 0x10000100, 0x10000, 0, 0, NULL);
    if (thrid >= 0) {
        sceKernelStartThread(thrid, 0, NULL);
    }
   
    MainObject->window = SDL_CreateWindow("audioplayer", SCREEN_WIDTH, SCREEN_HEIGHT, 0);
    MainObject->renderer = SDL_CreateRenderer(MainObject->window, 0);

    if (!MainObject->renderer) {
        return -1;
    }

    curl_global_init(CURL_GLOBAL_ALL);
    httpInit();
    netInit();

    SceIoStat *dirStat = (SceIoStat*)malloc(sizeof(SceIoStat));
    if (sceIoGetstat(output_folder, dirStat) < 0) {
        sceIoMkdir(output_folder, 0777);
    }
    free(dirStat); 

    MainObject->FONT00 = TTF_OpenFont(FONT0, 28);
    MainObject->FONT01 = TTF_OpenFont(FONT1, 12);
    
    int FrameCount = 0;
    SDL_Event event;
    bool running = true;
    uint64_t timestamp = SDL_GetTicks();
    uint64_t update;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }
        
        update = SDL_GetTicks();
        if (timestamp + Timeinterval < update) {
            timestamp = update;
            FrameCount++;
        }
        
        UILoop(FrameCount,update);
    }

    // Cleanup Audio, libmpg123, and Network
    if (MainObject->AudioInit && mh) {
        mpg123_close(mh);
        mpg123_delete(mh);
        mh = nullptr;
    }
    mpg123_exit();

    curl_global_cleanup();
    httpTerm();
    netTerm();

    SDL_DestroyRenderer(MainObject->renderer);
    SDL_DestroyWindow(MainObject->window);
    TTF_Quit();
    SDL_Quit();
    
    free(MainObject);
    return 0;
}