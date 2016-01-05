/*
 * tiny_initrd - Minimalistic initrd implementation
 * Copyright (C) 2016 Christian Seiler <christian@iwakd.de>
 *
 * io.c: I/O helper functions
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include "tiny_initrd.h"

typedef struct fstab_find_fs_data {
  const char *dest;
  fstab_info *info;
  int found;
} fstab_find_fs_data;

static int process_fstab_entry(fstab_find_fs_data *data, const char *line, int line_is_incomplete);

int fstab_find_fs(const char *dest, fstab_info *info)
{
  int r;
  fstab_find_fs_data data = { dest, info, 0 };
  r = traverse_file_by_line(TARGET_DIRECTORY FSTAB_FILENAME, (traverse_line_t)process_fstab_entry, &data);
  if (r < 0)
    return r;
  if (!data.found)
    return -ENOENT;
  return 0;
}

int process_fstab_entry(fstab_find_fs_data *data, const char *orig_line, int line_is_incomplete)
{
  char *saveptr;
  char *token;
  int had_nws, i;
  char line[MAX_LINE_LEN] = { 0 };
  char *fields[6] = { 0 };

  /* more than MAX_LINE_LEN in fstab? just ignore it */
  if (line_is_incomplete)
    return 0;

  /* ignore comments and empty lines */
  if (!orig_line || !*orig_line || *orig_line == '#')
    return 0;

  /* ignore whitespace-only lines */
  had_nws = 0;
  for (token = (char *)orig_line; *token; token++) {
    if (*token != ' ' && *token != '\t') {
      had_nws = 1;
      break;
    }
  }
  if (!had_nws)
    return 0;

  strncpy(line, orig_line, MAX_LINE_LEN - 1);

  i = 0;
  for (token = strtok_r(line, " \t", &saveptr); token != NULL; token = strtok_r(NULL, " \t", &saveptr)) {
    if (i < 6) {
      fields[i] = token;
    }
    i++;
  }

  if (i != 6)
    return 0;

  /* we are only interested in /usr */
  if (strcmp(fields[1], data->dest) != 0)
    return 0;

  if (strncmp(fields[0], "/dev/", 5) != 0)
    return -ENODEV;

  /* NOTE: for the /usr use casee this is sufficient, but in general
   *       one needs to de-escape the source and dest fields, see e.g.
   *       the fstab-decode utility.
   */
  strncpy(data->info->source, fields[0], MAX_PATH_LEN - 1);
  strncpy(data->info->dest, fields[1], MAX_PATH_LEN - 1);
  strncpy(data->info->type, fields[2], MAX_PATH_LEN - 1);
  strncpy(data->info->options, fields[3], MAX_LINE_LEN - 1);
  data->info->dump = strtoul(fields[4], NULL, 10);
  data->info->pass = strtoul(fields[5], NULL, 10);
  data->found = 1;
  return 1;
}
