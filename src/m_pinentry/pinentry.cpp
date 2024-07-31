/* pinentry.c - The PIN entry support library
 * Copyright (C) 2002, 2003, 2007, 2008, 2010, 2015, 2016, 2021 g10 Code GmbH
 *
 * This file is part of PINENTRY.
 *
 * PINENTRY is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * PINENTRY is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 * SPDX-License-Identifier: GPL-2.0+
 */

#include "pinentry.h"

#include <qhash.h>
#include <qstring.h>

#include "GFModuleCommonUtils.hpp"
#include "GFSDKBasic.h"

#if defined(_WIN32) || defined(WIN32)
#define getpid() GetCurrentProcessId()
#endif

/* Keep the name of our program here. */
static char this_pgmname[50];

struct pinentry pinentry;

static const char *flavor_flag;

/* Return a malloced copy of the commandline for PID.  If this is not
 * possible NULL is returned.  */
#if !(defined(_WIN32) || defined(WIN32))
static char *get_cmdline(unsigned long pid) {
  char buffer[200];
  FILE *fp;
  size_t i, n;

  snprintf(buffer, sizeof buffer, "/proc/%lu/cmdline", pid);

  fp = fopen(buffer, "rb");
  if (!fp) return NULL;
  n = fread(buffer, 1, sizeof buffer - 1, fp);
  if (n < sizeof buffer - 1 && ferror(fp)) {
    /* Some error occurred.  */
    fclose(fp);
    return NULL;
  }
  fclose(fp);
  if (n == 0) return NULL;
  /* Arguments are delimited by Nuls.  We should do proper quoting but
   * that can be a bit complicated, thus we simply replace the Nuls by
   * spaces.  */
  for (i = 0; i < n; i++)
    if (!buffer[i] && i < n - 1) buffer[i] = ' ';
  buffer[i] = 0; /* Make sure the last byte is the string terminator.  */

  return strdup(buffer);
}
#endif /*!WINDOWS*/

/* Atomically ask the kernel for information about process PID.
 * Return a malloc'ed copy of the process name as long as the process
 * uid matches UID.  If it cannot determine that the process has uid
 * UID, it returns NULL.
 *
 * This is not as informative as get_cmdline, but it verifies that the
 * process does belong to the user in question.
 */
#if !(defined(_WIN32) || defined(WIN32))
static char *get_pid_name_for_uid(unsigned long pid, int uid) {
  char buffer[400];
  FILE *fp;
  size_t end, n;
  char *uidstr;

  snprintf(buffer, sizeof buffer, "/proc/%lu/status", pid);

  fp = fopen(buffer, "rb");
  if (!fp) return NULL;
  n = fread(buffer, 1, sizeof buffer - 1, fp);
  if (n < sizeof buffer - 1 && ferror(fp)) {
    /* Some error occurred.  */
    fclose(fp);
    return NULL;
  }
  fclose(fp);
  if (n == 0) return NULL;
  buffer[n] = 0;
  /* Fixme: Is it specified that "Name" is always the first line?  For
   * robustness I would prefer to have a real parser here. -wk  */
  if (strncmp(buffer, "Name:\t", 6)) return NULL;
  end = strcspn(buffer + 6, "\n") + 6;
  buffer[end] = 0;

  /* check that uid matches what we expect */
  uidstr = strstr(buffer + end + 1, "\nUid:\t");
  if (!uidstr) return NULL;
  if (atoi(uidstr + 6) != uid) return NULL;

  return strdup(buffer + 6);
}
#endif /*!WINDOWS*/

const char *pinentry_get_pgmname(void) { return this_pgmname; }

/* Run a quality inquiry for PASSPHRASE of LENGTH.  (We need LENGTH
   because not all backends might be able to return a proper
   C-string.).  Returns: A value between -100 and 100 to give an
   estimate of the passphrase's quality.  Negative values are use if
   the caller won't even accept that passphrase.  Note that we expect
   just one data line which should not be escaped in any represent a
   numeric signed decimal value.  Extra data is currently ignored but
   should not be send at all.  */
int pinentry_inq_quality(const QString &passphrase) {
  int score = 0;

  score += std::min(40, static_cast<int>(passphrase.length()) * 2);

  bool has_upper = false;
  bool has_lower = false;
  bool has_digit = false;
  bool has_special = false;
  for (const auto ch : passphrase) {
    if (ch.isUpper()) has_upper = true;
    if (ch.isLower()) has_lower = true;
    if (ch.isDigit()) has_digit = true;
    if (!ch.isLetterOrNumber()) has_special = true;
  }

  int const variety_count =
      static_cast<int>(has_upper) + static_cast<int>(has_lower) +
      static_cast<int>(has_digit) + static_cast<int>(has_special);
  score += variety_count * 10;

  for (auto i = 0; i < passphrase.length() - 1; ++i) {
    if (passphrase[i] == passphrase[i + 1]) {
      score -= 5;
    }
  }

  QHash<QChar, int> char_count;
  for (const auto ch : passphrase) {
    char_count[ch]++;
  }
  for (auto &p : char_count) {
    if (p > 1) {
      score -= (p - 1) * 3;
    }
  }

  QString const lower_password = passphrase.toLower();
  if (lower_password.contains("password") ||
      lower_password.contains("123456")) {
    score -= 30;
  }

  return std::max(-100, std::min(100, score));
}

/* Try to make room for at least LEN bytes in the pinentry.  Returns
   new buffer on success and 0 on failure or when the old buffer is
   sufficient.  */
char *pinentry_setbufferlen(pinentry_t pin, int len) {
  char *newp;

  if (pin->pin_len)
    assert(pin->pin);
  else
    assert(!pin->pin);

  if (len < 2048) len = 2048;

  if (len <= pin->pin_len) return pin->pin;

  newp = SecureReallocAsType<char>(pin->pin, len);
  if (newp) {
    pin->pin = newp;
    pin->pin_len = len;
  } else {
    GFFreeMemory(pin->pin);
    pin->pin = 0;
    pin->pin_len = 0;
  }
  return newp;
}

/* Set the optional flag used with getinfo. */
void pinentry_set_flavor_flag(const char *string) { flavor_flag = string; }
