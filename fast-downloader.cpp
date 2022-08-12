// Author - Aman-Rohilla@rohilla.co.in

// Compilation command
// g++ -o fast-downloader.exe fast-downloader.cpp -std=c++17 -I. -ICPP_COMPILER_INCLUDE_DIRECTORY -Lcurl-win\lib -Lcurl-win\lib64 -Icurl-win\include -lshlwapi -lcurl -lnghttp2 -lidn2 -lssh2 -lssh2 -ladvapi32 -lcrypt32 -lssl -lcrypto -lssl -lcrypto -lgdi32 -lwldap32 -lzstd -lzstd -lz -lws2_32 -static -lbrotlidec-static -lgsasl -lbrotlienc-static -lbrotlicommon-static

// Replace CPP_COMPILER_INCLUDE_DIRECTORY in above command

#include <utils.h>
#include <curl_cert.h>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <fstream>

using namespace std;
#define TIMETYPE curl_off_t
int threads = 5;
int cli_args=1;

void exiting() {
  cout<<"\n\tPress Enter to exit...";
  string s;
  getline(cin, s);
  cout<<endl;
}

void pbar(float comp, COORD pt, int color, int blocks = 50) {
  int x = pt.X, y = pt.Y;
  current_console_cursor(x, y, 1);
  float per = 1.0 / blocks;
  int chars = comp / per;
  string s;

  for (int i = 0; i < chars; i++)
    s += "=";

  if (s.length() && comp != 1)
    s[s.length() - 1] = '>';
  for (int i = 0; i < blocks - chars; i++)
    s += "-";

  setConsoleColor(2 + (color - 2) % 14);
  cout << s;
  setConsoleColor(7);
  cout << " " << add_gaps(conv2str(float_precision(comp * 100, 0)), 3, 1) << " %";
}

struct progress__ {
  CURL *curl;
  atomic<uint64_t> downloded = 0;
  size_t total = 0;
  COORD pt;
  int color = 7;
};

progress__ *progress;

mutex mpvar;
condition_variable cpvar;
atomic<int> pbar_f;

atomic<uint64_t> download_bytes;
atomic<uint64_t> last_downloaded;
int done = 0;

curl_off_t tot = 0;

int startx = 0;
int starty = 21;
string last_speed;
int download = 1;
void print_speed() {
  size_t siz = stoull(conv2str((size_t)download_bytes.load()));

  double db;
  int ff = 1;
  string s;
  int mb;
  console_cursor(0);
  while (ff) {
    mb = 1;
    uint64_t sz = download_bytes.load() - last_downloaded.load();
    db = (double)sz;
    if (double_precision(db / MB2B(1), 1) >= 1)
      db = double_precision(db / MB2B(1), 1);
    else {
      mb = 0;
      db = double_precision(db / 1000, 0);
      if (db < 0.1)
        db = 0;
    }

    current_console_cursor(startx, starty, 1);
    setConsoleColor(11);
    s = add_gaps(conv2str(db), 5, 1);
    if (mb)
      s += " MB/s\n";
    else
      s += " kB/s\n";
    if (!done)
      cout << s;
    else
      cout << last_speed;
    last_speed = s;
    setConsoleColor(7);

    last_downloaded.exchange(download_bytes.load());

    for (int i = 0; i < threads; i++)
      pbar(((float)progress[i].downloded) / progress[i].total, progress[i].pt, progress[i].color);

    Sleep(1000);

    if (done && ff == 2)
      ff = 0;
    else if (done)
      ff = 2;
  }
}

static int xferinfo(void *p, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
  struct progress__ *progress = (struct progress__ *)p;
  download_bytes.exchange((uint64_t)(dlnow)-progress->downloded.load() + download_bytes.load());
  progress->downloded.exchange((uint64_t)dlnow);

  return 0;
}
struct wrtdatafn__ {
  char *memory = (char *)malloc(1);
  size_t memsize = 0;
  FILE *fptr;
  size_t start = 0;
  size_t written = 0;
  size_t total = 0;
  size_t downloaded = 0;
  int id = 0;
};

wrtdatafn__ *wdfpg;
mutex mvar;
condition_variable cvar;
int ready = 1;

atomic<int> thread_flag;
atomic<int> fl;

static size_t data_writer(void *ptr, size_t size, size_t nmemb, void *stream) {
  wrtdatafn__ *wdfp = (wrtdatafn__ *)stream;
  size_t buff_size = size * nmemb;

  char *new_ptr = (char *)realloc(wdfp->memory, wdfp->memsize + buff_size + 1);
  if (!new_ptr) {
    cout << "\n\tMemory Error...\n";
    return 0;
  }

  wdfp->memory = new_ptr;
  memcpy(&wdfp->memory[wdfp->memsize], ptr, buff_size);
  wdfp->memsize += buff_size;
  wdfp->memory[wdfp->memsize] = 0;

  wdfp->downloaded += buff_size;

  if (wdfp->downloaded == wdfp->total || wdfp->memsize >= MB2B(1)) {
    unique_lock<mutex> ul(mvar);
    cvar.wait(ul, []() { return thread_flag.load(); });
    thread_flag.exchange(0);

    fseek(wdfp->fptr, wdfp->start + wdfp->written, SEEK_SET);
    fwrite(wdfp->memory, wdfp->memsize, 1, wdfp->fptr);
    wdfp->written += wdfp->memsize;

    if (wdfp->downloaded == wdfp->total)
      free(wdfp->memory);
    else {
      wdfp->memsize = 0;
      wdfp->memory = (char *)realloc(wdfp->memory, 1);
      wdfp->memory[wdfp->memsize] = 0;
    }

    thread_flag.exchange(1);
    ul.unlock();
    cvar.notify_all();
  }
  return buff_size;
}

struct MemoryStruct {
  char *memory = (char *)malloc(1);
  size_t size = 0;
};

MemoryStruct header;

static size_t in_memory_writer(void *contents, size_t size, size_t nmemb, void *userp) {
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;

  char *ptr = (char *)realloc(mem->memory, mem->size + realsize + 1);
  if (!ptr) {
    cout<< "\n\tMemory Error\n";
    if(cli_args == 1) exiting();
    exit(0);
  }

  mem->memory = ptr;
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}



curl_off_t get_download_size(string url) {
  CURL *curl;
  CURLcode rv;
  curl = curl_easy_init();
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

  curl_easy_setopt(curl, CURLOPT_HEADER, 0L);
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
  curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_CTX_FUNCTION, sslctx_function);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, in_memory_writer);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, (void *)&header);

  curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 6.1; Win64; x64; rv:84.0) Gecko/20100101 Firefox/84.0");
  curl_off_t cl = -1;

  if (curl) {
    rv = curl_easy_perform(curl);

    if (CURLE_OK == rv)
      rv = curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &cl);
  }
  curl_easy_cleanup(curl);
  if(cl==-1) {
    cout<<"\n\tUnknown Error...\n";
    if(cli_args==1) exiting();
    exit(0);
  }
  return cl;
}


COORD start_pt;

int endx = 0;

CURL *set_curl_opt(string url, FILE *fs, int nth, int total, size_t dl_size) {
  CURL *curl = curl_easy_init();

  progress[nth].pt.X = start_pt.X + 3 + nth;
  progress[nth].pt.Y = 0;
  progress[nth].color = nth + 2;
  wdfpg[nth].fptr = fs;
  wdfpg[nth].id = nth;

  wdfpg[nth].start = (size_t)((nth * dl_size) / total);
  string range = conv2str((size_t)((nth * dl_size) / total)) + "-" + conv2str((size_t)(((nth + 1) * dl_size) / total) - 1);
  progress[nth].total = stoull(erase_find(range, "-")) - stoull(erase_find_found_to_end(range, "-")) + 1;
  wdfpg[nth].total = stoull(erase_find(range, "-")) - stoull(erase_find_found_to_end(range, "-")) + 1;

  endx = start_pt.X + 5 + nth;

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_HEADER, 0L);
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, data_writer);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)(&wdfpg[nth]));
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, stderr);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

  curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_CTX_FUNCTION, sslctx_function);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_RANGE, range.c_str());
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 1L);
  curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xferinfo);
  curl_easy_setopt(curl, CURLOPT_XFERINFODATA, (void *)(&progress[nth]));
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 6.1; Win64; x64; rv:84.0) Gecko/20100101 Firefox/84.0");

  return curl;
}

FILE * no_dl_size_fptr = NULL;
static size_t write_data_no_dl_size(void *ptr, size_t size, size_t nmemb, void *stream) {
    // auto wdfp = (wrtdatafn__ *) stream;
    // fseek(wdfp->fptr,wdfp->start+wdfp->written,SEEK_SET);
    // size_t written = fwrite(ptr, size, nmemb, wdfp->fptr);
    size_t written = fwrite(ptr, size, nmemb, no_dl_size_fptr);
    return written;
}

timer__ timer(1);
uint64_t no_dl_size_downloaded_bytes=0;
int no_dl_size_pos_X;
int no_dl_size_pos_Y;

static int xferinfo_no_dl_size(void *p, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
  double seconds;
  if(!timer.interval_elapsed(&seconds)) return 0;

  auto db = dlnow - no_dl_size_downloaded_bytes;
  double speed = (double)db;
  bool MB = true;
  if (speed / MB2B(1) >= 1)
    speed = double_precision(speed / MB2B(1), 2);
  else {
    MB = 0;
    speed = double_precision(speed / 1000, 0);
    if (speed < 0.1)
    speed = 0;
  }

  string speed_str = add_gaps(conv2str(speed), 5, 1);
  if (MB)
    speed_str += " MB/s";
  else
    speed_str += " kB/s";
  
  current_console_cursor(no_dl_size_pos_X, no_dl_size_pos_Y, 1);
  cout<<speed_str;
  no_dl_size_downloaded_bytes = dlnow;

  return 0;
}

int main(int argc, char *argv[]) {
  if (argc == 2 && ((string)(argv[1]) == "-v" || (string)(argv[1]) == "--version")) {
    cout<<"1.0.0";
    return 0;
  }

  string url, path;
  cli_args = argc;
  if (argc == 1) {
    cout<<R"(
      Fast Downloader by Aman-Rohilla@rohilla.co.in
_________________________________________________________
)";
  
    cout<<"\n\tEnter http/https URL : ";
    getline(cin, url);
    cout<<"\tEnter output file path : ";
    getline(cin, path);
    cout<<"\tEnter number of parts (max=20, default=5) : ";
    string s;
    getline(cin, s);
    if (s != "") threads = stoi(s);

  }
  else if (argc == 2 && ((string)(argv[1]) == "-h" || (string)(argv[1]) == "--help")) {
    cout<<R"(
      Fast Downloader By Aman-Rohilla@rohilla.co.in
_________________________________________________________

      Usage : 
        
        fast-downloader http/https-url output-file
        fast-downloader http/https-url output-file --parts 10

      Flags : parts (5)
)";

    return 0;
  } else if(argc<3) {
    cout<<"\n\tArgument Error...\n";
    return 0;
  } else {
    url = argv[1];
    path = argv[2];
  }
  if(argc>=5 && (string)argv[3] == "--parts") {
    threads = stoi(argv[4]);
  }

  thread_flag.store(1);
  pbar_f.store(1);
  fl.store(1);
  download_bytes.store(0);
  last_downloaded.store(0);
  console_cursor(0);
  int start_x, start_y;
  current_console_cursor(start_x, start_y);
  start_pt.X = start_x;
  start_pt.Y = start_y;
  startx = start_x + 1;

  path = name_assigner(validate_path(path));
  FILE *fptr = fopen(path.c_str(), "wb");

  curl_global_init(CURL_GLOBAL_ALL);
  atexit([]{console_cursor(1);});
  atexit([]{setConsoleColor(7);});
  
  auto dl_size = get_download_size(url);
  threads = min((size_t)threads, dl_size / MB2B(1));

  if(dl_size==0) {
    CURL *curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HEADER, 0L);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data_no_dl_size);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, stderr);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

    curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_CTX_FUNCTION, sslctx_function);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 1L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xferinfo_no_dl_size);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 6.1; Win64; x64; rv:84.0) Gecko/20100101 Firefox/84.0");

    no_dl_size_fptr = fptr;
    cout<< "\n\tDownloading : ";
    setConsoleColor(11);
    current_console_cursor(no_dl_size_pos_X, no_dl_size_pos_Y);

    console_cursor(0);
    curl_easy_perform(curl);
    console_cursor(1);
    setConsoleColor(7);
    cout<<"\tDone...\n";


    fclose(fptr);
    curl_easy_cleanup(curl);
    curl_global_cleanup();

    if(cli_args==1) exiting();
    return 0;
  }

  string head_str(header.memory, header.size);
  head_str = replace_string_all(head_str, "\r", "");

  head_str = "\n" + head_str + "\n";
  head_str = lower_case_converter(head_str);
  bool accept_range = str_find(head_str, "\naccept-ranges: bytes\n");

  if (!threads)
    threads = 1;

  if (!accept_range)
    threads = 1;
  else if (threads > 1 && accept_range) {
    if (threads > (dl_size / MB2B(1)))
      threads = dl_size / MB2B(1);
  }
  if (!threads)
    threads = 1;

  if (threads > 20) 
    threads = 20;

  wdfpg = new wrtdatafn__[threads];
  progress = new progress__[threads];

  CURLM *multi_handle = curl_multi_init();
  CURL *cha[threads];

  for (int i = 0; i < threads; i++) {
    cha[i] = set_curl_opt(url, fptr, i, threads, dl_size);
    curl_multi_add_handle(multi_handle, cha[i]);
  }

  thread th(print_speed);

  int still_running = 1;
  while (still_running) {
    int queued;
    CURLMcode mc = curl_multi_perform(multi_handle, &still_running);

    if (still_running)
      mc = curl_multi_poll(multi_handle, NULL, 0, 1000, NULL);  

    if (mc) break;

    CURLMsg *msg;
    do {
      msg = curl_multi_info_read(multi_handle, &queued);
    } while (msg);
  }

  for (int i = 0; i < threads; i++)
    curl_multi_remove_handle(multi_handle, cha[i]);

  curl_multi_cleanup(multi_handle);

  for (int i = 0; i < threads; i++)
    curl_easy_cleanup(cha[i]);

  fclose(fptr);
  curl_global_cleanup();

  done = 1;
  th.join();

  int endy = 0;
  current_console_cursor(endx, endy, 1);
  if(cli_args==1) exiting();
}
