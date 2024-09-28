/*
 * Copyright (C) 2007, 2008, 2012, 2017 David Shaw <dshaw@jabberwocky.com>
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

#include "parse.h"

#include <qcryptographichash.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "GFModuleCommonUtils.hpp"
#include "GFSDKBasic.h"
#include "output.h"
#include "packets.h"

extern int verbose;
extern int ignore_crc_error;

struct packet *parse(FILE *input, unsigned char want, unsigned char stop) {
  int byte;
  struct packet *packet = NULL;

  while ((byte = fgetc(input)) != EOF) {
    unsigned char type;
    unsigned int length;

    if (byte & 0x80) {
      int tmp;

      type = byte & 0x3F;

      /* Old-style packet type */
      if (!(byte & 0x40)) type >>= 2;

      if (type == stop) {
        ungetc(byte, input);
        break;
      }

      if (byte & 0x40) {
        /* New-style packets */
        byte = fgetc(input);
        if (byte == EOF) goto fail;

        if (byte == 255) {
          /* 4-byte length */
          tmp = fgetc(input);
          if (tmp == EOF) goto fail;
          length = tmp << 24;
          tmp = fgetc(input);
          if (tmp == EOF) goto fail;
          length |= tmp << 16;
          tmp = fgetc(input);
          if (tmp == EOF) goto fail;
          length |= tmp << 8;
          tmp = fgetc(input);
          if (tmp == EOF) goto fail;
          length |= tmp;
        } else if (byte >= 224) {
          /* Partial body length, so fail (keys can't use
             partial body) */
          LOG_ERROR("Invalid partial packet encoding");
          goto fail;
        } else if (byte >= 192) {
          /* 2-byte length */
          tmp = fgetc(input);
          if (tmp == EOF) goto fail;
          length = ((byte - 192) << 8) + tmp + 192;
        } else
          length = byte;
      } else {
        /* Old-style packets */
        switch (byte & 0x03) {
          case 0:
            /* 1-byte length */
            byte = fgetc(input);
            if (byte == EOF) goto fail;
            length = byte;
            break;

          case 1:
            /* 2-byte length */
            byte = fgetc(input);
            if (byte == EOF) goto fail;
            tmp = fgetc(input);
            if (tmp == EOF) goto fail;
            length = byte << 8;
            length |= tmp;
            break;

          case 2:
            /* 4-byte length */
            tmp = fgetc(input);
            if (tmp == EOF) goto fail;
            length = tmp << 24;
            tmp = fgetc(input);
            if (tmp == EOF) goto fail;
            length |= tmp << 16;
            tmp = fgetc(input);
            if (tmp == EOF) goto fail;
            length |= tmp << 8;
            tmp = fgetc(input);
            if (tmp == EOF) goto fail;
            length |= tmp;
            break;

          default:
            LOG_ERROR("unable to parse old-style length");
            goto fail;
        }
      }

      if (verbose > 1) {
        FLOG_DEBUG("Found packet of type %d, length %d", type, length);
      }
    } else {
      LOG_ERROR("unable to parse OpenPGP packets (is this armored data?)");
      goto fail;
    }

    if (want == 0 || type == want) {
      packet =
          static_cast<struct packet *>(GFAllocateMemory(sizeof(struct packet)));
      packet->type = type;
      packet->buf = static_cast<unsigned char *>(GFAllocateMemory(length));
      packet->len = length;
      packet->size = length;
      if (fread(packet->buf, 1, packet->len, input) < packet->len) {
        FLOG_ERROR("Short read on packet type %d", type);
        goto fail;
      }
      break;
    } else {
      /* We don't want it, so skip the packet.  We don't use fseek
         here since the input might be on stdin and that isn't
         seekable. */

      size_t i;

      for (i = 0; i < length; i++) fgetc(input);
    }
  }

  return packet;

fail:
  return NULL;
}

int calculate_fingerprint(struct packet *packet, size_t public_len,
                          unsigned char fingerprint[20]) {
  if (packet->buf[0] == 3) {
    return -1;
  } else if (packet->buf[0] == 4) {
    QCryptographicHash sha(QCryptographicHash::Sha1);
    QByteArray head;

    head.append(static_cast<char>(0x99));
    head.append(static_cast<char>(public_len >> 8));
    head.append(static_cast<char>(public_len & 0xFF));

    sha.addData(head);
    sha.addData(reinterpret_cast<const char *>(packet->buf), public_len);
    QByteArray result = sha.result();

    if (result.size() != 20) {
      return -1;
    }

    std::memcpy(fingerprint, result.constData(), 20);
  }

  return 0;
}

#define MPI_LENGTH(_start) (((((_start)[0] << 8 | (_start)[1]) + 7) / 8) + 2)

ssize_t extract_secrets(struct packet *packet) {
  size_t offset;

  if (packet->len == 0) return -1;

  /* Secret keys consist of a public key with some secret material
     stuck on the end.  To get to the secrets, we have to skip the
     public stuff. */

  if (packet->buf[0] == 3) {
    LOG_ERROR("Version 3 (PGP 2.x style) keys are not supported.");
    return -1;
  } else if (packet->buf[0] == 4) {
    /* Jump 5 bytes in.  That gets us past 1 byte of version, and 4
       bytes of timestamp. */

    offset = 5;
  } else
    return -1;

  if (packet->len <= offset) return -1;

  switch (packet->buf[offset++]) {
    case 1: /* RSA */
      /* Skip 2 MPIs */
      offset += MPI_LENGTH(&packet->buf[offset]);
      if (packet->len <= offset) return -1;
      offset += MPI_LENGTH(&packet->buf[offset]);
      if (packet->len <= offset) return -1;
      break;

    case 16: /* Elgamal */
      /* Skip 3 MPIs */
      offset += MPI_LENGTH(&packet->buf[offset]);
      if (packet->len <= offset) return -1;
      offset += MPI_LENGTH(&packet->buf[offset]);
      if (packet->len <= offset) return -1;
      offset += MPI_LENGTH(&packet->buf[offset]);
      if (packet->len <= offset) return -1;
      break;

    case 17: /* DSA */
      /* Skip 4 MPIs */
      offset += MPI_LENGTH(&packet->buf[offset]);
      if (packet->len <= offset) return -1;
      offset += MPI_LENGTH(&packet->buf[offset]);
      if (packet->len <= offset) return -1;
      offset += MPI_LENGTH(&packet->buf[offset]);
      if (packet->len <= offset) return -1;
      offset += MPI_LENGTH(&packet->buf[offset]);
      if (packet->len <= offset) return -1;
      break;

    case 18: /* ECDH */
      /* Skip the curve ID and its length byte, plus an MPI, plus the
         KDF parameters and their length byte */
      offset += packet->buf[offset] + 1;
      if (packet->len <= offset) return -1;
      offset += MPI_LENGTH(&packet->buf[offset]);
      if (packet->len <= offset) return -1;
      offset += packet->buf[offset] + 1;
      if (packet->len <= offset) return -1;
      break;

    case 19: /* ECDSA */
    case 22: /* EdDSA - note that this is from an expired draft
                https://tools.ietf.org/html/draft-koch-eddsa-for-openpgp-04,
                but GnuPG is using algorithm 22 for it. */
      /* Skip the curve ID and its length byte, plus an MPI */
      offset += packet->buf[offset] + 1;
      if (packet->len <= offset) return -1;
      offset += MPI_LENGTH(&packet->buf[offset]);
      if (packet->len <= offset) return -1;
      break;

    default:
      /* What algorithm? */
      FLOG_ERROR("Unable to parse algorithm %u", packet->buf[offset - 1]);
      return -1;
  }

  return offset;
}

struct packet *read_secrets_file(FILE *secrets, enum data_type input_type) {
  struct packet *packet = NULL;
  int final_crc = 0;
  unsigned long my_crc = 0;

  if (input_type == RAW) {
    unsigned char buffer[1024];
    size_t got;

    while ((got = fread(buffer, 1, 1024, secrets)))
      packet = append_packet(packet, buffer, got);

    if (got == 0 && !feof(secrets)) {
      LOG_ERROR("unable to read secrets file");
      free_packet(packet);
      return NULL;
    }

    if (packet->len >= 3) {
      /* Grab the last 3 bytes to be the CRC24 */
      my_crc = packet->buf[packet->len - 3] << 16;
      my_crc |= packet->buf[packet->len - 2] << 8;
      my_crc |= packet->buf[packet->len - 1];
      final_crc = 1;
      packet->len -= 3;
    }
  } else {
    char line[1024];
    unsigned int next_linenum = 1;

    while (fgets(line, 1024, secrets)) {
      unsigned int linenum, did_digit = 0;
      unsigned long line_crc = CRC24_INIT;
      char *tok;

      if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;

      linenum = atoi(line);
      if (linenum != next_linenum) {
        FLOG_ERROR("missing line number %u (saw %u)", next_linenum,
                   linenum);
        free_packet(packet);
        return NULL;
      } else
        next_linenum = linenum + 1;

      tok = strchr(line, ':');
      if (tok) {
        tok = strchr(tok, ' ');

        while (tok) {
          char *next;

          while (*tok == ' ') tok++;

          next = strchr(tok, ' ');

          if (next == NULL) {
            /* End of line, so check the CRC. */
            unsigned long new_crc;

            if (sscanf(tok, "%06lX", &new_crc)) {
              if (did_digit) {
                if ((new_crc & 0xFFFFFFL) != (line_crc & 0xFFFFFFL)) {
                  FLOG_ERROR("CRC on line %d does not match (%06lX!=%06lX)",
                             linenum, new_crc & 0xFFFFFFL,
                             line_crc & 0xFFFFFFL);
                  if (!ignore_crc_error) {
                    free_packet(packet);
                    return NULL;
                  }
                }
              } else {
                final_crc = 1;
                my_crc = new_crc;
              }
            }
          } else {
            unsigned int digit;

            if (sscanf(tok, "%02X", &digit)) {
              unsigned char d = digit;
              packet = append_packet(packet, &d, 1);
              do_crc24(&line_crc, &d, 1);
              did_digit = 1;
            }
          }

          tok = next;
        }
      } else {
        FLOG_ERROR("No colon ':' found in line %u", linenum);
        free_packet(packet);
        return NULL;
      }
    }
  }

  if (final_crc) {
    unsigned long all_crc = CRC24_INIT;

    do_crc24(&all_crc, packet->buf, packet->len);

    if ((my_crc & 0xFFFFFFL) != (all_crc & 0xFFFFFFL)) {
      FLOG_ERROR("CRC of secret does not match (%06lX!=%06lX)",
                 my_crc & 0xFFFFFFL, all_crc & 0xFFFFFFL);
      if (!ignore_crc_error) {
        free_packet(packet);
        return NULL;
      }
    }
  } else {
    LOG_ERROR("CRC of secret is missing");
    if (!ignore_crc_error) {
      free_packet(packet);
      return NULL;
    }
  }

  return packet;
}