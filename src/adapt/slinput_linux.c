#include <sys/select.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wctype.h>
#include <wchar.h>
#include <errno.h>
#include <limits.h>

#include "include/slinput.h"
#include "src/slinputi.h"

/** Input stream state */
typedef struct LinuxInputStream {
  FILE *file;  /**< File pointer for input stream */
  size_t buffer_size;  /**< The size of buffer in bytes */
  char *buffer;  /**< holds input bytes */
  size_t buffer_write_index;  /**< buffer index of next character to write */
  size_t buffer_read_index;  /**< buffer index of next character to read */
} LinuxInputStream;

int SLINPUT_EnterRaw_Default(
    const SLINPUT_State *state,
    SLINPUT_Stream stream_in,
    SLINPUT_TermAttr *original_term_attr) {
  const LinuxInputStream *input = (LinuxInputStream *) stream_in.stream_data;
  const int fd = fileno(input->file);
  struct termios term_attr;
  int result = tcgetattr(fd, &term_attr);

  original_term_attr->term_attr_data = NULL;
  if (result == -1) {
    result = -errno;
  } else {
    const TermInfo *term_info = &state->term_info;
    struct termios *prev_term_attr =
      term_info->malloc_in(term_info->alloc_info, sizeof(term_attr));
    if (prev_term_attr) {
      *prev_term_attr = term_attr;

      term_attr.c_iflag &= ~((tcflag_t)(IGNBRK | BRKINT | PARMRK | ISTRIP |
        INLCR | IGNCR | ICRNL | IXON));
      term_attr.c_lflag &=
        ~((tcflag_t)(ECHO | ECHONL | ICANON | ISIG | IEXTEN));
      term_attr.c_cflag &= ~((tcflag_t)CSIZE);
      term_attr.c_cflag |= CS8;
      term_attr.c_cc[VMIN] = 1;
      term_attr.c_cc[VTIME] = 0;
      result = tcsetattr(fd, TCSAFLUSH, &term_attr);
      if (result == -1) {
        result = -errno;
        term_info->free_in(term_info->alloc_info, prev_term_attr);
      } else {
        original_term_attr->term_attr_data = prev_term_attr;
      }
    } else {
      result = -1;
    }
  }

  return result;
}

int SLINPUT_LeaveRaw_Default(
    const SLINPUT_State *state,
    SLINPUT_Stream stream_in,
    SLINPUT_TermAttr previous_attr) {
  const TermInfo *term_info = &state->term_info;
  const LinuxInputStream *input = (LinuxInputStream *) stream_in.stream_data;
  const int fd = fileno(input->file);
  struct termios *prev_term_attr =
    (struct termios *) previous_attr.term_attr_data;
  int result = 0;
  if (prev_term_attr) {
    result = tcsetattr(fd, TCSAFLUSH, prev_term_attr);
    if (result == -1)
      result = -errno;
  }

  term_info->free_in(term_info->alloc_info, prev_term_attr);
  return result;
}

/** Mapping of escape sequence to key code */
typedef struct EscapeSequenceMapping {
  const char *sequence;  /**< The escape sequence */
  SLINPUT_KeyCode key_code_mapped;  /**< The mapped key code */
} EscapeSequenceMapping;

/* Input escape sequence mappings */
static const EscapeSequenceMapping M_SequenceMapping[] = {
  { "\033[C", SLINPUT_KC_RIGHT }, /* Cursor right */
  { "\033[D", SLINPUT_KC_LEFT }, /* Cursor left */
  { "\033[A", SLINPUT_KC_UP }, /* Cursor up */
  { "\033[B", SLINPUT_KC_DOWN }, /* Cursor down */
  { "\033[3~", SLINPUT_KC_DEL }, /* Delete */
  { "\033[1;2D", SLINPUT_KC_WARP_LEFT }, /* With shift cursor warp left */
  { "\033[1;2C", SLINPUT_KC_WARP_RIGHT }, /* With shift cursor warp right */
  { "\033[1;5D", SLINPUT_KC_WARP_LEFT }, /* With control cursor warp left */
  { "\033[1;5C", SLINPUT_KC_WARP_RIGHT }, /* With control cursor warp right */
  { "\033[H", SLINPUT_KC_HOME }, /* Home cursor to start of line */
  { "\033[F", SLINPUT_KC_END }, /* End cursor to end of line */
  { NULL, SLINPUT_KC_NUL } /* End of mappings */
};

/** Mapping of character to key code */
typedef struct CharMapping {
  char char_in;  /**< The input character */
  SLINPUT_KeyCode key_code_mapped;  /**< The input key code */
} CharMapping;

static const CharMapping M_CharMapping[] = {
  { '\x7f', SLINPUT_KC_BACKSPACE },
  { '\x04', SLINPUT_KC_END_OF_TRANSMISSION },
  { '\x1b', SLINPUT_KC_ESCAPE },
  { '\t', SLINPUT_KC_TAB },
  { '\0' }
};

static int IsCharAvailableOnFd(LinuxInputStream *input) {
  const int fd = fileno(input->file);
  fd_set readfds;
  struct timeval timeout;
  int sel_rv;
  timeout.tv_sec = 0;
  timeout.tv_usec = 0;

  FD_ZERO(&readfds);
  FD_SET(fd, &readfds);

  sel_rv = select(fd + 1, &readfds, NULL, NULL, &timeout);
  if (sel_rv == -1)
    sel_rv = -errno;

  return sel_rv;
}

/* Get an input key code and character */
int SLINPUT_GetCharIn_Default(
    const SLINPUT_State *state,
    SLINPUT_Stream stream_in,
    SLINPUT_KeyCode *key_code,
    SLICHAR *character) {
  LinuxInputStream *input = (LinuxInputStream *) stream_in.stream_data;
  int result = 0;
  SLINPUT_KeyCode key_code_input = SLINPUT_KC_NUL;
  SLICHAR wchar_input = L'\0';

  if (key_code)
    *key_code = SLINPUT_KC_NUL;

  if (character)
    *character = '\0';

  if (input->buffer_read_index == input->buffer_write_index) {
    /* More input required */
    const int fd = fileno(input->file);
    char char_in = '\0';
    /* Blocking read */
    ssize_t bytes_read = read(fd, &char_in, 1);
    input->buffer_read_index = 0;
    input->buffer_write_index = 0;

    if (bytes_read == -1) {
      /* Error */
      result = -errno;
    } else if (bytes_read != 1) {
      /* EOF */
      result = -1;
    } else {
      /* Read one byte */
      input->buffer[input->buffer_write_index++] = char_in;
      input->buffer[input->buffer_write_index] = '\0';

      /* Get any subsequent characters if available, e.g. escape sequence or
      pasted text. */
      while ((result = IsCharAvailableOnFd(input)) > 0 &&
          input->buffer_write_index < input->buffer_size - 1) {
        char char_in_next = '\0';
        bytes_read = read(fd, &char_in_next, 1);
        if (bytes_read == -1) {
          /* Error */
          result = -errno;
          break;
        } else if (bytes_read != 1) {
          /* EOF */
          result = -1;
          break;
        } else {
          /* Read one byte */
          input->buffer[input->buffer_write_index++] = char_in_next;
          input->buffer[input->buffer_write_index] = '\0';

          if (char_in_next == '\n') {
            /* Got a line */
            break;
          }
        }
      }

      if (result > 0)
        result = 0;
    }
  }

  if (result < 0) {
    /* Error reading from input stream */
    input->buffer_read_index = 0;
    input->buffer_write_index = 0;
    return result;
  }

  if (0) {
    size_t i;
    printf("BUFFER: ");
    for (i = input->buffer_read_index;
        i < input->buffer_write_index; ++i) {
      printf("0x%x ", input->buffer[i]);
    }
    printf("\n");
  }

  if (input->buffer[input->buffer_read_index] == '\033' &&
      input->buffer_write_index - input->buffer_read_index > 1) {
    /* escape sequence */
    int16_t sequence_mapping_index;
    for (sequence_mapping_index = 0;
        M_SequenceMapping[sequence_mapping_index].sequence;
        ++sequence_mapping_index) {
      const size_t sequence_len =
        strlen(M_SequenceMapping[sequence_mapping_index].sequence);
      if (input->buffer_write_index - input->buffer_read_index ==
          sequence_len && strncmp(input->buffer,
          M_SequenceMapping[sequence_mapping_index].sequence,
          sequence_len) == 0) {
        key_code_input =
          M_SequenceMapping[sequence_mapping_index].key_code_mapped;
        break;
      }
    }

    input->buffer_read_index = 0;
    input->buffer_write_index = 0;
  } else {
    /* Input character(s) */
    /* Check if it's a mapping */
    int16_t char_mapping_index;
    for (char_mapping_index = 0;
        M_CharMapping[char_mapping_index].char_in; ++char_mapping_index) {
      if (input->buffer[input->buffer_read_index] ==
          M_CharMapping[char_mapping_index].char_in) {
        key_code_input = M_CharMapping[char_mapping_index].key_code_mapped;
        ++input->buffer_read_index;
        break;
      }
    }

    if (key_code_input == SLINPUT_KC_NUL) {
      const char *src_ptr = &input->buffer[input->buffer_read_index];
      wchar_t convert[2] = { L'\0' };
      mbstate_t mbs;
      memset(&mbs, 0, sizeof(mbs));
      if (mbsrtowcs(convert, &src_ptr, 1, &mbs) == (size_t) -1) {
        result = -errno;
      } else {
        wchar_input = convert[0];
        input->buffer_read_index +=
          (size_t) (src_ptr - &input->buffer[input->buffer_read_index]);
        result = 0;
      }
    }
  }

  if (result < 0) {
    input->buffer_read_index = 0;
    input->buffer_write_index = 0;
  }

  if (key_code)
    *key_code = key_code_input;

  if (character)
    *character = wchar_input;

  return result;
}

int SLINPUT_IsCharAvailable_Default(
    const SLINPUT_State *state,
    SLINPUT_Stream stream_in) {
  LinuxInputStream *input = (LinuxInputStream *) stream_in.stream_data;
  if (input->buffer_read_index < input->buffer_write_index)
    return 1;

  return IsCharAvailableOnFd(input);
}

int SLINPUT_IsSpace_Default(
    const SLINPUT_State *state,
    SLINPUT_Stream stream_in,
    SLICHAR character) {
  return iswspace((wint_t) character);
}

void *SLINPUT_Malloc_Default(
    SLINPUT_AllocInfo alloc_info,
    size_t size) {
  return malloc(size);
}

void SLINPUT_Free_Default(
    SLINPUT_AllocInfo alloc_info,
    void *ptr) {
  free(ptr);
}

static char *WideCharToChar(
    const SLINPUT_State *state,
    const wchar_t *wide_string,
    size_t *num_mchars_out) {
  const TermInfo *term_info = &state->term_info;
  const wchar_t *w_ptr = wide_string;
  size_t num_mchars;
  mbstate_t mbs;
  char *multibyte_buffer;
  *num_mchars_out = 0;

  memset(&mbs, 0, sizeof(mbs));
  num_mchars = wcsrtombs(NULL, &w_ptr, 0, &mbs);
  if (num_mchars == (size_t) -1)
    return NULL;

  multibyte_buffer = term_info->malloc_in(term_info->alloc_info,
    num_mchars + 1);
  if (multibyte_buffer) {
    w_ptr = wide_string;
    memset(&mbs, 0, sizeof(mbs));
    if (wcsrtombs(multibyte_buffer, &w_ptr, num_mchars, &mbs) == num_mchars) {
      *num_mchars_out = num_mchars;
      multibyte_buffer[num_mchars] = '\0';
    } else {
      term_info->free_in(term_info->alloc_info, multibyte_buffer);
      multibyte_buffer = NULL;
    }
  }

  return multibyte_buffer;
}

static const char *SLINPUT_CursorControlTable[SLINPUT_CCC_MAX] = {
  "\033[1C",    /* SLINPUT_CCC_CURSOR_RIGHT */
  "\033[1D",    /* SLINPUT_CCC_CURSOR_LEFT */
  "\033[0K",    /* SLINPUT_CCC_CLEAR_TO_END_OF_LINE */
  "\033[?25h",  /* SLINPUT_CCC_ENABLE_CURSOR */
  "\033[?25l",  /* SLINPUT_CCC_DISABLE_CURSOR */
  "\033[s",     /* SLINPUT_CCC_SAVE_CURSOR */
  "\033[u",     /* SLINPUT_CCC_RESTORE_CURSOR */
  "\033[2K\r",  /* SLINPUT_CCC_CLEAR_LINE */
  "\033[7h",    /* SLINPUT_CCC_WRAP_ON */
  "\033[7l"     /* SLINPUT_CCC_WRAP_OFF */
};

int SLINPUT_CursorControl_Default(
    const SLINPUT_State *state,
    SLINPUT_Stream stream_out,
    SLINPUT_CursorControlCode cursor_control_code) {
  const char *code;

  if (cursor_control_code >= SLINPUT_CCC_MAX)
    return -1;

  code = SLINPUT_CursorControlTable[cursor_control_code];
  return fprintf((FILE *)stream_out.stream_data, "%s", code);
}

int SLINPUT_Putchar_Default(
    const SLINPUT_State *state,
    SLINPUT_Stream stream_out,
    SLICHAR c) {
  size_t num_mchars = 0;
  char *multibyte_buffer;
  wchar_t wide_string[2];
  int result;

  wide_string[0] = c;
  wide_string[1] = L'\0';

  multibyte_buffer = WideCharToChar(state, wide_string, &num_mchars);
  if (multibyte_buffer == NULL)
    return -1;

  result = fprintf((FILE *)stream_out.stream_data, "%s", multibyte_buffer);
  free(multibyte_buffer);

  if ((size_t) result == num_mchars)
    result = 0;
  else
    result = -1;

  return result;
}

int SLINPUT_Flush_Default(
    const SLINPUT_State *state,
    SLINPUT_Stream stream_out) {
  int result = fflush((FILE *) stream_out.stream_data);
  if (result == EOF)
    result = -errno;

  return result;
}

int SLINPUT_GetTerminalWidth_Default(
    const SLINPUT_State *state,
    SLINPUT_Stream stream_in,
    uint16_t *width) {
  LinuxInputStream *input = (LinuxInputStream *) stream_in.stream_data;
  const int fd = fileno(input->file);
  const char *env_columns = getenv("SLINPUT_COLUMNS");
  int result = 0;

  *width = 0;
  if (env_columns) {
    int env_columns_number = atoi(env_columns);
    if (env_columns_number >= SLINPUT_MIN_COLUMNS &&
        env_columns_number <= SLINPUT_MAX_COLUMNS) {
      *width = (uint16_t) env_columns_number;
    }
  }

  if (!*width) {
    struct winsize ws;
    result = ioctl(fd, TIOCGWINSZ, &ws);
    if (result == -1)
      result = -errno;
    else
      *width = ws.ws_col;
  }

  return result;
}

/* Create default versions of input and output streams */
int SLINPUT_CreateStreams_Default(
    const SLINPUT_State *state,
    SLINPUT_Stream *stream_in,
    SLINPUT_Stream *stream_out) {
  const TermInfo *term_info = &state->term_info;
  LinuxInputStream *input = term_info->malloc_in(term_info->alloc_info,
    sizeof(LinuxInputStream));

  if (input == NULL)
    return -1;

  input->buffer_size = (SLINPUT_MAX_COLUMNS * MB_LEN_MAX) + 1;
  input->buffer = term_info->malloc_in(term_info->alloc_info,
    input->buffer_size);
  if (input->buffer == NULL) {
    term_info->free_in(term_info->alloc_info, input);
    return -1;
  }

  input->file = stdin;
  input->buffer[0] = '\0';
  input->buffer_write_index = 0;
  input->buffer_read_index = 0;

  stream_in->stream_data = input;
  stream_out->stream_data = stdout;
  return 0;
}

void SLINPUT_DestroyStreams_Default(
    const SLINPUT_State *state,
    SLINPUT_Stream *stream_in,
    SLINPUT_Stream *stream_out) {
  const TermInfo *term_info = &state->term_info;
  LinuxInputStream *input = (LinuxInputStream*) stream_in->stream_data;
  term_info->free_in(term_info->alloc_info, input->buffer);
  term_info->free_in(term_info->alloc_info, input);
  stream_in->stream_data = NULL;
  stream_out->stream_data = NULL;
}
