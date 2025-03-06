#include "message_log.h"
#include "config.h"

VitaChiakiMessageLog* message_log_create() {
  VitaChiakiMessageLog* ml = (VitaChiakiMessageLog*)malloc(sizeof(VitaChiakiMessageLog));
  ml->start_offset = 0; // offset of first line
  ml->lines = 0; // total lines
  ml->last_update = 0; // update time

  // create an extra blank line at the end
  ml->log[MLOG_LINES][0] = 0;

#if LOG_TO_FILE
  // Check if the file exists
  struct stat buffer;
  if (stat(LOG_FILENAME, &buffer) == 0) {
    // File exists, delete it
    if (remove(LOG_FILENAME) != 0) {
      ml->fp = NULL;
      write_message_log(ml, "Failed to delete existing log file, debug file is not written");
	  }
  }
  // Open a new file for writing
  ml->fp = fopen(LOG_FILENAME, "w");
  if (ml->fp == NULL) {
    write_message_log(ml, "Failed to open log file for writing: " LOG_FILENAME);
  }
#else
    ml->fp = NULL;
#endif
  return ml;
}

void write_message_log(VitaChiakiMessageLog* ml, const char* text) {
  size_t text_len = strlen(text);
  if (text_len == 0) return;

  int text_lines = 1 + (text_len-1) / MLOG_LINE_LEN;
  int start_line = text_lines > MLOG_LINES ? (text_lines - MLOG_LINES) : 0;
  int offset = 0;
  for (int j = start_line; j < text_lines; j++) {
    int n = MLOG_LINE_LEN;
    if (j == text_lines-1) { // on the last line
        n = ((text_len-1) % MLOG_LINE_LEN)+1;
    }

    int line_offset = ml->lines; // if not yet wrapped
    if (ml->lines >= MLOG_LINES) {
      line_offset = ml->start_offset;
      ml->start_offset = (ml->start_offset + 1) % MLOG_LINES;
    }

    memcpy(ml->log[line_offset], text + offset, n);
    ml->log[line_offset][n+1] = 0; // add null char
    ml->lines++;
    if (ml->lines > MLOG_LINES) ml->lines = MLOG_LINES;
    offset += MLOG_LINE_LEN;

  }
  ml->last_update = sceKernelGetProcessTimeWide();

  if (ml->fp) {
    fprintf(ml->fp, "%s\n", text);
    fflush(ml->fp);
  }
}

char* get_message_log_line(VitaChiakiMessageLog* ml, size_t line) {
    if (ml->lines == 0) return ml->log[MLOG_LINES];

    int line_offset = (ml->start_offset + line) % MLOG_LINES;

    if (line_offset < 0) line_offset = MLOG_LINES;
    if (line_offset >= ml->lines) line_offset = MLOG_LINES;
    if (line_offset >= MLOG_LINES) line_offset = MLOG_LINES;

    return ml->log[line_offset];
}
