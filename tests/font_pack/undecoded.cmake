# Negative control for tests/font_pack: ink_font.c with the unpacking taken out, reading compressed glyphs as bitmaps.
file(READ ${IN} source)
set(line "f->runs = rd16(d + 4) == 2;")
string(FIND "${source}" "${line}" at)
if(at LESS 0)
  message(FATAL_ERROR "${IN} no longer has `${line}`: update the negative control")
endif()
string(REPLACE "${line}" "f->runs = false; /* negative control: no unpacking */" source "${source}")
file(WRITE ${OUT} "${source}")
