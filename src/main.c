#include <curses.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <locale.h>

#define SMOOTH_VALUE 0.8

#include "pulse.h"
#include "fft.h"

void quit()
{
  endwin();
}

void color_init(int *color) {
  
  // Check if terminal supports colors, if not disable color switching
  if (has_colors() == 0) {
    return;
  }
  *color = 1;

  use_default_colors();
  start_color();

  init_pair(1, COLOR_WHITE, -1);  
  init_pair(2, COLOR_RED, -1);    
  init_pair(3, COLOR_BLUE, -1);   
  init_pair(4, COLOR_GREEN, -1); 
  init_pair(5, COLOR_MAGENTA, -1);
  init_pair(6, COLOR_CYAN, -1);   
  init_pair(7, COLOR_YELLOW, -1);  

  attron(COLOR_PAIR(1));
}

void color_switch(WINDOW *win, int *color) {

  if (*color == 0) {
    // Color switching disabled
    return;
  }

  wattroff(win, COLOR_PAIR(*color));

  if (*color == 7) {
    *color = 1;
  } else {
    (*color)++;
  }

  wattron(win, COLOR_PAIR(*color));
}

void draw_fft(WINDOW *win, double *fft_bins) {
  
  // Add max to visualize volume changes on same frequency
  const double max_db = 80.0;

  // Get maximum terminal size to limit graph
  int max_x, max_y;
  getmaxyx(win, max_y, max_x);
  
  // Limit the bars to terminal width
  int num_bins = FFT_BUF_SIZE / 2;
  if (num_bins>max_x) {
    num_bins = max_x;
  }

  werase(win);

  for (int i = 0; i < num_bins; i++) {
    double db = fft_bins[i];

    // Normalize to terminal height
    int bar_height = (int)((db / max_db) * (max_y - 1));

    // Draw char to height, from bottom to top
    for (int j = 0; j < bar_height; j++) {
      mvwaddwstr(win, max_y - 1 - j, i, L"█");
    }
  }

  wrefresh(win);
}

void draw_basic_bars(WINDOW *win, double *fft_bins) {
  const int band_ranges[7][2] = {
      {0, 1},    // Sub Bass: 20–60 Hz
      {1, 3},    // Bass: 60–250 Hz
      {3, 5},    // Low Midrange: 250–500 Hz
      {5, 23},   // Midrange: 500–2000 Hz
      {23, 46},  // Upper Midrange: 2000–4000 Hz
      {46, 69},  // Presence: 4000–6000 Hz
      {69, 232}  // Brilliance: 6000–20000 Hz
  };

  int max_x, max_y;
  getmaxyx(win, max_y, max_x);

  double band_values[7] = {0};

  // average db for each band
  for (int b = 0; b < 7; b++) {
      double sum = 0;
      int count = 0;
      for (int i = band_ranges[b][0]; i <= band_ranges[b][1]; i++) {
          sum += fft_bins[i];
          count++;
      }
      band_values[b] = count > 0 ? sum / count : 0;
  }

  // ToDo: find good value
  const double max_db = 80.0;

  int offset_x = (max_x / 2) - 30;

  werase(win);

  // Draw each band as a vertical bar
  for (int i = 0; i < 7; i++) {
      int height = (int)((band_values[i] / max_db) * (max_y - 1));
      for (int j = 0; j < height; j++) {
          mvwaddwstr(win, max_y - 2 - j, offset_x + (i * 10) - 3, L"███████");
      }
  }

  wrefresh(win);
}

void draw_colored_bars(WINDOW *win, double *fft_bins) {
  const int band_ranges[7][2] = {
      {0, 1},    // Sub Bass: 20–60 Hz
      {1, 3},    // Bass: 60–250 Hz
      {3, 5},    // Low Midrange: 250–500 Hz
      {5, 23},   // Midrange: 500–2000 Hz
      {23, 46},  // Upper Midrange: 2000–4000 Hz
      {46, 69},  // Presence: 4000–6000 Hz
      {69, 232}  // Brilliance: 6000–20000 Hz
  };

  int max_x, max_y;
  getmaxyx(win, max_y, max_x);

  double band_values[7] = {0};

  // average db for each band
  for (int b = 0; b < 7; b++) {
      double sum = 0;
      int count = 0;
      for (int i = band_ranges[b][0]; i <= band_ranges[b][1]; i++) {
          sum += fft_bins[i];
          count++;
      }
      band_values[b] = count > 0 ? sum / count : 0;
  }

  // ToDo: find good value
  const double max_db = 80.0;

  int offset_x = (max_x / 2) - 30;
  int height_color = 0;

  werase(win);

  // Draw each band as a vertical bar
  for (int i = 0; i < 7; i++) {
      int height = (int)((band_values[i] / max_db) * (max_y - 1));
      for (int j = 0; j < height; j++) {
        if (j >= 0 && j <= 30) {
          height_color = 4;
        } else if (j > 30 && j <= 40) {
          height_color = 7;
        } else if (j > 40) {
          height_color = 2;
        }
          wattron(win, COLOR_PAIR(height_color));
          mvwaddwstr(win, max_y - 2 - j, offset_x + (i * 10) - 3, L"▄▄▄▄▄▄▄");
          wattroff(win, COLOR_PAIR(height_color));
      }
  }

  wrefresh(win);
}

int main(void)
{
  // Unicode init
  setlocale(LC_ALL, "");

  // Ncurses init
  initscr();
  atexit(quit);
  nodelay(stdscr, TRUE);
  noecho();
  curs_set(FALSE);

  // Color, Style init
  int color = 0;
  int style = 0;
  color_init(&color);

  // Pulseaudio, FFTW init
  pa_init();
  fft_init();

  // New window to draw to
  WINDOW *win = newwin(0, 0, 0, 0);
  keypad(win, TRUE);

  // FFTW input/output buffers
  double fft_input[FFT_BUF_SIZE];
  double fft_bins[FFT_BUF_SIZE/2];
  double smoothed_bins[FFT_BUF_SIZE/2] = {0}; // Initialize to 0 for first call of smooth_fft

  while(1) {
    if (is_fft_ready()) {
      get_pa_fft_buf(fft_input);
      compute_fft(fft_input, fft_bins);
      smooth_fft(fft_bins, smoothed_bins, SMOOTH_VALUE);

      // Switch between styles
      switch(style) {
        case 0:
          draw_fft(win, smoothed_bins);  // Draw FFT
          break;
        case 1:
          draw_basic_bars(win, smoothed_bins);  // Draw basic bars
          break;
        case 2:
          draw_colored_bars(win, smoothed_bins);  // Draw colored bars
          break;
      }
    }
  
    int ch = getch();

    // Quit SoundvisualiCer
    if (ch == 'q' || ch == 'Q') {
      break;
    }

    // Change color
    if (ch == 'c' || ch == 'C') {
      color_switch(win, &color);
    }

    // ToDo: Change when there are more than two different styles
    if(ch == 'n' || ch == 'N' || ch == 'p' || ch == 'P') {
      if(style == 2) {
        style = 0;
      } else {
        style++;
      }
    }

    // Sleep to limit framerate
    usleep(13000);
  }

  // Free memory using fftw
  destroy_fft();

  return 0;
}
