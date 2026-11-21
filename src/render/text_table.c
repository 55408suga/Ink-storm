#include "text_table.h"

typedef struct {
    const char *ja;
    const char *en;
} TextEntry;

static const TextEntry k_text_table[TEXT_COUNT] = {
    [TEXT_NONE] = {.ja = "", .en = ""},
    [TEXT_GAME_TITLE] = {.ja = "\xe5\xa2\xa8\xe5\xb5\x90", .en = "INKSTORM"},
};

const char *text_get(TextId id, bool use_ja)
{
    if ((unsigned)id >= (unsigned)TEXT_COUNT) {
        return "";
    }
    return use_ja ? k_text_table[id].ja : k_text_table[id].en;
}
