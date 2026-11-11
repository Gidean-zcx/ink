#pragma once
#include <expat.h>
static inline XML_Parser ink_reader_parser_create(const XML_Char *encoding) {
    XML_Parser parser=XML_ParserCreate(encoding);
#ifndef ESP_PLATFORM
    if (parser) XML_SetHashSalt(parser,1);
#endif
    return parser;
}
#define XML_ParserCreate ink_reader_parser_create
