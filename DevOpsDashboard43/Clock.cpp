// =============================================================================
//  DevOpsDashboard43 (from DevOpsDashboard, based on MeteoPlaneRadar by chiptron.cz)
//  Clock - SNTP + HTTP "Date" fallback. See Clock.h.
//
//  The "Date" parsing comes from MeteoPlaneRadar's Outside.cpp (chiptron.cz).
// =============================================================================
#include "Clock.h"
#include "Config.h"
#include "Status.h"
#include "TimeUtil.h"
#include <esp_sntp.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>

static volatile bool s_ntpSynced = false;
static bool s_dateSet = false;
static bool s_logged = false;

// Runs in the lwIP task - only set a flag.
static void onSntpSync(struct timeval*) { s_ntpSynced = true; }

void Clock_Begin() {
  configTzTime(TZ_INFO, NTP_SERVER_1, NTP_SERVER_2);
  sntp_set_time_sync_notification_cb(onSntpSync);
}

bool Clock_NtpSynced()  { return s_ntpSynced; }
bool Clock_TimeValid()  { return s_ntpSynced || s_dateSet; }

static int monthFromName(const char* m) {
  static const char* N[12] = { "Jan","Feb","Mar","Apr","May","Jun",
                               "Jul","Aug","Sep","Oct","Nov","Dec" };
  for (int i = 0; i < 12; i++) if (strncmp(m, N[i], 3) == 0) return i + 1;
  return 0;
}

void Clock_NoteHttpDate(const char* date) {
  if (s_ntpSynced) return;               // SNTP is more precise - leave it alone
  if (!date || !*date) return;
  // RFC 7231 preferred form: "Sun, 09 Aug 2026 20:00:56 GMT".
  const char* p = strchr(date, ' ');
  if (!p) return;
  p++;
  int D = 0, Y = 0, hh = 0, mm = 0, ss = 0;
  char mon[4] = {0};
  if (sscanf(p, "%d %3s %d %d:%d:%d", &D, mon, &Y, &hh, &mm, &ss) != 6) return;
  int Mo = monthFromName(mon);
  if (Mo == 0 || D < 1 || D > 31 || Y < 2025 || hh > 23 || mm > 59 || ss > 60) return;

  time_t utc = TimeUtil_UtcToEpoch(Y, Mo, D, hh, mm, ss);
  struct timeval tv = { .tv_sec = utc, .tv_usec = 0 };
  settimeofday(&tv, nullptr);
  s_dateSet = true;
}

void Clock_Tick() {
  // The SNTP callback runs in another task, so the note is left from here.
  static bool s_ntpNoted = false;
  if (s_ntpSynced && !s_ntpNoted) {
    s_ntpNoted = true;
    Status_Set(ST_NTP, "OK, NTP");
  } else if (s_dateSet && !s_ntpSynced && !s_logged) {
    Status_Set(ST_NTP, "OK, HTTP Date header (no NTP yet)");
  }

  if (s_logged || !Clock_TimeValid()) return;
  s_logged = true;
  time_t now = time(nullptr);
  struct tm lt;
  localtime_r(&now, &lt);
  Serial.printf("Clock: %04d-%02d-%02d %02d:%02d:%02d (%s)\n",
                lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday,
                lt.tm_hour, lt.tm_min, lt.tm_sec,
                s_ntpSynced ? "NTP" : "HTTP Date header");
}

void Clock_StatusText(char* buf, size_t cap) {
  if (!buf || cap == 0) return;
  buf[0] = '\0';
  if (!Clock_TimeValid()) return;
  time_t now = time(nullptr);
  struct tm lt;
  localtime_r(&now, &lt);
  snprintf(buf, cap, "%02d:%02d", lt.tm_hour, lt.tm_min);
}
