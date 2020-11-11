// 2020-11-04

#define INCLUDING_SM

#include "macros.h"
#include "state_machine.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GENERIC_BUF_SIZE 256
#define FB_MAX_BUF_SIZE 20480 // 20 KiB
#define LABEL_MAX_SIZE 64

#define FB_INITIALIZED 0x01
#define FB_LOADED_LABELS 0x04

const static char filespath[] = "./dfiles/";


typedef struct {
  long int fpos; // Position of label in file
  char   * text; // Pointer into fb.label_texts for text of label
} label;

typedef struct {
  const char   * fname;       // File name
  FILE         * fhandle;     // Handle for open file
  char         * label_texts; // Block of memory to hold all labels
  label        * labels;      // Block of memory to hold all label structs
  long int       fsize;       // File size
  unsigned int   label_count; // Number of labels in file
  char           operations;  // Indicator for actions performed
//char           file_in_buf; // Boolean indicating if entire file is in buf
  char           no_use_buf;  // Boolean to prevent loading buf, if desired
  char         * buf;         // Buffer for file contents if within set limit
} fileblock;


long int get_file_size(FILE *);
int dump_file_contents(FILE *);

int init_fileblock(fileblock *);
int close_fileblock(fileblock *);
int load_fileblock_file_maybe(fileblock *);
int load_fileblock_labels(fileblock *);


/* The main event.
 */
int main(int argl, char ** argv){

  const char * fname;

  if(argl > 1){
    fname = argv[1];
  } else {
    fname = filespath;
  }

  fileblock fblock = {
    .fname = fname,
    //.no_use_buf = 1,
  };
  fileblock * fb = &fblock;
  init_fileblock(fb);

  printf("Opened fileblock for: %s\n", fb->fname);
  printf("Size: %ld\n", fb->fsize);

  if(fb->buf != NULL){
    printf("Contents of buffer:\n");
    fwrite(fb->buf, sizeof(char), fb->fsize, stdout);
    printf("\n");
  } else {
    printf("Buffer not used\n");
  }

  close_fileblock(fb);
  printf("Closed fileblock for: %s\n", fb->fname);

  return 0;
}


/* Initializes a fileblock by inspecting the file to fill its fields.
 * The fileblock pass to this function should have a filename set in the fname field.
 */
int init_fileblock(fileblock * fb){
  if(fb == NULL) return 1;
  if(fb->fname == NULL) return 2;

  fb->operations &= ~(FB_INITIALIZED);

  // Clean up potential old data from fileblock
  close_fileblock(fb);

  if(fb->operations != 0)
    fprintf(stderr, "Warning: fb operations not clear (%d)\n", (int)fb->operations);

  // The following are already set appropriately by close_fileblock
  // fb->label_texts
  // fb->labels
  // fb->label_count
  // fb->operations
  // fb->file_in_buf
  // fb->buf

  // Load new data into fileblock

  fb->fhandle = fopen(fb->fname, "r");
  if(fb->fhandle == NULL) return 3;

  fb->fsize = get_file_size(fb->fhandle);
  if(!fb->fsize){
    fprintf(stderr, "Error in getting file size\n");
    return 3;
  }

  load_fileblock_file_maybe(fb);

  load_fileblock_labels(fb);

  // TODO

  // Set to initialized only
  fb->operations = FB_INITIALIZED;

  return 0;
}


/* Clean up a fileblock and free all memory that was allocated.
 */
int close_fileblock(fileblock * fb){
  if(fb == NULL) return 1;

  if(fb->fhandle != NULL){
    DEBUGPRINT("Cleaning up opened file")
    if(fclose(fb->fhandle) != 0){
      fprintf(stderr, "Error closing already opened file\n");
      return  2;
    }
    fb->fhandle = NULL;
  }

  if(fb->labels != NULL) free(fb->labels);
  else if(fb->label_count){
    DEBUGPRINT("No label pointer to clean up")
  }
  fb->labels = NULL;

  if(fb->label_texts != NULL) free(fb->label_texts);
  else if(fb->label_count){
    DEBUGPRINT("No label text pointer to clean up")
  }
  fb->label_texts = NULL;

  // Labels cleared, so clear action flag
  fb->operations &= ~(FB_LOADED_LABELS);

  // Not strictly a part of closing the fileblock, but since we're clearing the
  // buffers, we will also set these to keep state consistent
  fb->label_count = 0;
  //fb->file_in_buf = 0;

  //memset(fb->buf, '\0', FB_MAX_BUF_SIZE);

  return 0;
}


/* If the size of the file is less than the defined threshold, create a buffer
 * and load the file contents into it. If the size is larger than the threshold,
 * do nothing.
 */
int load_fileblock_file_maybe(fileblock * fb){
  // No use buf flag set -- not an error condition
  if(fb->no_use_buf) return 0;
  // File larger than threshold -- not an error condition
  if(fb->fsize > FB_MAX_BUF_SIZE) return 0;

  size_t buf_size = fb->fsize * sizeof(char);

  // Create a buffer to match the file size
  fb->buf = (char *)malloc(buf_size);
  if(fb->buf == NULL){
    fprintf(stderr, "Error allocating whole-file buffer\n");
    return 1;
  }

  rewind(fb->fhandle);

  char reset_buf = 0;
  size_t read_bytes = fread(fb->buf, sizeof(char), fb->fsize, fb->fhandle);

  if(read_bytes != buf_size){
    // Print general info
    fprintf(
      stderr,
      "Mismatch between read size (%llu) and file size (%lu): ",
      (long long)read_bytes,
      fb->fsize
    );

    // Determine and print more specific info
    if(feof(fb->fhandle)){
      fprintf(stderr, "End of file reached while reading\n");
    } else if(ferror(fb->fhandle)){
      fprintf(stderr, "Read error occurred\n");
      perror("Error message");
      reset_buf = 1;
    } else {
      fprintf(stderr, "Unknown reason for mismatch\n");
      reset_buf = 1;
    }
  }

  if(reset_buf){
    free(fb->buf);
    fb->buf = NULL;
    return 1;
  }

  return 0;
}


/* Reads the labels from the configured file in the provided initialized
 * fileblock and stores them properly into said fileblock.
 *
 * This function uses the buf in the fileblock if available.
 */
int load_fileblock_labels(fileblock * fb){
  fprintf(stderr, "load labels under construction\n"); return -1;
  if(fb == NULL) return 1;
  if(!(fb->operations | FB_INITIALIZED)) return 2;
  if(fb->operations & FB_LOADED_LABELS) return 3;

  FILE * f = fb->fhandle;
  char * buf;
  char using_fb_buf;
  long int bufsize = 0;
  unsigned int lcount = 0;
  sm_func smf = NULL;

  // Use fb buf if available, otherwise allocate buffer
  if(fb->buf == NULL){
    using_fb_buf = 0;
    bufsize = 4096;
    buf = malloc(bufsize * sizeof(char));
  } else {
    using_fb_buf = 1;
    bufsize = fb->fsize;
    buf = fb->buf;
  }

  if(buf == NULL){
    fprintf(stderr, "Error allocating buffer to load labels\n");
    return 5;
  }

  // Linked list of position blocks for tracking all label positions
  int lt_pos_size = 128;
  struct labeltrack {
    struct labeltrack * next;
    long int positions[lt_pos_size];
  };

  // First pass: Figure out how many labels are present
  // TODO: Track some (or all w/e.g. linked list) of label positions in file

  struct labeltrack root;
  struct labeltrack * lt_current = &root;
  int lt_blocks = 1;
  int lt_pos = 0;

  if(!using_fb_buf) rewind(f);
  while(1){
    size_t read;

    // Fill buffer as needed, set size of data
    if(!using_fb_buf){
      if(feof(f)) break;
      read = fread(buf, sizeof(char), bufsize, f);
      if(ferror(f)){
        perror("Error reading file to get label count");
        return 4;
      }
    } else {
      read = fb->fsize;
    }

    // Run state machine on the current chunk of data
    for(long int pos = 0; pos < read; ++pos){
      int rval = run_iteration(&smf, buf[pos], NULL);

      // Track transition into label
      if(rval == 2){
        long int cl_pos;
        if(using_fb_buf) cl_pos = pos;
        else cl_pos = ftell(f);

        lt_current->positions[lt_pos] = cl_pos;
        ++lcount;

        if(++lt_pos % lt_pos_size == 0){
          lt_current->next = (struct labeltrack *)malloc(sizeof(struct labeltrack));
          lt_current = lt_current->next;
          ++lt_blocks;
        }
      }
    }

    // If using fb buf, all of file done in one go
    if(using_fb_buf) break;
  }

  DEBUGPRINTD("Found labels", lcount);
  DEBUGPRINTD("Used labeltrack blocks", lt_blocks);

  // TEST PRINT::
  lt_current = &root;
  int bc = 1;
  while(lt_current != NULL){
    for(int p = 0; p < lt_pos_size; ++p)
      fprintf(stderr, "Block %2d,%3d : %ld\n", bc, p, lt_current->positions[p]);
    ++bc;
  }
  // ::TEST PRINT

  // Allocate appropriately sized stuff

  fb->label_count = lcount;
  fb->label_texts = malloc(lcount * LABEL_MAX_SIZE * sizeof(char));
  fb->labels = malloc(lcount * sizeof(label));

  if(fb->label_texts == NULL || fb->labels == NULL){
    free(fb->label_texts);
    free(fb->labels);
    fprintf(stderr, "Error allocating space for labels\n");
    return 5;
  }

  // Second pass: Store label information
  // TODO: Use the labeltrack chain

  unsigned int lcurrent = 0;
  if(!using_fb_buf) rewind(f);
  while(1){
    size_t read;

    // Fill buffer as needed, set size of data
    if(!using_fb_buf){
      if(feof(f)) break;
      read = fread(buf, sizeof(char), bufsize, f);
      if(ferror(f)){
        perror("Error reading file to get label count");
        return 4;
      }
    } else {
      read = fb->fsize;
    }

    // Run state machine on the current chunk of data
    for(long int pos = 0; pos < read; ++pos){
      int rval = run_iteration(&smf, buf[pos], NULL);

      // State machine ends after label
      if(rval == 0) ++lcount;
    }

    // If using fb buf, all of file done in one go
    if(using_fb_buf) break;
  }

  // TODO: Free labeltrack chain

  // Free buffer if we allocated our own
  if(!using_fb_buf) free(buf);
}


/* Print out the size of the given file (or so).
 * Returns a negative value on error.
 * May be improved by using e.g. fstat
 */
long int get_file_size(FILE * f){
  if(f == NULL) return -1L;

  long int ppos = ftell(f);

  if(fseek(f, 0, SEEK_END) != 0){
    perror("Error occurred while seeking");
    return -1L;
  }

  long int size = ftell(f);

  if(fseek(f, ppos, SEEK_SET) != 0){
    perror("Error occurred while seeking to reset");
    // Okay to continue, but position in file has likely changed
  }

  return size;
}


/* Dump out the contents of the given file.
 */
int dump_file_contents(FILE * f){
  if(f == NULL) return 1;

  rewind(f);

  printf(":::Contents Start:::\n");
  char buf[GENERIC_BUF_SIZE] = {0};
  do {
    int to_read = GENERIC_BUF_SIZE - 1;
    int read = fread((void *)buf, sizeof(char), to_read, f);
    buf[read] = '\0';
    fprintf(stdout, "%s", buf);

    if(read < to_read){
      if(!feof(f)) perror("Error occurred in reading file");
      break;
    }
  } while(1);
  printf(":::Contents End:::\n");
}

