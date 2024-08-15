#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <tos.h>

#include "include/slinput.h"
#include "src/slinput_internal.h"

static long os_Cnecin(void) {
  return Cnecin();
}

static int16_t os_Cconis(void) {
  return Cconis();
}

static long os_Kbshift(int16_t param) {
  return Kbshift(param);
}

int SLINPUT_EnterRaw_Default(
    const SLINPUT_State *state,
    SLINPUT_Stream stream_in,
    SLINPUT_TermAttr *original_term_attr) {
   SLINPUT_TermAttr term_attr = { NULL };
   *original_term_attr = term_attr;
   return 0;
}

int SLINPUT_LeaveRaw_Default(const SLINPUT_State *state,
    SLINPUT_Stream stream_in, SLINPUT_TermAttr previous_attr) {
  return 0;
}

int SLINPUT_GetCharIn_Default(const SLINPUT_State *state,
    SLINPUT_Stream stream_in, SLINPUT_KeyCode *key_code, SLICHAR *character) {
  const long cin = os_Cnecin();

  const long kbshift_state = os_Kbshift(-1);

  const int control = !!(kbshift_state & 0x4);
  const int left_shift = !!(kbshift_state & 0x1);
  const int right_shift = !!(kbshift_state & 0x2);
  const int shifted = left_shift | right_shift;

  const uint8_t kcv = cin >> 16;
  const uint8_t chv = cin & 0xff;

  SLINPUT_KeyCode kc_enum_value = SLINPUT_KC_NUL;

  switch (kcv) {
    case 0x47: /* home key */
        kc_enum_value = shifted ? SLINPUT_KC_HOME : SLINPUT_KC_END;
      break;
    case 0x48: /* up arrow */
        kc_enum_value = SLINPUT_KC_UP;
      break;
    case 0x4B: /* left arrow */
        kc_enum_value = shifted ? SLINPUT_KC_WARP_LEFT : SLINPUT_KC_LEFT;
      break;
    case 0x4D: /* right arrow */
        kc_enum_value = shifted ? SLINPUT_KC_WARP_RIGHT : SLINPUT_KC_RIGHT;
      break;
    case 0x50: /* down arrow */
        kc_enum_value = SLINPUT_KC_DOWN;
      break;
    case 0x53: /* delete */
        kc_enum_value = SLINPUT_KC_DEL;
      break;
  }

  switch (chv) {
    case '\t':
      kc_enum_value = SLINPUT_KC_TAB;
      break;
    case '\b':
      kc_enum_value = SLINPUT_KC_BACKSPACE;
      break;
    case '\x1b':
      kc_enum_value = SLINPUT_KC_ESCAPE;
      break;
    case 0x4:
      if (control)
        kc_enum_value = SLINPUT_KC_END_OF_TRANSMISSION;
      break;
  }

  if (key_code)
    *key_code = kc_enum_value;
  if (character)
    *character = chv;

  return 0;
}

int SLINPUT_IsCharAvailable_Default(const SLINPUT_State *state,
    SLINPUT_Stream stream_in) {
  return !!os_Cconis();
}

int SLINPUT_IsSpace_Default(const SLINPUT_State *state,
    SLINPUT_Stream stream_in, SLICHAR character) {
  return isspace(character);
}

void *SLINPUT_Malloc_Default(SLINPUT_AllocInfo alloc_info, size_t size) {
  return malloc(size);
}

void SLINPUT_Free_Default(SLINPUT_AllocInfo alloc_info, void *ptr) {
  free(ptr);
}

int SLINPUT_Putchar_Default(const SLINPUT_State *state,
    SLINPUT_Stream stream_out, SLICHAR c) {
  return fputc(c, (FILE *) stream_out.stream_data) != EOF ? 0 : -1;
}

int SLINPUT_Flush_Default(const SLINPUT_State *state,
    SLINPUT_Stream stream_out) {
  return fflush((FILE *) stream_out.stream_data) == 0 ? 0 : -1;
}

static __regsused("d0/d1/a0/a1") LONG LineAParameterBlock(VOID) =
  "\tmove.l\td2,-(sp)\n"
  "\tmove.l\ta2,-(sp)\n"
  "\tdc.w\t$A000\n"
  "\tmove.l\t(sp)+,a2\n"
  "\tmove.l\t(sp)+,d2\n";

int SLINPUT_GetTerminalWidth_Default(const SLINPUT_State *state,
    SLINPUT_Stream stream_in, uint16_t *width) {
  *width = 0;
  const char *env_columns = getenv("SLINPUT_COLUMNS");
  if (env_columns) {
    int env_columns_number = atoi(env_columns);
    if (env_columns_number >= SLINPUT_MIN_COLUMNS &&
        env_columns_number <= SLINPUT_MAX_COLUMNS) {
      *width = (uint16_t) env_columns_number;
    }
  }

  if (!*width ) {
    /* Use negative line-a variable */
    const uint8_t *pb = (const uint8_t *) LineAParameterBlock();
    *width = *(const uint16_t *)(pb-0x2c) + 1;
  }

  return 0;
}

/* Create default versions of input and output streams */
int SLINPUT_CreateStreams_Default(const SLINPUT_State *state,
    SLINPUT_Stream *stream_in, SLINPUT_Stream *stream_out) {
  stream_in->stream_data = stdin;
  stream_out->stream_data = stdout;
  return 0;
}

void SLINPUT_DestroyStreams_Default(const SLINPUT_State *state,
    SLINPUT_Stream *stream_in, SLINPUT_Stream *stream_out) {
  stream_in->stream_data = NULL;
  stream_out->stream_data = NULL;
}

static const char *SLINPUT_CursorControlTable[SLINPUT_CCC_MAX] = {
  "\033C",    /* SLINPUT_CCC_CURSOR_RIGHT */
  "\033D",    /* SLINPUT_CCC_CURSOR_LEFT */
  "\033K",    /* SLINPUT_CCC_CLEAR_TO_END_OF_LINE */
  "\033e",    /* SLINPUT_CCC_ENABLE_CURSOR */
  "\033f",    /* SLINPUT_CCC_DISABLE_CURSOR */
  "\033j",    /* SLINPUT_CCC_SAVE_CURSOR */
  "\033k",    /* SLINPUT_CCC_RESTORE_CURSOR */
  "\033l",    /* SLINPUT_CCC_CLEAR_LINE */
  "\033v",    /* SLINPUT_CCC_WRAP_ON */
  "\033w"     /* SLINPUT_CCC_WRAP_OFF */
};

int SLINPUT_CursorControl_Default(
    const SLINPUT_State *state,
    SLINPUT_Stream stream_out,
    SLINPUT_CursorControlCode cursor_control_code) {
  if (cursor_control_code >= SLINPUT_CCC_MAX)
    return -1;

  const char *code = SLINPUT_CursorControlTable[cursor_control_code];
  return fprintf((FILE *)stream_out.stream_data, "%s", code);
}
