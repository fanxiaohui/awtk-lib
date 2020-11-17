/*
 * lcd_wayland_1.c
 *
 *  Created on: 2019Äê11ÔÂ18ÈÕ
 *      Author: zjm09
 */
#include "lcd_wayland.h"

#include "base/idle.h"
#include "base/timer.h"
#include "base/window_manager.h"
#include "main_loop/main_loop_simple.h"
#include "tkc/thread.h"

enum key_repeat_state{
	repeat_key_released = 0,
	repeat_key_pressed = 10,
	repeat_key_delay,
	repeat_key_rate,
};

enum key_repeat_state __repeat_state = repeat_key_released;
static int key_value;

static ret_t wayland_flush(lcd_t* lcd)
{
	lcd_wayland_t *lw = lcd->impl_data;
	struct buffer *buf = lw->impl_data;
	ref_display(lw->objs.surface,buf->bufs[buf->idx].wl_buffer,lcd->w,lcd->h);

	return RET_OK;
}

static ret_t wayland_sync(lcd_t* lcd)
{
	lcd_wayland_t *lw = lcd->impl_data;
	struct buffer *buf = lw->impl_data;
	ThreadSignal_Wait(&buf->used);
	return RET_OK;
}

static ret_t wayland_lcd_swap(lcd_t* lcd)
{
	lcd_wayland_t *lw = lcd->impl_data;
	struct buffer *buf = lw->impl_data;

	wayland_flush(lcd);

	buf->idx ^= 1;

	return RET_OK;
}

static ret_t wayland_begin_frame(lcd_t* lcd, rect_t* dirty_rect) {
	lcd_wayland_t *lw = lcd->impl_data;
	struct buffer *buf = lw->impl_data;
	lcd_mem_t* lcd_mem = (lcd_mem_t*)(lcd);

	lcd_mem->offline_fb = (void *)buf->bufs[buf->idx].pixels;

	return RET_OK;
}

static void buffer_release_cb(void *data, struct wl_buffer *buf) {
	(void) buf;
	struct buffer *b = data;
	ThreadSignal_Signal(&b->used);
}

static const struct wl_buffer_listener buffer_listener = { buffer_release_cb };

struct buffer *wayland_create_double_buffer(struct wl_shm *shm, int width,
		int height) {
	size_t size = (width * height * 4) * 2;
	int fd = shm_open("/wayland_frame_buffer",
	O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);

	ftruncate(fd, size);

	void *raw = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (raw == MAP_FAILED) {
		perror("Could not map file to memory.\n");
		return MAP_FAILED;
	}

	struct buffer *buffer = calloc(1, sizeof(struct buffer));
	buffer->width = width;
	buffer->height = height;
	ThreadSignal_Init(&buffer->used);

	struct wl_shm_pool * pool = wl_shm_create_pool(shm, fd, size);

	buffer->bufs[0].wl_buffer = wl_shm_pool_create_buffer(pool, 0, width, height,
			width * 4, WL_SHM_FORMAT_ARGB8888);
	if (buffer->bufs[0].wl_buffer == NULL)
		return NULL;
	buffer->bufs[0].pixels = raw;
	wl_buffer_add_listener(buffer->bufs[0].wl_buffer, &buffer_listener,
			buffer);

	buffer->bufs[1].wl_buffer = wl_shm_pool_create_buffer(pool, 4 * width * height,
			width, height, width * 4, WL_SHM_FORMAT_ARGB8888);
	if (buffer->bufs[1].wl_buffer == NULL)
		return NULL;
	buffer->bufs[1].pixels = ((uint32_t*) raw) + width * height;
	wl_buffer_add_listener(buffer->bufs[1].wl_buffer, &buffer_listener,
			buffer);

	buffer->idx = 0;

	return buffer;
}

static lcd_t* lcd_linux_create_flushable(lcd_wayland_t *lw)
{
	struct wayland_data *objs = &lw->objs;
	struct wayland_output *out = container_of ( objs->monitors->next,
		  struct wayland_output,
		  link);
	size_t width = out->info.width;
	size_t height = out->info.height;

	if(out->info.transform == WL_OUTPUT_TRANSFORM_90 || out->info.transform ==WL_OUTPUT_TRANSFORM_270){
		height = out->info.width;
		width = out->info.height;
	}

	int line_length = width * 4;

	struct buffer *buffer = wayland_create_double_buffer(objs->shm,width,height);

	uint8_t* online_fb = (void*)buffer->bufs[0].pixels;
	uint8_t* offline_fb = (void*)buffer->bufs[1].pixels;

//	lw->current = buffer->bufs;
	lw->impl_data = buffer;

	lcd_t *lcd = lcd_mem_rgba8888_create_double_fb(width, height, online_fb, offline_fb);

	if(lcd != NULL) {
		lcd->impl_data = lw;
		lcd->sync = wayland_sync;
		lcd->flush = wayland_flush;
		lcd->begin_frame = wayland_begin_frame;
		lcd->swap = wayland_lcd_swap;
		lcd_mem_set_line_length(lcd, line_length);
	}

	return lcd;
}

#if 0
static void *kb_repeat(struct wayland_data *objs)
{
	static uint32_t repeat_count = 0;
	while(1){
		usleep(10*1000);
		switch(__repeat_state){
			case repeat_key_pressed:
				repeat_count = 0;
				__repeat_state = repeat_key_delay;
				break;
			case repeat_key_delay:
				repeat_count ++;
				if(repeat_count >= 20){
					repeat_count = 0;
					__repeat_state = repeat_key_rate;
				}
				break;
			case repeat_key_rate:
				repeat_count ++;
				if((repeat_count % 2) == 0){
					event_queue_req_t req;

					req.event.type = EVT_KEY_DOWN;
					req.key_event.key = map_key(key_value);
					printf("key down\n");
					input_dispatch_to_main_loop(main_loop(), &(req));
				}
				break;
			default:
			case repeat_key_released:
				break;
		}
	}

	return NULL;
}
#endif

void setup_input_cb(struct input_bundle *input);

lcd_wayland_t *lcd_wayland_create(void)
{
	lcd_wayland_t *lw = calloc(1,sizeof(lcd_wayland_t));
	if (lw && setup_wayland (&lw->objs,1) != SETUP_OK){
		destroy_wayland_data (&lw->objs);
		return NULL;
	}

//	tk_thread_t* thread = tk_thread_create(kb_repeat, NULL);
//	if (thread != NULL) {
//		tk_thread_start(thread);
//	}

	setup_input_cb(&lw->objs.inputs);

	{
		struct wayland_data *objs = &lw->objs;
		struct wayland_output *out = container_of ( objs->monitors->next,
			  struct wayland_output,
			  link);
		lw->objs.width = out->info.width;
		lw->objs.height = out->info.height;

		if(out->info.transform == WL_OUTPUT_TRANSFORM_90 || out->info.transform ==WL_OUTPUT_TRANSFORM_270){
			lw->objs.height = out->info.width;
			lw->objs.width = out->info.height;
		}
	}

//	return lcd_linux_create_flushable(lw);
	return lw;
}


