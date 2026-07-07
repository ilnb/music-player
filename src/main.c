#include "list.h"
#include "utils.h"
#include <raylib.h>
#include <string.h>

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
int fontSize = 25;
int spacing = 2;
Rectangle play_button = {0};
Rectangle progress_bar = {0};
Music music;
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

int getElapsedTime() {
  if (playing && !paused)
    return (int)(GetTime() - start_time);
  else
    return (int)(pause_time - start_time);
}

void initComponents() {
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
  float x = buttons[0].dim.x + buttons[0].dim.width + 50;
  float y = screen_height - 100;
  float e = screen_width - 100 - x;

  play_button = (Rectangle){x + e / 2 - 5, y + 25, MeasureText("Pause", fontSize) + 10, 30};
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
  DrawTextEx(font, getFilename(file), (Vector2){x, y - 50}, fontSize, spacing, WHITE);
  DrawTextEx(font, end, (Vector2){w + x - MeasureText(end, 15), y + fontSize}, fontSize, spacing,
             WHITE);
  DrawRectangleRec(play_button, main_bg);
  DrawTextEx(font, playing ? "Pause" : "Play", (Vector2){play_button.x + 5, play_button.y + 5},
             fontSize, spacing, WHITE);
  Time t = {0};
  if (!done_playing)
    t = getTime(getElapsedTime());
  memset(buf, 0, 128);
  if (t.h)
    sprintf(buf, "%02d:%02d:%02d", t.h, t.m, t.s);
  else
    sprintf(buf, "%02d:%02d", t.m, t.s);
  DrawTextEx(font, buf, (Vector2){x, y + 25}, fontSize, spacing, WHITE);
  DrawTexture(texture, x + (w - x - texture.width) - 5, 40, WHITE);
  float p = getElapsedTime() / getSec(end_time);
  if (p > 1.0f)
    p = 1.0f;
  progress_icon = (Circle){x + p * w, y + 2, 6};
  DrawCircle(progress_icon.x, progress_icon.y, progress_icon.r, WHITE);
}

void paintScreen() {
  Rectangle menu = {20, 20, 60, 60};
  DrawRectangleRec(menu, main_bg);
  DrawTextEx(font, "Menu", (Vector2){45, 35}, fontSize, spacing, txt_fg);
  drawButtons();
}

void freeResources() {
  for (int i = 0; i < b_count; ++i)
    freeArrs(buttons[i].header, corners[i], circles[i]);
  freeArrs(buttons, corners, circles, songs);
  corners = NULL, circles = NULL, buttons = NULL, songs = NULL;
  remove("/tmp/cover.jpg");
  remove("/tmp/music.mp3");
}

int setupSongs(int argc, char ***argv) {
  if (song_setup_done)
    return 0;

  ARR(songs, argc - 1);

  for (int i = 0; i < argc - 1; ++i) {
    songs[i].path = (*argv)[i + 1];
    list_add_tail(&songs[i].node, &list);
  }

  song_setup_done = 1;
  return 0;
}

int setupScreen() {
  if (screen_setup_done)
    return 0;
  memset(buf, 0, 128);
  Song *song = list_entry(list.next, Song, node);
  const char *file_path = song->path;
  remove("/tmp/cover.jpg");
  if (getImage(file_path, "/tmp/cover.jpg")) {
    fprintf(stderr, "Not able to extract cover image.\n");
    return 1;
  }
  Image cover = LoadImage("/tmp/cover.jpg");
  ImageResize(&cover, (screen_height - 200) * cover.width / cover.height, screen_height - 200);
  texture = LoadTextureFromImage(cover);
  UnloadImage(cover);
  cover = (Image){0};
  InitAudioDevice();
  memset(buf, 0, 128);
  const char *dest = "/tmp/music.mp3";
  remove(dest);
  if (transcodeToMp3(file_path, dest) < 0) {
    fprintf(stderr, "Failed to transcode %s to mp3\n", file_path);
  }
  music = LoadMusicStream("/tmp/music.mp3");
  music.looping = 0;
  PlayMusicStream(music);
  done_playing = 0;
  SetMusicVolume(music, GetMasterVolume());
  end_time = getTime(getAudioDuration(song->path));
  if (end_time.h)
    sprintf(end, "%02d:%02d:%02d", end_time.h, end_time.m, end_time.s);
  else
    sprintf(end, "%02d:%02d", end_time.m, end_time.s);
  start_time = GetTime();
  pause_time = 0;
  playing = 1;
  screen_setup_done = 1;
  return 0;
}

int main(int argc, char **argv) {
  // SetConfigFlags(FLAG_WINDOW_MAXIMIZED | FLAG_WINDOW_RESIZABLE);
  InitWindow(screen_width, screen_height, "Music Player");
  SetAudioStreamBufferSizeDefault(10000);
  initComponents();
  font = LoadFont("/usr/share/fonts/TTF/JetBrainsMono-Regular.ttf");

  while (!WindowShouldClose()) {
    mouse = GetMousePosition();
    BeginDrawing();
    ClearBackground(main_bg);
    if (argc < 2) {
      const char *msg = "Usage: mp <filename>";
      float x = screen_width / 2 - MeasureText(msg, 40);
      float y = screen_height / 2;
      DrawTextEx(font, msg, (Vector2){x, y}, 40, spacing, WHITE);
      Rectangle exit = {x + 150, y + 80, 95, 50};
      DrawRectangleRec(exit, main_bg);
      DrawTextEx(font, "EXIT", (Vector2){x + 160, y + 90}, 30, spacing, WHITE);
      if (CheckCollisionPointRec(mouse, exit) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) ||
          IsKeyPressed(KEY_SPACE)) {
        freeResources();
        CloseWindow();
        return 0;
      }
      EndDrawing();
      continue;
    }
    if (setupSongs(argc, &argv))
      exit(1);
    if (list_empty(&list))
      return 0;
    if (setupScreen())
      exit(1);
    UpdateMusicStream(music);
    if (!IsMusicStreamPlaying(music) && playing) {
      StopMusicStream(music);
      UnloadMusicStream(music);
      list_del(list.next);
      UnloadTexture(texture);
      if (list_empty(&list)) {
        playing = 0;
        paused = 0;
        done_playing = 1;
      } else {
        done_playing = 0;
        playing = 1;
        paused = 0;
        screen_setup_done = 0;
      }
      continue;
    }
    paintScreen();
    if (!list_empty(&list))
      drawDetails(list_entry(list.next, Song, node)->path);
    if (CheckCollisionPointRec(mouse, play_button) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) ||
        IsKeyPressed(KEY_SPACE)) {
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
    if (CheckCollisionPointRec(mouse, progress_bar) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      float p = (mouse.x - progress_bar.x) / progress_bar.width;
      float new_time = getSec(end_time) * p;
      SeekMusicStream(music, new_time);
      start_time = GetTime() - new_time;
      paused = 0;
      playing = 1;
    }
    EndDrawing();
  }
  CloseAudioDevice();
  freeResources();
  return 0;
}
