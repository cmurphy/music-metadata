#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int read_id3v1_tag(char * buffer, size_t max, FILE * fp)
{
  int index = 0;
  while(index < max) {
    buffer[index] = fgetc(fp);
    ++index;
  }
  // Get rid of annoying space padding
  if (index == max && buffer[index-1] == ' ') {
    for(--index; buffer[index] == ' '; --index);
    buffer[index+1] = '\0';
  }
}

int find_id3v1_start(FILE * fp)
{
  fseek(fp, -3, SEEK_END);
  long int end = ftell(fp);
  int max_read = 200;
  while(end - ftell(fp) < max_read) {
    char c;
    if((c = fgetc(fp)) == 'T') {
      if((c = fgetc(fp)) == 'A') {
        if((c = fgetc(fp)) == 'G') {
          return 0;
        }
        ungetc(c, fp);
      }
      ungetc(c, fp);
    }
    ungetc(c, fp);
    fseek(fp, -1, SEEK_CUR);
  }
  return 1;
}

void get_id3v1_tags(const char * file, char * title, char * artist, char * album)
{
  FILE *fp = fopen(file, "r");
  find_id3v1_start(fp);
  size_t len = 30;
  read_id3v1_tag(title, len, fp);
  read_id3v1_tag(artist, len, fp);
  read_id3v1_tag(album, len, fp);
  fclose(fp);
}

int find_frame_id(FILE *fp, const char * frame_id, int frame_id_length)
{
  const int max_search = 2000;
  int current;
  int frame_id_byte_index = 0;
  char * chars_read = malloc(sizeof(char) * (frame_id_length + 1));
  while((current = ftell(fp)) < max_search && frame_id_byte_index < frame_id_length) {
    frame_id_byte_index = 0;
    char c;
    while((ftell(fp) - current) < frame_id_length && (c = fgetc(fp)) == frame_id[frame_id_byte_index]) {
      chars_read[frame_id_byte_index] = c;
      ++frame_id_byte_index;
    }
    // There was a partial false positive, so backtrack
    if(frame_id_byte_index > 0 && frame_id_byte_index < frame_id_length) {
      ungetc(c, fp);
      --frame_id_byte_index;
      while(frame_id_byte_index > 0) {
        ungetc(chars_read[frame_id_byte_index], fp);
        --frame_id_byte_index;
      }
    }
  }
  if(current == max_search) {
    //TODO: turn this into debug logging
    //printf("Failed to find frame id %s.\n", frame_id);
    return 1;
  }
  return 0;
}

int eat_garbage(FILE *fp)
{
  fgetc(fp); fgetc(fp); // Eat flags
  int is_unicode = fgetc(fp);
  if(is_unicode == 1) {
    fgetc(fp); fgetc(fp); // Eat encoding descriptor
    return 3;
  }
  return 1;
}

void read_frame_body(FILE *fp, int size, char * buffer)
{
  int buffer_index = 0;
  int source_index = 0;
  while(source_index < size) {
    char c = fgetc(fp);
    if(c != 0) {
      buffer[buffer_index++] = c;
    }
    ++source_index;
  }
  buffer[buffer_index] = 0;
}

int read_id3v22_tag(char * buffer, const char * tag, FILE *fp)
{
  int failed = find_frame_id(fp, tag, 3);
  if(failed) {
    return 1;
  }
  int ind = 0;
  while(ind < 2) {
    fgetc(fp);
    ++ind;
  }
  int size = fgetc(fp) - 2;
  fgetc(fp);
  read_frame_body(fp, size, buffer);
  fseek(fp, 0, SEEK_SET);
  return 0;
}

int read_id3v23_tag(char * buffer, const char * tag, FILE *fp)
{
  int failed = find_frame_id(fp, tag, 4);
  if(failed) {
    return 1;
  }
  int ind = 0;
  while(ind < 3) {
    fgetc(fp);
    ++ind;
  }
  int size = fgetc(fp);
  size = size - eat_garbage(fp);
  read_frame_body(fp, size, buffer);
  fseek(fp, 0, SEEK_SET);
  return 0;
}

int read_id3v24_tag(char * buffer, const char * tag, FILE *fp)
{
  int failed = find_frame_id(fp, tag, 4);
  if(failed) {
    return 1;
  }
  int ind = 0;
  while(ind < 3) {
    fgetc(fp);
    ++ind;
  }
  int size = fgetc(fp);
  size = size - eat_garbage(fp);
  read_frame_body(fp, size, buffer);
  fseek(fp, 0, SEEK_SET);
  return 0;
}

int get_id3v22_tags(const char * file, char * title, char * artist, char * album)
{
  FILE *fp = fopen(file, "r");
  int failed = 1;
  failed &= read_id3v22_tag(title, "TT2", fp);
  failed &= read_id3v22_tag(artist, "TP1", fp);
  failed &= read_id3v22_tag(album, "TAL", fp);
  fclose(fp);
  return failed;
}

int get_id3v23_tags(const char * file, char * title, char * artist, char * album)
{
  FILE *fp = fopen(file, "r");
  int failed = 1;
  failed &= read_id3v23_tag(title, "TIT2", fp);
  failed &= read_id3v23_tag(artist, "TPE1", fp);
  failed &= read_id3v23_tag(album, "TALB", fp);
  fclose(fp);
  return failed;
}

int get_id3v24_tags(const char * file, char * title, char * artist, char * album)
{
  FILE *fp = fopen(file, "r");
  int failed = 1;
  failed &= read_id3v24_tag(title, "TIT2", fp);
  failed &= read_id3v24_tag(artist, "TPE1", fp);
  failed &= read_id3v24_tag(album, "TALB", fp);
  fclose(fp);
  return failed;
}

int is_id3(FILE *fp)
{
  fseek(fp, 0, SEEK_SET);
  char buffer[4];
  fgets(buffer, 4, fp);
  if (strcmp(buffer, "ID3") == 0) {
    return 1;
  }
  return 0;
}

int is_id3v22(FILE *fp)
{
  return is_id3(fp) && (fgetc(fp) == 2);
}

int is_id3v23(FILE *fp)
{
  return is_id3(fp) && (fgetc(fp) == 3);
}

int is_id3v24(FILE *fp)
{
  return is_id3(fp) && (fgetc(fp) == 4);
}

int get_format(const char * file)
{
  int format = -1;
  FILE *fp = fopen(file, "r");
  // todo - check is_id3 first
  // todo - use consts for format ids
  if (is_id3v22(fp)) {
    format = 22;
  } else if (is_id3v23(fp)) {
    format = 23;
  } else if (is_id3v24(fp)) {
    format = 24;
  } else {
    format = 1;
  }
  fclose(fp);
  return format;
}

int main(int argc, char ** argv)
{
  char title[100] = "title not found";
  char artist[100] = "artist not found";
  char album[100] = "album not found";
  if(argc < 2) {
    fprintf(stderr, "Must provide file path to read.\n");
    exit(1);
  }
  int format = get_format(argv[1]);
  int failed = 0;
  switch(format) {
    case 1:
      get_id3v1_tags(argv[1], title, artist, album);
      break;
    case 22:
      failed = get_id3v22_tags(argv[1], title, artist, album);
      if(failed) {
        get_id3v1_tags(argv[1], title, artist, album);
      }
      break;
    case 23:
      failed = get_id3v23_tags(argv[1], title, artist, album);
      if(failed) {
        get_id3v1_tags(argv[1], title, artist, album);
      }
      break;
    case 24:
      failed = get_id3v24_tags(argv[1], title, artist, album);
      if(failed) {
        get_id3v1_tags(argv[1], title, artist, album);
      }
  }
  printf("%s, %s, %s\n", title, artist, album);
  return 0;
}
