#ifndef UTILS_H
#define UTILS_H

#define _WIN32_WINNT 0x0600
#pragma comment(lib, "ws2_32")
#include <winsock2.h>

#include <future>
#include <fstream>
#include <iostream>
#include <string>
#include <algorithm>
#include <shlwapi.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>
#include <conio.h>
#include <windows.h>
#include <tlhelp32.h>
#include <math.h>

#define CURL_STATICLIB
using namespace std;

HANDLE hstdout = GetStdHandle(STD_OUTPUT_HANDLE);

bool str_find(string s, string str, size_t i = 0) {
  if (s.find(str, i) != string::npos)
    return 1;

  return 0;
}

string erase_find(string s, string str) {
  size_t f = s.find(str);
  if (f != string::npos)
    s.erase(0, f + 1);
  return s;
}

string erase_find_found_to_end(string s, string str) {
  size_t f = s.find(str);
  if (f != string::npos)
    s.erase(f, s.length() - f);

  return s;
}

COORD GetConsoleCursorPosition(HANDLE hConsoleOutput) {
  CONSOLE_SCREEN_BUFFER_INFO cbsi;
  if (GetConsoleScreenBufferInfo(hConsoleOutput, &cbsi)) {
    return cbsi.dwCursorPosition;
  }
  else {
    COORD invalid = {0, 0};
    return invalid;
  }
}

int current_console_cursor(int &x, int &y, int set = 0) {
  COORD c; // = {a,b};
  if (!set) {
    c = GetConsoleCursorPosition(hstdout);
    x = c.Y;
    y = c.X;
    return 1;
  }
  else {
    c.X = y;
    c.Y = x;
    bool b = SetConsoleCursorPosition(hstdout, c);
    return b;
  }
}

void setConsoleColor(WORD c) {
  SetConsoleTextAttribute(hstdout, c);
}
string add_gaps(string s, int n = 0, int front = 0, string gap = " ") {
  int gaps = n - s.length();
  for (int i = 0; i < gaps; i++) {
    if (front)
      s = gap + s;
    else
      s += gap;
  }
  return s;
}

template <typename T>
string conv2str(T var) {
  stringstream ss;
  ss << var;
  return ss.str();
}

float float_precision(float f, int precision = 2) {
  stringstream tmp;
  tmp << setprecision(precision) << fixed << f;
  return stof(tmp.str());
}

double double_precision(double f, int precision = 2) {
  stringstream tmp;
  tmp << setprecision(precision) << fixed << f;
  return stod(tmp.str());
}

struct timer__ {
  chrono::system_clock::time_point starting, ending;
  size_t reset_interval;
  timer__(size_t reset_interval_ = 0) {
    if (reset_interval_ > 0)
      reset_interval = reset_interval_;
    starting = chrono::system_clock::now();
  }

  void start() {
    starting = chrono::system_clock::now();
  }

  void stop() {
    ending = chrono::system_clock::now();
  }

  bool interval_elapsed(double *seconds) {
    auto current = chrono::system_clock::now();
    double elapsed_seconds = (current - starting).count() / (double)pow(10, 9);
    if (elapsed_seconds >= reset_interval) {
      starting = current;
      if (seconds)
        *seconds = elapsed_seconds;
      return true;
    }
    return false;
  }

  double seconds(int current = 1, int reset = 0) {
    if (current)
      ending = chrono::system_clock::now();

    double elapsed_seconds = (ending - starting).count()/(double)pow(10, 9);
    if (reset || (elapsed_seconds > reset_interval && reset_interval))
      starting = ending;

    return elapsed_seconds;
  }
};

void console_cursor(bool show) {
  CONSOLE_CURSOR_INFO cursorInfo;
  GetConsoleCursorInfo(hstdout, &cursorInfo);
  cursorInfo.bVisible = show;
  SetConsoleCursorInfo(hstdout, &cursorInfo);
}

size_t inline MB2B(float size) {
  return 1000 * 1000 * size;
}

bool exists(string s) {
  if (s == "")
    return 0;
  return PathFileExistsA(s.c_str());
}

string del_ex(string in) {
  string s = in;
  size_t f = s.rfind(".");
  if (f == string::npos)
    return s;

  s.erase(f, s.length() - f);
  return s;
}

string ext_ex(string in, int dot = 1) {
  string s = in;
  size_t f = s.rfind(".");
  if (f == string::npos)
    return "";

  if (dot)
    s.erase(0, f);
  else
    s.erase(0, f + 1);
  return s;
}

string out__file(string s, string app) {
  return del_ex(s) + app + ext_ex(s);
}

string name_assigner(string s) {
  if (!exists(s))
    return s;

  int i = 1;
  while (exists(out__file(s, "_" + to_string(i))))
    i++;
  return out__file(s, "_" + to_string(i));
}

string replace_string_all(string s, string old, string new_s, int total = 0) {
  size_t f = s.find(old);
  while (f != string::npos) {
    s.replace(f, old.length(), new_s);
    if (!total)
      f = s.find(old, f + new_s.length());
    else
      f = s.find(old);
  }
  return s;
}

string validate_path(string s) {
  // Invalid chars <>:"/\|?*
  // integer value 127
  // Integer value 0 to 32, -256 to -224

  if (s.length() == 0)
    return "";

  s = replace_string_all(s, "\n", "_");
  s = replace_string_all(s, "\r", "_");

  int len = s.length();
  vector<int> arr = {-256, -255, -254, -253, -252, -251, -250, -249, -248, -247, -246, -245, -244, -243,
  -242, -241, -240, -239, -238, -237, -236, -235, -234, -233, -232, -231, -230, -229,
  -228, -227, -226, -225, -224, -222, -214, -209, -198, -196, -194, -193, -164, -132,
  -129, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
  24, 25, 26, 27, 28, 29, 30, 31, 34, 42, 47, 58, 60, 62, 63, 124, 127};

  // char chars[] = {'<','>',':','"','/','\\','|','?','*'};
  for (int i = 0; i < len; i++)
    if (binary_search(arr.begin(), arr.end(), (int)s[i])) {
      if (s[i] == ':' && s[1] == ':' && s[2] == '\\')
        continue;
      s.replace(i, 1, "_");
    }

  len = s.length();

  if (s[len - 1] == ' ' || s[len - 1] == '.')
    s.replace(len - 1, 1, "_");

  return s;
}

string lower_case_converter(string s) {
  int len = s.length();
  if (!len)
    return s;

  for (int i = 0; i < len; i++)
    if (s[i] <= 'Z' && s[i] >= 'A')
      s[i] = (char)((int)s[i] + 32);

  return s;
}

#endif