// File: xmlsel/extern/utf.h
/*
 * Copyright (c) 2009-2016 Petri Lehtinen <petri@digip.org>
 *
 * Jansson is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef UTF_H
#define UTF_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline int utf8_encode(int32_t codepoint, char *buffer, size_t *size);
static inline size_t utf8_check_first(char byte);
static inline size_t utf8_check_full(const char *buffer, size_t size,
                                     int32_t *codepoint);
static inline const char *utf8_iterate(const char *buffer, size_t size,
                                       int32_t *codepoint);
static inline int utf8_check_string(const char *string, size_t length);

static inline int utf8_encode(int32_t codepoint, char *buffer, size_t *size) {
  if (codepoint < 0)
    return -1;
  else if (codepoint < 0x80) {
    buffer[0] = (char)codepoint;
    *size = 1;
  } else if (codepoint < 0x800) {
    buffer[0] = (char)(0xC0 + ((codepoint & 0x7C0) >> 6));
    buffer[1] = (char)(0x80 + ((codepoint & 0x03F)));
    *size = 2;
  } else if (codepoint < 0x10000) {
    buffer[0] = (char)(0xE0 + ((codepoint & 0xF000) >> 12));
    buffer[1] = (char)(0x80 + ((codepoint & 0x0FC0) >> 6));
    buffer[2] = (char)(0x80 + ((codepoint & 0x003F)));
    *size = 3;
  } else if (codepoint <= 0x10FFFF) {
    buffer[0] = (char)(0xF0 + ((codepoint & 0x1C0000) >> 18));
    buffer[1] = (char)(0x80 + ((codepoint & 0x03F000) >> 12));
    buffer[2] = (char)(0x80 + ((codepoint & 0x000FC0) >> 6));
    buffer[3] = (char)(0x80 + ((codepoint & 0x00003F)));
    *size = 4;
  } else
    return -1;

  return 0;
}

static inline size_t utf8_check_first(char byte) {
  unsigned char u = (unsigned char)byte;

  if (u < 0x80)
    return 1;

  if (0x80 <= u && u <= 0xBF) {
    return 0;
  } else if (u == 0xC0 || u == 0xC1) {
    return 0;
  } else if (0xC2 <= u && u <= 0xDF) {
    return 2;
  } else if (0xE0 <= u && u <= 0xEF) {
    return 3;
  } else if (0xF0 <= u && u <= 0xF4) {
    return 4;
  } else {
    return 0;
  }
}

static inline size_t utf8_check_full(const char *buffer, size_t size,
                                     int32_t *codepoint) {
  size_t i;
  int32_t value = 0;
  unsigned char u = (unsigned char)buffer[0];

  if (size == 2) {
    value = u & 0x1F;
  } else if (size == 3) {
    value = u & 0xF;
  } else if (size == 4) {
    value = u & 0x7;
  } else
    return 0;

  for (i = 1; i < size; i++) {
    u = (unsigned char)buffer[i];

    if (u < 0x80 || u > 0xBF) {
      return 0;
    }

    value = (value << 6) + (u & 0x3F);
  }

  if (value > 0x10FFFF) {
    return 0;
  } else if (0xD800 <= value && value <= 0xDFFF) {
    return 0;
  } else if ((size == 2 && value < 0x80) || (size == 3 && value < 0x800) ||
             (size == 4 && value < 0x10000)) {
    return 0;
  }

  if (codepoint)
    *codepoint = value;

  return 1;
}

static inline const char *utf8_iterate(const char *buffer, size_t bufsize,
                                       int32_t *codepoint) {
  size_t count;
  int32_t value;

  if (!bufsize)
    return buffer;

  count = utf8_check_first(buffer[0]);
  if (count == 0)
    return NULL;

  if (count == 1)
    value = (unsigned char)buffer[0];
  else {
    if (count > bufsize || !utf8_check_full(buffer, count, &value))
      return NULL;
  }

  if (codepoint)
    *codepoint = value;

  return buffer + count;
}

static inline int utf8_check_string(const char *string, size_t length) {
  size_t i;

  for (i = 0; i < length; i++) {
    size_t count = utf8_check_first(string[i]);
    if (count == 0)
      return 0;
    else if (count > 1) {
      if (count > length - i)
        return 0;

      if (!utf8_check_full(&string[i], count, NULL))
        return 0;

      i += count - 1;
    }
  }

  return 1;
}

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include <cstdio>
#include <ostream>
#include <string>

static inline void dump_string(const char *str, size_t len, std::ostream &out) {
  const char *pos = str;
  const char *lim = str + len;

  while (pos < lim) {
    const char *scan = pos;
    while (scan < lim) {
      unsigned char c = (unsigned char)*scan;
      if (c >= 0x20 && c < 0x7F && c != '\\' && c != '"') {
        scan++;
      } else {
        break;
      }
    }

    if (scan != pos) {
      out.write(pos, (std::streamsize)(scan - pos));
      pos = scan;
    }

    if (pos == lim) {
      break;
    }

    int32_t codepoint = 0;
    const char *next = utf8_iterate(pos, (size_t)(lim - pos), &codepoint);
    if (!next) {
      return;
    }

    if (codepoint == '\\' || codepoint == '"' || codepoint < 0x20) {
      const char *text = nullptr;
      char seq[13];
      int length = 2;

      switch (codepoint) {
      case '\\':
        text = "\\\\";
        length = 2;
        break;
      case '\"':
        text = "\\\"";
        length = 2;
        break;
      case '\b':
        text = "\\b";
        break;
      case '\f':
        text = "\\f";
        break;
      case '\n':
        text = "\\n";
        break;
      case '\r':
        text = "\\r";
        break;
      case '\t':
        text = "\\t";
        break;
      default: {
        std::snprintf(seq, sizeof(seq), "\\u%04X", (unsigned int)codepoint);
        length = 6;
        text = seq;
        break;
      }
      }
      out.write(text, length);
    } else if (codepoint > 0x7F) {
      char seq[13];
      int length;
      const char *text;

      if (codepoint < 0x10000) {
        std::snprintf(seq, sizeof(seq), "\\u%04X", (unsigned int)codepoint);
        length = 6;
      } else {
        int32_t first, last;
        int32_t adjusted = codepoint - 0x10000;
        first = 0xD800 | ((adjusted & 0xFFC00) >> 10);
        last = 0xDC00 | (adjusted & 0x003FF);

        std::snprintf(seq, sizeof(seq), "\\u%04X\\u%04X", (unsigned int)first,
                      (unsigned int)last);
        length = 12;
      }
      text = seq;
      out.write(text, length);
    } else {
      out.put((char)codepoint);
    }

    pos = next;
  }
}
#endif // __cplusplus

#endif // UTF_H