#include <pebble.h>
#include <stdio.h>
#include "math.h"

#define SETTINGS_KEY 1

static Window *s_window;
static Layer *bitmap_layer;

static GColor background_color;
static GColor dot_color;
static int direction = 0;
// 0 top
// 1 bottom
// 2 left
// 3 right
// 4 random

typedef struct ClaySettings {
  GColor BackgroundColor;
  GColor DotColor;
  int Direction;
} ClaySettings;
static ClaySettings settings;

static void bitmap_layer_update_proc(Layer *layer, GContext* ctx) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  int minute = (*t).tm_min;
  int hour = (*t).tm_hour;
  int tens = (int)(minute + hour * 60) / 10;

  GRect bounds = layer_get_bounds(layer);

  // background color
  graphics_context_set_fill_color(ctx, background_color);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  float x_margin = 0.1;
  float y_margin = 0.1;
  float start_x = bounds.size.w * x_margin;
  float x_step = (bounds.size.w * (1 - x_margin * 2)) / 11;

  float start_y = bounds.size.h * y_margin;
  float y_step = (bounds.size.h * (1 - y_margin * 2)) / 11;

  int map[12][12];

  if (direction == 4) {
    for (int i = 0; i < 12; i++) {
      for (int j = 0; j < 12; j++) {
        if (i * 12 + j < tens) {
          map[i][j] = 1;
        } else {
          map[i][j] = 0;
        }
      }
    }

    srand(time(NULL));
    for (int i = 0; i < 144; i++) {
      int from = rand() % 144;
      int to = rand() % 144;
      int val = map[from / 12][from % 12];
      map[from / 12][from % 12] = map[to / 12][to % 12];
      map[to / 12][to % 12] = val;
    }
  }

  graphics_context_set_fill_color(ctx, dot_color);
  for (int i = 0; i < 12; i++) {
    for (int j = 0; j < 12; j++) {

      int dot_size = 3;

      if (direction == 0) {
        // top
        if (j * 12 + i < tens) {
          dot_size = 7;
        }
      } else if (direction == 1) {
        // bottom
        if (143 - (j * 12 + i) < tens) {
          dot_size = 7;
        }
      } else if (direction == 2) {
        // left
        if (i * 12 + j < tens) {
          dot_size = 7;
        }
      } else if (direction == 3) {
        //right
        if ((11-i) * 12 + j < tens) {
          dot_size = 7;
        }
      } else if (direction == 4) {
        if (map[i][j] == 1) {
          dot_size = 7;
        }
      }

      GRect dot = GRect(
        (int)round(start_x + x_step * i - (dot_size-1)/2),
        (int)round(start_y + y_step * j - (dot_size-1)/2),
        dot_size,
        dot_size);
      graphics_fill_rect(ctx, dot, 0, GCornerNone);
    }
  }
}

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  bitmap_layer = layer_create(bounds);
  layer_set_update_proc(bitmap_layer, bitmap_layer_update_proc);
  layer_add_child(window_layer, bitmap_layer);
}

static void prv_window_unload(Window *window) {
  layer_destroy(bitmap_layer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  int minute = (*tick_time).tm_min;
  int hour = (*tick_time).tm_hour;
  if ((minute + hour * 60) % 10 == 0) {
    layer_mark_dirty(bitmap_layer);
  }
}

static void prv_inbox_received_handler(DictionaryIterator *iter, void *context) {
  // Read color preferences
  Tuple *bg_color_t = dict_find(iter, MESSAGE_KEY_background_color);
  if(bg_color_t) {
    background_color = GColorFromHEX(bg_color_t->value->int32);
  }
  Tuple *dot_color_t = dict_find(iter, MESSAGE_KEY_dot_color);
  if(dot_color_t) {
    dot_color = GColorFromHEX(dot_color_t->value->int32);
  }
  Tuple *fill_t = dict_find(iter, MESSAGE_KEY_fill_direction);
  if(fill_t) {
    char *thing = fill_t->value->cstring;
    if (strcmp(thing, "top") == 0) {
      direction = 0;
    } else if (strcmp(thing, "bottom") == 0) {
      direction = 1;
    } else if (strcmp(thing, "left") == 0) {
      direction = 2;
    } else if (strcmp(thing, "right") == 0) {
      direction = 3;
    } else if (strcmp(thing, "random") == 0) {
      direction = 4;
    }
  }
  layer_mark_dirty(bitmap_layer);

  settings.BackgroundColor = background_color;
  settings.DotColor = dot_color;
  settings.Direction = direction;
  persist_write_data(SETTINGS_KEY, &settings, sizeof(settings));
}

static void prv_init(void) {

  // default settings
  settings.BackgroundColor = GColorFromRGBA(205, 34, 49, 255);
  settings.DotColor = GColorWhite;
  settings.Direction = 0;

  persist_read_data(SETTINGS_KEY, &settings, sizeof(settings));

  // apply saved data
  background_color = settings.BackgroundColor;
  dot_color = settings.DotColor;
  direction = settings.Direction;

  app_message_register_inbox_received(prv_inbox_received_handler);
  app_message_open(128, 128);

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  const bool animated = true;
  window_stack_push(s_window, animated);
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
}

static void prv_deinit(void) {
  window_destroy(s_window);
}

int main(void) {
  prv_init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", s_window);

  app_event_loop();
  prv_deinit();
}
