/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Base Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘   
========================================================*/

#include <ply-base.h>

using namespace ply;

//  ▄▄     ▄▄          ▄▄▄                ▄▄
//  ██▄▄▄  ▄▄  ▄▄▄▄▄  ██    ▄▄▄▄  ▄▄▄▄▄  ▄██▄▄
//  ██  ██ ██ ██  ██ ▀██▀▀ ██  ██ ██  ██  ██
//  ██▄▄█▀ ██ ▀█▄▄██  ██   ▀█▄▄█▀ ██  ██  ▀█▄▄
//             ▄▄▄█▀

// clang-format off
const char* GlyphData[] = {
    "A ,,,, B,,,,, C ,,,, D,,,,, E,,,,,F,,,,,G ,,,, H,,  ,,I,,,,J    ,,K,,  ,,L,,   M,,   ,,",
    " ##  ## ##  ## ##  `` ##  ## ##    ##    ##  `` ##  ##  ##      ## ##,#`  ##    ###,###",
    " ##``## ##``#, ##     ##  ## ##``  ##``  ## `## ##``##  ##  ,,  ## ###,   ##    ##`#`##",
    " ##  ## ##,,#` `#,,#` ##,,#` ##,,, ##    `#,,## ##  ## ,##, `#,,#` ## `#, ##,,, ##   ##",
    "                                                                                       ",
    "N,,  ,,O ,,,, P,,,,, Q ,,,, R,,,,, S ,,,, T,,,,,,U,,  ,,V,,   ,,W,,    ,,X,,  ,,Y,,  ,,Z,,,,,,",
    " ### ## ##  ## ##  ## ##  ## ##  ## ##  ``   ##   ##  ## ##   ## ## ,, ## `#,,#` ##  ##    ,#`",
    " ##`### ##  ## ##```  ##  ## ##``#,  ```#,   ##   ##  ##  ## ##  `#,##,#`  ,##,   `##`   ,#`  ",
    " ##  ## `#,,#` ##     `#,,#` ##  ## `#,,#`   ##   `#,,#`   `#`    ##``##  ##  ##   ##   ##,,,,",
    "                          ``                                                                  ",
    "a      b,,    c     d    ,,e      f  ,,,g      h,,    i,,j   ,,k,,    l,,, m        ",
    "  ,,,,  ##,,,   ,,,,  ,,,##  ,,,,   ##    ,,,,, ##,,,  ,,    ,, ##  ,,  ##  ,,,,,,, ",
    "  ,,,## ##  ## ##    ##  ## ##,,## `##`` ##  ## ##  ## ##    ## ##,#`   ##  ## ## ##",
    " `#,,## ##,,#` `#,,, `#,,## `#,,,   ##   `#,,## ##  ## ##    ## ## `#, ,##, ## ## ##",
    "                                          ,,,#`           `#,#`                        ",
    "n      o      p      q      r      s      t ,,  u      v       w        x      y      z      ",
    " ,,,,,   ,,,,  ,,,,,   ,,,,, ,,,,,   ,,,,  ,##,, ,,  ,, ,,   ,, ,,    ,, ,,  ,, ,,  ,, ,,,,,,",
    " ##  ## ##  ## ##  ## ##  ## ##  `` `#,,,   ##   ##  ## `#, ,#` ## ## ##  `##`  ##  ##   ,#` ",
    " ##  ## `#,,#` ##,,#` `#,,## ##      ,,,#`  `#,, `#,,##   `#`    ##``##  ,#``#, `#,,## ,##,,,",
    "               ##         ##                                                     ,,,#`       ",
    "0 ,,,, 1 ,, 2 ,,,, 3 ,,,, 4   ,,, 5,,,,,,6 ,,,, 7,,,,,,8 ,,,, 9 ,,,, _     .  ~   & ,,,   /    ,,-    ",
    " ## ,## `##  ``  ## ``  ##  ,#`##  ##     ##         ## ##  ## ##  ##              ## ``      ,#`     ",
    " ##` ##  ##   ,#``    ``#, ##,,##, ````#, ##``#,   ,#`  ,#``#,  ```##              ,#`#,``  ,#`   ,,,,",
    " `#,,#` ,##, ##,,,, `#,,#`     ##  `#,,#` `#,,#`   ##   `#,,#`  ,,,#` ,,,,, ,,     `#,,`#, ##         ",
    "                                                                                                      ",
};
// clang-format on

struct BigGlyph {
    u32 row = 0;
    u32 col = 0;
    u32 width = 0;
    static constexpr u32 height = 5;
};

void print_bigfont(StringView text) {
    Array<BigGlyph> glyphs;
    glyphs.resize(128);
    u32 num_rows = PLY_STATIC_ARRAY_SIZE(GlyphData) / BigGlyph::height;
    for (u32 i = 0; i < num_rows; i++) {
        const char* row = GlyphData[i * BigGlyph::height];
        u32 start_col = 0;
        for (u32 j = start_col + 1;; j++) {
            if (StringView{" ,#`"}.find(row[j]) < 0) {
                char c = row[start_col];
                if (c == '~') {
                    c = ' ';
                }
                glyphs[c].row = i;
                glyphs[c].col = start_col + 1;
                glyphs[c].width = j - start_col - 1;
                start_col = j;
            }
            if (row[j] == 0)
                break;
        }
    }

    Stream out = get_stdout();
    for (u32 i = 0; i < BigGlyph::height; i++) {
        MemStream mem;
        mem.write("// ");
        for (u32 j = 0; j < text.num_bytes; j++) {
            // Look up glyph
            char c = text[j];
            if ((u8) c >= glyphs.num_items())
                continue;
            const BigGlyph& glyph = glyphs[c];
            if (glyph.width == 0)
                continue;

            // Print current row of glyph
            mem.write(' ');
            const char* data = GlyphData[glyph.row * BigGlyph::height + i] + glyph.col;
            for (u32 k = 0; k < glyph.width; k++) {
                char p = data[k];
                if (p == ' ') {
                    mem.write(' ');
                } else if (p == ',') {
                    mem.write(u8"▄");
                } else if (p == '#') {
                    mem.write(u8"█");
                } else if (p == '`') {
                    mem.write(u8"▀");
                }
            }
        }
        out.format("{}\n", mem.move_to_string().trim_right());
    }
}

int main(int argc, const char* argv[]) {
#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);
#endif
    if (argc != 2) {
        get_stderr().write("error: expected exactly 1 argument\n");
        return 1;
    }
    print_bigfont(argv[1]);
    return 0;
}
