/*
 * Copyright (C) 2007 David Shaw <dshaw@jabberwocky.com>
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

#include "packets.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "GFSDKBasic.h"
#include "output.h"

extern int verbose;

auto append_packet(struct packet *packet, unsigned char *buf,
                   size_t len) -> struct packet * {
  if (packet != nullptr) {
    while (packet->size - packet->len < len) {
      packet->size += 100;
      packet->buf = static_cast<unsigned char *>(
          GFReallocateMemory(packet->buf, packet->size));
    }

    memcpy(&packet->buf[packet->len], buf, len);
    packet->len += len;
  } else {
    packet =
        static_cast<struct packet *>(GFAllocateMemory(sizeof(struct packet)));
    packet->type = 0;
    packet->buf = static_cast<unsigned char *>(GFAllocateMemory(len));
    packet->len = len;
    packet->size = len;

    memcpy(packet->buf, buf, len);
  }

  return packet;
}

void free_packet(struct packet *packet) {
  if (packet != nullptr) {
    GFFreeMemory(packet->buf);
    GFFreeMemory(packet);
  }
}