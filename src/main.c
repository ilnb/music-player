#include <raylib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  int h, m, s;
} Time;

Time get_time(int _t) {
  Time t;
  t.s = _t % 60;
  _t /= 60;
  t.m = _t % 60;
  _t /= 60;
  t.h = _t;
  return t;
}

float get_sec(Time t) { return t.h * 3600 + t.m * 60 + t.s; }

typedef struct {
  bool l;
  bool r;
} ClickState;

typedef struct {
  int x;
  int y;
  int r;
} Circle;

typedef struct {
  Rectangle dim;
  char *header;
} Button;

const float screen_width = 1280;
const float screen_height = 720;
int b_count = 4;
// int b_count = 2;
Button *buttons = NULL;
Circle **circles;
Circle progress_icon = {0};
Rectangle **corners;
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
char buf[128];
char end[15] = {0};

float start_time = 0;
float pause_time = 0;
Time end_time;
bool playing = true;
bool paused = false;
bool done_playing = false;

int getElapsedTime() {
  if (playing && !paused)
    return (int)(GetTime() - start_time);
  else
    return (int)(pause_time - start_time);
}

char *get_filename(char *path) {
  char *s = path + strlen(path);
  while (*s != '/' && s != path)
    --s;
  // if (s == full_path) return s;
  // char *t = s;
  // memset(full_path, 0, s - full_path);
  // strcat(full_path, s);
  return *s == '/' ? s + 1 : s;
}

void InitComponents() {
  buttons = calloc(b_count, sizeof(Button));
  corners = calloc(b_count, sizeof(Rectangle *));
  circles = calloc(b_count, sizeof(Circle *));
  for (int i = 0; i < b_count; ++i) {
    corners[i] = calloc(4, sizeof(Button));
    circles[i] = calloc(4, sizeof(Circle));
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

  play_button =
      (Rectangle){x + e / 2 - 5, y + 25, MeasureText("Pause", 20) + 10, 30};
  progress_bar = (Rectangle){x, y, e, 2};
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
    // DrawText(buttons[i].header, x, y, 20, WHITE);
  }
}

void drawDetails(char *file) {
  float x = progress_bar.x;
  float y = progress_bar.y;
  float e = progress_bar.width;
  DrawRectangleRec(progress_bar, bar_bg);
  DrawText(get_filename(file), x, y - 50, 20, WHITE);
  DrawText(end, e + x - MeasureText(end, 15), y + 20, 20, WHITE);
  DrawRectangleRec(play_button, main_bg);
  DrawText(playing ? "Pause" : "Play", play_button.x + 5, play_button.y + 5, 20,
           WHITE);
  Time t = {0};
  if (!done_playing)
    t = get_time(getElapsedTime());
  memset(buf, 0, 128);
  if (t.h)
    sprintf(buf, "%02d:%02d:%02d", t.h, t.m, t.s);
  else
    sprintf(buf, "%02d:%02d", t.m, t.s);
  DrawText(buf, x, y + 25, 20, WHITE);
  DrawTexture(texture, x + (e - x - texture.width) - 5, 40, WHITE);
  float p = getElapsedTime() / get_sec(end_time);
  if (p > 1.0f)
    p = 1.0f;
  progress_icon = (Circle){x + p * (e - x), y, 6};
  DrawCircle(progress_icon.x, progress_icon.y, progress_icon.r, WHITE);
}

void paintScreen() {
  const int width = GetScreenWidth();
  const int height = GetScreenHeight();
  Rectangle menu = {20, 20, 60, 60};
  DrawRectangleRec(menu, main_bg);
  DrawText("Menu", 45, 35, 20, txt_fg);
  drawButtons();
}

void freeResources() {
  for (int i = 0; i < b_count; ++i) {
    free(buttons[i].header);
    free(corners[i]);
    free(circles[i]);
  }
  free(buttons);
  free(corners);
  free(circles);
  corners = NULL;
  circles = NULL;
  buttons = NULL;
}

int main(int argc, char **argv) {
  // SetConfigFlags(FLAG_WINDOW_MAXIMIZED | FLAG_WINDOW_RESIZABLE);
  InitWindow(screen_width, screen_height, "Music Player");
  SetAudioStreamBufferSizeDefault(10000);
  InitComponents();
  if (argc == 2) {
    memset(buf, 0, 128);
    sprintf(buf, "ffmpeg -y -i %s -an -vcodec copy /tmp/cover.jpg", argv[1]);
    system(buf);
    Image cover = LoadImage("/tmp/cover.jpg");
    ImageResize(&cover, (screen_height - 200) * cover.width / cover.height,
                screen_height - 200);
    texture = LoadTextureFromImage(cover);
    UnloadImage(cover);
    cover = (Image){0};
    InitAudioDevice();
    memset(buf, 0, 128);
    sprintf(buf,
            "ffmpeg -y -i %s -ar 48000 -ac 2 -codec:a libmp3lame -qscale:a 2 "
            "/tmp/music.mp3",
            argv[1]);
    system(buf);
    music = LoadMusicStream("/tmp/music.mp3");
    music.looping = false;
    PlayMusicStream(music);
    SetMusicVolume(music, GetMasterVolume());
    memset(buf, 0, 128);
    sprintf(buf,
            "ffprobe -v error -show_entries format=duration -of "
            "default=noprint_wrappers=1:nokey=1 %s",
            argv[1]);
    FILE *fp = popen(buf, "r");
    memset(buf, 0, 128);
    fgets(buf, sizeof(buf), fp);
    pclose(fp);
    end_time = get_time((int)strtof(buf, NULL));
    if (end_time.h)
      sprintf(end, "%02d:%02d:%02d", end_time.h, end_time.m, end_time.s);
    else
      sprintf(end, "%02d:%02d", end_time.m, end_time.s);
  }

  while (!WindowShouldClose()) {
    mouse = GetMousePosition();
    BeginDrawing();
    ClearBackground(main_bg);
    if (argc == 1 || argc > 2) {
      const char *msg = "Usage: mp <filename>";
      float x = screen_width / 2 - MeasureText(msg, 40);
      float y = screen_height / 2;
      DrawText(msg, x, y, 40, WHITE);
      Rectangle exit = {x + 150, y + 80, 95, 50};
      DrawRectangleRec(exit, main_bg);
      DrawText("EXIT", x + 160, y + 90, 30, WHITE);
      if (CheckCollisionPointRec(mouse, exit) &&
              IsMouseButtonPressed(MOUSE_BUTTON_LEFT) ||
          IsKeyPressed(KEY_SPACE)) {
        freeResources();
        CloseWindow();
        return 0;
      }
      EndDrawing();
      continue;
    }
    UpdateMusicStream(music);
    if (!IsMusicStreamPlaying(music) && playing) {
      playing = false;
      paused = false;
      done_playing = true;
    }
    // paintScreen();
    drawDetails(argv[1]);
    if (CheckCollisionPointRec(mouse, play_button) &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON) ||
        IsKeyPressed(KEY_SPACE)) {
      if (!done_playing) {
        if (playing) {
          PauseMusicStream(music);
          pause_time = GetTime();
          playing = false;
          paused = true;
        } else if (paused) {
          ResumeMusicStream(music);
          start_time += (GetTime() - pause_time);
          playing = true;
          paused = false;
        }
      } else {
        StopMusicStream(music);
        paused = true;
        start_time = 0;
        pause_time = 0;
      }
    }
    if (CheckCollisionPointRec(mouse, progress_bar) &&
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      float p = (mouse.x - progress_bar.x) / progress_bar.width;
      float new_time = get_sec(end_time) * p;
      // SeekMusicStream(music, new_time);
      SeekMusicStream(music, 0);
      start_time = GetTime() - new_time;
      paused = false;
      playing = true;
    }
    EndDrawing();
  }
  UnloadMusicStream(music);
  CloseAudioDevice();
  system("rm /tmp/cover.jpg");
  system("rm /tmp/music.mp3");
  freeResources();
  return 0;
}
