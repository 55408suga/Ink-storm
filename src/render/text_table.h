#ifndef INK_TEXT_TABLE_H
#define INK_TEXT_TABLE_H

#include <stdbool.h>

typedef enum { TEXT_NONE = 0, TEXT_GAME_TITLE, TEXT_COUNT } TextId;

const char *text_get(TextId id, bool use_ja);

#endif
