#include <pebble.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_TASKS 12
#define MAX_CONTEXTS 12
#define FIELD_LEN 128

enum {
  VIEW_CURRENT = 0,
  VIEW_TASKS,
  VIEW_ACTIONS,
  VIEW_CONTEXTS,
  VIEW_DELETE_CONFIRM
};

static Window *s_window;
static MenuLayer *s_menu;
static ScrollLayer *s_scroll;
static TextLayer *s_header, *s_title, *s_project, *s_timer, *s_body, *s_footer;
static AppTimer *s_clock_timer;
static int s_view = VIEW_CURRENT;
static int s_selected_task = 0;
static int s_selected_action = 0;
static int s_selected_context = 0;
static int s_task_count = 0;
static int s_context_count = 0;
static char s_uuid[MAX_TASKS][64];
static char s_description[MAX_TASKS][FIELD_LEN];
static char s_project_names[MAX_TASKS][64];
static char s_urgency[MAX_TASKS][24];
static char s_contexts[MAX_CONTEXTS][64];
static char s_active_context[80] = "";
static char s_current_uuid[64] = "";
static char s_current_title[FIELD_LEN] = "Waiting for Taskwarrior";
static char s_current_project[80] = "";
static char s_details[640] = "";
static char s_status[80] = "CONNECTING";
static uint32_t s_elapsed = 0;
static bool s_timer_running = false;
static uint8_t s_theme = 0;
static uint8_t s_accent = 0;
static bool s_show_urgency = true, s_show_tags = true, s_show_annotations = true, s_show_due = true;
static GColor s_bg, s_fg, s_accent_color;

static GColor accent_color(uint8_t value) {
  switch (value) {
    case 1: return GColorRed;
    case 2: return GColorGreen;
    case 3: return GColorBlue;
    case 4: return GColorPurple;
    case 5: return GColorOrange;
    default: return GColorChromeYellow;
  }
}
static void apply_theme(void) {
  if (s_theme == 2) { s_bg = GColorWhite; s_fg = GColorBlack; }
  else if (s_theme == 3) { s_bg = GColorBlack; s_fg = GColorWhite; }
  else { s_bg = GColorBlack; s_fg = GColorWhite; }
  s_accent_color = accent_color(s_accent);
}
static void send_command(const char *command, const char *uuid, const char *action, const char *context) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) return;
  dict_write_cstring(iter, MESSAGE_KEY_Command, command ? command : "");
  if (uuid) dict_write_cstring(iter, MESSAGE_KEY_SelectedUuid, uuid);
  if (action) dict_write_cstring(iter, MESSAGE_KEY_SelectedAction, action);
  if (context) dict_write_cstring(iter, MESSAGE_KEY_Context, context);
  dict_write_end(iter);
  app_message_outbox_send();
}
static void clear_view_layers(void) {
  if (s_menu) { layer_remove_from_parent(menu_layer_get_layer(s_menu)); menu_layer_destroy(s_menu); s_menu = NULL; }
  if (s_scroll) { layer_remove_from_parent(scroll_layer_get_layer(s_scroll)); scroll_layer_destroy(s_scroll); s_scroll = NULL; }
  if (s_body) { layer_remove_from_parent(text_layer_get_layer(s_body)); text_layer_destroy(s_body); s_body = NULL; }
  if (s_header) { layer_remove_from_parent(text_layer_get_layer(s_header)); text_layer_destroy(s_header); s_header = NULL; }
  if (s_title) { layer_remove_from_parent(text_layer_get_layer(s_title)); text_layer_destroy(s_title); s_title = NULL; }
  if (s_project) { layer_remove_from_parent(text_layer_get_layer(s_project)); text_layer_destroy(s_project); s_project = NULL; }
  if (s_timer) { layer_remove_from_parent(text_layer_get_layer(s_timer)); text_layer_destroy(s_timer); s_timer = NULL; }
  if (s_footer) { layer_remove_from_parent(text_layer_get_layer(s_footer)); text_layer_destroy(s_footer); s_footer = NULL; }
}
static TextLayer *make_text(GRect rect, const char *font, GTextAlignment align) {
  TextLayer *layer = text_layer_create(rect);
  text_layer_set_background_color(layer, s_bg);
  text_layer_set_text_color(layer, s_fg);
  text_layer_set_font(layer, fonts_get_system_font(font));
  text_layer_set_text_alignment(layer, align);
  layer_add_child(window_get_root_layer(s_window), text_layer_get_layer(layer));
  return layer;
}
static void format_timer(char *out, size_t size) {
  uint32_t seconds = s_elapsed;
  snprintf(out, size, "%02lu:%02lu:%02lu", (unsigned long)(seconds / 3600), (unsigned long)((seconds / 60) % 60), (unsigned long)(seconds % 60));
}
static void update_timer(void) {
  if (s_timer) {
    char value[24];
    format_timer(value, sizeof(value));
    text_layer_set_text(s_timer, value);
  }
}
static void clock_tick(void *data) {
  (void)data;
  s_clock_timer = NULL;
  if (s_timer_running) { s_elapsed++; update_timer(); }
  s_clock_timer = app_timer_register(1000, clock_tick, NULL);
}
static uint16_t menu_rows(MenuLayer *menu, uint16_t section, void *data) {
  (void)menu; (void)section; (void)data;
  if (s_view == VIEW_TASKS) return s_task_count ? s_task_count : 1;
  if (s_view == VIEW_CONTEXTS) return s_context_count ? s_context_count : 1;
  if (s_view == VIEW_ACTIONS) return 3;
  if (s_view == VIEW_DELETE_CONFIRM) return 2;
  return 0;
}
static void menu_draw(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
  (void)data;
  char title[FIELD_LEN + 8], subtitle[80];
  const char *sub = NULL;
  title[0] = '\0'; subtitle[0] = '\0';
  if (s_view == VIEW_TASKS) {
    if (!s_task_count) { snprintf(title, sizeof(title), "No tasks received"); }
    else {
      int i = cell_index->row;
      snprintf(title, sizeof(title), "%s", s_description[i]);
      if (s_project_names[i][0]) snprintf(subtitle, sizeof(subtitle), "%s  U:%s", s_project_names[i], s_urgency[i]);
      else snprintf(subtitle, sizeof(subtitle), "Urgency %s", s_urgency[i]);
      sub = subtitle;
    }
  } else if (s_view == VIEW_CONTEXTS) {
    if (!s_context_count) snprintf(title, sizeof(title), "No contexts received");
    else {
      int i = cell_index->row;
      snprintf(title, sizeof(title), "%s%s", strcmp(s_contexts[i], s_active_context) == 0 ? "✓ " : "", s_contexts[i]);
      if (strcmp(s_contexts[i], s_active_context) == 0) { snprintf(subtitle, sizeof(subtitle), "CURRENT CONTEXT"); sub = subtitle; }
    }
  } else if (s_view == VIEW_ACTIONS) {
    const char *actions[] = { "▶ Start / Resume", "✓ Mark as Done", "× Delete Task" };
    snprintf(title, sizeof(title), "%s", actions[cell_index->row]);
    if (cell_index->row == 2) { snprintf(subtitle, sizeof(subtitle), "Cannot be undone"); sub = subtitle; }
  } else {
    const char *actions[] = { "DELETE TASK", "Cancel" };
    snprintf(title, sizeof(title), "%s", actions[cell_index->row]);
    if (cell_index->row == 0) { snprintf(subtitle, sizeof(subtitle), "Permanently remove task"); sub = subtitle; }
  }
  menu_cell_basic_draw(ctx, cell_layer, title, sub, NULL);
}
static void show_view(int view);
static void back_click(ClickRecognizerRef recognizer, void *context);
static void menu_select(MenuLayer *menu, MenuIndex *index, void *data) {
  (void)menu; (void)data;
  if (s_view == VIEW_TASKS) {
    if (!s_task_count) { send_command("refresh", NULL, NULL, NULL); return; }
    s_selected_task = index->row;
    show_view(VIEW_ACTIONS);
  } else if (s_view == VIEW_CONTEXTS) {
    if (!s_context_count) { send_command("refresh", NULL, NULL, NULL); return; }
    s_selected_context = index->row;
    send_command("set_context", NULL, NULL, s_contexts[s_selected_context]);
    snprintf(s_active_context, sizeof(s_active_context), "%s", s_contexts[s_selected_context]);
    snprintf(s_status, sizeof(s_status), "CONTEXT SET");
    show_view(VIEW_CURRENT);
  } else if (s_view == VIEW_ACTIONS) {
    s_selected_action = index->row;
    if (index->row == 0) {
      send_command("action", (s_selected_task >= 0 && s_selected_task < s_task_count) ? s_uuid[s_selected_task] : s_current_uuid, "start", NULL);
      if (s_selected_task >= 0 && s_selected_task < s_task_count) {
        snprintf(s_current_uuid, sizeof(s_current_uuid), "%s", s_uuid[s_selected_task]);
        snprintf(s_current_title, sizeof(s_current_title), "%s", s_description[s_selected_task]);
        snprintf(s_current_project, sizeof(s_current_project), "%s", s_project_names[s_selected_task]);
      }
      s_elapsed = 0; s_timer_running = true; show_view(VIEW_CURRENT);
    } else if (index->row == 1) {
      send_command("action", (s_selected_task >= 0 && s_selected_task < s_task_count) ? s_uuid[s_selected_task] : s_current_uuid, "done", NULL);
      s_timer_running = false; show_view(VIEW_CURRENT);
    } else show_view(VIEW_DELETE_CONFIRM);
  } else if (s_view == VIEW_DELETE_CONFIRM) {
    if (index->row == 0) {
      send_command("action", (s_selected_task >= 0 && s_selected_task < s_task_count) ? s_uuid[s_selected_task] : s_current_uuid, "delete", NULL);
      s_timer_running = false;
      snprintf(s_status, sizeof(s_status), "TASK DELETED");
      show_view(VIEW_CURRENT);
    } else show_view(VIEW_ACTIONS);
  }
}
static void menu_select_click(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer; (void)context;
}
static void detail_click_config(void *context);
static void show_view(int view) {
  s_view = view;
  clear_view_layers();
  Layer *root = window_get_root_layer(s_window);
  GRect bounds = layer_get_bounds(root);
  if (view == VIEW_TASKS || view == VIEW_ACTIONS || view == VIEW_CONTEXTS || view == VIEW_DELETE_CONFIRM) {
    const char *header = view == VIEW_TASKS ? "TASKS • URGENCY" : view == VIEW_ACTIONS ? "TASK ACTIONS" : view == VIEW_CONTEXTS ? "SET CONTEXT" : "CONFIRM DELETE";
    s_header = make_text(GRect(4, 0, bounds.size.w - 8, 24), FONT_KEY_GOTHIC_14_BOLD, GTextAlignmentLeft);
    text_layer_set_text(s_header, header);
    s_menu = menu_layer_create(GRect(0, 24, bounds.size.w, bounds.size.h - 24));
    menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks){
      .get_num_rows = menu_rows,
      .draw_row = menu_draw,
      .select_click = menu_select
    });
    menu_layer_set_highlight_colors(s_menu, s_accent_color, s_bg);
    menu_layer_set_click_config_onto_window(s_menu, s_window);
    window_single_click_subscribe(BUTTON_ID_BACK, back_click);
    layer_add_child(root, menu_layer_get_layer(s_menu));
    menu_layer_set_normal_colors(s_menu, s_bg, s_fg);
    menu_layer_set_highlight_colors(s_menu, s_accent_color, s_bg);
    MenuIndex first = { .section = 0, .row = 0 };
    menu_layer_set_selected_index(s_menu, first, MenuRowAlignCenter, false);
    return;
  }
  s_header = make_text(GRect(4, 0, bounds.size.w - 8, 22), FONT_KEY_GOTHIC_14_BOLD, GTextAlignmentLeft);
  text_layer_set_text(s_header, "TASKWARRIOR");
  s_title = make_text(GRect(4, 23, bounds.size.w - 8, 37), FONT_KEY_GOTHIC_18_BOLD, GTextAlignmentLeft);
  text_layer_set_text(s_title, s_current_title);
  s_project = make_text(GRect(4, 59, bounds.size.w - 8, 18), FONT_KEY_GOTHIC_14, GTextAlignmentLeft);
  text_layer_set_text(s_project, s_current_project[0] ? s_current_project : s_active_context);
  s_timer = make_text(GRect(4, 76, bounds.size.w - 8, 44), FONT_KEY_BITHAM_30_BLACK, GTextAlignmentLeft);
  text_layer_set_text_color(s_timer, s_accent_color);
  update_timer();
  s_scroll = scroll_layer_create(GRect(0, 122, bounds.size.w, bounds.size.h - 139));
  s_body = text_layer_create(GRect(4, 0, bounds.size.w - 12, 180));
  text_layer_set_background_color(s_body, s_bg);
  text_layer_set_text_color(s_body, s_fg);
  text_layer_set_font(s_body, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_body, GTextAlignmentLeft);
  text_layer_set_text(s_body, s_details[0] ? s_details : "No task details received yet.\n\nUP: task list\nDOWN: context selector\nSELECT: actions");
  scroll_layer_add_child(s_scroll, text_layer_get_layer(s_body));
  scroll_layer_set_content_size(s_scroll, GSize(bounds.size.w, 180));
  scroll_layer_set_click_config_onto_window(s_scroll, s_window);
  layer_add_child(root, scroll_layer_get_layer(s_scroll));
  s_footer = make_text(GRect(4, bounds.size.h - 17, bounds.size.w - 8, 16), FONT_KEY_GOTHIC_14, GTextAlignmentRight);
  text_layer_set_text(s_footer, s_status);
}
static void up_click(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer; (void)context;
  if (s_view == VIEW_CURRENT) show_view(VIEW_TASKS);
}
static void down_click(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer; (void)context;
  if (s_view == VIEW_CURRENT) { send_command("refresh", NULL, NULL, NULL); show_view(VIEW_CONTEXTS); }
}
static void select_click(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer; (void)context;
  if (s_view == VIEW_CURRENT) {
    s_selected_task = -1;
    show_view(VIEW_ACTIONS);
  }
}
static void back_click(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer; (void)context;
  if (s_view == VIEW_CURRENT) { window_stack_pop(false); return; }
  if (s_view == VIEW_ACTIONS) show_view(s_selected_task >= 0 ? VIEW_TASKS : VIEW_CURRENT);
  else if (s_view == VIEW_DELETE_CONFIRM) show_view(VIEW_ACTIONS);
  else show_view(VIEW_CURRENT);
}
static void click_config(void *context) {
  (void)context;
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
  window_single_click_subscribe(BUTTON_ID_BACK, back_click);
}
static void split_fields(char *line, char *fields[], int count) {
  int n = 0; fields[n++] = line;
  for (char *p = line; *p && n < count; p++) if (*p == '~') { *p = '\0'; fields[n++] = p + 1; }
  while (n < count) fields[n++] = "";
}
static void parse_lines(const char *src, int max, char dest[][FIELD_LEN], int *count) {
  *count = 0;
  if (!src || !src[0]) return;
  char buf[2048]; snprintf(buf, sizeof(buf), "%s", src);
  char *save = NULL;
  for (char *line = strtok_r(buf, "\n", &save); line && *count < max; line = strtok_r(NULL, "\n", &save)) {
    snprintf(dest[*count], FIELD_LEN, "%s", line);
    (*count)++;
  }
}
static void inbox_received(DictionaryIterator *iter, void *context) {
  (void)context;
  Tuple *t;
  if ((t = dict_find(iter, MESSAGE_KEY_TaskLines))) {
    char buf[2048]; snprintf(buf, sizeof(buf), "%s", t->value->cstring);
    s_task_count = 0; char *save = NULL;
    for (char *line = strtok_r(buf, "\n", &save); line && s_task_count < MAX_TASKS; line = strtok_r(NULL, "\n", &save)) {
      char *f[4]; split_fields(line, f, 4);
      snprintf(s_uuid[s_task_count], sizeof(s_uuid[0]), "%s", f[0]);
      snprintf(s_description[s_task_count], sizeof(s_description[0]), "%s", f[1]);
      snprintf(s_project_names[s_task_count], sizeof(s_project_names[0]), "%s", f[2]);
      snprintf(s_urgency[s_task_count], sizeof(s_urgency[0]), "%s", f[3]);
      s_task_count++;
    }
  } else if ((t = dict_find(iter, MESSAGE_KEY_TaskCount))) s_task_count = t->value->uint8;
  if ((t = dict_find(iter, MESSAGE_KEY_ContextLines))) {
    char buf[1024]; snprintf(buf, sizeof(buf), "%s", t->value->cstring);
    s_context_count = 0; char *save = NULL;
    for (char *line = strtok_r(buf, "\n", &save); line && s_context_count < MAX_CONTEXTS; line = strtok_r(NULL, "\n", &save)) {
      snprintf(s_contexts[s_context_count++], sizeof(s_contexts[0]), "%s", line);
    }
  }
  if ((t = dict_find(iter, MESSAGE_KEY_CurrentTitle))) snprintf(s_current_title, sizeof(s_current_title), "%s", t->value->cstring);
  if ((t = dict_find(iter, MESSAGE_KEY_CurrentProject))) snprintf(s_current_project, sizeof(s_current_project), "%s", t->value->cstring);
  if ((t = dict_find(iter, MESSAGE_KEY_CurrentUuid))) snprintf(s_current_uuid, sizeof(s_current_uuid), "%s", t->value->cstring);
  if ((t = dict_find(iter, MESSAGE_KEY_CurrentDetails))) snprintf(s_details, sizeof(s_details), "%s", t->value->cstring);
  if ((t = dict_find(iter, MESSAGE_KEY_ActiveContext))) snprintf(s_active_context, sizeof(s_active_context), "%s", t->value->cstring);
  if ((t = dict_find(iter, MESSAGE_KEY_StatusText))) snprintf(s_status, sizeof(s_status), "%s", t->value->cstring);
  if ((t = dict_find(iter, MESSAGE_KEY_Elapsed))) { s_elapsed = t->value->uint32; update_timer(); }
  if ((t = dict_find(iter, MESSAGE_KEY_Theme))) s_theme = t->value->uint8;
  if ((t = dict_find(iter, MESSAGE_KEY_Accent))) s_accent = t->value->uint8;
  if ((t = dict_find(iter, MESSAGE_KEY_ShowUrgency))) s_show_urgency = t->value->uint8 != 0;
  if ((t = dict_find(iter, MESSAGE_KEY_ShowTags))) s_show_tags = t->value->uint8 != 0;
  if ((t = dict_find(iter, MESSAGE_KEY_ShowAnnotations))) s_show_annotations = t->value->uint8 != 0;
  if ((t = dict_find(iter, MESSAGE_KEY_ShowDue))) s_show_due = t->value->uint8 != 0;
  apply_theme();
  if (s_window) window_set_background_color(s_window, s_bg);
  if (s_menu) menu_layer_reload_data(s_menu);
  if (s_window && s_view == VIEW_CURRENT) show_view(VIEW_CURRENT);
}
static void inbox_dropped(AppMessageResult reason, void *context) {
  (void)context; snprintf(s_status, sizeof(s_status), "MESSAGE ERROR %d", reason);
}
static void outbox_failed(DictionaryIterator *iter, AppMessageResult reason, void *context) {
  (void)iter; (void)context; snprintf(s_status, sizeof(s_status), "SEND ERROR %d", reason);
}
static void window_load(Window *window) {
  (void)window;
  apply_theme();
  window_set_background_color(s_window, s_bg);
  show_view(VIEW_CURRENT);
  window_set_click_config_provider(s_window, click_config);
  send_command("refresh", NULL, NULL, NULL);
}
static void window_unload(Window *window) {
  (void)window; clear_view_layers();
}
static void init(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){ .load = window_load, .unload = window_unload });
  window_stack_push(s_window, true);
  app_message_register_inbox_received(inbox_received);
  app_message_register_inbox_dropped(inbox_dropped);
  app_message_register_outbox_failed(outbox_failed);
  app_message_open(4096, 1024);
  s_clock_timer = app_timer_register(1000, clock_tick, NULL);
}
static void deinit(void) {
  if (s_clock_timer) app_timer_cancel(s_clock_timer);
  window_destroy(s_window);
}
int main(void) { init(); app_event_loop(); deinit(); }
