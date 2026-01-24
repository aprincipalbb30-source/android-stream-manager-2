/*
   base64.cpp and base64.h

   base64 encoding and decoding with C++.
   More information at
     https://renenyffenegger.ch/notes/development/Base64/Encoding-and-decoding-base-64-with-cpp

   Version: 0.5.0

   Copyright (C) 2004-2023 René Nyffenegger

   This source code is provided 'as-is', without any express or implied
   warranty. In no event will the author be held liable for any damages
   arising from the use of this software.

   Permission is granted to anyone to use this software for any purpose,
   including commercial applications, and to alter it and redistribute it
   freely, subject to the following restrictions:

   1. The origin of this source code must not be misrepresented; you must not
      claim that you wrote the original source code. If you use this source code
      in a product, an acknowledgment in the product documentation would be
      appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
      misrepresented as being the original source code.

   3. This notice may not be removed or altered from any source distribution.

   René Nyffenegger rene.nyffenegger@adp-gmbh.ch

*/

#include "base64.h"
#include <stdexcept>

namespace {
    const std::string base64_chars =
                 "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                 "abcdefghijklmnopqrstuvwxyz"
                 "0123456789+/";

    const std::string base64_url_chars =
                 "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                 "abcdefghijklmnopqrstuvwxyz"
                 "0123456789-_";

    static bool is_base64(unsigned char c) {
      return (isalnum(c) || (c == '+') || (c == '/'));
    }

    unsigned char pos_of_char(const unsigned char chr) {
      if (chr >= 'A' && chr <= 'Z') return chr - 'A';
      if (chr >= 'a' && chr <= 'z') return chr - 'a' + ('Z' - 'A') + 1;
      if (chr >= '0' && chr <= '9') return chr - '0' + ('Z' - 'A') + ('z' - 'a') + 2;
      if (chr == '+' || chr == '-') return 62;
      if (chr == '/' || chr == '_') return 63;
      throw std::runtime_error("Input is not valid base64-encoded data.");
    }
}

std::string base64_encode(std::string const& s) {
    int counter = 0;
    int n = 0;
    unsigned char c;

    std::string ret;
    ret.reserve(s.length() * 2);

    for (char const& c_in : s) {
        c = c_in;
        switch (counter) {
            case 0:
                ret += base64_chars[c >> 2];
                n = (c & 0x03) << 4;
                break;
            case 1:
                ret += base64_chars[n | (c >> 4)];
                n = (c & 0x0f) << 2;
                break;
            case 2:
                ret += base64_chars[n | (c >> 6)];
                ret += base64_chars[c & 0x3f];
                break;
        }
        counter++;
        if (counter == 3) counter = 0;
    }

    if (counter > 0) {
        ret += base64_chars[n];
        for (; counter < 3; counter++) {
            ret += '=';
        }
    }

    return ret;
}

std::string base64_decode(std::string const& s) {
    if (s.length() % 4 != 0) throw std::runtime_error("Input is not valid base64-encoded data.");

    int n = 0;
    int counter = 0;
    unsigned char c;

    std::string ret;
    ret.reserve(s.length());

    for (char const& c_in : s) {
        if (c_in == '=') break;

        c = pos_of_char(c_in);
        switch (counter) {
            case 0:
                n = c << 2;
                break;
            case 1:
                ret += (n | (c >> 4));
                n = (c & 0x0f) << 4;
                break;
            case 2:
                ret += (n | (c >> 2));
                n = (c & 0x03) << 6;
                break;
            case 3:
                ret += (n | c);
                break;
        }
        counter++;
        if (counter == 4) counter = 0;
    }

    return ret;
}
