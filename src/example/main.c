#include <stdio.h>
#include <stdlib.h>
#include <locale.h>

#include "include/slinput.h"

static int completion_request(SLINPUT_State *state,
    SLINPUT_CompletionInfo completion_info,
    sli_ushort string_length,
    const sli_char *string) {
  if (string_length == 1 && string[0] == L'u') {
    SLINPUT_CompletionReplace(state, L"up");
  } else if (string_length == 1 && string[0] == L'd') {
    SLINPUT_CompletionReplace(state, L"down");
  } else {
    printf("\nNo completion options\n");
    SLINPUT_CompletionReplace(state, string);
  }

  return 0;
}

int main(int argc, char **argv) {
  SLINPUT_AllocInfo alloc_info = { NULL };
  SLINPUT_CompletionInfo completion_info = { NULL };
  int result = 1;
  SLINPUT_State *state;
  sli_char buffer[256];

  if (setlocale(LC_CTYPE, "") == NULL)
    return EXIT_FAILURE;

  state = SLINPUT_CreateState(alloc_info, NULL, NULL);
  if (state == NULL)
    return EXIT_FAILURE;

  SLINPUT_Set_CompletionRequest(state, completion_info,
    completion_request);

  while (result > 0) {
    result = SLINPUT_Get(state, L"> ", NULL, sizeof(buffer)/sizeof(buffer[0]),
      buffer);
    if (result > 0 && SLINPUT_Save(state, buffer) < 0)
      result = -1;
  }

  SLINPUT_DestroyState(state);

  return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
