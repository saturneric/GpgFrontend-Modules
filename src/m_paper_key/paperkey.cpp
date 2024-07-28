/*
 * Copyright (C) 2007, 2008, 2009, 2012, 2016 David Shaw <dshaw@jabberwocky.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "extract.h"
#include "output.h"

int verbose = 0, ignore_crc_error = 0;
unsigned int output_width = 78;
char *comment = nullptr;

enum options {
  OPT_HELP = 256,
  OPT_VERSION,
  OPT_VERBOSE,
  OPT_OUTPUT,
  OPT_INPUT_TYPE,
  OPT_OUTPUT_TYPE,
  OPT_OUTPUT_WIDTH,
  OPT_SECRET_KEY,
  OPT_PUBRING,
  OPT_SECRETS,
  OPT_IGNORE_CRC_ERROR,
  OPT_FILE_FORMAT,
  OPT_COMMENT
};

static struct option long_options[] = {
    {"help", no_argument, NULL, OPT_HELP},
    {"version", no_argument, NULL, OPT_VERSION},
    {"verbose", no_argument, NULL, OPT_VERBOSE},
    {"output", required_argument, NULL, OPT_OUTPUT},
    {"input-type", required_argument, NULL, OPT_INPUT_TYPE},
    {"output-type", required_argument, NULL, OPT_OUTPUT_TYPE},
    {"output-width", required_argument, NULL, OPT_OUTPUT_WIDTH},
    {"secret-key", required_argument, NULL, OPT_SECRET_KEY},
    {"pubring", required_argument, NULL, OPT_PUBRING},
    {"secrets", required_argument, NULL, OPT_SECRETS},
    {"ignore-crc-error", no_argument, NULL, OPT_IGNORE_CRC_ERROR},
    {"file-format", no_argument, NULL, OPT_FILE_FORMAT},
    {"comment", required_argument, NULL, OPT_COMMENT},
    {NULL, 0, NULL, 0}};

static void usage(void) {
  printf("Usage: paperkey [OPTIONS]\n");
  printf("  --help (-h)\n");
  printf("  --version (-V)\n");
  printf("  --verbose (-v)  be more verbose\n");
  printf("  --output (-o)   write output to this file\n");
  printf("  --input-type    auto, base16 or raw (binary)\n");
  printf("  --output-type   base16 or raw (binary)\n");
  printf("  --output-width  maximum width of base16 output\n");
  printf(
      "  --secret-key"
      "    extract secret data from this secret key\n");
  printf(
      "  --pubring"
      "       public keyring to find non-secret data\n");
  printf(
      "  --secrets       file containing secret"
      " data to join with the public key\n");
  printf("  --ignore-crc-error  don't reject corrupted input\n");
  printf("  --file-format   show the paperkey file format\n");
  printf("  --comment       add a comment to the base16 output\n");
}
