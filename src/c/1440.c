#include <pebble.h>
#include <stdio.h>
#include "math.h"

#define SETTINGS_KEY 1
#define ROUND_RING_COUNT 8

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

static GPoint coordinate_of(GRect bounds, int x_offset, int y_offset) {
  bool is_round = PBL_IF_ROUND_ELSE(true, false);
  if (!is_round) {
    float x_margin = 0.1;
    float y_margin = 0.1;
    float start_x = bounds.size.w * x_margin;
    float x_step = (bounds.size.w * (1 - x_margin * 2)) / 11;

    float start_y = bounds.size.h * y_margin;
    float y_step = (bounds.size.h * (1 - y_margin * 2)) / 11;

    return GPoint(start_x + x_step * x_offset, start_y + y_step * y_offset);
  }
  const int dots_in_ring[ROUND_RING_COUNT] = { 6, 9, 12, 15, 18, 21, 27, 36 };
  int index = x_offset * 12 + y_offset;
  int ring_index = 0;
  int index_in_ring = 0;
  int acc = 0;
  for (int i = 0; i < ROUND_RING_COUNT; i++) {
    if (index >= acc && index < acc + dots_in_ring[i]) {
      ring_index = i;
      index_in_ring = index - acc;
      break;
    } else {
      acc += dots_in_ring[i];
    }
  }

  float center_void = 30.0;

  float ring_height = (bounds.size.h - center_void) / (ROUND_RING_COUNT * 2.0);
  float radius = (center_void / 2.0) + ring_height * ring_index;
  GPoint center = grect_center_point(&bounds);
  int dots_count = dots_in_ring[ring_index];
  int32_t angle = (TRIG_MAX_ANGLE / dots_count) * index_in_ring;

  return GPoint(
    center.x + (int32_t)radius * sin_lookup(angle) / TRIG_MAX_RATIO,
    center.y + (int32_t)radius * cos_lookup(angle) / TRIG_MAX_RATIO
  );
}

static void bitmap_layer_update_proc(Layer *layer, GContext* ctx) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  int minute = (*t).tm_min;
  int hour = (*t).tm_hour;
  int tens = (int)(minute + hour * 60) / 10;
  const int small_dot_size = PBL_IF_ROUND_ELSE(2, 3);
  const int big_dot_size = PBL_IF_ROUND_ELSE(4, 7);

  GRect bounds = layer_get_bounds(layer);

  // background color
  graphics_context_set_fill_color(ctx, background_color);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);


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

      int dot_size = small_dot_size;

      if (direction == 0) {
        // top
        if (j * 12 + i < tens) {
          dot_size = big_dot_size;
        }
      } else if (direction == 1) {
        // bottom
        if (143 - (j * 12 + i) < tens) {
          dot_size = big_dot_size;
        }
      } else if (direction == 2) {
        // left
        if (i * 12 + j < tens) {
          dot_size = big_dot_size;
        }
      } else if (direction == 3) {
        //right
        if ((11-i) * 12 + j < tens) {
          dot_size = big_dot_size;
        }
      } else if (direction == 4) {
        if (map[i][j] == 1) {
          dot_size = big_dot_size;
        }
      }

      GPoint point = coordinate_of(bounds, i, j);
      GRect dot = GRect(
        (int)round(point.x - (dot_size-1)/2),
        (int)round(point.y - (dot_size-1)/2),
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
