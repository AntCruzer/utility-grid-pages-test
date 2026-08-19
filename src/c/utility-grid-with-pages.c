/* Code that runs on the Pebble device itself.
  Manages what's displayed, recieves setting choice AppMessages from PebbleJS 
  and retains in Persisted Storage */

  // TEST: PAGES version



  
#include <pebble.h>
#include <time.h>
#include <ctype.h>


#define PERSIST_KEY_USE_CELSIUS 1       // Persistent storage key used on the watch
#define DEFAULT_USE_CELSIUS true        // Default temperature unit to Celsius
#define WEATHER_REFRESH_MINUTES 30      // Request fresh weather every 30 minutes

// Current in-memory setting
static bool s_use_celsius;

// Current weather data
static bool s_has_temperature;
static int s_temperature_celsius;

// Health service subscription state
static bool s_health_subscribed;

// Main window pointer
static Window *s_main_window;

// Custom layers for the grid and center time box
static Layer *s_grid_layer;
static Layer *s_time_box_layer;

// Text layers for watchface information
static TextLayer *s_time_layer;
static TextLayer *s_date_layer;
static TextLayer *s_weather_layer;
static TextLayer *s_steps_layer;
static TextLayer *s_event_layer;

// Text buffers must remain in memory while TextLayers display them
static char s_time_buffer[8];
static char s_date_buffer[16];
static char s_weather_buffer[16];
static char s_steps_buffer[24];


/**
 * Function: Conversts lowercase text to uppercase text
 */
static void uppercase_text(char *text) {

  for (int i = 0; text[i] != '\0'; i++) {
    text[i] = (char)toupper((unsigned char)text[i]);
  }
}


/**
 * Function: Draws the quadrant divider lines.
 *///////////
static void grid_update_proc(Layer *layer, GContext *ctx) {

  GRect bounds = layer_get_bounds(layer);
  int center_x = bounds.size.w / 2;
  int center_y = bounds.size.h / 2;

  graphics_context_set_stroke_color(ctx, GColorBlack);

  // Vertical divider
  graphics_draw_line(
    ctx,
    GPoint(center_x, 0),
    GPoint(center_x, bounds.size.h)
  );

  // Horizontal divider
  graphics_draw_line(
    ctx,
    GPoint(0, center_y),
    GPoint(bounds.size.w, center_y)
  );
}


/**
 * Function: Draws center box that sits over quadrants to hold current time
 *///////////
static void time_box_update_proc(Layer *layer, GContext *ctx) {

  GRect bounds = layer_get_bounds(layer);

  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_rect(ctx, bounds);
}


/**
 * Function: Applies the user's temperature unit preference to the interface
 */
static void apply_settings(void) {

  // Case: weather layer not rendered
  if (!s_weather_layer) {
    return;
  }

  // Case: No weather has been received yet
  if (!s_has_temperature) {

    snprintf(
      s_weather_buffer,
      sizeof(s_weather_buffer),
      "--\xC2\xB0%c",
      s_use_celsius ? 'C' : 'F'
    );

    text_layer_set_text(s_weather_layer, s_weather_buffer);
    return;
  }

  int displayed_temperature = s_temperature_celsius;

  // C to F conversion
  if (!s_use_celsius) {
    displayed_temperature = (s_temperature_celsius * 9) / 5 + 32;
  }

  snprintf(
    s_weather_buffer,
    sizeof(s_weather_buffer),
    "%d\xC2\xB0%c",
    displayed_temperature,
    s_use_celsius ? 'C' : 'F'
  );

  text_layer_set_text(s_weather_layer, s_weather_buffer);
}


/**
 * Function: Loads settings stored on the watch.
 */
static void load_settings(void) {

  // Case: Setting is stored on watch
  if (persist_exists(PERSIST_KEY_USE_CELSIUS)) {

    s_use_celsius =
      persist_read_bool(PERSIST_KEY_USE_CELSIUS);

  } else {  
    
    // On first launch, Celsius is used by default
    s_use_celsius = DEFAULT_USE_CELSIUS;
  }
}


/**
 * Function: Updates the displayed time and date
 */
static void update_time_and_date(void) {

  // Get current time
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  // Format time in 24h
  if (clock_is_24h_style() == true) {

    strftime(
      s_time_buffer,
      sizeof(s_time_buffer),
      "%H:%M",
      tick_time
    );

  } else {    // Format time in 12h

    char twelve_hour_buffer[8];

    strftime(
      twelve_hour_buffer,
      sizeof(twelve_hour_buffer),
      "%I:%M",
      tick_time
    );

    // Remove the leading zero from 12-hour times such as 01:00
    snprintf(
      s_time_buffer,
      sizeof(s_time_buffer),
      "%s",
      twelve_hour_buffer[0] == '0'
        ? twelve_hour_buffer + 1
        : twelve_hour_buffer
    );
  }

  text_layer_set_text(s_time_layer, s_time_buffer);

  // Build a two-line date: FRI / AUG 07
  char day_buffer[4];
  char month_buffer[4];

  strftime(day_buffer, sizeof(day_buffer), "%a", tick_time);
  strftime(month_buffer, sizeof(month_buffer), "%b", tick_time);

  uppercase_text(day_buffer);
  uppercase_text(month_buffer);

  snprintf(
    s_date_buffer,
    sizeof(s_date_buffer),
    "%s\n%s %02d",
    day_buffer,
    month_buffer,
    tick_time->tm_mday
  );

  text_layer_set_text(s_date_layer, s_date_buffer);
}


/**
 * Function: Updates today's step count using Pebble Health.
 *////////////
static void update_steps(void) {

// Check if device supports Pebble Health capabilities
#if defined(PBL_HEALTH)

  time_t start = time_start_of_today(); // mark start of day
  time_t end = time(NULL);              

  // Check that today's step data is available before reading it
  HealthServiceAccessibilityMask mask =
    health_service_metric_accessible(
      HealthMetricStepCount,
      start,
      end
    );

  if (mask & HealthServiceAccessibilityMaskAvailable) {

    HealthValue steps =
      health_service_sum_today(HealthMetricStepCount);

    snprintf(
      s_steps_buffer,
      sizeof(s_steps_buffer),
      "%ld\nSTEPS",
      (long)steps
    );

  } else {

    snprintf(
      s_steps_buffer,
      sizeof(s_steps_buffer),
      "--\nSTEPS"
    );
  }

#else

  snprintf(
    s_steps_buffer,
    sizeof(s_steps_buffer),
    "--\nSTEPS"
  );

#endif

  if (s_steps_layer) {
    text_layer_set_text(s_steps_layer, s_steps_buffer);
  }
}


/**
 * Function: Requests weather data from PebbleKit JS
 */
static void request_weather(void) {

  DictionaryIterator *iterator;

  AppMessageResult result =
    app_message_outbox_begin(&iterator);

  // AppMessage is currently unavailable
  if (result != APP_MSG_OK || !iterator) {
    return;
  }

  dict_write_uint8(
    iterator,
    MESSAGE_KEY_RequestWeather,
    1
  );

  app_message_outbox_send();
}


/**
 * Function: AppMessage inbox sent from phone
 * 
 * Runs when PebbleKit JS sends weather data and its setting prefernce
 */
static void inbox_received_handler(
  DictionaryIterator *iterator,
  void *context
) {

  // Receive current temperature from PebbleKit JS
  Tuple *temperature_tuple =
    dict_find(iterator, MESSAGE_KEY_TemperatureCelsius);

  if (temperature_tuple) {

    s_temperature_celsius =
      (int)temperature_tuple->value->int32;

    s_has_temperature = true;

    apply_settings();
  }

  // Receive Celsius/Fahrenheit setting from PebbleKit JS
  Tuple *use_celsius_tuple =
    dict_find(iterator, MESSAGE_KEY_UseCelsius);

  if (use_celsius_tuple) {

    s_use_celsius =
      use_celsius_tuple->value->int32 == 1;

    apply_settings();

    // Save the setting on the watch
    persist_write_bool(
      PERSIST_KEY_USE_CELSIUS,
      s_use_celsius
    );
  }
}


/**
 * Function: Minute tick handler, runs once per minute
 */
static void tick_handler(
  struct tm *tick_time,
  TimeUnits units_changed
) {

  update_time_and_date();

  // Ask PebbleKit JS for new weather every 30 minutes
  if (tick_time->tm_min % WEATHER_REFRESH_MINUTES == 0) {
    request_weather();
  }
}


/**
 * Function: Health state change hander.
 */
static void health_handler(
  HealthEventType event,
  void *context
) {

  if (
    event == HealthEventSignificantUpdate ||
    event == HealthEventMovementUpdate
  ) {
    update_steps();
  }
}



/**
 * Function: Runs when main window is created
 *///////////
static void main_window_load(Window *window) {

  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  window_set_background_color(window, GColorWhite);

  // Create quadrant divider layer
  s_grid_layer = layer_create(bounds);
  layer_set_update_proc(s_grid_layer, grid_update_proc);
  layer_add_child(window_layer, s_grid_layer);


  // Create and configure the date layer (top-left quadrant)
  s_date_layer = text_layer_create(
    GRect(4, 8, 64, 48)
  );
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, GColorBlack);
  text_layer_set_text(s_date_layer, "---\n--- --");
  text_layer_set_font(
    s_date_layer,
    fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD)
  );
  text_layer_set_text_alignment(
    s_date_layer,
    GTextAlignmentCenter
  );


  // Create and configure the weather layer (top-right quadrant)
  s_weather_layer = text_layer_create(
    GRect(76, 18, 64, 32)
  );
  text_layer_set_background_color(s_weather_layer, GColorClear);
  text_layer_set_text_color(s_weather_layer, GColorBlack);
  text_layer_set_font(
    s_weather_layer,
    fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD)
  );
  text_layer_set_text_alignment(
    s_weather_layer,
    GTextAlignmentCenter
  );


  // Create and configure the event layer (bottom-left quadrant)
  // PebbleKit JS does not expose the phone's calendar directly, so this
  // section is reserved for the upcoming-event integration added later.
  s_event_layer = text_layer_create(
    GRect(4, 110, 64, 44)
  );
  text_layer_set_background_color(s_event_layer, GColorClear);
  text_layer_set_text_color(s_event_layer, GColorBlack);
  text_layer_set_text(s_event_layer, "--:--\nNO EVENT");
  text_layer_set_font(
    s_event_layer,
    fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD)
  );
  text_layer_set_text_alignment(
    s_event_layer,
    GTextAlignmentCenter
  );


  // Create and configure the steps layer (bottom-right quadrant)
  s_steps_layer = text_layer_create(
    GRect(76, 108, 64, 50)
  );
  text_layer_set_background_color(s_steps_layer, GColorClear);
  text_layer_set_text_color(s_steps_layer, GColorBlack);
  text_layer_set_text(s_steps_layer, "--\nSTEPS");
  text_layer_set_font(
    s_steps_layer,
    fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD)
  );
  text_layer_set_text_alignment(
    s_steps_layer,
    GTextAlignmentCenter
  );


  // Add informational quadrant layers to the window
  layer_add_child(
    window_layer,
    text_layer_get_layer(s_date_layer)
  );
  layer_add_child(
    window_layer,
    text_layer_get_layer(s_weather_layer)
  );
  layer_add_child(
    window_layer,
    text_layer_get_layer(s_event_layer)
  );
  layer_add_child(
    window_layer,
    text_layer_get_layer(s_steps_layer)
  );


  // Create the center time box above the quadrant divider lines
  s_time_box_layer = layer_create(
    GRect(27, 64, 90, 40)
  );
  layer_set_update_proc(
    s_time_box_layer,
    time_box_update_proc
  );
  layer_add_child(window_layer, s_time_box_layer);


  // Create and configure the time layer
  s_time_layer = text_layer_create(
    GRect(27, 65, 90, 38)
  );
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorBlack);
  text_layer_set_text(s_time_layer, "00:00");
  text_layer_set_font(
    s_time_layer,
    fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK)
  );
  text_layer_set_text_alignment(
    s_time_layer,
    GTextAlignmentCenter
  );
  layer_add_child(
    window_layer,
    text_layer_get_layer(s_time_layer)
  );


  // Apply the loaded setting now that the weather layer exists
  apply_settings();
}


/**
 * Function: Runs when main window is destroyed
 */
static void main_window_unload(Window *window) {

  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_date_layer);
  text_layer_destroy(s_weather_layer);
  text_layer_destroy(s_steps_layer);
  text_layer_destroy(s_event_layer);

  layer_destroy(s_time_box_layer);
  layer_destroy(s_grid_layer);
}


/**
 * Function: App initialization / Setup
 */
static void init(void) {

  load_settings();      // Load settings before constructing the interface

  // Create main window
  s_main_window = window_create();

  // Set handlers for window lifecycle
  window_set_window_handlers(
    s_main_window,
    (WindowHandlers) {
      .load = main_window_load,
      .unload = main_window_unload
    }
  );

  // Display the window
  window_stack_push(s_main_window, true);

  // Event service: Minute tick service
  tick_timer_service_subscribe(
    MINUTE_UNIT,
    tick_handler
  );

  // Event service: Pebble Health movement updates
#if defined(PBL_HEALTH)
  s_health_subscribed =
    health_service_events_subscribe(
      health_handler,
      NULL
    );
#endif

  // AppMessaging: Receive weather and settings from PebbleKit JS
  app_message_register_inbox_received(
    inbox_received_handler
  );

  // Allocate AppMessage inbox and outbox buffers
  app_message_open(128, 128);

  update_time_and_date();  // My function: Display current time and date
  update_steps();          // My function: Display current step count
}


/**
 * Function: App clean up
 */
static void deinit(void) {

  /* Deregister and unsubscribe from services on watchface exit */
  tick_timer_service_unsubscribe();

#if defined(PBL_HEALTH)
  if (s_health_subscribed) {
    health_service_events_unsubscribe();
  }
#endif

  app_message_deregister_callbacks();

  window_destroy(s_main_window);
}


/**
 * Function: Watch face starting point 
 */
int main(void) {

  init();             // My Function: App initialization / Setup

  app_event_loop();   // Main watchface events handler

  deinit();           // Clean up: Destroy watchface elements
}
