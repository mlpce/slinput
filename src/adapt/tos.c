#include <stdlib.h>

#if defined(__TOS__) && defined(__PUREC__)
#include <tos.h>
#include <linea.h>
#elif (defined(ATARI) && defined(LATTICE))
#include <tos.h>
#include <linea.h>
#elif defined (__VBCC__)
#include <tos.h>
#elif (defined(__GNUC__) && defined(__atarist__))
#include <osbind.h>
#include <mint/linea.h>
#else
#error What includes for this compiler?
#endif

#include "include/slinput.h"
#include "src/slinputi.h"

typedef struct TOSInputStream {
  const unsigned char *linea_pb;  /**< Pointer to LineA parameter block */
  sli_ushort env_width;  /**< Width of terminal from environment */
} TOSInputStream;

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
    SLINPUT_Stream stream_in, SLINPUT_KeyCode *key_code, sli_char *character) {
  const long cin = Cnecin();

  const long kbshift_state = Kbshift(-1);

  const int control = !!(kbshift_state & 0x4);
  const int left_shift = !!(kbshift_state & 0x1);
  const int right_shift = !!(kbshift_state & 0x2);
  const int shifted = left_shift | right_shift;

  const unsigned char kcv = (unsigned char) (cin >> 16);
  const unsigned char chv = (unsigned char) (cin & 0xff);

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
    *character = (sli_char) chv;

  return 0;
}

int SLINPUT_IsCharAvailable_Default(const SLINPUT_State *state,
    SLINPUT_Stream stream_in) {
  return !!Cconis();
}

int SLINPUT_IsSpace_Default(const SLINPUT_State *state,
    SLINPUT_Stream stream_in, sli_char character) {
  return character == 32;
}

void *SLINPUT_Malloc_Default(SLINPUT_AllocInfo alloc_info, size_t size) {
  return malloc(size);
}

void SLINPUT_Free_Default(SLINPUT_AllocInfo alloc_info, void *ptr) {
  free(ptr);
}

int SLINPUT_Putchar_Default(const SLINPUT_State *state,
    SLINPUT_Stream stream_out, sli_char c) {
  Cconout(c);

  if (c == '\n')
    Cconout('\r');

  return 0;
}

int SLINPUT_Flush_Default(const SLINPUT_State *state,
    SLINPUT_Stream stream_out) {
  return 0;
}

int SLINPUT_GetTerminalWidth_Default(const SLINPUT_State *state,
    SLINPUT_Stream stream_in, sli_ushort *width) {
  TOSInputStream *input = (TOSInputStream *) stream_in.stream_data;

  if (input->env_width) {
    *width = input->env_width;
  } else if (input->linea_pb) {
    /* Use negative line-a variable */
    *width = (sli_ushort) (*(const sli_ushort *)(input->linea_pb-0x2c) + 1u);
  } else {
    /* Can't get terminal width */
    return -1;
  }

  return 0;
}

#ifdef __VBCC__
static __regsused("d0/d1/a0/a1") LONG GetLineA_PB(VOID) =
  "\tmove.l\td2,-(sp)\n"
  "\tmove.l\ta2,-(sp)\n"
  "\tdc.w\t$A000\n"
  "\tmove.l\t(sp)+,a2\n"
  "\tmove.l\t(sp)+,d2\n";
#endif

/* Create default versions of input and output streams */
int SLINPUT_CreateStreams_Default(const SLINPUT_State *state,
    SLINPUT_Stream *stream_in, SLINPUT_Stream *stream_out) {
  const char *env_columns = getenv("SLINPUT_COLUMNS");
  const TermInfo *term_info = &state->term_info;
  TOSInputStream *input = term_info->malloc_in(term_info->alloc_info,
    sizeof(TOSInputStream));

  if (!input)
    return -1;

  input->env_width = 0;
  if (env_columns) {
    int env_columns_number = atoi(env_columns);
    if (env_columns_number >= SLINPUT_MIN_COLUMNS &&
        env_columns_number <= SLINPUT_MAX_COLUMNS) {
      input->env_width = (sli_ushort) env_columns_number;
    }
  }

  if (!input->env_width) {
    /* Width not specified by environment, use LineA */
#if defined(__TOS__) && defined(__PUREC__)
    if (!Linea)
      linea_init();
    input->linea_pb = (const unsigned char *) Linea;
#elif (defined(ATARI) && defined(LATTICE))
    input->linea_pb = (const unsigned char *) linea0();
#elif defined (__VBCC__)
    input->linea_pb = (const unsigned char *) GetLineA_PB();
#elif (defined(__GNUC__) && defined(__atarist__))
    if (!__aline)
      linea0();
    input->linea_pb = (const unsigned char *) __aline;
#else
#error How to get LineA parameter block?
#endif
  }

  stream_in->stream_data = input;
  stream_out->stream_data = NULL;

  return 0;
}

void SLINPUT_DestroyStreams_Default(const SLINPUT_State *state,
    SLINPUT_Stream *stream_in, SLINPUT_Stream *stream_out) {
  const TermInfo *term_info = &state->term_info;

  term_info->free_in(term_info->alloc_info, stream_in->stream_data);
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
  "\033l\r",  /* SLINPUT_CCC_CLEAR_LINE */
  "\033v",    /* SLINPUT_CCC_WRAP_ON */
  "\033w"     /* SLINPUT_CCC_WRAP_OFF */
};

int SLINPUT_CursorControl_Default(
    const SLINPUT_State *state,
    SLINPUT_Stream stream_out,
    SLINPUT_CursorControlCode cursor_control_code) {
  const char *code;
  if (cursor_control_code >= SLINPUT_CCC_MAX)
    return -1;

  code = SLINPUT_CursorControlTable[cursor_control_code];
  (void) Cconws(code);
  return 0;
}
