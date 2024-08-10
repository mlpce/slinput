#include <list>
#include <optional>

#include <gtest/gtest.h>

extern "C" {
#include "include/slinput.h"
}

/** Single line input unit test class */
class SingleLineInput : public ::testing::Test {
 public:
  SingleLineInput();

 protected:
  /* Input functions */
  static SLINPUT_EnterRaw EnterRawIn;  /**< Callback fn */
  static SLINPUT_LeaveRaw LeaveRawIn;  /**< Callback fn */
  static SLINPUT_GetCharIn GetCharInIn;  /**< Callback fn */
  static SLINPUT_IsCharAvailable IsCharAvailableIn;  /**< Callback fn */
  static SLINPUT_IsSpace IsSpaceIn;  /**< Callback fn */
  static SLINPUT_Malloc MallocIn;  /**< Callback fn */
  static SLINPUT_Free FreeIn;  /**< Callback fn */

  /* Output functions */
  static SLINPUT_CursorControl CursorControlOut;  /**< Callback fn */
  static SLINPUT_Putchar PutCharOut;  /**< Callback fn */
  static SLINPUT_Flush FlushOut;  /**< Callback fn */
  static SLINPUT_GetTerminalWidth GetTerminalWidth;  /**< Callback fn */

  /** Initialise the unit test state */
  void SetUp() override {
    in_raw_ = 0;
    terminal_width_ = 20;
    input_.clear();
    output_.clear();
    allocated_memory_ = 0;
  }

  /**
   * Initialises SLINPUT_State with callbacks pointing to SingleLineInput
   * static functions. Sets default values for num columns, cursor margin,
   * and the continuation characters
   * @param[in] state the state to initialise
   */
  void InitState(SLINPUT_State *state) {
    SLINPUT_Set_EnterRaw(state, EnterRawIn);
    SLINPUT_Set_LeaveRaw(state, LeaveRawIn);
    SLINPUT_Set_GetCharIn(state, GetCharInIn);
    SLINPUT_Set_IsCharAvailable(state, IsCharAvailableIn);
    SLINPUT_Set_IsSpace(state, IsSpaceIn);
    SLINPUT_Set_CursorControl(state, CursorControlOut);
    SLINPUT_Set_Putchar(state, PutCharOut);
    SLINPUT_Set_Flush(state, FlushOut);
    SLINPUT_Set_GetTerminalWidth(state, GetTerminalWidth);

    SLINPUT_Set_NumColumns(state, 0);
    SLINPUT_Set_CursorMargin(state, 0);
    SLINPUT_Set_ContinuationCharacterLeft(state, L' ');
    SLINPUT_Set_ContinuationCharacterRight(state, L' ');
  }

  /** Holds an input (key code or character) */
  struct KeyInput {
    SLINPUT_KeyCode key_code = SLINPUT_KC_NUL;  /**< Input key code */
    SLICHAR character = L'\0';  /**< Input character */
  };

  /**
   * Gets a KeyInput from the input list
   * @return optional KeyInput
   */
  std::optional<KeyInput> GetInput() {
    std::optional<KeyInput> key_input;
    if (!input_.empty()) {
      key_input = input_.front();
      input_.pop_front();
    }
    return key_input;
  }

  std::list<KeyInput> input_;  /**< List of KeyInput for the test */
  std::wstring output_;  /**< The generated output for the test */
  int32_t in_raw_ = 0;  /**< Counts how many times raw mode has been entered */
  uint16_t terminal_width_ = 0;  /**< The width of the terminal for the test */
  bool is_flushing_ = false;  /**< true if the input is being flushed */
  size_t allocated_memory_ = 0;  /**< Counts alloc'd memory for the test */
};

SingleLineInput::SingleLineInput() {
}

int SingleLineInput::EnterRawIn(const SLINPUT_State *state,
    SLINPUT_Stream stream_in, SLINPUT_TermAttr *original_term_attr) {
  SingleLineInput *self =
    static_cast<SingleLineInput *>(stream_in.stream_data);
  ++self->in_raw_;
  original_term_attr->term_attr_data = &self->in_raw_;
  return 0;
}

int SingleLineInput::LeaveRawIn(const SLINPUT_State *state,
    SLINPUT_Stream stream_in, SLINPUT_TermAttr previous_attr) {
  SingleLineInput *self =
    static_cast<SingleLineInput *>(stream_in.stream_data);
  return (previous_attr.term_attr_data == &self->in_raw_ &&
    --self->in_raw_ == 0) ? 0 : -1;
}

int SingleLineInput::GetCharInIn(const SLINPUT_State *state,
    SLINPUT_Stream stream_in, SLINPUT_KeyCode *key_code, SLICHAR *character) {
  SingleLineInput *self =
    static_cast<SingleLineInput *>(stream_in.stream_data);

  /* SLINPUT_Get passes null pointer for key_code and
  character when it is flushing input. It keeps calling GetCharIn to
  flush until IsCharAvailable return false. */
  if (key_code == nullptr && character == nullptr) {
    /* Set flushing flag so IsCharAvailable will return false. */
    self->is_flushing_ = true;
    return 0;
  }

  self->is_flushing_ = false;

  *key_code = SLINPUT_KC_NUL;
  *character = L'\0';

  std::optional<KeyInput> key_input = self->GetInput();
  if (key_input != std::nullopt) {
    *key_code = key_input->key_code;
    *character = key_input->character;
  }

  return 0;
}

int SingleLineInput::IsCharAvailableIn(const SLINPUT_State *state,
    SLINPUT_Stream stream_in) {
  SingleLineInput *self =
    static_cast<SingleLineInput *>(stream_in.stream_data);
  return !self->is_flushing_ && !self->input_.empty();
}

int SingleLineInput::IsSpaceIn(const SLINPUT_State *state,
    SLINPUT_Stream stream_in, SLICHAR character) {
  return iswspace(character);
}

void *SingleLineInput::MallocIn(SLINPUT_AllocInfo alloc_info, size_t size) {
  SingleLineInput *self =
    static_cast<SingleLineInput *>(alloc_info.alloc_info_data);
  char *ptr = static_cast<char *>(malloc(size + sizeof(size_t)));
  *reinterpret_cast<size_t *>(ptr) = size;
  self->allocated_memory_ += size;
  ptr += sizeof(size_t);
  return ptr;
}

void SingleLineInput::FreeIn(SLINPUT_AllocInfo alloc_info, void *ptr) {
  SingleLineInput *self =
    static_cast<SingleLineInput *>(alloc_info.alloc_info_data);
  char *char_ptr = static_cast<char *>(ptr);
  char_ptr -= sizeof(size_t);
  const size_t alloc_size = *reinterpret_cast<size_t *>(char_ptr);
  self->allocated_memory_ -= alloc_size;
  free(char_ptr);
}

int SingleLineInput::CursorControlOut(const SLINPUT_State *state,
    SLINPUT_Stream stream_out, SLINPUT_CursorControlCode cursor_control_code) {
  SingleLineInput *self =
    static_cast<SingleLineInput *>(stream_out.stream_data);

  const wchar_t *SLINPUT_CursorControlTable[SLINPUT_CCC_MAX] = {
    L"[SLINPUT_CCC_CURSOR_RIGHT]",    /* SLINPUT_CCC_CURSOR_RIGHT */
    L"[SLINPUT_CCC_CURSOR_LEFT]",    /* SLINPUT_CCC_CURSOR_LEFT */
    L"[SLINPUT_CCC_CLEAR_TO_END_OF_LINE]",    /* SLINPUT_CCC_CLEAR_TO_END_OF_LINE */
    L"[SLINPUT_CCC_ENABLE_CURSOR]",  /* SLINPUT_CCC_ENABLE_CURSOR */
    L"[SLINPUT_CCC_DISABLE_CURSOR]",  /* SLINPUT_CCC_DISABLE_CURSOR */
    L"[SLINPUT_CCC_SAVE_CURSOR]",     /* SLINPUT_CCC_SAVE_CURSOR */
    L"[SLINPUT_CCC_RESTORE_CURSOR]",     /* SLINPUT_CCC_RESTORE_CURSOR */
    L"[SLINPUT_CCC_CLEAR_LINE]",  /* SLINPUT_CCC_CLEAR_LINE */
    L"[SLINPUT_CCC_WRAP_ON]",    /* SLINPUT_CCC_WRAP_ON */
    L"[SLINPUT_CCC_WRAP_OFF]"     /* SLINPUT_CCC_WRAP_OFF */
  };

  const wchar_t *str = SLINPUT_CursorControlTable[cursor_control_code];
  self->output_.append(str);
  return wcslen(str);
}

int SingleLineInput::PutCharOut(const SLINPUT_State *state,
    SLINPUT_Stream stream_out, SLICHAR c) {
  SingleLineInput *self =
    static_cast<SingleLineInput *>(stream_out.stream_data);
  self->output_.push_back(static_cast<SLICHAR>(c));
  return 1;
}

int SingleLineInput::FlushOut(const SLINPUT_State *state,
    SLINPUT_Stream stream_out) {
  return 1;
}

int SingleLineInput::GetTerminalWidth(const SLINPUT_State *state,
    SLINPUT_Stream stream_out, uint16_t *width) {
  SingleLineInput *self =
    static_cast<SingleLineInput *>(stream_out.stream_data);
  *width = self->terminal_width_;
  return 0;
}

/* Precheck of InputLine testing functions */

TEST_F(SingleLineInput, PrecheckEnterLeaveRawIn) {
  SLINPUT_Stream stream_in = { this };
  SLINPUT_TermAttr term_attr;
  EXPECT_GE(EnterRawIn(nullptr, stream_in, &term_attr), 0);
  EXPECT_TRUE(term_attr.term_attr_data == &in_raw_);
  EXPECT_EQ(in_raw_, 1);
  EXPECT_EQ(LeaveRawIn(nullptr, stream_in, term_attr), 0);
  EXPECT_EQ(in_raw_, 0);
}

TEST_F(SingleLineInput, PrecheckGetInput) {
  SLINPUT_Stream stream_in = { this };
  EXPECT_FALSE(IsCharAvailableIn(nullptr, stream_in));

  input_.push_back(
    KeyInput {
      SLINPUT_KC_HOME, 'a'
    }
  );

  EXPECT_TRUE(IsCharAvailableIn(nullptr, stream_in));

  SLINPUT_KeyCode key_code = SLINPUT_KC_NUL;
  SLICHAR character = L'\0';
  GetCharInIn(nullptr, stream_in, &key_code, &character);
  EXPECT_EQ(key_code, SLINPUT_KC_HOME);
  EXPECT_EQ(character, L'a');

  EXPECT_FALSE(IsCharAvailableIn(nullptr, stream_in));
}

TEST_F(SingleLineInput, PrecheckPutCharOut) {
  SLINPUT_Stream stream_out = { this };
  output_ = L"CheckPutCharOut: ";
  EXPECT_EQ(PutCharOut(nullptr, stream_out, L'A'), 1);
  EXPECT_EQ(output_, L"CheckPutCharOut: A");
}

TEST_F(SingleLineInput, PrecheckGetTerminalWidth) {
  SLINPUT_Stream stream_out = { this };
  uint16_t width = 0;
  EXPECT_GE(GetTerminalWidth(nullptr, stream_out, &width), 0);
  EXPECT_EQ(width, terminal_width_);
}

TEST_F(SingleLineInput, InitConfTerm) {
  SLINPUT_Stream stream = { this };
  SLINPUT_AllocInfo alloc_info = { this };
  SLINPUT_State *state =
    SLINPUT_CreateState(alloc_info, MallocIn, FreeIn);
  SLINPUT_Set_Streams(state, stream, stream);
  EXPECT_TRUE(state);
  SLINPUT_DestroyState(state);
  EXPECT_EQ(allocated_memory_, 0);
}

TEST_F(SingleLineInput, GetNewLine) {
  SLINPUT_Stream stream = { this };
  SLINPUT_AllocInfo alloc_info = { this };
  SLINPUT_State *state =
    SLINPUT_CreateState(alloc_info, MallocIn, FreeIn);
  ASSERT_TRUE(state);
  SLINPUT_Set_Streams(state, stream, stream);
  InitState(state);

  SLICHAR buffer[256];
  terminal_width_ = 20;

  /* A single new line */
  input_.push_back( KeyInput { SLINPUT_KC_NUL, L'\n' } );
  EXPECT_EQ(SLINPUT_Get(state, L"> ", nullptr,
    sizeof(buffer)/sizeof(buffer[0]), buffer), 1);

  /* Produces a single new line */
  EXPECT_STREQ(buffer, L"\n");
  EXPECT_STREQ(output_.c_str(),
    /* Line wrap off */
    L"[SLINPUT_CCC_WRAP_OFF]"
    /* ApplyDimension and RedrawLine */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* New line */
    L"\n"
    /* Line wrap on */
    L"[SLINPUT_CCC_WRAP_ON]");

  SLINPUT_DestroyState(state);
  EXPECT_EQ(allocated_memory_, 0);
}

TEST_F(SingleLineInput, GetSimpleLine) {
  SLINPUT_Stream stream = { this };
  SLINPUT_AllocInfo alloc_info = { this };
  SLINPUT_State *state =
    SLINPUT_CreateState(alloc_info, MallocIn, FreeIn);
  ASSERT_TRUE(state);
  SLINPUT_Set_Streams(state, stream, stream);
  InitState(state);

  SLICHAR buffer[256];
  terminal_width_ = 20;

  /* Characters followed by new line */
  const SLICHAR *input = L"Simple\n";
  while (*input)
    input_.push_back( KeyInput { SLINPUT_KC_NUL, *input++ } );

  EXPECT_EQ(SLINPUT_Get(state, L"> ", nullptr,
    sizeof(buffer)/sizeof(buffer[0]), buffer), 6);
  EXPECT_STREQ(output_.c_str(),
    /* Line wrap off */
    L"[SLINPUT_CCC_WRAP_OFF]"
    /* ApplyDimension and RedrawLine */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Key presses */
    L"[SLINPUT_CCC_DISABLE_CURSOR]S[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]i[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]m[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]p[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]l[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* New line */
    L"\n"
    /* Line wrap on */
    L"[SLINPUT_CCC_WRAP_ON]");

  /* Produces the characters */
  EXPECT_STREQ(buffer, L"Simple");

  SLINPUT_DestroyState(state);
  EXPECT_EQ(allocated_memory_, 0);
}

TEST_F(SingleLineInput, GetSimpleLineGreekLetters) {
  SLINPUT_Stream stream = { this };
  SLINPUT_AllocInfo alloc_info = { this };
  SLINPUT_State *state =
    SLINPUT_CreateState(alloc_info, MallocIn, FreeIn);
  ASSERT_TRUE(state);
  SLINPUT_Set_Streams(state, stream, stream);
  InitState(state);

  SLICHAR buffer[256];
  terminal_width_ = 20;

  /* Characters followed by new line */
  const SLICHAR *input = L"ΑαΒβΓγΔδ\n";
  while (*input)
    input_.push_back( KeyInput { SLINPUT_KC_NUL, *input++ } );

  EXPECT_EQ(SLINPUT_Get(state, L"> ", nullptr,
    sizeof(buffer)/sizeof(buffer[0]), buffer), 8);
  EXPECT_STREQ(output_.c_str(),
    /* Line wrap off */
    L"[SLINPUT_CCC_WRAP_OFF]"
    /* ApplyDimension and RedrawLine */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Key presses */
    L"[SLINPUT_CCC_DISABLE_CURSOR]\x391[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]\x3B1[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]\x392[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]\x3B2[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]\x393[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]\x3B3[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]\x394[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]\x3B4[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* New line */
    L"\n"
    /* Line wrap on */
    L"[SLINPUT_CCC_WRAP_ON]");

  /* Produces the characters */
  EXPECT_STREQ(buffer, L"ΑαΒβΓγΔδ");

  SLINPUT_DestroyState(state);
  EXPECT_EQ(allocated_memory_, 0);
}

TEST_F(SingleLineInput, CursorLeftInsert) {
  SLINPUT_Stream stream = { this };
  SLINPUT_AllocInfo alloc_info = { this };
  SLINPUT_State *state =
    SLINPUT_CreateState(alloc_info, MallocIn, FreeIn);
  ASSERT_TRUE(state);
  SLINPUT_Set_Streams(state, stream, stream);
  InitState(state);

  SLICHAR buffer[256];
  terminal_width_ = 40;

  /* Characters */
  const SLICHAR *first_input = L"One two three four";
  while (*first_input)
    input_.push_back( KeyInput { SLINPUT_KC_NUL, *first_input++ } );

  /* Left arrow four times */
  for (int16_t i = 0; i < 4; ++i)
    input_.push_back( KeyInput { SLINPUT_KC_LEFT, L'\0' } );

  /* Insert text '3.5 ' */
  const SLICHAR *second_input = L"3.5 \n";
  while (*second_input)
    input_.push_back( KeyInput { SLINPUT_KC_NUL, *second_input++ } );

  EXPECT_EQ(SLINPUT_Get(state, L"> ", nullptr,
    sizeof(buffer)/sizeof(buffer[0]), buffer), 22);
  EXPECT_STREQ(buffer, L"One two three 3.5 four");

  EXPECT_STREQ(output_.c_str(),
    /* Line wrap off */
    L"[SLINPUT_CCC_WRAP_OFF]"
    /* ApplyDimension and RedrawLine */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Key presses */
    L"[SLINPUT_CCC_DISABLE_CURSOR]O[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]n[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]t[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]w[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]o[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]t[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]h[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]r[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]f[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]o[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]u[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]r[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Cursor left presses */
    L"[SLINPUT_CCC_CURSOR_LEFT]"
    L"[SLINPUT_CCC_CURSOR_LEFT]"
    L"[SLINPUT_CCC_CURSOR_LEFT]"
    L"[SLINPUT_CCC_CURSOR_LEFT]"
    /* Key presses */
    L"[SLINPUT_CCC_DISABLE_CURSOR]3[SLINPUT_CCC_SAVE_CURSOR]four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR].[SLINPUT_CCC_SAVE_CURSOR]four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]5[SLINPUT_CCC_SAVE_CURSOR]four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR]four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* New line */
    L"\n"
    /* Line wrap on */
    L"[SLINPUT_CCC_WRAP_ON]");

  SLINPUT_DestroyState(state);
  EXPECT_EQ(allocated_memory_, 0);
}

TEST_F(SingleLineInput, HomeInsertEndInsert) {
  SLINPUT_Stream stream = { this };
  SLINPUT_AllocInfo alloc_info = { this };
  SLINPUT_State *state =
    SLINPUT_CreateState(alloc_info, MallocIn, FreeIn);
  ASSERT_TRUE(state);
  SLINPUT_Set_Streams(state, stream, stream);
  InitState(state);

  SLICHAR buffer[256];
  terminal_width_ = 40;

  /* Characters followed by new line */
  const SLICHAR *first_input = L"one two three four";
  while (*first_input)
    input_.push_back( KeyInput { SLINPUT_KC_NUL, *first_input++ } );

  /* HOME (move to start of line). */
  input_.push_back( KeyInput { SLINPUT_KC_HOME, L'\0' } );

  /* Insert text 'Zero ' */
  const SLICHAR *second_input = L"Zero ";
  while (*second_input)
    input_.push_back( KeyInput { SLINPUT_KC_NUL, *second_input++ } );

  /* END (move to end of line). */
  input_.push_back( KeyInput { SLINPUT_KC_END, L'\0' } );

  /* Insert text ' five\n' */
  const SLICHAR *third_input = L" five\n";
  while (*third_input)
    input_.push_back( KeyInput { SLINPUT_KC_NUL, *third_input++ } );

  EXPECT_EQ(SLINPUT_Get(state, L"> ", nullptr,
    sizeof(buffer)/sizeof(buffer[0]), buffer), 28);
  EXPECT_STREQ(buffer, L"Zero one two three four five");

  EXPECT_STREQ(output_.c_str(),
    /* Line wrap off */
    L"[SLINPUT_CCC_WRAP_OFF]"
    /* ApplyDimension and RedrawLine */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Key presses */
    L"[SLINPUT_CCC_DISABLE_CURSOR]o[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]n[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]t[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]w[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]o[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]t[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]h[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]r[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]f[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]o[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]u[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]r[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* HOME (move to start of line) */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  [SLINPUT_CCC_SAVE_CURSOR]one two three four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Key presses */
    L"[SLINPUT_CCC_DISABLE_CURSOR]Z[SLINPUT_CCC_SAVE_CURSOR]one two three four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR]one two three four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]r[SLINPUT_CCC_SAVE_CURSOR]one two three four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]o[SLINPUT_CCC_SAVE_CURSOR]one two three four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR]one two three four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* END (move to end of line) */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  Zero one two three four[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Key presses */
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]f[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]i[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]v[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* New line */
    L"\n"
    /* Line wrap on */
    L"[SLINPUT_CCC_WRAP_ON]");

  SLINPUT_DestroyState(state);
  EXPECT_EQ(allocated_memory_, 0);
}

TEST_F(SingleLineInput, HomeRightInsert) {
  SLINPUT_Stream stream = { this };
  SLINPUT_AllocInfo alloc_info = { this };
   SLINPUT_State *state =
    SLINPUT_CreateState(alloc_info, MallocIn, FreeIn);
  ASSERT_TRUE(state);
  SLINPUT_Set_Streams(state, stream, stream);
  InitState(state);

  SLICHAR buffer[256];
  terminal_width_ = 40;

  /* Characters followed by new line */
  const SLICHAR *first_input = L"One two three four";
  while (*first_input)
    input_.push_back( KeyInput { SLINPUT_KC_NUL, *first_input++ } );

  /* Home (move to start of line). */
  input_.push_back( KeyInput { SLINPUT_KC_HOME, L'\0' } );

  /* Right arrow four times */
  for (int16_t i = 0; i < 4; ++i)
    input_.push_back( KeyInput { SLINPUT_KC_RIGHT, L'\0' } );

  /* Insert text '1.5 ' */
  const SLICHAR *second_input = L"1.5 \n";
  while (*second_input)
    input_.push_back( KeyInput { SLINPUT_KC_NUL, *second_input++ } );

  EXPECT_EQ(SLINPUT_Get(state, L"> ", nullptr,
    sizeof(buffer)/sizeof(buffer[0]), buffer), 22);
  EXPECT_STREQ(buffer, L"One 1.5 two three four");

  EXPECT_STREQ(output_.c_str(),
    /* Line wrap off */
    L"[SLINPUT_CCC_WRAP_OFF]"
    /* ApplyDimension and RedrawLine */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Key presses */
    L"[SLINPUT_CCC_DISABLE_CURSOR]O[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]n[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]t[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]w[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]o[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]t[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]h[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]r[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]f[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]o[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]u[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]r[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Move to start of line */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  [SLINPUT_CCC_SAVE_CURSOR]One two three four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Right key presses */
    L"[SLINPUT_CCC_CURSOR_RIGHT][SLINPUT_CCC_CURSOR_RIGHT][SLINPUT_CCC_CURSOR_RIGHT][SLINPUT_CCC_CURSOR_RIGHT]"
    /* Key presses */
    L"[SLINPUT_CCC_DISABLE_CURSOR]1[SLINPUT_CCC_SAVE_CURSOR]two three four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR].[SLINPUT_CCC_SAVE_CURSOR]two three four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]5[SLINPUT_CCC_SAVE_CURSOR]two three four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR]two three four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* New line */
    L"\n"
    /* Line wrap on */
    L"[SLINPUT_CCC_WRAP_ON]");

  SLINPUT_DestroyState(state);
  EXPECT_EQ(allocated_memory_, 0);
}

TEST_F(SingleLineInput, InsertLeftBackspace) {
  SLINPUT_Stream stream = { this };
  SLINPUT_AllocInfo alloc_info = { this };
  SLINPUT_State *state =
    SLINPUT_CreateState(alloc_info, MallocIn, FreeIn);
  ASSERT_TRUE(state);
  SLINPUT_Set_Streams(state, stream, stream);
  InitState(state);

  SLICHAR buffer[256];
  terminal_width_ = 40;

  /* Characters */
  const SLICHAR *first_input = L"One two three four";
  while (*first_input)
    input_.push_back( KeyInput { SLINPUT_KC_NUL, *first_input++ } );

  /* Left arrow four times */
  for (int16_t i = 0; i < 4; ++i)
    input_.push_back( KeyInput { SLINPUT_KC_LEFT, L'\0' } );

  /* Backspace more than there is input characters */
  for (int16_t i = 0; i < 100; ++i)
    input_.push_back( KeyInput { SLINPUT_KC_BACKSPACE, L'\0' } );

  /* End input */
  input_.push_back( KeyInput { SLINPUT_KC_NUL, L'\n' } );

  EXPECT_EQ(SLINPUT_Get(state, L"> ", nullptr,
    sizeof(buffer)/sizeof(buffer[0]), buffer), 4);
  EXPECT_STREQ(buffer, L"four");

  EXPECT_STREQ(output_.c_str(),
    /* Line wrap off */
    L"[SLINPUT_CCC_WRAP_OFF]"
    /* ApplyDimension and RedrawLine */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Key presses */
    L"[SLINPUT_CCC_DISABLE_CURSOR]O[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]n[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]t[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]w[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]o[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]t[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]h[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]r[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]f[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]o[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]u[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]r[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Cursor left */
    L"[SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT]"
    /* Backspaces */
    L"\b[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_TO_END_OF_LINE][SLINPUT_CCC_SAVE_CURSOR]four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"\b[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_TO_END_OF_LINE][SLINPUT_CCC_SAVE_CURSOR]four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"\b[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_TO_END_OF_LINE][SLINPUT_CCC_SAVE_CURSOR]four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"\b[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_TO_END_OF_LINE][SLINPUT_CCC_SAVE_CURSOR]four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"\b[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_TO_END_OF_LINE][SLINPUT_CCC_SAVE_CURSOR]four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"\b[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_TO_END_OF_LINE][SLINPUT_CCC_SAVE_CURSOR]four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"\b[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_TO_END_OF_LINE][SLINPUT_CCC_SAVE_CURSOR]four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"\b[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_TO_END_OF_LINE][SLINPUT_CCC_SAVE_CURSOR]four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"\b[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_TO_END_OF_LINE][SLINPUT_CCC_SAVE_CURSOR]four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"\b[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_TO_END_OF_LINE][SLINPUT_CCC_SAVE_CURSOR]four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"\b[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_TO_END_OF_LINE][SLINPUT_CCC_SAVE_CURSOR]four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"\b[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_TO_END_OF_LINE][SLINPUT_CCC_SAVE_CURSOR]four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"\b[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_TO_END_OF_LINE][SLINPUT_CCC_SAVE_CURSOR]four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"\b[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_TO_END_OF_LINE][SLINPUT_CCC_SAVE_CURSOR]four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* New line */
    L"\n"
    /* Line wrap on */
    L"[SLINPUT_CCC_WRAP_ON]");

  SLINPUT_DestroyState(state);
  EXPECT_EQ(allocated_memory_, 0);
}

TEST_F(SingleLineInput, InsertLeftDelete) {
  SLINPUT_Stream stream = { this };
  SLINPUT_AllocInfo alloc_info = { this };
  SLINPUT_State *state =
    SLINPUT_CreateState(alloc_info, MallocIn, FreeIn);
  ASSERT_TRUE(state);
  SLINPUT_Set_Streams(state, stream, stream);
  InitState(state);

  SLICHAR buffer[256];
  terminal_width_ = 40;

  /* Characters */
  const SLICHAR *first_input = L"One two three four";
  while (*first_input)
    input_.push_back( KeyInput { SLINPUT_KC_NUL, *first_input++ } );

  /* Left arrow five times */
  for (int16_t i = 0; i < 5; ++i)
    input_.push_back( KeyInput { SLINPUT_KC_LEFT, L'\0' } );

  /* Delete more than there is input characters to the right */
  for (int16_t i = 0; i < 100; ++i)
    input_.push_back( KeyInput { SLINPUT_KC_DEL, L'\0' } );

  /* End input */
  input_.push_back( KeyInput { SLINPUT_KC_NUL, L'\n' } );

  EXPECT_EQ(SLINPUT_Get(state, L"> ", nullptr,
    sizeof(buffer)/sizeof(buffer[0]), buffer), 13);
  EXPECT_STREQ(buffer, L"One two three");

  EXPECT_STREQ(output_.c_str(),
    /* Line wrap off */
    L"[SLINPUT_CCC_WRAP_OFF]"
    /* ApplyDimension and RedrawLine */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Key presses */
    L"[SLINPUT_CCC_DISABLE_CURSOR]O[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]n[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]t[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]w[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]o[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]t[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]h[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]r[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]f[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]o[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]u[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]r[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Left arrow five times */
    L"[SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT]"
    /* Deletes */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_TO_END_OF_LINE][SLINPUT_CCC_SAVE_CURSOR]four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_TO_END_OF_LINE][SLINPUT_CCC_SAVE_CURSOR]our [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_TO_END_OF_LINE][SLINPUT_CCC_SAVE_CURSOR]ur [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_TO_END_OF_LINE][SLINPUT_CCC_SAVE_CURSOR]r [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_TO_END_OF_LINE][SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* New line */
    L"\n"
    /* Line wrap on */
    L"[SLINPUT_CCC_WRAP_ON]");

  SLINPUT_DestroyState(state);
  EXPECT_EQ(allocated_memory_, 0);
}

TEST_F(SingleLineInput, InsertMoreCharactersThanBufferSize) {
  SLINPUT_Stream stream = { this };
  SLINPUT_AllocInfo alloc_info = { this };
  SLINPUT_State *state =
    SLINPUT_CreateState(alloc_info, MallocIn, FreeIn);
  ASSERT_TRUE(state);
  SLINPUT_Set_Streams(state, stream, stream);
  InitState(state);

  SLICHAR buffer[20];
  terminal_width_ = 40;

  /* Insert more characters than buffer size */
  for (int16_t i = 0; i < 100; ++i)
    input_.push_back( KeyInput { SLINPUT_KC_NUL, L'*' } );

  /* End input */
  input_.push_back( KeyInput { SLINPUT_KC_NUL, L'\n' } );

  /* Nineteen asterix in buffer */
  EXPECT_EQ(SLINPUT_Get(state, L"> ", nullptr,
    sizeof(buffer)/sizeof(buffer[0]), buffer), 19);
  EXPECT_STREQ(buffer, L"**********" "*********");

  EXPECT_STREQ(output_.c_str(),
    /* Line wrap off */
    L"[SLINPUT_CCC_WRAP_OFF]"
    /* ApplyDimension and RedrawLine */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Key presses */
    L"[SLINPUT_CCC_DISABLE_CURSOR]*[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]*[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]*[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]*[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]*[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]*[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]*[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]*[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]*[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]*[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]*[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]*[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]*[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]*[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]*[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]*[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]*[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]*[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]*[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* New line */
    L"\n"
    /* Line wrap on */
    L"[SLINPUT_CCC_WRAP_ON]");

  SLINPUT_DestroyState(state);
  EXPECT_EQ(allocated_memory_, 0);
}

TEST_F(SingleLineInput, InsertLeftInsertMoreCharactersThanBufferSize) {
  SLINPUT_Stream stream = { this };
  SLINPUT_AllocInfo alloc_info = { this };
  SLINPUT_State *state =
    SLINPUT_CreateState(alloc_info, MallocIn, FreeIn);
  ASSERT_TRUE(state);
  SLINPUT_Set_Streams(state, stream, stream);
  InitState(state);

  SLICHAR buffer[20];
  terminal_width_ = 40;

  /* Characters */
  const SLICHAR *first_input = L"One two three four";
  while (*first_input)
    input_.push_back( KeyInput { SLINPUT_KC_NUL, *first_input++ } );

  /* Left arrow four times */
  for (int16_t i = 0; i < 4; ++i)
    input_.push_back( KeyInput { SLINPUT_KC_LEFT, L'\0' } );

  input_.push_back( KeyInput { SLINPUT_KC_NUL, L'+' } );
  input_.push_back( KeyInput { SLINPUT_KC_NUL, L' ' } );

  /* End input */
  input_.push_back( KeyInput { SLINPUT_KC_NUL, L'\n' } );

  /* The 'space' wasn't inserted due to buffer being full */
  EXPECT_EQ(SLINPUT_Get(state, L"> ", nullptr,
    sizeof(buffer)/sizeof(buffer[0]), buffer), 19);
  EXPECT_STREQ(buffer, L"One two three +four");

  EXPECT_STREQ(output_.c_str(),
    /* Line wrap off */
    L"[SLINPUT_CCC_WRAP_OFF]"
    /* ApplyDimension and RedrawLine */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Key presses */
    L"[SLINPUT_CCC_DISABLE_CURSOR]O[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]n[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]t[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]w[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]o[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]t[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]h[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]r[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]f[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]o[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]u[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]r[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Left key presses */
    L"[SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT]"
    /* Only the plus inserted */
    L"[SLINPUT_CCC_DISABLE_CURSOR]+[SLINPUT_CCC_SAVE_CURSOR]four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* New line */
    L"\n"
    /* Line wrap on */
    L"[SLINPUT_CCC_WRAP_ON]");

  SLINPUT_DestroyState(state);
  EXPECT_EQ(allocated_memory_, 0);
}

TEST_F(SingleLineInput, InsertWarpLeftDeleteInsertWarpRightDeleteInsert) {
  SLINPUT_Stream stream = { this };
  SLINPUT_AllocInfo alloc_info = { this };
  SLINPUT_State *state =
    SLINPUT_CreateState(alloc_info, MallocIn, FreeIn);
  ASSERT_TRUE(state);
  SLINPUT_Set_Streams(state, stream, stream);
  InitState(state);

  SLICHAR buffer[20];
  terminal_width_ = 40;

  /* Characters */
  const SLICHAR *first_input = L"One two three four";
  while (*first_input)
    input_.push_back( KeyInput { SLINPUT_KC_NUL, *first_input++ } );

  /* Warp left three times */
  input_.push_back( KeyInput { SLINPUT_KC_WARP_LEFT, L'\0' } );
  input_.push_back( KeyInput { SLINPUT_KC_WARP_LEFT, L'\0' } );
  input_.push_back( KeyInput { SLINPUT_KC_WARP_LEFT, L'\0' } );

  /* Delete */
  for (int16_t i = 0; i < 3; ++i)
    input_.push_back( KeyInput { SLINPUT_KC_DEL, L'\0' } );

  /* Insert */
  input_.push_back( KeyInput { SLINPUT_KC_NUL, L'2' } );

  /* Warp right*/
  input_.push_back( KeyInput { SLINPUT_KC_WARP_RIGHT, L'\0' } );

  /* Delete */
  for (int16_t i = 0; i < 5; ++i)
    input_.push_back( KeyInput { SLINPUT_KC_DEL, L'\0' } );

  /* Insert */
  input_.push_back( KeyInput { SLINPUT_KC_NUL, L'3' } );

  /* End input */
  input_.push_back( KeyInput { SLINPUT_KC_NUL, L'\n' } );

  EXPECT_EQ(SLINPUT_Get(state, L"> ", nullptr,
    sizeof(buffer)/sizeof(buffer[0]), buffer), 12);
  EXPECT_STREQ(buffer, L"One 2 3 four");

  EXPECT_STREQ(output_.c_str(),
    /* Line wrap off */
    L"[SLINPUT_CCC_WRAP_OFF]"
    /* ApplyDimension and RedrawLine */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Key presses */
    L"[SLINPUT_CCC_DISABLE_CURSOR]O[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]n[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]t[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]w[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]o[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]t[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]h[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]r[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]f[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]o[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]u[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]r[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Warp left three times - cursor warps left to first character of 'two' */
    L"[SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT]"
    /* Delete three times - word 'two' is deleted */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_TO_END_OF_LINE][SLINPUT_CCC_SAVE_CURSOR]wo three four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_TO_END_OF_LINE][SLINPUT_CCC_SAVE_CURSOR]o three four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_TO_END_OF_LINE][SLINPUT_CCC_SAVE_CURSOR] three four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* '2' is inserted */
    L"[SLINPUT_CCC_DISABLE_CURSOR]2[SLINPUT_CCC_SAVE_CURSOR] three four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Warp right to first letter of 'three' */
    L"[SLINPUT_CCC_CURSOR_RIGHT]"
    /* Delete five times - word 'three' is deleted */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_TO_END_OF_LINE][SLINPUT_CCC_SAVE_CURSOR]hree four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_TO_END_OF_LINE][SLINPUT_CCC_SAVE_CURSOR]ree four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_TO_END_OF_LINE][SLINPUT_CCC_SAVE_CURSOR]ee four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_TO_END_OF_LINE][SLINPUT_CCC_SAVE_CURSOR]e four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_TO_END_OF_LINE][SLINPUT_CCC_SAVE_CURSOR] four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* 3 is inserted */
    L"[SLINPUT_CCC_DISABLE_CURSOR]3[SLINPUT_CCC_SAVE_CURSOR] four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* New line */
    L"\n"
    /* Line wrap on */
    L"[SLINPUT_CCC_WRAP_ON]");

  SLINPUT_DestroyState(state);
  EXPECT_EQ(allocated_memory_, 0);
}

TEST_F(SingleLineInput, WarpsStopAtLimits) {
  SLINPUT_Stream stream = { this };
  SLINPUT_AllocInfo alloc_info = { this };
  SLINPUT_State *state =
    SLINPUT_CreateState(alloc_info, MallocIn, FreeIn);
  ASSERT_TRUE(state);
  SLINPUT_Set_Streams(state, stream, stream);
  InitState(state);

  SLICHAR buffer[20];
  terminal_width_ = 40;

  /* Characters */
  const SLICHAR *first_input = L"One two three four";
  while (*first_input)
    input_.push_back( KeyInput { SLINPUT_KC_NUL, *first_input++ } );

  /* Warp left many times */
  for (int16_t i = 0; i < 100; ++i)
    input_.push_back( KeyInput { SLINPUT_KC_WARP_LEFT, L'\0' } );

  /* Delete 'One ' */
  for (int16_t i = 0; i < 4; ++i)
    input_.push_back( KeyInput { SLINPUT_KC_DEL, L'\0' } );

  /* Warp right many times */
  for (int16_t i = 0; i < 100; ++i)
    input_.push_back( KeyInput { SLINPUT_KC_WARP_RIGHT, L'\0' } );

  /* Backspace to remove ' four' */
  for (int16_t i = 0; i < 5; ++i)
    input_.push_back( KeyInput { SLINPUT_KC_BACKSPACE, L'\0' } );

  /* End input */
  input_.push_back( KeyInput { SLINPUT_KC_NUL, L'\n' } );

  EXPECT_EQ(SLINPUT_Get(state, L"> ", nullptr,
    sizeof(buffer)/sizeof(buffer[0]), buffer), 9);
  EXPECT_STREQ(buffer, L"two three");

  EXPECT_STREQ(output_.c_str(),
    /* Line wrap off */
    L"[SLINPUT_CCC_WRAP_OFF]"
    /* ApplyDimension and RedrawLine */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Key presses */
    L"[SLINPUT_CCC_DISABLE_CURSOR]O[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]n[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]t[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]w[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]o[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]t[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]h[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]r[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]f[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]o[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]u[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]r[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Warp left many times - cursor left 18 times to beginning of buffer */
    L"[SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT]"
    /* Delete 'One ' */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_TO_END_OF_LINE][SLINPUT_CCC_SAVE_CURSOR]ne two three four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_TO_END_OF_LINE][SLINPUT_CCC_SAVE_CURSOR]e two three four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_TO_END_OF_LINE][SLINPUT_CCC_SAVE_CURSOR] two three four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_TO_END_OF_LINE][SLINPUT_CCC_SAVE_CURSOR]two three four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Warp right many times - cursor right 14 times to end of buffer */
    L"[SLINPUT_CCC_CURSOR_RIGHT][SLINPUT_CCC_CURSOR_RIGHT][SLINPUT_CCC_CURSOR_RIGHT][SLINPUT_CCC_CURSOR_RIGHT][SLINPUT_CCC_CURSOR_RIGHT][SLINPUT_CCC_CURSOR_RIGHT][SLINPUT_CCC_CURSOR_RIGHT][SLINPUT_CCC_CURSOR_RIGHT][SLINPUT_CCC_CURSOR_RIGHT][SLINPUT_CCC_CURSOR_RIGHT][SLINPUT_CCC_CURSOR_RIGHT][SLINPUT_CCC_CURSOR_RIGHT][SLINPUT_CCC_CURSOR_RIGHT][SLINPUT_CCC_CURSOR_RIGHT]"
    /* Backspace to remove ' four' */
    L"\b[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_TO_END_OF_LINE][SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"\b[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_TO_END_OF_LINE][SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"\b[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_TO_END_OF_LINE][SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"\b[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_TO_END_OF_LINE][SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"\b[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_TO_END_OF_LINE][SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* New line */
    L"\n"
    /* Line wrap on */
    L"[SLINPUT_CCC_WRAP_ON]");

  SLINPUT_DestroyState(state);
  EXPECT_EQ(allocated_memory_, 0);
}

TEST_F(SingleLineInput, InsertToScrollZeroMarginThenHOME) {
  SLINPUT_Stream stream = { this };
  SLINPUT_AllocInfo alloc_info = { this };
  SLINPUT_State *state =
    SLINPUT_CreateState(alloc_info, MallocIn, FreeIn);
  ASSERT_TRUE(state);
  SLINPUT_Set_Streams(state, stream, stream);
  InitState(state);

  /* Testing with zero margin */
  SLINPUT_Set_CursorMargin(state, 0);

  SLICHAR buffer[40];
  terminal_width_ = 20;

  /* Characters */
  const SLICHAR *first_input = L"One two three four five six";
  while (*first_input)
    input_.push_back( KeyInput { SLINPUT_KC_NUL, *first_input++ } );

  /* HOME (move to start of line). */
  input_.push_back( KeyInput { SLINPUT_KC_HOME, L'\0' } );

  /* End input */
  input_.push_back( KeyInput { SLINPUT_KC_NUL, L'\n' } );

  EXPECT_EQ(SLINPUT_Get(state, L"> ", nullptr,
    sizeof(buffer)/sizeof(buffer[0]), buffer), 27);
  EXPECT_STREQ(buffer, L"One two three four five six");

  EXPECT_STREQ(output_.c_str(),
    /* Line wrap off */
    L"[SLINPUT_CCC_WRAP_OFF]"
    /* ApplyDimension and RedrawLine */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Key presses */
    L"[SLINPUT_CCC_DISABLE_CURSOR]O[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]n[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]t[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]w[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]o[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]t[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]h[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]r[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]f[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Key presses with scrolling */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  ne two three fo[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  e two three fou[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>   two three four[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  two three four [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  wo three four f[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  o three four fi[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>   three four fiv[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  three four five[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  hree four five [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  ree four five s[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  ee four five si[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  e four five six[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* HOME - move to start of line */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  [SLINPUT_CCC_SAVE_CURSOR]One two three f [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* New line */
    L"\n"
    /* Line wrap on */
    L"[SLINPUT_CCC_WRAP_ON]");

  SLINPUT_DestroyState(state);
  EXPECT_EQ(allocated_memory_, 0);
}

TEST_F(SingleLineInput, InsertToScrollZeroMarginThenWarpAndEnd) {
  SLINPUT_Stream stream = { this };
  SLINPUT_AllocInfo alloc_info = { this };
  SLINPUT_State *state =
    SLINPUT_CreateState(alloc_info, MallocIn, FreeIn);
  ASSERT_TRUE(state);
  SLINPUT_Set_Streams(state, stream, stream);
  InitState(state);

  /* Testing with zero margin */
  SLINPUT_Set_CursorMargin(state, 0);

  SLICHAR buffer[40];
  terminal_width_ = 20;

  /* Characters */
  const SLICHAR *first_input = L"One two three four five six";
  while (*first_input)
    input_.push_back( KeyInput { SLINPUT_KC_NUL, *first_input++ } );

  /* Warp left five times so scroll left occurs */
  input_.push_back( KeyInput { SLINPUT_KC_WARP_LEFT, L'\0' } );
  input_.push_back( KeyInput { SLINPUT_KC_WARP_LEFT, L'\0' } );
  input_.push_back( KeyInput { SLINPUT_KC_WARP_LEFT, L'\0' } );
  input_.push_back( KeyInput { SLINPUT_KC_WARP_LEFT, L'\0' } );
  input_.push_back( KeyInput { SLINPUT_KC_WARP_LEFT, L'\0' } );

  /* Warp right four times so scroll right occurs */
  input_.push_back( KeyInput { SLINPUT_KC_WARP_RIGHT, L'\0' } );
  input_.push_back( KeyInput { SLINPUT_KC_WARP_RIGHT, L'\0' } );
  input_.push_back( KeyInput { SLINPUT_KC_WARP_RIGHT, L'\0' } );
  input_.push_back( KeyInput { SLINPUT_KC_WARP_RIGHT, L'\0' } );

  /* END (move to end of line). */
  input_.push_back( KeyInput { SLINPUT_KC_END, L'\0' } );

  /* End input */
  input_.push_back( KeyInput { SLINPUT_KC_NUL, L'\n' } );

  EXPECT_EQ(SLINPUT_Get(state, L"> ", nullptr,
    sizeof(buffer), buffer), 27);
  EXPECT_STREQ(buffer, L"One two three four five six");

  EXPECT_STREQ(output_.c_str(),
    /* Line wrap off */
    L"[SLINPUT_CCC_WRAP_OFF]"
    /* ApplyDimension and RedrawLine */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Key presses */
    L"[SLINPUT_CCC_DISABLE_CURSOR]O[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]n[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]t[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]w[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]o[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]t[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]h[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]r[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]f[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Key presses with scrolling */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  ne two three fo[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  e two three fou[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>   two three four[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  two three four [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  wo three four f[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  o three four fi[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>   three four fiv[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  three four five[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  hree four five [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  ree four five s[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  ee four five si[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  e four five six[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Warping left */
    L"[SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT]"
    /* Warping left scroll has engaged */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  [SLINPUT_CCC_SAVE_CURSOR]three four five [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  [SLINPUT_CCC_SAVE_CURSOR]two three four  [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Warping right */
    L"[SLINPUT_CCC_CURSOR_RIGHT][SLINPUT_CCC_CURSOR_RIGHT][SLINPUT_CCC_CURSOR_RIGHT][SLINPUT_CCC_CURSOR_RIGHT][SLINPUT_CCC_CURSOR_RIGHT][SLINPUT_CCC_CURSOR_RIGHT][SLINPUT_CCC_CURSOR_RIGHT][SLINPUT_CCC_CURSOR_RIGHT][SLINPUT_CCC_CURSOR_RIGHT][SLINPUT_CCC_CURSOR_RIGHT][SLINPUT_CCC_CURSOR_RIGHT][SLINPUT_CCC_CURSOR_RIGHT][SLINPUT_CCC_CURSOR_RIGHT][SLINPUT_CCC_CURSOR_RIGHT][SLINPUT_CCC_CURSOR_RIGHT]"
    /* Warping right engages scroll */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  hree four five [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* END - move to end of line */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  e four five six[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* New line */
    L"\n"
    /* Line wrap on */
    L"[SLINPUT_CCC_WRAP_ON]");

  SLINPUT_DestroyState(state);
  EXPECT_EQ(allocated_memory_, 0);
}

/* Will be same as with zero margin because cursor always at end of string */
TEST_F(SingleLineInput, InsertToScrollFiveMarginThenHOME) {
  SLINPUT_Stream stream = { this };
  SLINPUT_AllocInfo alloc_info = { this };
  SLINPUT_State *state =
    SLINPUT_CreateState(alloc_info, MallocIn, FreeIn);
  ASSERT_TRUE(state);
  SLINPUT_Set_Streams(state, stream, stream);
  InitState(state);

  /* Testing with five margin */
  SLINPUT_Set_CursorMargin(state, 5);

  SLICHAR buffer[40];
  terminal_width_ = 20;

  /* Characters */
  const SLICHAR *first_input = L"One two three four five six";
  while (*first_input)
    input_.push_back( KeyInput { SLINPUT_KC_NUL, *first_input++ } );

  /* HOME (move to start of line). */
  input_.push_back( KeyInput { SLINPUT_KC_HOME, L'\0' } );

  /* End input */
  input_.push_back( KeyInput { SLINPUT_KC_NUL, L'\n' } );

  EXPECT_EQ(SLINPUT_Get(state, L"> ", nullptr,
    sizeof(buffer)/sizeof(buffer[0]), buffer), 27);
  EXPECT_STREQ(buffer, L"One two three four five six");

  EXPECT_STREQ(output_.c_str(),
    /* Line wrap off */
    L"[SLINPUT_CCC_WRAP_OFF]"
    /* ApplyDimension and RedrawLine */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Key presses */
    L"[SLINPUT_CCC_DISABLE_CURSOR]O[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]n[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]t[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]w[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]o[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]t[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]h[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]r[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]f[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Key presses with scrolling */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  ne two three fo[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  e two three fou[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>   two three four[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  two three four [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  wo three four f[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  o three four fi[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>   three four fiv[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  three four five[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  hree four five [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  ree four five s[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  ee four five si[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  e four five six[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* HOME - move to start of line */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  [SLINPUT_CCC_SAVE_CURSOR]One two three f [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* New line */
    L"\n"
    /* Line wrap on */
    L"[SLINPUT_CCC_WRAP_ON]");

  SLINPUT_DestroyState(state);
  EXPECT_EQ(allocated_memory_, 0);
}

/* This will be different because cursor moves through margins */
TEST_F(SingleLineInput, InsertToScrollFiveMarginThenWarpAndEnd) {
  SLINPUT_Stream stream = { this };
  SLINPUT_AllocInfo alloc_info = { this };
  SLINPUT_State *state =
    SLINPUT_CreateState(alloc_info, MallocIn, FreeIn);
  ASSERT_TRUE(state);
  SLINPUT_Set_Streams(state, stream, stream);
  InitState(state);

  /* Testing with five margin */
  SLINPUT_Set_CursorMargin(state, 5);

  SLICHAR buffer[40];
  terminal_width_ = 20;

  /* Characters */
  const SLICHAR *first_input = L"One two three four five six";
  while (*first_input)
    input_.push_back( KeyInput { SLINPUT_KC_NUL, *first_input++ } );

  /* Warp left five times so scroll left occurs */
  input_.push_back( KeyInput { SLINPUT_KC_WARP_LEFT, L'\0' } );
  input_.push_back( KeyInput { SLINPUT_KC_WARP_LEFT, L'\0' } );
  input_.push_back( KeyInput { SLINPUT_KC_WARP_LEFT, L'\0' } );
  input_.push_back( KeyInput { SLINPUT_KC_WARP_LEFT, L'\0' } );
  input_.push_back( KeyInput { SLINPUT_KC_WARP_LEFT, L'\0' } );

  /* Warp right four times so scroll right occurs */
  input_.push_back( KeyInput { SLINPUT_KC_WARP_RIGHT, L'\0' } );
  input_.push_back( KeyInput { SLINPUT_KC_WARP_RIGHT, L'\0' } );
  input_.push_back( KeyInput { SLINPUT_KC_WARP_RIGHT, L'\0' } );
  input_.push_back( KeyInput { SLINPUT_KC_WARP_RIGHT, L'\0' } );

  /* END (move to end of line). */
  input_.push_back( KeyInput { SLINPUT_KC_END, L'\0' } );

  /* End input */
  input_.push_back( KeyInput { SLINPUT_KC_NUL, L'\n' } );

  EXPECT_EQ(SLINPUT_Get(state, L"> ", nullptr,
    sizeof(buffer)/sizeof(buffer[0]), buffer), 27);
  EXPECT_STREQ(buffer, L"One two three four five six");

  EXPECT_STREQ(output_.c_str(),
    /* Line wrap off */
    L"[SLINPUT_CCC_WRAP_OFF]"
    /* ApplyDimension and RedrawLine */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Key presses */
    L"[SLINPUT_CCC_DISABLE_CURSOR]O[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]n[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]t[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]w[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]o[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]t[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]h[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]r[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]f[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Key presses with scrolling */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  ne two three fo[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  e two three fou[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>   two three four[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  two three four [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  wo three four f[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  o three four fi[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>   three four fiv[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  three four five[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  hree four five [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  ree four five s[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  ee four five si[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  e four five six[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Warping left */
    L"[SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT]"
    /* Warping left scroll has engaged (with five margin) */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  hree [SLINPUT_CCC_SAVE_CURSOR]four five  [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>   two [SLINPUT_CCC_SAVE_CURSOR]three four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  One [SLINPUT_CCC_SAVE_CURSOR]two three f [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Warping right */
    L"[SLINPUT_CCC_CURSOR_RIGHT][SLINPUT_CCC_CURSOR_RIGHT][SLINPUT_CCC_CURSOR_RIGHT][SLINPUT_CCC_CURSOR_RIGHT]"
    /* Warping right scroll has engaged (with five margin) */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  two three [SLINPUT_CCC_SAVE_CURSOR]four  [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  hree four [SLINPUT_CCC_SAVE_CURSOR]five  [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  e four five [SLINPUT_CCC_SAVE_CURSOR]six [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* End */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  e four five six[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* New line */
    L"\n"
    /* Line wrap on */
    L"[SLINPUT_CCC_WRAP_ON]"
    );

  SLINPUT_DestroyState(state);
  EXPECT_EQ(allocated_memory_, 0);
}

/* This time with continuation characters */
TEST_F(SingleLineInput, InsertToScrollFiveMarginThenWarpAndEndWithContinuationCharacters) {
  SLINPUT_Stream stream = { this };
  SLINPUT_AllocInfo alloc_info = { this };
  SLINPUT_State *state =
    SLINPUT_CreateState(alloc_info, MallocIn, FreeIn);
  ASSERT_TRUE(state);
  SLINPUT_Set_Streams(state, stream, stream);
  InitState(state);

  /* Testing with five margin */
  SLINPUT_Set_CursorMargin(state, 5);
  /* Testing with continuation characters */
  SLINPUT_Set_ContinuationCharacterLeft(state, L'\x2190');
  SLINPUT_Set_ContinuationCharacterRight(state, L'\x2192');

  SLICHAR buffer[40];
  terminal_width_ = 20;

  /* Characters */
  const SLICHAR *first_input = L"One two three four five six";
  while (*first_input)
    input_.push_back( KeyInput { SLINPUT_KC_NUL, *first_input++ } );

  /* Warp left five times so scroll left occurs */
  input_.push_back( KeyInput { SLINPUT_KC_WARP_LEFT, L'\0' } );
  input_.push_back( KeyInput { SLINPUT_KC_WARP_LEFT, L'\0' } );
  input_.push_back( KeyInput { SLINPUT_KC_WARP_LEFT, L'\0' } );
  input_.push_back( KeyInput { SLINPUT_KC_WARP_LEFT, L'\0' } );
  input_.push_back( KeyInput { SLINPUT_KC_WARP_LEFT, L'\0' } );

  /* Warp right four times so scroll right occurs */
  input_.push_back( KeyInput { SLINPUT_KC_WARP_RIGHT, L'\0' } );
  input_.push_back( KeyInput { SLINPUT_KC_WARP_RIGHT, L'\0' } );
  input_.push_back( KeyInput { SLINPUT_KC_WARP_RIGHT, L'\0' } );
  input_.push_back( KeyInput { SLINPUT_KC_WARP_RIGHT, L'\0' } );

  /* END (move to end of line). */
  input_.push_back( KeyInput { SLINPUT_KC_END, L'\0' } );

  /* End input */
  input_.push_back( KeyInput { SLINPUT_KC_NUL, L'\n' } );

  EXPECT_EQ(SLINPUT_Get(state, L"> ", nullptr,
    sizeof(buffer)/sizeof(buffer[0]), buffer), 27);
  EXPECT_STREQ(buffer, L"One two three four five six");

  EXPECT_STREQ(output_.c_str(),
    /* Line wrap off */
    L"[SLINPUT_CCC_WRAP_OFF]"
    /* ApplyDimension and RedrawLine */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Key presses */
    L"[SLINPUT_CCC_DISABLE_CURSOR]O[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]n[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]t[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]w[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]o[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]t[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]h[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]r[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]f[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Key presses with scrolling */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]> \x2190ne two three fo[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]> \x2190" L"e two three fou[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]> \x2190 two three four[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]> \x2190two three four [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]> \x2190wo three four f[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]> \x2190o three four fi[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]> \x2190 three four fiv[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]> \x2190three four five[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]> \x2190hree four five [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]> \x2190ree four five s[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]> \x2190" L"ee four five si[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]> \x2190" L"e four five six[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Warping left */
    L"[SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT]"
    /* Warping left scroll has engaged (with five margin) */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]> \x2190hree [SLINPUT_CCC_SAVE_CURSOR]four five \x2192[SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]> \x2190 two [SLINPUT_CCC_SAVE_CURSOR]three four\x2192[SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  One [SLINPUT_CCC_SAVE_CURSOR]two three f\x2192[SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Warping right */
    L"[SLINPUT_CCC_CURSOR_RIGHT][SLINPUT_CCC_CURSOR_RIGHT][SLINPUT_CCC_CURSOR_RIGHT][SLINPUT_CCC_CURSOR_RIGHT]"
    /* Warping right scroll has engaged (with five margin) */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]> \x2190two three [SLINPUT_CCC_SAVE_CURSOR]four \x2192[SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]> \x2190hree four [SLINPUT_CCC_SAVE_CURSOR]five \x2192[SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]> \x2190" L"e four five [SLINPUT_CCC_SAVE_CURSOR]six [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* End */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]> \x2190" L"e four five six[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* New line */
    L"\n"
    /* Line wrap on */
    L"[SLINPUT_CCC_WRAP_ON]"
  );

  SLINPUT_DestroyState(state);
  EXPECT_EQ(allocated_memory_, 0);
}

/* test backspace into left margin */
TEST_F(SingleLineInput, BackspaceIntoLeftMarginWithScroll) {
  SLINPUT_Stream stream = { this };
  SLINPUT_AllocInfo alloc_info = { this };
  SLINPUT_State *state =
    SLINPUT_CreateState(alloc_info, MallocIn, FreeIn);
  ASSERT_TRUE(state);
  SLINPUT_Set_Streams(state, stream, stream);
  InitState(state);

  /* Testing with five margin */
  SLINPUT_Set_CursorMargin(state, 5);

  SLICHAR buffer[40];
  terminal_width_ = 20;

  /* Characters */
  const SLICHAR *first_input = L"One two three four five six";
  while (*first_input)
    input_.push_back( KeyInput { SLINPUT_KC_NUL, *first_input++ } );

  /* Warp left four times so scroll left occurs */
  input_.push_back( KeyInput { SLINPUT_KC_WARP_LEFT, L'\0' } );
  input_.push_back( KeyInput { SLINPUT_KC_WARP_LEFT, L'\0' } );
  input_.push_back( KeyInput { SLINPUT_KC_WARP_LEFT, L'\0' } );
  input_.push_back( KeyInput { SLINPUT_KC_WARP_LEFT, L'\0' } );

  /* Backspace */
  for (int16_t i = 0; i < 8; ++i)
    input_.push_back( KeyInput { SLINPUT_KC_BACKSPACE, L'\0' } );

  /* End input */
  input_.push_back( KeyInput { SLINPUT_KC_NUL, L'\n' } );

  EXPECT_EQ(SLINPUT_Get(state, L"> ", nullptr,
    sizeof(buffer)/sizeof(buffer[0]), buffer), 19);
  EXPECT_STREQ(buffer, L"three four five six");

  EXPECT_STREQ(output_.c_str(),
    /* Line wrap off */
    L"[SLINPUT_CCC_WRAP_OFF]"
    /* ApplyDimension and RedrawLine */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Key presses */
    L"[SLINPUT_CCC_DISABLE_CURSOR]O[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]n[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]t[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]w[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]o[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]t[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]h[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]r[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]f[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Key presses with scrolling */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  ne two three fo[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  e two three fou[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>   two three four[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  two three four [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  wo three four f[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  o three four fi[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>   three four fiv[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  three four five[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  hree four five [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  ree four five s[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  ee four five si[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  e four five six[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Warping left */
    L"[SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT]"
    /* Warping left scroll has engaged (with five margin) */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  hree [SLINPUT_CCC_SAVE_CURSOR]four five  [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>   two [SLINPUT_CCC_SAVE_CURSOR]three four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Backspaces */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  e two[SLINPUT_CCC_SAVE_CURSOR]three four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  ne tw[SLINPUT_CCC_SAVE_CURSOR]three four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  One t[SLINPUT_CCC_SAVE_CURSOR]three four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  One [SLINPUT_CCC_SAVE_CURSOR]three four  [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  One[SLINPUT_CCC_SAVE_CURSOR]three four f [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  On[SLINPUT_CCC_SAVE_CURSOR]three four fi [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  O[SLINPUT_CCC_SAVE_CURSOR]three four fiv [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  [SLINPUT_CCC_SAVE_CURSOR]three four five [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* New line */
    L"\n"
    /* Line wrap on */
    L"[SLINPUT_CCC_WRAP_ON]"
  );

  SLINPUT_DestroyState(state);
  EXPECT_EQ(allocated_memory_, 0);
}

/* test insertion into right margin */
TEST_F(SingleLineInput, InsertionIntoRightMarginWithScroll) {
  SLINPUT_Stream stream = { this };
  SLINPUT_AllocInfo alloc_info = { this };
  SLINPUT_State *state =
    SLINPUT_CreateState(alloc_info, MallocIn, FreeIn);
  ASSERT_TRUE(state);
  SLINPUT_Set_Streams(state, stream, stream);
  InitState(state);

  /* Testing with five margin */
  SLINPUT_Set_CursorMargin(state, 5);

  SLICHAR buffer[40];
  terminal_width_ = 20;

  /* Characters */
  const SLICHAR *first_input = L"One two three four five six";
  while (*first_input)
    input_.push_back( KeyInput { SLINPUT_KC_NUL, *first_input++ } );

  /* Warp left */
  input_.push_back( KeyInput { SLINPUT_KC_WARP_LEFT, L'\0' } );

  /* Characters */
  const SLICHAR *second_input = L"5.5 ";
  while (*second_input)
    input_.push_back( KeyInput { SLINPUT_KC_NUL, *second_input++ } );

  /* End input */
  input_.push_back( KeyInput { SLINPUT_KC_NUL, L'\n' } );

  EXPECT_EQ(SLINPUT_Get(state, L"> ", nullptr,
    sizeof(buffer)/sizeof(buffer[0]), buffer), 31);
  EXPECT_STREQ(buffer, L"One two three four five 5.5 six");

  EXPECT_STREQ(output_.c_str(),
    /* Line wrap off */
    L"[SLINPUT_CCC_WRAP_OFF]"
    /* ApplyDimension and RedrawLine */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Key presses */
    L"[SLINPUT_CCC_DISABLE_CURSOR]O[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]n[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]t[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]w[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]o[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]t[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]h[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]r[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]f[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Key presses with scrolling */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  ne two three fo[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  e two three fou[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>   two three four[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  two three four [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  wo three four f[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  o three four fi[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>   three four fiv[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  three four five[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  hree four five [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  ree four five s[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  ee four five si[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  e four five six[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Warp left */
    L"[SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT]"
    /* Insert 5.5 */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>   four five 5[SLINPUT_CCC_SAVE_CURSOR]six [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  four five 5.[SLINPUT_CCC_SAVE_CURSOR]six [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  our five 5.5[SLINPUT_CCC_SAVE_CURSOR]six [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  ur five 5.5 [SLINPUT_CCC_SAVE_CURSOR]six [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* New line */
    L"\n"
    /* Line wrap on */
    L"[SLINPUT_CCC_WRAP_ON]");

  SLINPUT_DestroyState(state);
  EXPECT_EQ(allocated_memory_, 0);
}

/* Check scrolling behaviour when deleting and backspace in middle of
scrollable buffer */
TEST_F(SingleLineInput, DeletionAndBackspaceAtMiddleWithScroll) {
  SLINPUT_Stream stream = { this };
  SLINPUT_AllocInfo alloc_info = { this };
  SLINPUT_State *state =
    SLINPUT_CreateState(alloc_info, MallocIn, FreeIn);
  ASSERT_TRUE(state);
  SLINPUT_Set_Streams(state, stream, stream);
  InitState(state);

  /* Testing with five margin */
  SLINPUT_Set_CursorMargin(state, 5);

  SLICHAR buffer[40];
  terminal_width_ = 20;

  /* Characters */
  const SLICHAR *first_input = L"One two three four five six";
  while (*first_input)
    input_.push_back( KeyInput { SLINPUT_KC_NUL, *first_input++ } );

  /* Warp left four times */
  input_.push_back( KeyInput { SLINPUT_KC_WARP_LEFT, L'\0' } );
  input_.push_back( KeyInput { SLINPUT_KC_WARP_LEFT, L'\0' } );
  input_.push_back( KeyInput { SLINPUT_KC_WARP_LEFT, L'\0' } );
  input_.push_back( KeyInput { SLINPUT_KC_WARP_LEFT, L'\0' } );

  /* delete 'three ' */
  for (int16_t i = 0; i < 6; ++i)
    input_.push_back( KeyInput { SLINPUT_KC_DEL, L'\0' } );

  /* backspace 'two' */
  for (int16_t i = 0; i < 4; ++i)
    input_.push_back( KeyInput { SLINPUT_KC_BACKSPACE, L'\0' } );

  /* Characters */
  const SLICHAR *second_input = L"2 3 ";
  while (*second_input)
    input_.push_back( KeyInput { SLINPUT_KC_NUL, *second_input++ } );

  /* End input */
  input_.push_back( KeyInput { SLINPUT_KC_NUL, L'\n' } );

  EXPECT_EQ(SLINPUT_Get(state, L"> ", nullptr,
    sizeof(buffer), buffer), 21);
  EXPECT_STREQ(buffer, L"One 2 3 four five six");

  EXPECT_STREQ(output_.c_str(),
    /* Line wrap off */
    L"[SLINPUT_CCC_WRAP_OFF]"
    /* ApplyDimension and RedrawLine */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Key presses */
    L"[SLINPUT_CCC_DISABLE_CURSOR]O[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]n[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]t[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]w[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]o[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]t[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]h[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]r[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]f[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Key presses with scrolling */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  ne two three fo[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  e two three fou[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>   two three four[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  two three four [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  wo three four f[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  o three four fi[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>   three four fiv[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  three four five[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  hree four five [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  ree four five s[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  ee four five si[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  e four five six[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Warp left */
    L"[SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT][SLINPUT_CCC_CURSOR_LEFT]"
    /* Warp left with scrolling */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  hree [SLINPUT_CCC_SAVE_CURSOR]four five  [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>   two [SLINPUT_CCC_SAVE_CURSOR]three four [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Delete three */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_TO_END_OF_LINE][SLINPUT_CCC_SAVE_CURSOR]hree four  [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_TO_END_OF_LINE][SLINPUT_CCC_SAVE_CURSOR]ree four f [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_TO_END_OF_LINE][SLINPUT_CCC_SAVE_CURSOR]ee four fi [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_TO_END_OF_LINE][SLINPUT_CCC_SAVE_CURSOR]e four fiv [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_TO_END_OF_LINE][SLINPUT_CCC_SAVE_CURSOR] four five [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_TO_END_OF_LINE][SLINPUT_CCC_SAVE_CURSOR]four five  [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Backspace to remove two */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  e two[SLINPUT_CCC_SAVE_CURSOR]four five  [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  ne tw[SLINPUT_CCC_SAVE_CURSOR]four five  [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  One t[SLINPUT_CCC_SAVE_CURSOR]four five  [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  One [SLINPUT_CCC_SAVE_CURSOR]four five s [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Key presses */
    L"[SLINPUT_CCC_DISABLE_CURSOR]2[SLINPUT_CCC_SAVE_CURSOR]four five  [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR]four five [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]3[SLINPUT_CCC_SAVE_CURSOR]four fiv [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR]four fi [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* New line */
    L"\n"
    /* Line wrap on */
    L"[SLINPUT_CCC_WRAP_ON]"
  );

  SLINPUT_DestroyState(state);
  EXPECT_EQ(allocated_memory_, 0);
}

/* Test escape clears the input buffer */
TEST_F(SingleLineInput, EscapeClearsInputBuffer) {
  SLINPUT_Stream stream = { this };
  SLINPUT_AllocInfo alloc_info = { this };
  SLINPUT_State *state =
    SLINPUT_CreateState(alloc_info, MallocIn, FreeIn);
  ASSERT_TRUE(state);
  SLINPUT_Set_Streams(state, stream, stream);
  InitState(state);

  SLICHAR buffer[40];
  terminal_width_ = 20;

  /* Characters */
  const SLICHAR *first_input = L"One two three four five six";
  while (*first_input)
    input_.push_back( KeyInput { SLINPUT_KC_NUL, *first_input++ } );

  /* Escape */
  input_.push_back( KeyInput { SLINPUT_KC_ESCAPE, L'\0' } );

  /* End input */
  input_.push_back( KeyInput { SLINPUT_KC_NUL, L'\n' } );

  EXPECT_EQ(SLINPUT_Get(state, L"> ", nullptr,
    sizeof(buffer)/sizeof(buffer[0]), buffer), 1);
  EXPECT_STREQ(buffer, L"\n");

  EXPECT_STREQ(output_.c_str(),
    /* Line wrap off */
    L"[SLINPUT_CCC_WRAP_OFF]"
    /* ApplyDimension and RedrawLine */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Key presses */
    L"[SLINPUT_CCC_DISABLE_CURSOR]O[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]n[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]t[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]w[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]o[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]t[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]h[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]r[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]f[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Key presses with scrolling */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  ne two three fo[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  e two three fou[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>   two three four[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  two three four [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  wo three four f[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  o three four fi[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>   three four fiv[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  three four five[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  hree four five [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  ree four five s[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  ee four five si[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  e four five six[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Escape clears the buffer */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* New line */
    L"\n"
    /* Line wrap on */
    L"[SLINPUT_CCC_WRAP_ON]");

  SLINPUT_DestroyState(state);
  EXPECT_EQ(allocated_memory_, 0);
}

TEST_F(SingleLineInput, History) {
  SLINPUT_Stream stream = { this };
  SLINPUT_AllocInfo alloc_info = { this };
  SLINPUT_State *state =
    SLINPUT_CreateState(alloc_info, MallocIn, FreeIn);
  ASSERT_TRUE(state);
  SLINPUT_Set_Streams(state, stream, stream);
  InitState(state);
  terminal_width_ = 40;

  SLINPUT_Save(state, L"Oranges and \r\nlemons\n");
  input_.push_back( KeyInput { SLINPUT_KC_UP, L'\0' } );
  input_.push_back( KeyInput { SLINPUT_KC_NUL, L'\n' } );

  SLICHAR buffer[40];

  EXPECT_EQ(SLINPUT_Get(state, L"> ", nullptr,
    sizeof(buffer)/sizeof(buffer[0]), buffer), 18);
  EXPECT_STREQ(buffer, L"Oranges and lemons");

  EXPECT_STREQ(output_.c_str(),
    /* Line wrap off */
    L"[SLINPUT_CCC_WRAP_OFF]"
    /* ApplyDimension and RedrawLine */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* History selection */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  Oranges and lemons[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* New line */
    "\n"
    /* Line wrap on */
    "[SLINPUT_CCC_WRAP_ON]"
  );

  SLINPUT_DestroyState(state);
  EXPECT_EQ(allocated_memory_, 0);
}

/* Test history */
TEST_F(SingleLineInput, HistorySelection) {
  SLINPUT_Stream stream = { this };
  SLINPUT_AllocInfo alloc_info = { this };
  SLINPUT_State *state =
    SLINPUT_CreateState(alloc_info, MallocIn, FreeIn);
  ASSERT_TRUE(state);
  SLINPUT_Set_Streams(state, stream, stream);
  InitState(state);

  /* Add some history */
  for (int32_t i = 0; i < 64; ++i) {
    SLICHAR history_buffer[16];

    swprintf(history_buffer, sizeof(history_buffer)/sizeof(history_buffer[0]),
      L"Entry: %d", i);

    SLINPUT_Save(state, history_buffer);
  }

  SLICHAR buffer[40];
  terminal_width_ = 20;

  /* Select history 8 times up - go up ten and down two */
  for (int32_t i = 0; i < 10; ++i)
    input_.push_back( KeyInput { SLINPUT_KC_UP, L'\0' } );
  
  input_.push_back( KeyInput { SLINPUT_KC_DOWN, L'\0' } );
  input_.push_back( KeyInput { SLINPUT_KC_DOWN, L'\0' } );

  /* End input */
  input_.push_back( KeyInput { SLINPUT_KC_NUL, L'\n' } );

  EXPECT_EQ(SLINPUT_Get(state, L"> ", nullptr,
    sizeof(buffer)/sizeof(buffer[0]), buffer), 9);
  EXPECT_STREQ(buffer, L"Entry: 56");

  EXPECT_STREQ(output_.c_str(),
    /* Line wrap off */
    L"[SLINPUT_CCC_WRAP_OFF]"
    /* ApplyDimension and RedrawLine */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* History selection */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  Entry: 63[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  Entry: 62[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  Entry: 61[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  Entry: 60[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  Entry: 59[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  Entry: 58[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  Entry: 57[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  Entry: 56[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  Entry: 55[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  Entry: 54[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  Entry: 55[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  Entry: 56[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* New line */
    L"\n"
    /* Line wrap on */
    L"[SLINPUT_CCC_WRAP_ON]");

  SLINPUT_DestroyState(state);
  EXPECT_EQ(allocated_memory_, 0);
}

/** Completion data for SLINPUT_CompletionInfo */
typedef struct CompletionData {
  uint32_t value;  /**< Holds value to check during completion test */
} CompletionData;

static int CompletionRequest(SLINPUT_State *state,
    SLINPUT_CompletionInfo completion_info, uint16_t string_length,
    const SLICHAR *string) {
  EXPECT_EQ(wcslen(string), string_length);

  CompletionData *completion_data =
    static_cast<CompletionData *>(completion_info.completion_info_data);
  EXPECT_EQ(completion_data->value, 42u);

  if (wcscmp(string, L"One ") == 0) {
    /* User would be promted here with options which would output text. */
    /* Therefore replace with same line to cause a redraw. */
    SLINPUT_CompletionReplace(state, string);
  } else if (wcscmp(string, L"One Two ") == 0) {
    SLINPUT_CompletionReplace(state, L"One Two Three");
  }

  return 0;
}

TEST_F(SingleLineInput, TabCommandCompletion) {
  SLINPUT_Stream stream = { this };
  SLINPUT_AllocInfo alloc_info = { this };
  SLINPUT_State *state =
    SLINPUT_CreateState(alloc_info, MallocIn, FreeIn);
  ASSERT_TRUE(state);
  SLINPUT_Set_Streams(state, stream, stream);
  InitState(state);

  SLICHAR buffer[40];
  terminal_width_ = 20;

  CompletionData completion_data;
  completion_data.value = 42;
  SLINPUT_CompletionInfo completion_info = { &completion_data };
  SLINPUT_Set_CompletionRequest(state, completion_info, CompletionRequest);
  SLINPUT_Set_LeaveRaw(state, LeaveRawIn);

  /* Characters */
  const SLICHAR *first_input = L"One ";
  while (*first_input)
    input_.push_back( KeyInput { SLINPUT_KC_NUL, *first_input++ } );

  /* Trigger command completion */
  input_.push_back( KeyInput { SLINPUT_KC_TAB, L'\0' } );

  const SLICHAR *second_input = L"Two ";
  while (*second_input)
    input_.push_back( KeyInput { SLINPUT_KC_NUL, *second_input++ } );

  /* Trigger command completion */
  input_.push_back( KeyInput { SLINPUT_KC_TAB, L'\0' } );

  /* End input */
  input_.push_back( KeyInput { SLINPUT_KC_NUL, L'\n' } );

  EXPECT_EQ(SLINPUT_Get(state, L"> ", nullptr,
    sizeof(buffer)/sizeof(buffer[0]), buffer), 13);
  EXPECT_STREQ(buffer, L"One Two Three");

  EXPECT_STREQ(output_.c_str(),
    /* Line wrap off */
    L"[SLINPUT_CCC_WRAP_OFF]"
    /* ApplyDimension and RedrawLine */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Key presses */
    L"[SLINPUT_CCC_DISABLE_CURSOR]O[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]n[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]e[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Line redrawn */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  One [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Key presses */
    L"[SLINPUT_CCC_DISABLE_CURSOR]T[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]w[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]o[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR] [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Command completion replace */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  One Two Three[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* New line */
    L"\n"
    /* Line wrap on */
    L"[SLINPUT_CCC_WRAP_ON]"
  );

  SLINPUT_DestroyState(state);
  EXPECT_EQ(allocated_memory_, 0);
}

TEST_F(SingleLineInput, InitialString) {
  SLINPUT_Stream stream = { this };
  SLINPUT_AllocInfo alloc_info = { this };
  SLINPUT_State *state =
    SLINPUT_CreateState(alloc_info, MallocIn, FreeIn);
  ASSERT_TRUE(state);
  SLINPUT_Set_Streams(state, stream, stream);
  InitState(state);

  SLICHAR buffer[256];
  terminal_width_ = 20;

  /* Characters followed by new line */
  const SLICHAR *input = L"String\n";
  while (*input)
    input_.push_back( KeyInput { SLINPUT_KC_NUL, *input++ } );

  EXPECT_EQ(SLINPUT_Get(state, L"> ", L"Initial ",
    sizeof(buffer)/sizeof(buffer[0]), buffer), 14);
  EXPECT_STREQ(output_.c_str(),
    /* Line wrap off */
    L"[SLINPUT_CCC_WRAP_OFF]"
    /* ApplyDimension and RedrawLine */
    L"[SLINPUT_CCC_DISABLE_CURSOR][SLINPUT_CCC_CLEAR_LINE]>  Initial [SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* Key presses */
    L"[SLINPUT_CCC_DISABLE_CURSOR]S[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]t[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]r[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]i[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]n[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    L"[SLINPUT_CCC_DISABLE_CURSOR]g[SLINPUT_CCC_SAVE_CURSOR] [SLINPUT_CCC_RESTORE_CURSOR][SLINPUT_CCC_ENABLE_CURSOR]"
    /* New line */
    L"\n"
    /* Line wrap on */
    L"[SLINPUT_CCC_WRAP_ON]"
  );

  /* Produces the characters */
  EXPECT_STREQ(buffer, L"Initial String");

  SLINPUT_DestroyState(state);
  EXPECT_EQ(allocated_memory_, 0);
}
