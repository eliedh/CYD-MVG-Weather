#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>

namespace core {

// Copies UTF-8 text into a fixed buffer without splitting a multi-byte sequence.
void copyUtf8(char* dst, size_t dstSize, const char* src);

// Strips HTML tags, decodes common entities (named + numeric), drops soft
// hyphens / zero-width chars, collapses whitespace. Block tags (<p>, <br>, <li>)
// become a single newline. Output is truncated cleanly to maxBytes-1.
void htmlToText(const char* html, char* out, size_t maxBytes);

// Percent-encodes everything except unreserved characters (RFC 3986).
std::string urlEncode(const char* s);

// ASCII case-insensitive equality.
bool equalsIgnoreCase(const char* a, const char* b);

// Appends one code point as UTF-8; returns bytes written (0 if no room).
size_t appendUtf8(char* dst, size_t room, uint32_t cp);

// Decodes the next UTF-8 code point and advances *p. Invalid bytes yield U+FFFD.
uint32_t nextCodePoint(const char*& p);

}  // namespace core
