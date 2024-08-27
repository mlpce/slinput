#include <stdlib.h>
#include <stddef.h>
#include <assert.h>

#include "include/slinput.h"
#include "src/slinputi.h"

static const sli_char EmptyString[1] = { '\0' };

/* Check pointers are within expected ranges */
static void CheckState(SLINPUT_State *state) {
  assert(state->line_info.end_ptr >= state->line_info.buffer);
  assert(state->line_info.cursor_ptr >= state->line_info.buffer);
  assert(state->line_info.scroll_ptr >= state->line_info.buffer);

  assert(state->line_info.end_ptr <= state->line_info.buffer +
    state->line_info.max_chars);
  assert(state->line_info.cursor_ptr <= state->line_info.end_ptr);
  assert(state->line_info.scroll_ptr <= state->line_info.cursor_ptr);
}

/* Minimum of two integer values, used when checking for negative error
values. */
static int Minimum(int value1, int value2) {
  return value1 < value2 ? value1 : value2;
}

/* Finds the start of a word by searching leftwards, using spaces as the
delimeter. */
static sli_char *FindStartOfWord(SLINPUT_State *state,
    const sli_char *buffer, sli_char *cursor_ptr) {
  const TermInfo *term_info = &state->term_info;
  while (cursor_ptr > buffer) {
    if (!term_info->is_space_in(state, term_info->stream_in, *cursor_ptr) &&
        term_info->is_space_in(state, term_info->stream_in,
          *(cursor_ptr - 1))) {
      break;
    }
    --cursor_ptr;
  }
  return cursor_ptr;
}

/* Skips spaces leftwards */
static sli_char *SkipSpacesLeft(SLINPUT_State *state,
    const sli_char *buffer, sli_char *cursor_ptr) {
  const TermInfo *term_info = &state->term_info;
  while (cursor_ptr > buffer) {
    if (!term_info->is_space_in(state, term_info->stream_in, *cursor_ptr))
      break;
    --cursor_ptr;
  }
  return cursor_ptr;
}

/* Skips rightwards until a space is found */
static sli_char *SkipWordRight(SLINPUT_State *state,
    const sli_char *end_ptr, sli_char *cursor_ptr) {
  const TermInfo *term_info = &state->term_info;
  while (cursor_ptr < end_ptr) {
    if (term_info->is_space_in(state, term_info->stream_in, *cursor_ptr))
      break;
    ++cursor_ptr;
  }
  return cursor_ptr;
}

/* Skips spaces rightwards */
static sli_char *SkipSpacesRight(SLINPUT_State *state,
    const sli_char *end_ptr, sli_char *cursor_ptr) {
  const TermInfo *term_info = &state->term_info;
  while (cursor_ptr < end_ptr) {
    if (!term_info->is_space_in(state, term_info->stream_in, *cursor_ptr))
      break;
    ++cursor_ptr;
  }
  return cursor_ptr;
}

/* Outputs characters until a nil or the max_chars count is reached */
static int OutputMaxChars(SLINPUT_State *state,
    ptrdiff_t max_chars, const sli_char *str) {
  SLINPUT_Stream stream = state->term_info.stream_out;
  SLINPUT_Putchar *putchar_out = state->term_info.putchar_out;
  int result = 0;
  while (max_chars-- > 0 && *str)
    result = Minimum(result, (*putchar_out)(state, stream, *str++));

  return result;
}

/* Outputs characters until a nil is reached */
static int OutputChars(SLINPUT_State *state, const sli_char *str) {
  SLINPUT_Stream stream = state->term_info.stream_out;
  SLINPUT_Putchar *putchar_out = state->term_info.putchar_out;
  int result = 0;
  while (*str)
    result = Minimum(result, (*putchar_out)(state, stream, *str++));

  return result;
}

/* Copies characters until a nil or the max_chars count is reached. Returns
pointer to the destination terminating nil character. */
static sli_char *CopyChars(sli_ushort max_chars, const sli_char *str,
    sli_char *dst_ptr) {
  while (max_chars-- > 0 && *str)
    *dst_ptr++ = *str++;
  *dst_ptr = '\0';
  return dst_ptr;
}

/* Complete input of the line, if nothing was entered then produce a single
newline */
static int LineEnter(SLINPUT_State *state) {
  SLINPUT_Stream stream = state->term_info.stream_out;
  int result = (*state->term_info.putchar_out)(state, stream, '\n');
  LineInfo *line_info = &state->line_info;
  if (line_info->end_ptr == line_info->buffer && line_info->max_chars) {
    *line_info->end_ptr++ = '\n';
    *line_info->end_ptr = '\0';
  }
  return result;
}

/* Completely redraws the input line, maintaining the cursor position */
static int RedrawLine(SLINPUT_State *state) {
  const TermInfo *term_info = &state->term_info;
  const LineInfo *line_info = &state->line_info;
  ptrdiff_t num_chars;
  /* Clear line and place cursor on left, output the prompt,
  output text up to cursor, save cursor position, output the
  buffer, restore cursor position */
  int result = term_info->cursor_control_out(state, term_info->stream_out,
    SLINPUT_CCC_DISABLE_CURSOR);
  result = Minimum(result,
    term_info->cursor_control_out(state, term_info->stream_out,
    SLINPUT_CCC_CLEAR_LINE));
  result = Minimum(result, OutputChars(state, line_info->prompt));

  /* Left continuation character */
  result = Minimum(result,
    term_info->putchar_out(state, term_info->stream_out,
    line_info->scroll_ptr != line_info->buffer ?
    term_info->continuation_character_left : ' '));

  result = Minimum(result,
    OutputMaxChars(state,
    line_info->cursor_ptr - line_info->scroll_ptr,
    line_info->scroll_ptr));
  result = Minimum(result,
    term_info->cursor_control_out(state, term_info->stream_out,
    SLINPUT_CCC_SAVE_CURSOR));
  num_chars = line_info->fit_len + line_info->scroll_ptr -
    line_info->cursor_ptr;
  result = Minimum(result,
    OutputMaxChars(state, num_chars, line_info->cursor_ptr));

  /* Right continuation character */
  result = Minimum(result,
    term_info->putchar_out(state, term_info->stream_out,
    line_info->scroll_ptr + line_info->fit_len < line_info->end_ptr ?
    term_info->continuation_character_right : ' '));

  result = Minimum(result,
    term_info->cursor_control_out(state, term_info->stream_out,
    SLINPUT_CCC_RESTORE_CURSOR));
  result = Minimum(result,
    term_info->cursor_control_out(state, term_info->stream_out,
    SLINPUT_CCC_ENABLE_CURSOR));

  return result;
}

/* Redraws the line from the cursor position onwards, maintaining the cursor
position */
static int RedrawLineFromCursor(SLINPUT_State *state) {
  const TermInfo *term_info = &state->term_info;
  const LineInfo *line_info = &state->line_info;
  ptrdiff_t num_chars;
  /* Clear to end of line, save cursor position,
  output string at cursor_ptr, restore cursor position. */
  int result = term_info->cursor_control_out(state, term_info->stream_out,
    SLINPUT_CCC_DISABLE_CURSOR);
  result = Minimum(result,
    term_info->cursor_control_out(state, term_info->stream_out,
    SLINPUT_CCC_CLEAR_TO_END_OF_LINE));
  result = Minimum(result,
    term_info->cursor_control_out(state, term_info->stream_out,
    SLINPUT_CCC_SAVE_CURSOR));
  num_chars = line_info->fit_len + line_info->scroll_ptr -
    line_info->cursor_ptr;
  result = Minimum(result,
    OutputMaxChars(state, num_chars, line_info->cursor_ptr));

  /* Right continuation character */
  result = Minimum(result,
    term_info->putchar_out(state, term_info->stream_out,
    line_info->scroll_ptr + line_info->fit_len < line_info->end_ptr ?
    term_info->continuation_character_right : ' '));

  result = Minimum(result,
    term_info->cursor_control_out(state, term_info->stream_out,
    SLINPUT_CCC_RESTORE_CURSOR));
  result = Minimum(result,
    term_info->cursor_control_out(state, term_info->stream_out,
    SLINPUT_CCC_ENABLE_CURSOR));

  return result;
}

/* Deletes the character to the left and move the cursor left */
static int LineBackspace(SLINPUT_State *state) {
  const TermInfo *term_info = &state->term_info;
  LineInfo *line_info = &state->line_info;

  int result = 0;
  if (line_info->cursor_ptr > line_info->buffer)  {
    sli_char *ptr;
    for (ptr = --line_info->cursor_ptr; ptr < line_info->end_ptr;
        ++ptr) {
      *ptr = *(ptr+1);
    }
    --line_info->end_ptr;

    if (line_info->cursor_ptr < line_info->scroll_ptr +
        line_info->cursor_margin) {
      if (line_info->scroll_ptr > line_info->buffer)
        --line_info->scroll_ptr;
      result = RedrawLine(state);
    } else {
      /* Backspace */
      result = Minimum(result,
        term_info->putchar_out(state, term_info->stream_out, '\b'));
      result = Minimum(result,
        RedrawLineFromCursor(state));
    }
  }

  return result;
}

/* Command completion */
static int LineTab(SLINPUT_State *state) {
  const TermInfo *term_info = &state->term_info;
  const LineInfo *line_info = &state->line_info;
  int result = 0;
  if (term_info->completion_request != NULL) {
    result = term_info->completion_request(state, term_info->completion_info,
      (sli_ushort) (line_info->end_ptr - line_info->buffer), line_info->buffer);
  }
  return result;
}

/* Clear the input line and place cursor at start of line */
static int LineEscape(SLINPUT_State *state) {
  LineInfo *line_info = &state->line_info;
  line_info->end_ptr = line_info->buffer;
  line_info->cursor_ptr = line_info->buffer;
  line_info->scroll_ptr = line_info->buffer;
  *line_info->end_ptr = '\0';

  return RedrawLine(state);
}

/* Input is ending, empty the buffer and output a new line */
static int LineEndOfTransmission(SLINPUT_State *state) {
  LineEscape(state);

  return (*state->term_info.putchar_out)(state,
    state->term_info.stream_out, '\n');
}

/* Delete the current character, cursor does not move */
static int LineDelete(SLINPUT_State *state) {
  LineInfo *line_info = &state->line_info;
  int result = 0;
  if (line_info->cursor_ptr < line_info->end_ptr) {
    sli_char *ptr;
    for (ptr = line_info->cursor_ptr; ptr < line_info->end_ptr; ++ptr)
      *ptr = *(ptr+1);
    --line_info->end_ptr;
    result = RedrawLineFromCursor(state);
  }

  return result;
}

/* Replaces the line with another string. Line is scrolled to display the end
of the string. */
static int LineReplace(SLINPUT_State *state, const sli_char *str, int redraw) {
  LineInfo *line_info = &state->line_info;
  line_info->end_ptr = CopyChars(line_info->max_chars, str,
    line_info->buffer);
  line_info->cursor_ptr = line_info->end_ptr;
  line_info->scroll_ptr = line_info->end_ptr - line_info->fit_len;
  if (line_info->scroll_ptr < line_info->buffer)
    line_info->scroll_ptr = line_info->buffer;

  return redraw ? RedrawLine(state) : 0;
}

/* Move the cursor to the left. If cursor_warp_enabled is set then move cursor
leftwards to first letter of word. */
static int LineKeyLeft(SLINPUT_State *state, int cursor_warp_enabled) {
  const TermInfo *term_info = &state->term_info;
  LineInfo *line_info = &state->line_info;
  const sli_char *orig_cursor_ptr = line_info->cursor_ptr;
  int result = 0;
  sli_char *original_scroll_ptr;
  ptrdiff_t left_delta;

  if (!cursor_warp_enabled ||
      line_info->cursor_ptr <= line_info->buffer + 1) {
    if (line_info->cursor_ptr > line_info->buffer)
      --line_info->cursor_ptr;
  } else {
    if (term_info->is_space_in(state, term_info->stream_in,
          *line_info->cursor_ptr) ||
        term_info->is_space_in(state, term_info->stream_in,
          *(line_info->cursor_ptr - 1))) {
      line_info->cursor_ptr = FindStartOfWord(state, line_info->buffer,
        SkipSpacesLeft(state, line_info->buffer, line_info->cursor_ptr - 1));
    } else {
      line_info->cursor_ptr = FindStartOfWord(state, line_info->buffer,
        line_info->cursor_ptr);
    }
  }

  if (line_info->cursor_ptr == orig_cursor_ptr) {
    /* Cursor not moved */
    return 0;
  }

  /* Adjust scroll pointer if necessary */
  original_scroll_ptr = line_info->scroll_ptr;
  left_delta = line_info->scroll_ptr + line_info->cursor_margin -
    line_info->cursor_ptr;
  if (left_delta > 0) {
    line_info->scroll_ptr -= left_delta;
    if (line_info->scroll_ptr < line_info->buffer)
      line_info->scroll_ptr = line_info->buffer;
  }

  if (line_info->scroll_ptr != original_scroll_ptr) {
    /* Scroll pointer has changed so redraw the line */
    result = RedrawLine(state);
  } else {
    /* Move cursor leftwards */
    while (line_info->cursor_ptr < orig_cursor_ptr--) {
      result = Minimum(result,
        term_info->cursor_control_out(state, term_info->stream_out,
        SLINPUT_CCC_CURSOR_LEFT));
    }
  }

  return result;
}

/* Move the cursor to the right. If cursor_warp_enabled is set then move cursor
rightwards until first letter of word. */
static int LineKeyRight(SLINPUT_State *state, int cursor_warp_enabled) {
  const TermInfo *term_info = &state->term_info;
  LineInfo *line_info = &state->line_info;
  const sli_char *orig_cursor_ptr = line_info->cursor_ptr;
  int result = 0;
  sli_char *original_scroll_ptr;
  ptrdiff_t right_delta;

  if (!cursor_warp_enabled ||
      line_info->cursor_ptr >= line_info->end_ptr - 1) {
    if (line_info->cursor_ptr < line_info->end_ptr)
      ++line_info->cursor_ptr;
  } else {
    if (term_info->is_space_in(state, term_info->stream_in,
          *line_info->cursor_ptr) ||
        term_info->is_space_in(state, term_info->stream_in,
          *(line_info->cursor_ptr + 1))) {
      line_info->cursor_ptr = SkipSpacesRight(state, line_info->end_ptr,
        line_info->cursor_ptr + 1);
    } else {
      line_info->cursor_ptr = SkipSpacesRight(state, line_info->end_ptr,
        SkipWordRight(state, line_info->end_ptr, line_info->cursor_ptr + 1));
    }
  }

  if (line_info->cursor_ptr == orig_cursor_ptr) {
    /* Cursor not moved */
    return 0;
  }

  /* Adjust scroll pointer if necessary */
  original_scroll_ptr = line_info->scroll_ptr;
  right_delta = line_info->cursor_ptr - line_info->scroll_ptr -
    line_info->fit_len + line_info->cursor_margin;
  if (right_delta > 0) {
    line_info->scroll_ptr += right_delta;
    if (line_info->scroll_ptr + line_info->fit_len > line_info->end_ptr) {
      line_info->scroll_ptr = line_info->end_ptr - line_info->fit_len;
      if (line_info->scroll_ptr < line_info->buffer)
        line_info->scroll_ptr = line_info->buffer;
    }
  }

  if (line_info->scroll_ptr != original_scroll_ptr) {
    /* Scroll pointer has changed so redraw the line */
    result = RedrawLine(state);
  } else {
    /* Move cursor rightwards */
    while (line_info->cursor_ptr > orig_cursor_ptr++) {
      result = Minimum(result,
        term_info->cursor_control_out(state, term_info->stream_out,
        SLINPUT_CCC_CURSOR_RIGHT));
    }
  }

  return result;
}

/* Move the cursor to the start of the line */
static int LineHome(SLINPUT_State *state) {
  LineInfo *line_info = &state->line_info;
  line_info->cursor_ptr = line_info->buffer;
  line_info->scroll_ptr = line_info->buffer;
  return RedrawLine(state);
}

/* Move the cursor to the end of the line */
static int LineEnd(SLINPUT_State *state) {
  LineInfo *line_info = &state->line_info;
  line_info->cursor_ptr = line_info->end_ptr;
  line_info->scroll_ptr = line_info->end_ptr - line_info->fit_len;
  if (line_info->scroll_ptr < line_info->buffer)
    line_info->scroll_ptr = line_info->buffer;
  return RedrawLine(state);
}

/* Input a character and move the cursor to the right */
static int LineCharIn(SLINPUT_State *state, sli_char char_in) {
  const TermInfo *term_info = &state->term_info;
  LineInfo *line_info = &state->line_info;

  int result = 0;
  if (line_info->end_ptr - line_info->buffer < line_info->max_chars) {
    ptrdiff_t working_margin;
    sli_char *ptr;
    for (ptr = line_info->end_ptr; ptr > line_info->cursor_ptr; --ptr)
      *ptr = *(ptr-1);
    *line_info->cursor_ptr++ = char_in;
    *++line_info->end_ptr = '\0';

    working_margin = line_info->end_ptr - line_info->cursor_ptr;
    if (working_margin > line_info->cursor_margin)
      working_margin = line_info->cursor_margin;

    if (line_info->cursor_ptr - line_info->scroll_ptr > line_info->fit_len -
        working_margin) {
      /* Scroll and redraw the line */
      ++line_info->scroll_ptr;
      result = RedrawLine(state);
    } else {
      result = Minimum(result,
        term_info->cursor_control_out(state, term_info->stream_out,
        SLINPUT_CCC_DISABLE_CURSOR));

      /* Output char_in, save cursor position, output string at
      cursor_ptr, restore cursor position. */
      result = Minimum(result,
        term_info->putchar_out(state, term_info->stream_out, char_in));
      result = Minimum(result,
        term_info->cursor_control_out(state, term_info->stream_out,
        SLINPUT_CCC_SAVE_CURSOR));

      result = Minimum(result,
        OutputMaxChars(state, line_info->fit_len + line_info->scroll_ptr -
        line_info->cursor_ptr, line_info->cursor_ptr));

      /* Right continuation character */
      result = Minimum(result,
        term_info->putchar_out(state, term_info->stream_out,
          line_info->scroll_ptr + line_info->fit_len < line_info->end_ptr ?
          term_info->continuation_character_right : ' '));

      result = Minimum(result,
        term_info->cursor_control_out(state, term_info->stream_out,
        SLINPUT_CCC_RESTORE_CURSOR));

      result = Minimum(result,
        term_info->cursor_control_out(state, term_info->stream_out,
        SLINPUT_CCC_ENABLE_CURSOR));
    }
  }

  return result;
}

/* Determine the length of the string in characters */
static size_t StringLength(const sli_char *str) {
  const sli_char *ptr = str;
  while (*ptr)
    ++ptr;

  return (size_t) (ptr - str);
}

/* Applies dimension constraints derived from available columns */
static int ApplyDimension(SLINPUT_State *state) {
  const TermInfo *term_info = &state->term_info;
  LineInfo *line_info = &state->line_info;
  sli_ushort columns = term_info->columns_in;
  int result = 0;
  size_t prompt_length;
  sli_ushort cursor_margin;

  /* If columns_in is zero query the terminal width through the callback */
  if (!columns) {
    result =
      term_info->get_terminal_width(state, term_info->stream_in, &columns);
  }

  /* Check if get_terminal_width failed */
  if (result < 0)
    return result;

  /* Constrain number of columns to minimum and maximum. If the minimum
  constrains larger than the actual terminal width, the line won't render
  correctly. If the maximum constrains smaller than the actual terminal width,
  the remaining terminal columns won't be used. */
  if (columns < SLINPUT_MIN_COLUMNS)
    columns = SLINPUT_MIN_COLUMNS;
  else if (columns > SLINPUT_MAX_COLUMNS)
    columns = SLINPUT_MAX_COLUMNS;

  /* Allow last column for cursor and two columns for continuation
  characters therefore subtract 3 off columns here. As we also need a column to
  display an actual character from the input string, this gives the value for
  SLINPUT_MIN_COLUMNS as 4. The choice for SLINPUT_MAX_COLUMNS is arbitary,
  but signed 16bit values are used for column computations. */
  columns -= 3;

  /* Now we know the number of columns available, we make choices of how we
  render. The choices being the prompt, the cursor margin and the actual input
  string. The input string must always be displayed, so that takes first
  priority. Secondly we then prioritise the prompt. Thirdly, the cursor
  margin, as this just provides a scrolling convenience. */

  prompt_length = StringLength(line_info->prompt_in);
  cursor_margin = term_info->cursor_margin_in;

  /* First, can we fit in the prompt and cursor margin? */
  if (prompt_length + cursor_margin >= columns) {
    /* Not enough space, so disable the cursor margin and try again. */
    cursor_margin = 0;
  }

  /* Secondly, can we fit in the prompt? */
  if (prompt_length >= columns) {
    /* Still not enough space. Disable the prompt also. */
    prompt_length = 0;
  }

  /* Now we know what can be rendered, enable or disable the cursor margin and
  prompt accordingly. */

  /* Set the active cursor margin. */
  line_info->cursor_margin = (sli_sshort) cursor_margin;

  /* Set the active prompt. */
  if (prompt_length)
    line_info->prompt = line_info->prompt_in;
  else
    line_info->prompt = EmptyString;

  /* The number of columns of input text that fit into the line */
  line_info->fit_len = (sli_sshort) (columns - prompt_length);

  /* If there's been no change in available columns, then don't need to
  redraw anything or change the scroll_ptr. */
  if (columns == line_info->columns)
    return 0;
 
  /* Dimensions have changed. Keep the current cursor pointer and adjust the
  scroll ptr so the cursor remains on screen. */
  line_info->columns = (sli_sshort) columns;

  line_info->scroll_ptr = line_info->cursor_ptr - line_info->fit_len;
  if (line_info->scroll_ptr < line_info->buffer)
    line_info->scroll_ptr = line_info->buffer;

  /* Return 1 to indicate a redraw needs to take place. */
  return 1;
}

/* Sets memory bytes to a value */
static void MemorySet(void *memory, unsigned char value, size_t num_values) {
  unsigned char *ptr = (unsigned char *) memory;
  const unsigned char *end_ptr = ptr + num_values;
  while (ptr < end_ptr)
    *ptr++ = value;
}

/* Flushes the input stream */
static int FlushInput(SLINPUT_State *state) {
  const TermInfo *term_info = &state->term_info;

  /* Flush input */
  int result = 0;
  while (result >= 0 && (result =
      term_info->is_char_available_in(state, term_info->stream_in)) > 0) {
    /* A character is available so get it */
    result =
      term_info->get_char_in_in(state, term_info->stream_in, NULL, NULL);
  }

  return result;
}

/* Processes input until enter is pressed or end of transmission */
static int ProcessInput(SLINPUT_State *state) {
  const TermInfo *term_info = &state->term_info;
  const sli_sshort max_history_index = term_info->num_history - 1;
  sli_sshort history_index = -1;
  int result;

  /* Disable line wrap */
  term_info->cursor_control_out(state, state->term_info.stream_out,
    SLINPUT_CCC_WRAP_OFF);

  /* Initial dimensions and line draw */
  result = ApplyDimension(state);
  if (result >= 0)
    result = RedrawLine(state);

  while (result >= 0) {
    SLINPUT_KeyCode key_code = SLINPUT_KC_NUL;
    sli_char char_in = 0;
    CheckState(state);

    result = term_info->flush_out(state, term_info->stream_out);
    if (result < 0)
      break;

    result = term_info->get_char_in_in(state, term_info->stream_in, &key_code,
      &char_in);
    if (result < 0)
      break;

    result = ApplyDimension(state);
    if (result == 1) {
      /* Dimensions have changed so redraw the line */
      RedrawLine(state);
    } else if (result < 0) {
      /* Failed to get dimensions */
      break;
    }

#if 0
    printf("key_code: 0x%x char_in: 0x%x\n", key_code, char_in);
#endif

    if (key_code == SLINPUT_KC_END_OF_TRANSMISSION) {
      /* Finish with an empty buffer */
      result = LineEndOfTransmission(state);
      break;
    } else if (char_in == '\r' || char_in == '\n') {
      /* Enter */
      result = LineEnter(state);
      break;
    } else if (key_code == SLINPUT_KC_TAB) {
      /* Key: tab */
      result = LineTab(state);
    } else if (key_code == SLINPUT_KC_ESCAPE) {
      /* Key: Escape */
      result = LineEscape(state);
    } else if (key_code == SLINPUT_KC_BACKSPACE) {
      /* Key: Backspace */
      result = LineBackspace(state);
    } else if (key_code == SLINPUT_KC_DEL) {
      /* Key: Delete */
      result = LineDelete(state);
    } else if (key_code == SLINPUT_KC_UP ||
        key_code == SLINPUT_KC_DOWN) {
      /* Key: up or down */
      /* Purpose: History browsing */
      if (key_code == SLINPUT_KC_UP && history_index < 0) {
        /* Start history browsing */
        history_index = max_history_index;
      } else {
        if (key_code == SLINPUT_KC_UP && history_index > 0) {
          /* Move earlier */
          --history_index;
        } else if (key_code == SLINPUT_KC_DOWN &&
            history_index >= 0) {
          if (history_index < max_history_index) {
            /* Move later */
            ++history_index;
          } else {
            /* Finish browsing */
            history_index = -1;
          }
        }
      }

      /* Update the buffer with the browsing result */
      result = LineReplace(state, history_index < 0 ? EmptyString :
        term_info->history[history_index], 1);
    } else if (key_code == SLINPUT_KC_LEFT) {
      /* Key: left */
      result = LineKeyLeft(state, 0);
    } else if (key_code == SLINPUT_KC_WARP_LEFT) {
      /* Key: warp left */
      result = LineKeyLeft(state, 1);
    } else if (key_code == SLINPUT_KC_RIGHT) {
      /* Key: right */
      result = LineKeyRight(state, 0);
    } else if (key_code == SLINPUT_KC_WARP_RIGHT) {
      /* Key: warp right */
      result = LineKeyRight(state, 1);
    } else if (key_code == SLINPUT_KC_HOME) {
      /* Key: Home */
      result = LineHome(state);
    } else if (key_code == SLINPUT_KC_END) {
      /* Key: End */
      result = LineEnd(state);
    } else if (char_in != '\0') {
      /* Key: any printable character */
      /* Purpose: input a character and move the cursor to the right */
      result = LineCharIn(state, char_in);
    }
  }

  /* Enable line wrap */
  result = Minimum(result,
    term_info->cursor_control_out(state, state->term_info.stream_out,
    SLINPUT_CCC_WRAP_ON));

  return result;
}

/* Gets a single line input. The terminal is placed into raw mode and the input
loop executed. On completion the previous terminal mode is restored. */
int SLINPUT_Get(SLINPUT_State *state, const sli_char *prompt,
    const sli_char *initial, sli_ushort buffer_chars, sli_char *buffer) {
  const TermInfo *term_info;
  LineInfo *line_info;
  int result;

  if (!state || !prompt || !buffer_chars || !buffer)
    return -1;

  term_info = &state->term_info;
  line_info = &state->line_info;

  line_info->prompt_in = prompt;
  line_info->prompt = prompt;
  line_info->buffer_size = buffer_chars;
  line_info->max_chars = buffer_chars - 1;
  line_info->buffer = buffer;
  line_info->end_ptr = buffer;
  line_info->cursor_ptr = buffer;
  line_info->scroll_ptr = buffer;
  *buffer = '\0';

  if (initial) {
    /* Copy in the initial string, without a redraw */
    LineReplace(state, initial, 0);
  }

  /* Enter raw mode */
  result = term_info->enter_raw_in(state, term_info->stream_in,
    &state->term_info.saved_term_attr_in);
  if (result < 0)
    return result;

  /* Flush input */
  result = FlushInput(state);
  if (result >= 0) {
    /* Process input */
    result = ProcessInput(state);
  }

  /* Restore previous term info */
  result = Minimum(result,
    term_info->leave_raw_in(state, term_info->stream_in,
    term_info->saved_term_attr_in));
  state->term_info.saved_term_attr_in.term_attr_data = NULL;

  if (result >= 0)
    result = (int) (line_info->end_ptr - line_info->buffer);

  return result;
}

/* Returns non-zero if both strings are identical */
static int StringIsSame(const sli_char *string1, const sli_char *string2) {
  while (*string1 && *string2 && *string1 == *string2) {
    ++string1;
    ++string2;
  }

  return *string1 == *string2;
}

/* Saves a single line into history. '\r' and '\n' characters are removed. Up
to SLINPUT_MAX_HISTORY lines can be stored, with the oldest line being removed
when the limit is reached. */
int SLINPUT_Save(SLINPUT_State *state, const sli_char *line) {
  /* Work out how long the line is with newlines removed */
  TermInfo *term_info = &state->term_info;
  const size_t line_length = StringLength(line);
  const sli_char *line_end = line + line_length;
  size_t reduced_line_length = 0;
  const sli_char *ptr;
  sli_char *save_line;
  sli_char *dest_ptr;
  for (ptr = line; ptr < line_end; ++ptr) {
    if (*ptr != '\r' && *ptr != '\n')
      ++reduced_line_length;
  }

  if (reduced_line_length == 0)
    return term_info->num_history;

  /* Allocate and copy the line with newlines removed */
  save_line = term_info->malloc_in(term_info->alloc_info,
    sizeof(sli_char)*(reduced_line_length + 1));
  if (!save_line) {
    /* Out of memory */
    return -1;
  }

  dest_ptr = save_line;
  for (ptr = line; ptr < line_end; ++ptr) {
    if (*ptr != '\r' && *ptr != '\n')
      *dest_ptr++ = *ptr;
  }
  save_line[reduced_line_length] = '\0';

  /* Don't save the line if it is identical to the previous one */
  if (term_info->num_history && StringIsSame(save_line,
      term_info->history[term_info->num_history - 1])) {
    term_info->free_in(term_info->alloc_info, save_line);
    return term_info->num_history;
  }

  /* If history is full, then delete the oldest */
  if (term_info->num_history == SLINPUT_MAX_HISTORY) {
    sli_ushort index;
    term_info->free_in(term_info->alloc_info, term_info->history[0]);
    for (index = 1; index < SLINPUT_MAX_HISTORY; ++index)
      term_info->history[index - 1] = term_info->history[index];

    term_info->num_history--;
  } 

  /* Store the new history */
  term_info->history[term_info->num_history++] = save_line;

  return term_info->num_history;
}

/* Set function pointer */
void SLINPUT_Set_EnterRaw(SLINPUT_State *state,
    SLINPUT_EnterRaw *enter_raw_cb) {
  state->term_info.enter_raw_in = enter_raw_cb;
}

/* Set function pointer */
void SLINPUT_Set_LeaveRaw(SLINPUT_State *state,
    SLINPUT_LeaveRaw *leave_raw_cb) {
  state->term_info.leave_raw_in = leave_raw_cb;
}

/* Set function pointer */
void SLINPUT_Set_GetCharIn(SLINPUT_State *state,
    SLINPUT_GetCharIn *get_char_in_cb) {
  state->term_info.get_char_in_in = get_char_in_cb;
}

/* Set function pointer */
void SLINPUT_Set_IsCharAvailable(SLINPUT_State *state,
    SLINPUT_IsCharAvailable *is_char_available_cb) {
  state->term_info.is_char_available_in = is_char_available_cb;
}

/* Set function pointer */
void SLINPUT_Set_IsSpace(SLINPUT_State *state,
    SLINPUT_IsSpace *is_space_cb) {
  state->term_info.is_space_in = is_space_cb;
}

/* Set function pointer */
void SLINPUT_Set_CursorControl(SLINPUT_State *state,
    SLINPUT_CursorControl *cursor_control_cb) {
  state->term_info.cursor_control_out = cursor_control_cb;
}

/* Set function pointer */
void SLINPUT_Set_Putchar(SLINPUT_State *state,
    SLINPUT_Putchar *put_char_cb) {
  state->term_info.putchar_out = put_char_cb;
}

/* Set function pointer */
void SLINPUT_Set_Flush(SLINPUT_State *state,
    SLINPUT_Flush *flush_cb) {
  state->term_info.flush_out = flush_cb;
}

/* Set function pointer */
void SLINPUT_Set_CompletionRequest(SLINPUT_State *state,
    SLINPUT_CompletionInfo completion_info,
    SLINPUT_CompletionRequest *completion_request_cb) {
  state->term_info.completion_info = completion_info;
  state->term_info.completion_request = completion_request_cb;
}

/* Set function pointer */
void SLINPUT_Set_GetTerminalWidth(SLINPUT_State *state,
    SLINPUT_GetTerminalWidth *get_terminal_width_cb) {
  state->term_info.get_terminal_width = get_terminal_width_cb;
}

/* Set number of columns */
void SLINPUT_Set_NumColumns(SLINPUT_State *state,
    sli_ushort num_columns) {
  state->term_info.columns_in = num_columns;
}

/* Set cursor margin */
void SLINPUT_Set_CursorMargin(SLINPUT_State *state,
    sli_ushort cursor_margin) {
  state->term_info.cursor_margin_in = cursor_margin;
}

/* Set left continuation character */
void SLINPUT_Set_ContinuationCharacterLeft(
    SLINPUT_State *state,
    sli_char continuation_character_left) {
  state->term_info.continuation_character_left = continuation_character_left;
}

/* Set right continuation character */
void SLINPUT_Set_ContinuationCharacterRight(
    SLINPUT_State *state,
    sli_char continuation_character_right) {
  state->term_info.continuation_character_right = continuation_character_right;
}

/* Set I/O streams */
void SLINPUT_Set_Streams(SLINPUT_State *state,
    SLINPUT_Stream stream_in, SLINPUT_Stream stream_out) {
  state->term_info.stream_in = stream_in;
  state->term_info.stream_out = stream_out;
}

/* Creates the state */
SLINPUT_State *SLINPUT_CreateState(
    SLINPUT_AllocInfo alloc_info,
    SLINPUT_Malloc malloc_cb, SLINPUT_Free free_cb) {
  SLINPUT_CompletionInfo completion_info = { NULL };
  SLINPUT_State *state;
  TermInfo *term_info;

  assert(sizeof(sli_char) == SLI_CHAR_SIZE);

  if (!malloc_cb)
    malloc_cb = SLINPUT_Malloc_Default;
  if (!free_cb)
    free_cb = SLINPUT_Free_Default;

  state = (*malloc_cb)(alloc_info, sizeof(SLINPUT_State));
  if (!state)
    return NULL;

  MemorySet(state, 0, sizeof(SLINPUT_State));

  term_info = &state->term_info;
  term_info->alloc_info = alloc_info;
  term_info->malloc_in = malloc_cb;
  term_info->free_in = free_cb;

  /* Create the default I/O streams */
  if (SLINPUT_CreateStreams_Default(state, &term_info->stream_in_default,
        &term_info->stream_out_default) < 0) {
      (*free_cb)(alloc_info, state);
    return NULL;
  }

  term_info->stream_in = term_info->stream_in_default;
  term_info->stream_out = term_info->stream_out_default;

  SLINPUT_Set_EnterRaw(state, SLINPUT_EnterRaw_Default);
  SLINPUT_Set_LeaveRaw(state, SLINPUT_LeaveRaw_Default);
  SLINPUT_Set_GetCharIn(state, SLINPUT_GetCharIn_Default);
  SLINPUT_Set_IsCharAvailable(state, SLINPUT_IsCharAvailable_Default);
  SLINPUT_Set_IsSpace(state, SLINPUT_IsSpace_Default);
  SLINPUT_Set_CursorControl(state, SLINPUT_CursorControl_Default);
  SLINPUT_Set_Putchar(state, SLINPUT_Putchar_Default);
  SLINPUT_Set_Flush(state, SLINPUT_Flush_Default);
  SLINPUT_Set_GetTerminalWidth(state, SLINPUT_GetTerminalWidth_Default);
  SLINPUT_Set_CompletionRequest(state, completion_info,
    (SLINPUT_CompletionRequest *) NULL);

  SLINPUT_Set_NumColumns(state, 0);
  SLINPUT_Set_CursorMargin(state, 5);
  SLINPUT_Set_ContinuationCharacterLeft(state, '<');
  SLINPUT_Set_ContinuationCharacterRight(state, '>');

  return state;
}

/* Destroys the state */
void SLINPUT_DestroyState(SLINPUT_State *state) {
  TermInfo *term_info;
  sli_ushort index;

  if (!state)
    return;

  term_info = &state->term_info;

  SLINPUT_DestroyStreams_Default(state, &term_info->stream_in_default,
    &term_info->stream_out_default);

  for (index = 0; index < term_info->num_history; ++index)
    term_info->free_in(term_info->alloc_info, term_info->history[index]);

  term_info->free_in(term_info->alloc_info, state);
}

/* Replaces the line during completion */
int SLINPUT_CompletionReplace(SLINPUT_State *state, const sli_char *string) {
  return LineReplace(state, string, 1);
}
