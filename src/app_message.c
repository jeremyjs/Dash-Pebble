#include <pebble.h>
#include <stdlib.h>
#include <string.h>
  
void menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data);
  
// Utility functions
unsigned int_to_int(unsigned k) { if (k == 0) return 0; if (k == 1) return 1; return (k % 2) + 10 * int_to_int(k / 2); }
static void reverse(char s[]) { int i, j; char c; for (i = 0, j = strlen(s)-1; i<j; i++, j--) { c = s[i]; s[i] = s[j]; s[j] = c; } }
static void itoa(int n, char s[]) { int i, sign; if ((sign = n) < 0) { n = -n; } i = 0; do { s[i++] = n % 10 + '0'; } while ((n /= 10) > 0); if (sign < 0) s[i++] = '-'; s[i] = '\0'; reverse(s); }
char *translate_error(AppMessageResult result) {
  switch (result) {
    case APP_MSG_OK: return "APP_MSG_OK";
    case APP_MSG_SEND_TIMEOUT: return "APP_MSG_SEND_TIMEOUT";
    case APP_MSG_SEND_REJECTED: return "APP_MSG_SEND_REJECTED";
    case APP_MSG_NOT_CONNECTED: return "APP_MSG_NOT_CONNECTED";
    case APP_MSG_APP_NOT_RUNNING: return "APP_MSG_APP_NOT_RUNNING";
    case APP_MSG_INVALID_ARGS: return "APP_MSG_INVALID_ARGS";
    case APP_MSG_BUSY: return "APP_MSG_BUSY";
    case APP_MSG_BUFFER_OVERFLOW: return "APP_MSG_BUFFER_OVERFLOW";
    case APP_MSG_ALREADY_RELEASED: return "APP_MSG_ALREADY_RELEASED";
    case APP_MSG_CALLBACK_ALREADY_REGISTERED: return "APP_MSG_CALLBACK_ALREADY_REGISTERED";
    case APP_MSG_CALLBACK_NOT_REGISTERED: return "APP_MSG_CALLBACK_NOT_REGISTERED";
    case APP_MSG_OUT_OF_MEMORY: return "APP_MSG_OUT_OF_MEMORY";
    case APP_MSG_CLOSED: return "APP_MSG_CLOSED";
    case APP_MSG_INTERNAL_ERROR: return "APP_MSG_INTERNAL_ERROR";
    default: return "UNKNOWN ERROR";
  }
}

// BLOCKS
typedef struct Block {
  int len;
  char* text;
  int index;
  struct Block* next;
} Block;

// New Block
Block* newBlock(char* text) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Creating a new block");
  Block* new = malloc(sizeof(Block));
  new->len = strlen(text);
  new->text = malloc(new->len + 1);
  strcpy(new->text, text);
  new->index = 0;
  new->next = NULL;
  APP_LOG(APP_LOG_LEVEL_INFO, "New block created");
  return new;
}

// STR NODE
typedef struct StrNode {
  char* val;
  struct StrNode* next;
} StrNode;

// New StrNode
StrNode* newStrNode(char* val) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Creating a new StrNode");
  StrNode* new = malloc(sizeof(StrNode));
  new->val = malloc(strlen(val) + 1);
  strcpy(new->val, val);
  new->next = NULL;
  APP_LOG(APP_LOG_LEVEL_INFO, "New StrNode created");
  return new;
}

// Variables
Window *window;
TextLayer *reading_layer;
TextLayer *wpm_layer;

StrNode* titles = NULL;
StrNode* last_title = NULL;
int num_titles = 0;

Block* curr_block = NULL;
char* curr_word = NULL;
unsigned int counter = 0;
unsigned int delay;
unsigned int user_wpm;
bool paused = true;
bool getting_block = false;

// Key values for AppMessage Dictionary
enum {
  BLOCK_KEY = 0,
	GET_BLOCK_KEY = 1,
  TITLE0_KEY = 2,
  TITLE1_KEY = 3,
  TITLE2_KEY = 4,
  BOOK_KEY = 5
};

// Setting WPM
static int wpm_to_ms(uint16_t wpm) {
  return (uint32_t)((60 * 1000) / wpm);
}
static void set_delay_wpm(uint16_t wpm) {
  delay = wpm_to_ms(wpm);
}
static char* strint(const char* str, int num) {
  char numstr[10];
  itoa(num, numstr);
  char* output = malloc(strlen(str) + strlen(numstr) + 1);
  strcat(output, str);
  strcat(output, numstr);
  return output;
}
static void set_wpm(int wpm) {
  user_wpm = wpm;
  set_delay_wpm(user_wpm);
  text_layer_set_text(wpm_layer, strint("WPM: ", user_wpm));
	APP_LOG(APP_LOG_LEVEL_DEBUG, "WPM: %d", user_wpm);
}

// Request a block
void get_block(void){
  getting_block = true;
  APP_LOG(APP_LOG_LEVEL_INFO, "Requesting a new block");
	DictionaryIterator *iter;
	
	app_message_outbox_begin(&iter);
	dict_write_uint8(iter, GET_BLOCK_KEY, 0x1);
	
	dict_write_end(iter);
  app_message_outbox_send();
}

// Get next word from current block
static char* getNextWord() {
  if(!curr_block->next && !getting_block) { get_block(); }
  if(curr_block->index >= curr_block->len) {
    get_block();
    if(curr_block->next) {
      curr_block = curr_block->next;
    }
    else {
      curr_block->index = -1;
      return "Loading";
    }
  }
  else if(curr_block->index == -1 && curr_block->next) {
    curr_block = curr_block->next;
  }
  curr_word = curr_block->text + curr_block->index;
  int i = 0;
  while(curr_block->text[curr_block->index + i] != ' ' &&
        curr_block->text[curr_block->index + i] != '\0') {
    i++;
  }
  curr_block->index += i;
  curr_block->text[curr_block->index] = '\0';
  curr_block->index++;
  return curr_word;
}

// Loading display
int loading_counter = -1;
static void loadingInc() {
  loading_counter++;
  if(loading_counter > 2) {
    loading_counter = 0;
  }
}
static char* loading() {
  loadingInc();
  switch(loading_counter) {
    case 0:
      return "Loading.";
    case 1:
      return "Loading..";
    case 2:
    default:
      return "Loading...";
  }
}

// Get Word Timer Callback
static void timer_callback(void* data) {
  if(!paused) {
    if(curr_block && curr_block->index != -1) {
      text_layer_set_text(reading_layer, getNextWord());
    }
    else {
      text_layer_set_text(reading_layer, loading());
    }
  }
  app_timer_register(delay, timer_callback, NULL);
}

// Click Handler
static void click_handler(ClickRecognizerRef recognizer, Window *window) {  
  switch (click_recognizer_get_button_id(recognizer)) {
    case BUTTON_ID_UP:
      // speed up
      set_wpm(user_wpm + 50);
      break;

    case BUTTON_ID_DOWN:
      // slow down
      user_wpm -= 50;
      if (user_wpm <= 0) user_wpm = 1;
      set_wpm(user_wpm);
      break;

    default:
    case BUTTON_ID_SELECT:
      // toggle pause
      paused = !paused;
      break;
  }
}
static void reading_config_provider(Window *window) {
  window_single_click_subscribe(BUTTON_ID_UP, (ClickHandler) click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, (ClickHandler) click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, (ClickHandler) click_handler);
}

// TITLES
// Add Title
static void addTitle(char* title) {
  StrNode* new = newStrNode(title);
  if(last_title) {
    last_title->next = new;
    last_title = new;
    new = NULL;
  }
  else {
    titles = new;
    last_title = new;
  }
  num_titles++;
}

// Get Title
static char* getTitle(int index) {
  StrNode* tmp = titles;
  for(int i = 0; i < index; i++) {
    tmp = tmp->next;
  }
  if(tmp && tmp->val) {
    return tmp->val;
  }
  else {
    return "-";
  }
}

// Load Book
static void loadBook(int title_num) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Requesting a book");
	DictionaryIterator *iter;
	
	app_message_outbox_begin(&iter);
	dict_write_uint8(iter, BOOK_KEY, title_num);
	
	dict_write_end(iter);
  app_message_outbox_send();
}

// MENU ----------------------------------------------

#define NUM_MENU_SECTIONS 1
#define NUM_MENU_ICONS 3
#define NUM_FIRST_MENU_ITEMS num_titles + 1
#define NUM_SECOND_MENU_ITEMS 0

// This is a menu layer
// You have more control than with a simple menu layer
static MenuLayer *menu_layer;

// Menu items can optionally have an icon drawn with them
// static GBitmap *menu_icons[NUM_MENU_ICONS];

// static int current_icon = 0;

// You can draw arbitrary things in a menu item such as a background
static GBitmap *menu_background;

// A callback is used to specify the amount of sections of menu items
// With this, you can dynamically add and remove sections
static uint16_t menu_get_num_sections_callback(MenuLayer *menu_layer, void *data) {
  return NUM_MENU_SECTIONS;
}

// Each section has a number of items;  we use a callback to specify this
// You can also dynamically add and remove items using this
static uint16_t menu_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *data) {
  switch (section_index) {
    case 0:
      return NUM_FIRST_MENU_ITEMS;

    // case 1:
    //   return NUM_SECOND_MENU_ITEMS;

    default:
      return 0;
  }
}

// A callback is used to specify the height of the section header
static int16_t menu_get_header_height_callback(MenuLayer *menu_layer, uint16_t section_index, void *data) {
  // This is a define provided in pebble.h that you may use for the default height
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

// Here we draw what each header is
static void menu_draw_header_callback(GContext* ctx, const Layer *cell_layer, uint16_t section_index, void *data) {
  // Determine which section we're working with
  switch (section_index) {
    case 0:
      // Draw title text in the section header
      menu_cell_basic_header_draw(ctx, cell_layer, "Books");
      break;

    // case 1:
    //   menu_cell_basic_header_draw(ctx, cell_layer, "Articles");
    //   break;
    
    default:
      menu_cell_basic_header_draw(ctx, cell_layer, "Other");
  }
}

// This is the menu item draw callback where you specify what each item should look like
static void menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
  // Determine which section we're going to draw in
  switch (cell_index->section) {
    case 0:
      // Use the row to specify which item we'll draw
      if(cell_index->row < 10) {
        menu_cell_title_draw(ctx, cell_layer, getTitle(cell_index->row));
      }
      else {
        menu_cell_title_draw(ctx, cell_layer, "More...");
      }
      break;

    // case X:
    //   // This is a basic menu icon with an icon
    //   menu_cell_basic_draw(ctx, cell_layer, "Icon Item", "Select to cycle", menu_icons[current_icon]);
    //   break;

    // case 1:
    //   switch (cell_index->row) {
    //     case 0:
    //       menu_cell_title_draw(ctx, cell_layer, "Some article");
    //       break;
    //   }
  }
}

void window_unload(Window *window) {
  // Destroy the menu layer
  menu_layer_destroy(menu_layer);

  // Cleanup the menu icons
  // for (int i = 0; i < NUM_MENU_ICONS; i++) {
  //   gbitmap_destroy(menu_icons[i]);
  // }

  // And cleanup the background
  // gbitmap_destroy(menu_background);
}

// Initialize menu upon window load
void window_load(Window *window) {
  // Here we load the bitmap assets
  // resource_init_current_app must be called before all asset loading
  // int num_menu_icons = 0;
  // menu_icons[num_menu_icons++] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MENU_ICON_BIG_WATCH);
  // menu_icons[num_menu_icons++] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MENU_ICON_SECTOR_WATCH);
  // menu_icons[num_menu_icons++] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MENU_ICON_BINARY_WATCH);

  // And also load the background
  // menu_background = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND_BRAINS);

  // Now we prepare to initialize the menu layer
  // We need the bounds to specify the menu layer's viewport size
  // In this case, it'll be the same as the window's
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_frame(window_layer);

  // Create the menu layer
  menu_layer = menu_layer_create(bounds);

  // Set all the callbacks for the menu layer
  menu_layer_set_callbacks(menu_layer, NULL, (MenuLayerCallbacks){
    .get_num_sections = menu_get_num_sections_callback,
    .get_num_rows = menu_get_num_rows_callback,
    .get_header_height = menu_get_header_height_callback,
    .draw_header = menu_draw_header_callback,
    .draw_row = menu_draw_row_callback,
    .select_click = menu_select_callback,
  });

  // Bind the menu layer's click config provider to the window for interactivity
  menu_layer_set_click_config_onto_window(menu_layer, window);

  // Add it to the window for display
  layer_add_child(window_layer, menu_layer_get_layer(menu_layer));
}

static void createMenu() {
  // Setup the window handlers
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });

  window_stack_push(window, true /* animation */);
}

// MESSAGES -------------------------------------------------

// Message Received
static void in_received_handler(DictionaryIterator *received, void *context) {
	Tuple *tuple;
	
	tuple = dict_find(received, BLOCK_KEY);
	if(tuple) {
    getting_block = false;
		APP_LOG(APP_LOG_LEVEL_DEBUG, "Received Block: %s", tuple->value->cstring);
    Block* block = newBlock(tuple->value->cstring);
    if(curr_block && curr_block->next) {
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Error, recieved extra block");
    }
    else if(curr_block) {
      curr_block->next = block;
    }
    else {
      curr_block = block;
    }
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Done receiving block");
    return;
	}
	tuple = dict_find(received, TITLE0_KEY);
  if(tuple) {
    addTitle(tuple->value->cstring);
  }
	tuple = dict_find(received, TITLE1_KEY);
  if(tuple) {
    addTitle(tuple->value->cstring);
  }
	tuple = dict_find(received, TITLE2_KEY);
  if(tuple) {
    addTitle(tuple->value->cstring);
  }
  else {
    tuple = dict_read_first(received);
		APP_LOG(APP_LOG_LEVEL_DEBUG, "Unknown message received - first key: %lu", tuple->key);
    return;
  }
  createMenu();
}

// Message Dropped
static void in_dropped_handler(AppMessageResult reason, void *context) {
   APP_LOG(APP_LOG_LEVEL_DEBUG, "In dropped: %i - %s", reason, translate_error(reason));
}

// Message Failed
static void out_failed_handler(DictionaryIterator *failed, AppMessageResult reason, void *context) {
   APP_LOG(APP_LOG_LEVEL_DEBUG, "In failed: %i - %s", reason, translate_error(reason));
}

// MAIN -----------------------------------------------------

// Start Reading
static void startReading() {
  window_unload(window);
  
  window_set_click_config_provider(window, (ClickConfigProvider) reading_config_provider);
  
  // Reading layer
	reading_layer = text_layer_create(GRect(0, 56, 144, 32));
	text_layer_set_font(reading_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
	text_layer_set_text_alignment(reading_layer, GTextAlignmentCenter);
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(reading_layer));
  
  // WPM layer
	wpm_layer = text_layer_create(GRect(0, 0, 144, 18));
	text_layer_set_font(wpm_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
	text_layer_set_text_alignment(wpm_layer, GTextAlignmentCenter);
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(wpm_layer));
  
	// Push the window
	window_stack_push(window, true);
  
  set_wpm(300);
  
  text_layer_set_text(reading_layer, "Loading.");
  app_timer_register(delay, timer_callback, NULL);
}

// Here we capture when a user selects a menu item
void menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
  loadBook(cell_index->row);
  startReading();
}

static void handle_init(void) {
	// Create the window
	window = window_create();
  
  // Messages
	app_message_register_inbox_received(in_received_handler); 
	app_message_register_inbox_dropped(in_dropped_handler); 
	app_message_register_outbox_failed(out_failed_handler);
	app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
  
  createMenu();
}

static void handle_deinit(void) {
	// Destroy layers
	text_layer_destroy(reading_layer);
	text_layer_destroy(wpm_layer);
	
	// Destroy the window
	window_destroy(window);
}

int main(void) {
	handle_init();
	app_event_loop();
	handle_deinit();
}
