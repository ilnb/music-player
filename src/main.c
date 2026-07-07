#include "list.h"
#include "utils.h"
#include <locale.h>
#include <raylib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

typedef struct {
  int l, r;
} ClickState;

typedef struct {
  int x, y, r;
} Circle;

typedef struct {
  Rectangle dim;
  char *header;
} Button;

typedef struct {
  char *path;
  int id;
  int cover_status;
  int audio_status;
  int is_mp3;
  list_head node;
} Song;

const float screen_width = 1280;
const float screen_height = 720;
int b_count = 4;
// int b_count = 2;
Button *buttons = NULL;
Circle **circles;
Circle progress_icon = {0};
Rectangle **corners;
Song *songs;
Font font;
int font_size = 25;
int spacing = 2;
int *codepoints = NULL;
Rectangle play_button = {0};
Rectangle progress_bar = {0};
Rectangle volume_bar = {0};
Rectangle shuffle_btn = {0};
Rectangle repeat_btn = {0};
Rectangle prev_btn = {0};
Rectangle next_btn = {0};
int shuffle_mode = 0;
int repeat_mode = 0;
float volume = 1.0f;
int is_dragging_time = 0;
float drag_time_ratio = 0.0f;
Music music;

#define MAX_HISTORY 256
Song *history[MAX_HISTORY];
int history_count = 0;
int history_idx = -1;
const Color button_bg = (Color){0x15, 0x15, 0x15, 0xFF};
const Color main_bg = (Color){0x20, 0x20, 0x20, 0xFF};
const Color bar_bg = (Color){0x30, 0x30, 0x30, 0xFF};
const Color txt_fg = (Color){0xFF, 0xFF, 0xFF, 0xFF};
int b_font = 20;
float pad = 30.0;
float gap = 20.0;
Vector2 mouse = {0};
Texture2D texture = {0};
char buf[1024];
char end[15] = {0};

float start_time = 0;
float pause_time = 0;
Time end_time;
int playing = 1;
int paused = 0;
int done_playing = 0;
int screen_setup_done = 0;
int song_setup_done = 0;
list_head list = LIST_HEAD_INIT(list);
Song *current_song = NULL;
Song *pending_song = NULL;
int scroll_offset = 0;

#define NUM_THREADS 4
pthread_t workers[NUM_THREADS];
pthread_mutex_t song_mutex = PTHREAD_MUTEX_INITIALIZER;

void *process_songs(void *arg) {
  while (1) {
    Song *target_cover = NULL;
    Song *target_audio = NULL;

    pthread_mutex_lock(&song_mutex);
    if (pending_song) {
      if (pending_song->cover_status == 0) {
        target_cover = pending_song;
        target_cover->cover_status = 1;
      }
      if (pending_song->audio_status == 0) {
        target_audio = pending_song;
        target_audio->audio_status = 1;
      }
    }

    if (!target_cover && !target_audio) {
      Song *s;
      list_for_each_entry(s, &list, node) {
        if (s->cover_status == 0) {
          target_cover = s;
          target_cover->cover_status = 1;
          break;
        }
      }
    }
    pthread_mutex_unlock(&song_mutex);

    if (!target_cover && !target_audio) {
      usleep(50000); // Wait 50ms
      continue;
    }

    if (target_cover) {
      char cover_path[256];
      sprintf(cover_path, "/tmp/cover_%d.png", target_cover->id);
      if (getImage(target_cover->path, cover_path) == 0) {
        target_cover->cover_status = 2;
      } else {
        target_cover->cover_status = -1;
      }
    }

    if (target_audio) {
      char audio_path[256];
      sprintf(audio_path, "/tmp/audio_%d.mp3", target_audio->id);
      if (transcodeToMp3(target_audio->path, audio_path) == 0) {
        target_audio->audio_status = 2;
      } else {
        target_audio->audio_status = -1;
      }
    }
  }
  return NULL;
}

int getElapsedTime() {
  if (done_playing)
    return 0;
  if (playing && !paused)
    return (int)(GetTime() - start_time);
  else
    return (int)(pause_time - start_time);
}

void initComponents() {
  /*
  ARR(buttons, b_count);
  ARR(corners, b_count);
  ARR(circles, b_count);
  for (int i = 0; i < b_count; ++i) {
    ARR(corners[i], 4);
    ARR(circles[i], 4);
  }
  // buttons[0].header = strdup("Home");

  // buttons[0].header = strdup("Sync data");
  // buttons[1].header = strdup("Playlists");
  // buttons[2].header = strdup("Add playlist");
  // buttons[3].header = strdup("Exit");
  float b = 50, l = MeasureText("Add playlist", b_font);

  // for (int i = 1; i < b_count; ++i) {
  //   int n = MeasureText(buttons[i].header, b_font);
  //   if (n > l)
  //     l = n;
  // }
  l += 40;

  for (int i = 0; i < b_count; i++) {
    float y = 80 + gap + i * (b + gap);
    buttons[i].dim = (Rectangle){pad, y, l, b};

    corners[i][0] = (Rectangle){pad, y};
    corners[i][1] = (Rectangle){pad + l - b / 5, y};
    corners[i][2] = (Rectangle){pad + l - b / 5, y + 4 * b / 5};
    corners[i][3] = (Rectangle){pad, y + 4 * b / 5};

    circles[i][0] = (Circle){pad + b / 5, y + b / 5};
    circles[i][1] = (Circle){pad + l - b / 5, y + b / 5};
    circles[i][2] = (Circle){pad + l - b / 5, y + 4 * b / 5};
    circles[i][3] = (Circle){pad + b / 5, y + 4 * b / 5};

    for (int j = 0; j < 4; ++j) {
      corners[i][j].width = corners[i][j].height = b / 5;
      circles[i][j].r = b / 5;
    }
  }
  */

  float x = 50;
  float y = screen_height - 100;
  float e = screen_width - 390;

  play_button = (Rectangle){x + e / 2 - 30, y + 20, 60, 30};
  prev_btn = (Rectangle){play_button.x - 40, y + 20, 30, 30};
  next_btn = (Rectangle){play_button.x + play_button.width + 10, y + 20, 30, 30};
  progress_bar = (Rectangle){x, y, e, 6};
}

void drawButtons() {
  for (int i = 0; i < b_count; ++i) {
    DrawRectangleRec(buttons[i].dim, button_bg);
    for (int j = 0; j < 4; ++j) {
      DrawRectangleRec(corners[i][j], main_bg);
      DrawCircle(circles[i][j].x, circles[i][j].y, circles[i][j].r, button_bg);
    }
    // float x = buttons[i].dim.x + 20;
    // float y = buttons[i].dim.y + 15;
    // DrawTextEx(font, buttons[i].header, (Vector2){x, y}, fontSize, spacing, WHITE);
  }
}

void drawDetails(char *file) {
  float x = progress_bar.x;
  float y = progress_bar.y;
  float w = progress_bar.width;
  DrawRectangleRec(progress_bar, bar_bg);
  
  char *title = getFilename(file);
  Vector2 title_size = MeasureTextEx(font, title, font_size, spacing);
  float title_x = x;
  if (title_size.x > w) {
    float overflow = title_size.x - w;
    float speed = 40.0f;
    float cycle = overflow / speed * 2.0f + 2.0f;
    float t = fmod(GetTime(), cycle);
    float offset = 0;
    if (t < 1.0f) offset = 0;
    else if (t < 1.0f + overflow / speed) offset = (t - 1.0f) * speed;
    else if (t < 2.0f + overflow / speed) offset = overflow;
    else offset = overflow - (t - (2.0f + overflow / speed)) * speed;
    title_x -= offset;
  }
  
  BeginScissorMode(x, 0, w, screen_height);
  DrawTextEx(font, title, (Vector2){title_x, y - 50}, font_size, spacing, WHITE);
  EndScissorMode();
  int time_font_size = 24;
  Vector2 end_size = MeasureTextEx(font, end, time_font_size, spacing);
  DrawTextEx(font, end, (Vector2){w + x - end_size.x, y + 25}, time_font_size, spacing, WHITE);
  DrawRectangleRec(play_button, main_bg);
  Vector2 play_size = MeasureTextEx(font, playing ? "Pause" : "Play", font_size, spacing);
  DrawTextEx(font, playing ? "Pause" : "Play", (Vector2){play_button.x + (play_button.width - play_size.x)/2, play_button.y + (play_button.height - play_size.y)/2}, font_size, spacing, WHITE);
  
  Vector2 p1 = {prev_btn.x + 25, prev_btn.y + 5};
  Vector2 p2 = {prev_btn.x + 5, prev_btn.y + 15};
  Vector2 p3 = {prev_btn.x + 25, prev_btn.y + 25};
  DrawTriangle(p1, p2, p3, WHITE);

  Vector2 n1 = {next_btn.x + 5, next_btn.y + 5};
  Vector2 n2 = {next_btn.x + 5, next_btn.y + 25};
  Vector2 n3 = {next_btn.x + 25, next_btn.y + 15};
  DrawTriangle(n1, n2, n3, WHITE);

  Time t = {0};
  if (!done_playing)
    t = getTime(getElapsedTime());
  memset(buf, 0, 128);
  if (t.h)
    sprintf(buf, "%02d:%02d:%02d", t.h, t.m, t.s);
  else
    sprintf(buf, "%02d:%02d", t.m, t.s);
  DrawTextEx(font, buf, (Vector2){x, y + 25}, time_font_size, spacing, WHITE);
  DrawTexture(texture, x + (w - texture.width) / 2, 40, WHITE);

  volume_bar = (Rectangle){(w + x - end_size.x) - 130, y + 35, 100, 6};
  DrawTextEx(font, "Vol", (Vector2){volume_bar.x - 35, volume_bar.y - 8}, 22, spacing, WHITE);
  DrawRectangleRec(volume_bar, bar_bg);
  float vol_w = volume_bar.width * volume;
  DrawRectangleRec((Rectangle){volume_bar.x, volume_bar.y, vol_w, volume_bar.height}, button_bg);
  DrawCircle(volume_bar.x + vol_w, volume_bar.y + volume_bar.height / 2, 6, button_bg);

  Vector2 curr_size = MeasureTextEx(font, buf, time_font_size, spacing);
  shuffle_btn = (Rectangle){x + curr_size.x + 15, y + 24, 45, 24};
  repeat_btn = (Rectangle){shuffle_btn.x + 55, y + 24, 40, 24};

  DrawRectangleRec(shuffle_btn, shuffle_mode ? button_bg : main_bg);
  Vector2 shuf_size = MeasureTextEx(font, "Shuf", 22, spacing);
  DrawTextEx(font, "Shuf", (Vector2){shuffle_btn.x + (shuffle_btn.width - shuf_size.x)/2, shuffle_btn.y + (shuffle_btn.height - shuf_size.y)/2}, 22, spacing, WHITE);

  DrawRectangleRec(repeat_btn, repeat_mode ? button_bg : main_bg);
  Vector2 rep_size = MeasureTextEx(font, "Rep", 22, spacing);
  DrawTextEx(font, "Rep", (Vector2){repeat_btn.x + (repeat_btn.width - rep_size.x)/2, repeat_btn.y + (repeat_btn.height - rep_size.y)/2}, 22, spacing, WHITE);

  float p = is_dragging_time ? drag_time_ratio : (getElapsedTime() / getSec(end_time));
  if (p > 1.0f)
    p = 1.0f;
  progress_icon = (Circle){x + p * w, y + 2, 6};
  DrawCircle(progress_icon.x, progress_icon.y, progress_icon.r, WHITE);
}

void paintScreen() {
  /*
  Rectangle menu = {20, 20, 60, 60};
  DrawRectangleRec(menu, main_bg);
  DrawTextEx(font, "Menu", (Vector2){45, 35}, font_size, spacing, txt_fg);
  drawButtons();
  */
}

void freeResources() {
  /*
  for (int i = 0; i < b_count; ++i)
    freeArrs(buttons[i].header, corners[i], circles[i]);
  freeArrs(buttons, corners, circles, songs, codepoints);
  */
  freeArrs(songs, codepoints);
  corners = NULL, circles = NULL, buttons = NULL, songs = NULL, codepoints = NULL;
  remove("/tmp/cover.png");
  remove("/tmp/music.mp3");
}

int setupSongs(int argc, char ***argv) {
  if (song_setup_done)
    return 0;

  ARR(songs, argc - 1);

  for (int i = 0; i < argc - 1; ++i) {
    songs[i].path = (*argv)[i + 1];
    songs[i].id = i;
    songs[i].cover_status = 0;
    
    int len = strlen(songs[i].path);
    if (len >= 4 && strcasecmp(songs[i].path + len - 4, ".mp3") == 0) {
      songs[i].is_mp3 = 1;
      songs[i].audio_status = 2; // already ready!
    } else {
      songs[i].is_mp3 = 0;
      songs[i].audio_status = 0;
    }
    
    list_add_tail(&songs[i].node, &list);
  }

  song_setup_done = 1;
  return 0;
}

int setupScreen(Song *song) {
  if (!song)
    return 1;

  if (texture.id != 0) {
    UnloadTexture(texture);
    texture = (Texture2D){0};
  }

  char cover_path[256];
  sprintf(cover_path, "/tmp/cover_%d.png", song->id);
  
  if (song->cover_status == 2) {
    Image cover = LoadImage(cover_path);
    if (cover.data != NULL) {
      ImageResize(&cover, (screen_height - 200) * cover.width / cover.height, screen_height - 200);
      texture = LoadTextureFromImage(cover);
      UnloadImage(cover);
    }
  }

  char audio_path[256];
  if (song->is_mp3) {
    strcpy(audio_path, song->path);
  } else {
    sprintf(audio_path, "/tmp/audio_%d.mp3", song->id);
  }

  if (current_song != NULL) {
    StopMusicStream(music);
    UnloadMusicStream(music);
  }

  music = LoadMusicStream(audio_path);
  music.looping = 0;
  PlayMusicStream(music);
  done_playing = 0;
  SetMasterVolume(volume);
  SetMusicVolume(music, 1.0f);
  end_time = getTime(getAudioDuration(song->path));
  if (end_time.h)
    sprintf(end, "%02d:%02d:%02d", end_time.h, end_time.m, end_time.s);
  else
    sprintf(end, "%02d:%02d", end_time.m, end_time.s);
  start_time = GetTime();
  pause_time = 0;
  playing = 1;
  paused = 0;
  current_song = song;
  screen_setup_done = 1;
  return 0;
}

void setupFont() {
  int max_codepoints = 2000;
  codepoints = malloc(max_codepoints * sizeof(int));
  int total = 0;

  for (int i = 32; i <= 126; i++) {
    codepoints[total++] = i;
  }

  Song *s;
  list_for_each_entry(s, &list, node) {
    const char *text = s->path;
    int bytes = 0;
    while (*text != '\0') {
      int codepoint = GetCodepoint(text, &bytes);
      int found = 0;
      for (int j = 0; j < total; j++) {
        if (codepoints[j] == codepoint) {
          found = 1;
          break;
        }
      }
      if (!found && total < max_codepoints) {
        codepoints[total++] = codepoint;
      }
      text += bytes;
    }
  }

  char *font_path = "assets/NotoSansJP-Regular.otf";
  font = LoadFontEx(font_path, font_size, codepoints, total);
  if (font.texture.id == 0) {
    fprintf(stderr, "Failed to load font from %s\n", font_path);
  }
}

Song* getNextSong(Song *current) {
  if (repeat_mode) return current;
  if (shuffle_mode) {
    int count = 0; Song *s;
    list_for_each_entry(s, &list, node) count++;
    if (count <= 1) return current;
    int rand_idx = rand() % count;
    Song *selected = NULL; int i = 0;
    list_for_each_entry(s, &list, node) {
      if (i == rand_idx) { selected = s; break; }
      i++;
    }
    if (selected == current) {
      list_head *next = selected->node.next;
      if (next == &list) next = list.next;
      selected = list_entry(next, Song, node);
    }
    return selected;
  }
  list_head *next = current->node.next;
  if (next == &list) return NULL;
  return list_entry(next, Song, node);
}

Song* getPrevSong(Song *current) {
  if (repeat_mode) return current;
  if (shuffle_mode) {
    int count = 0; Song *s;
    list_for_each_entry(s, &list, node) count++;
    if (count <= 1) return current;
    int rand_idx = rand() % count;
    Song *selected = NULL; int i = 0;
    list_for_each_entry(s, &list, node) {
      if (i == rand_idx) { selected = s; break; }
      i++;
    }
    if (selected == current) {
      list_head *prev = selected->node.prev;
      if (prev == &list) prev = list.prev;
      selected = list_entry(prev, Song, node);
    }
    return selected;
  }
  list_head *prev = current->node.prev;
  if (prev == &list) prev = list.prev;
  return list_entry(prev, Song, node);
}

void playNext() {
  if (history_idx < history_count - 1) {
    history_idx++;
    pending_song = history[history_idx];
  } else {
    Song *s = getNextSong(current_song);
    if (!s) {
      playing = 0; paused = 0; done_playing = 1; return;
    }
    if (history_count == MAX_HISTORY) {
      memmove(history, history + 1, (MAX_HISTORY - 1) * sizeof(Song*));
      history_count--;
      history_idx--;
    }
    history[++history_idx] = s;
    history_count = history_idx + 1;
    pending_song = s;
  }
}

void playPrev() {
  if (history_idx > 0) {
    history_idx--;
    pending_song = history[history_idx];
  } else {
    Song *s = getPrevSong(current_song);
    if (!s) return;
    if (history_count == MAX_HISTORY) {
      history_count--;
    }
    memmove(history + 1, history, history_count * sizeof(Song*));
    history[0] = s;
    history_count++;
    history_idx = 0;
    pending_song = s;
  }
}

int main(int argc, char **argv) {
  srand(time(NULL));
  setlocale(LC_ALL, "");
  // SetConfigFlags(FLAG_WINDOW_MAXIMIZED | FLAG_WINDOW_RESIZABLE);
  InitWindow(screen_width, screen_height, "Music Player");
  SetAudioStreamBufferSizeDefault(10000);
  initComponents();
  if (argc >= 2) {
    if (setupSongs(argc, &argv))
      exit(1);
  }
  setupFont();

  InitAudioDevice();

  for (int i = 0; i < NUM_THREADS; i++) {
    pthread_create(&workers[i], NULL, process_songs, NULL);
  }

  // Force GLFW to load the system cursor (fixes invisible cursor bugs on Wayland/Linux)
  SetMouseCursor(MOUSE_CURSOR_ARROW);
  
  int is_dragging_volume = 0;

  while (!WindowShouldClose()) {
    mouse = GetMousePosition();

    if (argc < 2) {
      BeginDrawing();
      ClearBackground(main_bg);
      const char *msg = "Usage: mp <filename>";
      float x = screen_width / 2 - MeasureText(msg, 40);
      float y = screen_height / 2;
      DrawTextEx(font, msg, (Vector2){x, y}, 40, spacing, WHITE);
      Rectangle exit = {x + 150, y + 80, 95, 50};
      DrawRectangleRec(exit, main_bg);
      DrawTextEx(font, "EXIT", (Vector2){x + 160, y + 90}, 30, spacing, WHITE);
      if (CheckCollisionPointRec(mouse, exit) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsKeyPressed(KEY_SPACE)) {
        freeResources();
        CloseWindow();
        return 0;
      }
      EndDrawing();
      continue;
    }

    if (argc >= 2 && list_empty(&list))
      return 0;

    if (!current_song && !pending_song && argc >= 2) {
      pending_song = list_entry(list.next, Song, node);
      history[0] = pending_song;
      history_count = 1;
      history_idx = 0;
    }

    if (pending_song) {
      if (current_song && playing && !paused) {
        PauseMusicStream(music);
        paused = 1;
      }
      if ((pending_song->audio_status == 2 || pending_song->audio_status == -1) && 
          (pending_song->cover_status == 2 || pending_song->cover_status == -1)) {
        setupScreen(pending_song);
        pending_song = NULL;
        paused = 0;
        playing = 1;
      }
    }

    if (current_song) {
      UpdateMusicStream(music);
    }

    if (current_song && !IsMusicStreamPlaying(music) && playing) {
      playNext();
      continue;
    }

    BeginDrawing();
    ClearBackground(main_bg);

    paintScreen();
    if (current_song) {
      drawDetails(current_song->path);
    }

    // Draw Playlist
    float playlist_w = 300;
    float playlist_x = screen_width - playlist_w - 20;
    float playlist_y = 20;
    float playlist_h = screen_height - 40;

    DrawRectangleRec((Rectangle){playlist_x, playlist_y, playlist_w, playlist_h}, bar_bg);

    float scroll_speed = GetMouseWheelMove() * 30.0f;
    if (scroll_speed != 0 && CheckCollisionPointRec(mouse, (Rectangle){playlist_x, playlist_y, playlist_w, playlist_h})) {
      scroll_offset += scroll_speed;
    }
    if (scroll_offset > 0)
      scroll_offset = 0;

    BeginScissorMode(playlist_x, playlist_y, playlist_w, playlist_h);
    int item_y = playlist_y + 10 + scroll_offset;
    Song *s;
    list_for_each_entry(s, &list, node) {
      char *fname = getFilename(s->path);
      Rectangle item_rect = {playlist_x + 10, item_y, playlist_w - 20, 40};

      Color item_bg = (s == current_song) ? button_bg : main_bg;
      if (CheckCollisionPointRec(mouse, item_rect)) {
        item_bg = (Color){0x40, 0x40, 0x40, 0xFF}; // Hover effect
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && s != current_song) {
          pending_song = s;
          if (history_count == MAX_HISTORY) {
            memmove(history, history + 1, (MAX_HISTORY - 1) * sizeof(Song*));
            history_idx--;
          }
          history[++history_idx] = s;
          history_count = history_idx + 1;
        }
      }
      DrawRectangleRec(item_rect, item_bg);
      
      float max_w = item_rect.width - 20;
      Vector2 tsize = MeasureTextEx(font, fname, 24, spacing);
      float text_x = item_rect.x + 10;
      
      if (tsize.x > max_w && (CheckCollisionPointRec(mouse, item_rect) || s == current_song)) {
        float overflow = tsize.x - max_w;
        float speed = 40.0f;
        float cycle = overflow / speed * 2.0f + 2.0f;
        float t = fmod(GetTime(), cycle);
        float offset = 0;
        if (t < 1.0f) offset = 0;
        else if (t < 1.0f + overflow / speed) offset = (t - 1.0f) * speed;
        else if (t < 2.0f + overflow / speed) offset = overflow;
        else offset = overflow - (t - (2.0f + overflow / speed)) * speed;
        text_x -= offset;
      }

      BeginScissorMode(item_rect.x + 10, playlist_y, item_rect.width - 20, playlist_h);
      DrawTextEx(font, fname, (Vector2){text_x, item_rect.y + 10}, 24, spacing, txt_fg);
      BeginScissorMode(playlist_x, playlist_y, playlist_w, playlist_h);
      
      item_y += 50;
    }
    EndScissorMode();

    if (CheckCollisionPointRec(mouse, play_button) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsKeyPressed(KEY_SPACE)) {
      if (!done_playing) {
        if (playing) {
          PauseMusicStream(music);
          pause_time = GetTime();
          playing = 0;
          paused = 1;
        } else if (paused) {
          ResumeMusicStream(music);
          start_time += (GetTime() - pause_time);
          playing = 1;
          paused = 0;
        }
      }
    }
    Rectangle expanded_prog_bar = {progress_bar.x, progress_bar.y - 10, progress_bar.width, progress_bar.height + 20};
    if (CheckCollisionPointRec(mouse, expanded_prog_bar) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
      is_dragging_time = 1;
    }
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && is_dragging_time) {
      is_dragging_time = 0;
      float new_time = getSec(end_time) * drag_time_ratio;
      if (done_playing) {
        PlayMusicStream(music);
        done_playing = 0;
      } else if (paused) {
        ResumeMusicStream(music);
      }
      SeekMusicStream(music, new_time);
      start_time = GetTime() - new_time;
      paused = 0;
      playing = 1;
    }

    if (is_dragging_time) {
      drag_time_ratio = (mouse.x - progress_bar.x) / progress_bar.width;
      if (drag_time_ratio < 0) drag_time_ratio = 0;
      if (drag_time_ratio > 1) drag_time_ratio = 1;
    }

    if (CheckCollisionPointRec(mouse, shuffle_btn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
      shuffle_mode = !shuffle_mode;
      history_count = history_idx + 1;
    }
    if (CheckCollisionPointRec(mouse, repeat_btn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
      repeat_mode = !repeat_mode;
      history_count = history_idx + 1;
    }

    if (CheckCollisionPointRec(mouse, prev_btn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
      playPrev();
    }
    if (CheckCollisionPointRec(mouse, next_btn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
      playNext();
    }

    Rectangle expanded_vol_bar = {volume_bar.x, volume_bar.y - 10, volume_bar.width, volume_bar.height + 20};
    if (CheckCollisionPointRec(mouse, expanded_vol_bar) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
      is_dragging_volume = 1;
    }
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
      is_dragging_volume = 0;
    }
    
    if (is_dragging_volume) {
      volume = (mouse.x - volume_bar.x) / volume_bar.width;
      if (volume < 0)
        volume = 0;
      if (volume > 1)
        volume = 1;
      SetMasterVolume(volume);
    }

    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_LEFT)) {
      float new_time = getElapsedTime() + (IsKeyPressed(KEY_RIGHT) ? 10.0f : -10.0f);
      if (new_time < 0)
        new_time = 0;
      if (new_time > getSec(end_time))
        new_time = getSec(end_time);

      if (done_playing) {
        PlayMusicStream(music);
        done_playing = 0;
      } else if (paused) {
        ResumeMusicStream(music);
      }
      SeekMusicStream(music, new_time);
      start_time = GetTime() - new_time;
      paused = 0;
      playing = 1;
    }

    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN)) {
      volume += IsKeyPressed(KEY_UP) ? 0.05f : -0.05f;
      if (volume < 0.0f)
        volume = 0.0f;
      if (volume > 1.0f)
        volume = 1.0f;
      SetMasterVolume(volume);
    }

    EndDrawing();
  }
  CloseAudioDevice();
  freeResources();
  return 0;
}
