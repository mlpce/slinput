/**
@file slinput_internal.h
@brief slinput header
*/
#ifndef SLINPUT_INTERNAL_HEADER
#define SLINPUT_INTERNAL_HEADER
#include "include/slinput.h"

/** The maximum number of lines stored as history */
#ifndef SLINPUT_MAX_HISTORY
#define SLINPUT_MAX_HISTORY 32
#endif

/** The minimum number of columns in a line */
#ifndef SLINPUT_MIN_COLUMNS
#define SLINPUT_MIN_COLUMNS 4
#endif

/** The maximum number of columns in a line */
#ifndef SLINPUT_MAX_COLUMNS
#define SLINPUT_MAX_COLUMNS 640
#endif

/* Default input functions. */

/** Default function for entering raw mode */
SLINPUT_EnterRaw SLINPUT_EnterRaw_Default;
/** Default function for leaving raw mode */
SLINPUT_LeaveRaw SLINPUT_LeaveRaw_Default;
/** Default function for getting an input character and key code */
SLINPUT_GetCharIn SLINPUT_GetCharIn_Default;
/** Default function for checking if input is available */
SLINPUT_IsCharAvailable SLINPUT_IsCharAvailable_Default;
/** Default function for checking if a character is a space */
SLINPUT_IsSpace SLINPUT_IsSpace_Default;
/** Default function for memory allocation */
SLINPUT_Malloc SLINPUT_Malloc_Default;
/** Default function for memory freeing */
SLINPUT_Free SLINPUT_Free_Default;

/* Default output functions. */

/** Default function for cursor control */
SLINPUT_CursorControl SLINPUT_CursorControl_Default;
/** Default function for putting a character to output */
SLINPUT_Putchar SLINPUT_Putchar_Default;
/** Default function for flushing output */
SLINPUT_Flush SLINPUT_Flush_Default;
/** Default function for getting the terminal width */
SLINPUT_GetTerminalWidth SLINPUT_GetTerminalWidth_Default;

/** Create default versions of input and output streams. Return a negative
value on error. */
int SLINPUT_CreateStreams_Default(
  const SLINPUT_State *state,
  SLINPUT_Stream *stream_in,
  SLINPUT_Stream *stream_out);

/** Destroy default versions of input and output streams */
void SLINPUT_DestroyStreams_Default(
  const SLINPUT_State *state,
  SLINPUT_Stream *stream_in,
  SLINPUT_Stream *stream_out);

/** Terminal information, callbacks and state */
typedef struct TermInfo {
  SLINPUT_Stream stream_in_default;  /**< The default input stream */
  SLINPUT_Stream stream_out_default;  /**< The default output stream */
  SLINPUT_Stream stream_out;  /**< The actual output stream */
  SLINPUT_GetTerminalWidth *get_terminal_width;  /**< Callback pointer */
  SLINPUT_CursorControl *cursor_control_out;  /**< Callback pointer */
  SLINPUT_Putchar *putchar_out;  /**< Callback pointer */
  SLINPUT_Flush *flush_out;  /**< Callback pointer */
  SLINPUT_Stream stream_in;  /**< The actual input stream */
  SLINPUT_TermAttr saved_term_attr_in;  /**< The saved terminal attributes */
  SLINPUT_EnterRaw *enter_raw_in;  /**< Callback pointer */
  SLINPUT_LeaveRaw *leave_raw_in;  /**< Callback pointer */
  SLINPUT_GetCharIn *get_char_in_in;  /**< Callback pointer */
  SLINPUT_IsCharAvailable *is_char_available_in;  /**< Callback pointer */
  SLINPUT_IsSpace *is_space_in;  /**< Callback pointer */
  SLINPUT_AllocInfo alloc_info;  /**< Alloc info used by alloc callbacks */
  SLINPUT_Malloc *malloc_in;  /**< Callback pointer */
  SLINPUT_Free *free_in;  /**< Callback pointer */
  SLINPUT_CompletionInfo completion_info;  /**< Completion callback info */
  SLINPUT_CompletionRequest *completion_request;  /**< Callback pointer */
  sli_char *history[SLINPUT_MAX_HISTORY];  /**< Holds pointers to saved lines */
  sli_sshort num_history;  /**< The number of entries in the history array */
  sli_ushort columns_in;  /**< The number of columns, zero uses width callback */
  sli_ushort cursor_margin_in;  /**< The cursor margin for scrolling to occur */
  sli_char continuation_character_left;  /**< Printed when left scrollable */
  sli_char continuation_character_right;  /**< Printed when right scrollable */
} TermInfo;

/** Information needed for the line being input */
typedef struct LineInfo {
  const sli_char *prompt_in;   /**< Original prompt */
  const sli_char *prompt;      /**< Prompt to render at start of line */
  sli_ushort buffer_size;       /**< Size of memory buffer in bytes */
  sli_ushort max_chars;         /**< Max chars allowed in memory buffer */
  sli_char *buffer;            /**< Start of memory buffer */
  sli_char *end_ptr;           /**< End of the line (points to '\0') */
  sli_char *cursor_ptr;        /**< Horizontal cursor position */
  sli_char *scroll_ptr;        /**< Horizontal scroll pointer */
  sli_sshort fit_len;            /**< Max chars that fit in a line */
  sli_sshort columns;            /**< The number of columns in the console */
  sli_sshort cursor_margin;      /**< Cursor margin before scroll performed */
} LineInfo;

/** Single line input state */
struct SLINPUT_State {
  TermInfo term_info;  /**< Terminal input state */
  LineInfo line_info;  /**< Line input state */
};

#endif
