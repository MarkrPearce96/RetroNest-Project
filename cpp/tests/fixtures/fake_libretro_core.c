#include "libretro.h"
#include <string.h>
#include <stdlib.h>

static retro_environment_t env_cb;
static retro_video_refresh_t video_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;

#define WIDTH 4
#define HEIGHT 4
static uint16_t fb[WIDTH * HEIGHT];   // RGB565 checkerboard
static int run_calls = 0;

void retro_set_environment(retro_environment_t cb) { env_cb = cb; }
void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { (void)cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { input_poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { input_state_cb = cb; }

unsigned retro_api_version(void) { return RETRO_API_VERSION; }

void retro_get_system_info(struct retro_system_info* info) {
    memset(info, 0, sizeof(*info));
    info->library_name = "FakeCore";
    info->library_version = "1.0";
    info->valid_extensions = "bin";
    info->need_fullpath = false;
    info->block_extract = false;
}

void retro_get_system_av_info(struct retro_system_av_info* info) {
    memset(info, 0, sizeof(*info));
    info->geometry.base_width = WIDTH;
    info->geometry.base_height = HEIGHT;
    info->geometry.max_width = WIDTH;
    info->geometry.max_height = HEIGHT;
    info->geometry.aspect_ratio = (float)WIDTH / HEIGHT;
    info->timing.fps = 60.0;
    info->timing.sample_rate = 32000.0;
}

void retro_init(void) {
    enum retro_pixel_format pf = RETRO_PIXEL_FORMAT_RGB565;
    if (env_cb) env_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &pf);
    for (int y = 0; y < HEIGHT; ++y)
        for (int x = 0; x < WIDTH; ++x)
            fb[y * WIDTH + x] = ((x + y) & 1) ? 0xFFFF : 0x0000;
}

void retro_deinit(void) {}
void retro_set_controller_port_device(unsigned p, unsigned d) { (void)p; (void)d; }
void retro_reset(void) { run_calls = 0; }

bool retro_load_game(const struct retro_game_info* g) { (void)g; return true; }
bool retro_load_game_special(unsigned t, const struct retro_game_info* g, size_t n) { (void)t; (void)g; (void)n; return false; }
void retro_unload_game(void) {}
unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }

void retro_run(void) {
    if (input_poll_cb) input_poll_cb();
    if (video_cb) video_cb(fb, WIDTH, HEIGHT, WIDTH * sizeof(uint16_t));
    int16_t silence[2 * 800] = {0};
    if (audio_batch_cb) audio_batch_cb(silence, 800);
    ++run_calls;
}

size_t retro_serialize_size(void) { return sizeof(run_calls); }
bool retro_serialize(void* data, size_t size) {
    if (size < sizeof(run_calls)) return false;
    memcpy(data, &run_calls, sizeof(run_calls));
    return true;
}
bool retro_unserialize(const void* data, size_t size) {
    if (size < sizeof(run_calls)) return false;
    memcpy(&run_calls, data, sizeof(run_calls));
    return true;
}

void retro_cheat_reset(void) {}
void retro_cheat_set(unsigned i, bool e, const char* c) { (void)i; (void)e; (void)c; }

void* retro_get_memory_data(unsigned id) { (void)id; return NULL; }
size_t retro_get_memory_size(unsigned id) { (void)id; return 0; }

// Pause symbol — counter for tests. retronest_set_paused increments
// the counter on every call; tests verify the pointer resolves AND
// is the right function. retronest_test_pause_call_count +
// retronest_test_last_pause_value + retronest_test_reset_pause_counter
// give tests read/reset access between cases.
static int s_pause_call_count = 0;
static int s_last_pause_value = -1;

void retronest_set_paused(bool paused) {
    s_pause_call_count++;
    s_last_pause_value = paused ? 1 : 0;
}

int retronest_test_pause_call_count(void) {
    return s_pause_call_count;
}

int retronest_test_last_pause_value(void) {
    return s_last_pause_value;
}

void retronest_test_reset_pause_counter(void) {
    s_pause_call_count = 0;
    s_last_pause_value = -1;
}
