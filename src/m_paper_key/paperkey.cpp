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
