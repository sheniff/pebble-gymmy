/*
ToDo:
- Exercise's name animation (control name's length to do properly...)
- Progress bar (split by `#exercises * #series` each)
*/
#include "pebble.h"

#define TIME_OPTIONS 3
#define ANIM_DELAY 500
#define ANIM_DURATION 4000

#define KEY_MESSAGE_TYPE 0
#define KEY_NUM_EXERCISES 1
#define KEY_EXERCISE_NUMBER 2
#define KEY_EXERCISE_NAME 3
#define KEY_EXERCISE_SERIES 4
#define KEY_EXERCISE_REPS 5
  
static Window *window;

static GBitmap *action_icon_plus;
static GBitmap *action_icon_minus;

static ActionBarLayer *action_bar;

static TextLayer *header_text_layer;
static TextLayer *signature_text_layer;
static TextLayer *superbody_text_layer;
static TextLayer *body_text_layer;
static TextLayer *subbody_text_layer;

// Timer data structures
static int selected_time = 0;
static int current_time_index = 0;
static bool counting_down = false;
static bool is_next = false;
int times[TIME_OPTIONS] = {30, 60, 120};
static TextLayer *times_layers[TIME_OPTIONS];

// Workout info data structure
struct Exercise {
  char name[40];
  int series;
  uint8_t reps[12];
};

struct Exercise workout[10];
static int num_exercises = 0;
static int current_exercise = 0;
static int current_series = 0;

static int16_t windowWidth;
static GRect initialNamePosition;
static PropertyAnimation *text_animation;

//////////////////////
// function prototypes
static void notify_is_next_workout(bool isNext);
static void set_next_workout();

static void refreshUI();
static void update_exercise_pos_text();
static void update_exercise_name_text();
static void update_exercise_desc_text();
static void update_timer_text();
static void update_times_menu();

static void anim_stopped_handler(Animation *animation, bool finished, void *context);
static void tick_countdown_handler(struct tm *tick_time, TimeUnits units_change);
static void next_workout_handler();
static void restart_handler(ClickRecognizerRef recognizer, void *context);
static void next_time_handler(ClickRecognizerRef recognizer, void *context);
static void reset_workout_handler();

static void inbox_received_callback(DictionaryIterator *iterator, void *context);
static void inbox_dropped_callback(AppMessageResult reason, void *context);
static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context);
static void outbox_sent_callback(DictionaryIterator *iterator, void *context);

static void digestWorkout(DictionaryIterator *iterator);
static int digestExercise(DictionaryIterator *iterator);

static void start_animation();
static void start_watch();
static void stop_watch();
static void pause_watch();

///////////////////////////
// function implementations

// START AppMessage Communication
static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  // Read first item
  Tuple *t = dict_read_first(iterator);
  int message_type = (int)t->value->int8;
  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "wut %d", message_type);
  
  if(message_type == 0) {
    current_exercise = 0;
    current_series = 0;
    digestWorkout(iterator);
  } else if (message_type == 1) { 
    digestExercise(iterator);
  }
  
  // update UI in any case
  refreshUI();
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}
// END AppMessage Communication

static void digestWorkout(DictionaryIterator *iterator) {
    // process workout
    Tuple *t = dict_read_next(iterator);
    num_exercises = (int)t->value->int8;

    APP_LOG(APP_LOG_LEVEL_DEBUG, "got num exercises %d", num_exercises);

    if (num_exercises > 0) {
      // Fetch first exercise
      APP_LOG(APP_LOG_LEVEL_DEBUG, "request exercise %d", 0);

      // Begin dictionary
      DictionaryIterator *iter;
      app_message_outbox_begin(&iter);
    
      // Add a key-value pair
      dict_write_uint8(iter, KEY_EXERCISE_NUMBER, 0);
    
      // Send the message!
      app_message_outbox_send();      
    }
}

static int digestExercise(DictionaryIterator *iterator) {
  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "digesting exercise...");

  Tuple *t = dict_read_next(iterator);
  // process exercise
  char name[40];
  int exNumber = -1;
  int exSeries = 0;
  uint8_t reps[12];

  while(t != NULL) {
    switch(t->key) {
      case KEY_EXERCISE_NUMBER:
      exNumber = (int)t->value->int8;
      break;
      case KEY_EXERCISE_NAME:
      snprintf(name, sizeof(name), "%s", t->value->cstring);
      break;
      case KEY_EXERCISE_SERIES:
      exSeries = (int)t->value->int8;
      break;
      case KEY_EXERCISE_REPS:
        APP_LOG(APP_LOG_LEVEL_DEBUG, "allocating!! %d", (int)t->length);
        memcpy(reps, t->value->data, t->length);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "copied!! %d", (int)reps[0]);
      break;
      default:
      APP_LOG(APP_LOG_LEVEL_ERROR, "Key %d not recognized!", (int)t->key);
      break;
    }

    // Look for next item
    t = dict_read_next(iterator);
  }

  // copy to the proper workout
  if(exNumber > -1) {
    strcpy(workout[exNumber].name, name);
    workout[exNumber].series = exSeries;
    memcpy(workout[exNumber].reps, reps, exSeries);
    
    if (exNumber < num_exercises) {
      int next = exNumber + 1;
      // Fetch next exercise
      APP_LOG(APP_LOG_LEVEL_DEBUG, "request exercise %d", next);

      // Begin dictionary
      DictionaryIterator *iter;
      app_message_outbox_begin(&iter);
    
      // Add a key-value pair
      dict_write_uint8(iter, KEY_EXERCISE_NUMBER, next);
    
      // Send the message!
      app_message_outbox_send();
    }
  }
  
  return exNumber;
}

static void refreshUI(){
  update_exercise_pos_text();
  update_exercise_name_text();
  update_exercise_desc_text();
  update_timer_text();
  update_times_menu();
}

static void update_exercise_pos_text() {
  static char txt[20];
  if(is_next) {
    snprintf(txt, sizeof(txt), "Next is... %u/%u", current_exercise + 1, num_exercises);  
  } else {
    snprintf(txt, sizeof(txt), "Exercise %u/%u", current_exercise + 1, num_exercises);  
  }
  text_layer_set_text(superbody_text_layer, txt);
}

static void anim_stopped_handler(Animation *animation, bool finished, void *context) {
  // Free the animation
  property_animation_destroy(text_animation);
  
  // Schedule the next one, unless the app is exiting
  if (finished) {
    update_exercise_name_text();
  }
}

static void start_animation(int offset) {
  // Set start and end
  GRect from_frame = layer_get_frame((Layer *) body_text_layer);
  GRect to_frame = GRect(from_frame.origin.x - offset, from_frame.origin.y, from_frame.size.w + offset, from_frame.size.h);

  // Create the animation
  text_animation = property_animation_create_layer_frame((Layer *) body_text_layer, &from_frame, &to_frame);
  animation_set_duration((Animation*)text_animation, ANIM_DURATION);
  animation_set_delay((Animation*)text_animation, ANIM_DELAY);
  animation_set_curve((Animation*)text_animation, AnimationCurveLinear);
  animation_set_handlers((Animation*)text_animation, (AnimationHandlers) {
    .stopped = anim_stopped_handler
  }, NULL);
  
  // Schedule to occur ASAP with default settings
  animation_schedule((Animation*) text_animation);
}

static void update_exercise_name_text() {
  layer_set_frame ((Layer *) body_text_layer, initialNamePosition);

  text_layer_set_text(body_text_layer, workout[current_exercise].name);
  
  GSize size = graphics_text_layout_get_content_size(
    workout[current_exercise].name,
    fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD),
    GRect(0,0,1000,30),
    GTextOverflowModeTrailingEllipsis,
    GTextAlignmentLeft
  );

  int offset = size.w - windowWidth;

  if(offset > 0) {
    animation_unschedule((Animation*) text_animation);
    start_animation(offset);
  }
}

static void update_exercise_desc_text() {
  static char txt[20];
  snprintf(txt, sizeof(txt), "%u/%u (%d reps)", current_series + 1, workout[current_exercise].series, workout[current_exercise].reps[current_series]);
  text_layer_set_text(subbody_text_layer, txt);  
}

static void update_timer_text() {
  static char txt[4];
  snprintf(txt, sizeof(txt), "%u", selected_time);
  text_layer_set_text(times_layers[current_time_index], txt);
}

static void update_times_menu() {
  for (int i = 0; i < TIME_OPTIONS; i++) {
    text_layer_set_text_color(times_layers[i], GColorBlack);
    text_layer_set_background_color(times_layers[i], GColorWhite);
    char *time_text = (char *)malloc(sizeof(char) * 4);
    snprintf(time_text, sizeof(time_text), "%u", times[i]);
    text_layer_set_text(times_layers[i], time_text);
  }

  text_layer_set_text_color(times_layers[current_time_index], GColorWhite);
  text_layer_set_background_color(times_layers[current_time_index], GColorBlack);  
}

static void notify_is_next_workout(bool isNext) {
  is_next = isNext;
  update_exercise_pos_text();
}

static void stop_watch() {
  tick_timer_service_unsubscribe();
  selected_time = times[current_time_index];
  notify_is_next_workout(false);
  counting_down = false;
}

static void pause_watch() {
  tick_timer_service_unsubscribe();
  counting_down = false;
}

static void tick_countdown_handler(struct tm *tick_time, TimeUnits units_change) {
  if(selected_time > 0) {
    selected_time = selected_time - 1;
    update_timer_text();
    
    if(selected_time == 10) {
      vibes_double_pulse();
    } else if (selected_time < 3 && selected_time > 0) {
      vibes_short_pulse();
    } else if (selected_time == 0) {
      vibes_long_pulse();
    }
  } else {
    stop_watch();
  }
}

static void set_next_workout() {
  if(current_series < (workout[current_exercise].series) - 1) {
    // next series
    current_series++;
  } else {
    // next exercise
    current_exercise = (current_exercise + 1) % num_exercises;
    current_series = 0;
    update_exercise_pos_text();
    update_exercise_name_text();
  }

  update_exercise_desc_text();
}

static void next_workout_handler() {
  stop_watch();
  update_times_menu();
  set_next_workout();
}

static void start_watch() {
  if(selected_time <= 0) {
    selected_time = times[current_time_index];
  }
  tick_timer_service_subscribe(SECOND_UNIT, tick_countdown_handler);
  notify_is_next_workout(true);
  set_next_workout();
  counting_down = true;
}

static void restart_handler(ClickRecognizerRef recognizer, void *context) {
  if(counting_down){
    pause_watch();
  } else {
    vibes_short_pulse();
    start_watch();
  }
}

static void next_time_handler(ClickRecognizerRef recognizer, void *context) {
  stop_watch();
  current_time_index = (current_time_index + 1) % TIME_OPTIONS;
  selected_time = times[current_time_index];
  update_times_menu();
}

static void reset_workout_handler() {
  current_exercise = 0;
  current_series = 0;
  update_exercise_pos_text();
  update_exercise_name_text();
  update_exercise_desc_text();
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, (ClickHandler) next_workout_handler);
  window_long_click_subscribe(BUTTON_ID_UP, 1000, (ClickHandler) reset_workout_handler, 0);
  window_single_click_subscribe(BUTTON_ID_DOWN, (ClickHandler) next_time_handler);
  window_long_click_subscribe(BUTTON_ID_DOWN, 500, (ClickHandler) restart_handler, 0);
}

static void window_load(Window *me) {
  action_bar = action_bar_layer_create();
  action_bar_layer_add_to_window(action_bar, me);
  action_bar_layer_set_click_config_provider(action_bar, click_config_provider);
  action_bar_layer_set_icon(action_bar, BUTTON_ID_UP, action_icon_plus);
  action_bar_layer_set_icon(action_bar, BUTTON_ID_DOWN, action_icon_minus);

  Layer *layer = window_get_root_layer(me);
  const int16_t width = windowWidth = layer_get_frame(layer).size.w - ACTION_BAR_WIDTH - 3;
  const int16_t height = layer_get_frame(layer).size.h;
  
  header_text_layer = text_layer_create(GRect(4, 0, width, 30));
  text_layer_set_font(header_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_background_color(header_text_layer, GColorClear);
  text_layer_set_text(header_text_layer, "Gymmy!");
  layer_add_child(layer, text_layer_get_layer(header_text_layer));

  signature_text_layer = text_layer_create(GRect(70, 10, 80, 20));
  text_layer_set_font(signature_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_background_color(signature_text_layer, GColorClear);
  text_layer_set_text(signature_text_layer, "by Sheniff");
  layer_add_child(layer, text_layer_get_layer(signature_text_layer));

  superbody_text_layer = text_layer_create(GRect(4, 34, width, 20));
  text_layer_set_font(superbody_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_background_color(superbody_text_layer, GColorClear);
  layer_add_child(layer, text_layer_get_layer(superbody_text_layer));

  initialNamePosition = GRect(4, 46, width, 30);
  body_text_layer = text_layer_create(initialNamePosition);
  text_layer_set_font(body_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_background_color(body_text_layer, GColorClear);
  layer_add_child(layer, text_layer_get_layer(body_text_layer));

  subbody_text_layer = text_layer_create(GRect(4, 68, width, 60));
  text_layer_set_font(subbody_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_background_color(subbody_text_layer, GColorClear);
  layer_add_child(layer, text_layer_get_layer(subbody_text_layer));

  // show time items
  const int wid = width / 3;
  
  for (int i = 0; i < TIME_OPTIONS; i++) {
    times_layers[i] = text_layer_create(GRect(wid * i + i + 1, height - 20, wid, 20));
    text_layer_set_font(times_layers[i], fonts_get_system_font(FONT_KEY_GOTHIC_18));
    text_layer_set_text_color(times_layers[i], GColorBlack);
    text_layer_set_background_color(times_layers[i], GColorWhite);
    text_layer_set_text_alignment(times_layers[i], GTextAlignmentCenter);
    layer_add_child(layer, text_layer_get_layer(times_layers[i]));
  }
  
  update_times_menu();
  update_exercise_pos_text();
  update_exercise_name_text();
  update_exercise_desc_text();
}

static void window_unload(Window *window) {
  text_layer_destroy(header_text_layer);
  text_layer_destroy(superbody_text_layer);
  text_layer_destroy(body_text_layer);
  text_layer_destroy(subbody_text_layer);

  action_bar_layer_destroy(action_bar);
}

static void init(void) {
  action_icon_plus = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ACTION_ICON_PLUS);
  action_icon_minus = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ACTION_ICON_MINUS);
  
  //initWorkout();
  
  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });

  window_stack_push(window, true /* Animated */);
  
  // Register callbacks for AppMessage
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);
  // Open AppMessage
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
}

static void deinit(void) {
  // Stop any animation in progress
  animation_unschedule_all();

  window_destroy(window);

  gbitmap_destroy(action_icon_plus);
  gbitmap_destroy(action_icon_minus);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
