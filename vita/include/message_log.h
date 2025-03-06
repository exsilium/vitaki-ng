#pragma once
#include <stdio.h>
#include <sys/stat.h>
#include <chiaki/common.h>
#include "string.h"
#include <stdint.h>
#include <stdlib.h>

#define MLOG_LINE_LEN 80
#define MLOG_LINES 500

// A hard-wrapped message log
typedef struct vita_chiaki_message_log_t {
  FILE *fp;
  char log[MLOG_LINES+1][MLOG_LINE_LEN+1];
  uint8_t start_offset;
  uint8_t lines;
  uint64_t last_update;
} VitaChiakiMessageLog;

VitaChiakiMessageLog* message_log_create();
void write_message_log(VitaChiakiMessageLog* ml, const char* text);
char* get_message_log_line(VitaChiakiMessageLog* ml, size_t line);
