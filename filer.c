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

#define LABELTRACK_SLOTS 64

#define FB_INITIALIZED 0x01
#define FB_LOADED_LABELS 0x04

const static char filespath[] = "./dfiles/";


typedef struct labeltracknode_s {
  struct labeltracknode_s * next;
  unsigned int              lengths[LABELTRACK_SLOTS];
  long int                  positions[LABELTRACK_SLOTS];
  char                      label_texts[LABELTRACK_SLOTS * LABEL_MAX_SIZE];
} labeltracknode;

typedef struct {
  char         * text;   // Pointer into fb.label_texts for text of label
  long int       fpos;   // Position of label in file
  unsigned int   length; // Length of label text
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


int init_fileblock(fileblock *);
int close_fileblock(fileblock *);
int load_fileblock_file_maybe(fileblock *);
int load_fileblock_labels(fileblock *);

void list_fileblock_labels(fileblock *);
void show_fileblock_section(fileblock *, unsigned int);

long int get_file_size(FILE *);
int dump_file_contents(FILE *);
void test_dump_fileblock(fileblock *);

int get_user_number(int, int, int *);


/* The main event.
 */
int main(int argl, char ** argv){

  const char * fname;
  int rval;

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

  if(rval = init_fileblock(fb)){
    fprintf(stderr, "Error initializing file (%d)\n", rval);
    return 2;
  }

  int input;
  char running = 1;

  while(running){
    if(feof(stdin) || ferror(stdin)) return 2;

    printf("\nFile: %s\n", fname);
    printf("\
Which action to take?\n\
  0: Exit\n\
  1: Test dump\n\
  2: List labels\n\
  3: View label contents\n\
");
    input = get_user_number(0, 3, NULL);
    if(input < 0){
      fprintf(stderr, "Input error\n");
      return 2;
    }

    printf("\n");

    switch(input){
    case 0: running = 0; break;
    case 1: test_dump_fileblock(fb); break;
    case 2: list_fileblock_labels(fb); break;
    case 3:
      if(fb->label_count < 1){
        printf("No labels found, sorry\n");
        break;
      }

      printf("Select label. ");

      input = get_user_number(0, fb->label_count - 1, NULL);
      if(input < 0){
        fprintf(stderr, "Input error\n");
        return 2;
      }

      show_fileblock_section(fb, (unsigned int)input);
      break;
    }
  }

  close_fileblock(fb);
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

  if(fb->buf != NULL) free(fb->buf);
  fb->buf = NULL;

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

  if(fb->buf != NULL){
    fprintf(stderr, "Buffer already allocated? Not reallocating.\n");
    return 1;
  }

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


/* Reads the labels from the given fileblock's file and adds label information
 * into to the labeltrack list, adding on new nodes as needed. This function
 * does not perform any cleanup of labeltrack nodes, even on error.
 *
 * This function uses the buf in the fileblock if available.
 *
 * Returns the number of labels found on success, or a negative number on error.
 */
int fill_labeltrackers(fileblock * fb, labeltracknode * root){
  if(fb == NULL) return -1;
  if(root == NULL) return -1;
  if(!(fb->operations | FB_INITIALIZED)) return -2;
  // This function doesn't care if labels were already loaded,
  // as it does not modify the fileblock

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
    return -3;
  }

  int lt_slot = 0;
  int lt_text_pos = 0;
  int lt_blocks = 1;
  labeltracknode * lt_current = root;

  memset(lt_current->label_texts, '\0', sizeof(lt_current->label_texts));

  // Outer loop is for reading chunks of file
  // Inner loop is for moving through read buffer
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
      char * lstore = (lt_text_pos < LABEL_MAX_SIZE)
        ? lt_current->label_texts + (LABEL_MAX_SIZE * lt_slot) + lt_text_pos
        : NULL
      ;

      int rval = run_iteration(&smf, buf[pos], lstore);

      if(rval == 0){
        // Finished with a label:
        // Put null char in text if room, store label length,
        // advance and reset counters/trackers, allocate new node if needed

        unsigned int lt_text_len = LABEL_MAX_SIZE;

        if(lt_text_pos < LABEL_MAX_SIZE){
          lt_current->label_texts[LABEL_MAX_SIZE * lt_slot + lt_text_pos] = '\0';
          lt_text_len = (unsigned int)strlen(lt_current->label_texts + LABEL_MAX_SIZE * lt_slot);
        }
        lt_text_pos = 0;

        lt_current->lengths[lt_slot] = lt_text_len;

        DEBUGPRINT_V("Finished label")

        // Increment slot and allocate a new node if all slots used up
        if(++lt_slot % LABELTRACK_SLOTS == 0){
          lt_current->next = (labeltracknode *)malloc(sizeof(labeltracknode));
          lt_current = lt_current->next;
          lt_slot = 0;
          ++lt_blocks;

          memset(lt_current->label_texts, '\0', sizeof(lt_current->label_texts));
        }
      } else if(rval == 2){
        // Track transition into label
        long int cl_pos;
        if(using_fb_buf) cl_pos = pos;
        else cl_pos = (ftell(f) - read) + pos;
        cl_pos += 1; // Shift forward to start of label

        DEBUGPRINTD_V("Label transition at", (int)cl_pos)

        lt_current->positions[lt_slot] = cl_pos;
        ++lcount;
      } else if(rval == 3){
        // Advance text store pointer if something was stored
        ++lt_text_pos;
      }
    }

    // If using fb buf, all of file done in one go
    if(using_fb_buf) break;
  }

  DEBUGPRINTD("Found labels", lcount)
  DEBUGPRINTD("Used labeltrack blocks", lt_blocks)

  // Free buffer if we allocated our own
  if(!using_fb_buf) free(buf);

  return lcount;
}


/* Follows the labeltrack chain from the given root, cleaning up everything as
 * it goes.
 */
void free_labeltrack_chain(labeltracknode * lt_current){
  while(lt_current != NULL){
    labeltracknode * lt_next = lt_current->next;
    free(lt_current);
    lt_current = lt_next;
  }
}


/* Reads the labels from the configured file in the provided initialized
 * fileblock and stores them properly into said fileblock.
 *
 * This function uses the buf in the fileblock if available.
 */
int load_fileblock_labels(fileblock * fb){
  //fprintf(stderr, "load labels under construction\n"); return -1;
  if(fb == NULL) return 1;
  if(!(fb->operations | FB_INITIALIZED)) return 2;
  if(fb->operations & FB_LOADED_LABELS) return 3;

  labeltracknode root = {0};
  labeltracknode * lt_current;

  // Step 1: Get info about labels

  int lcount = fill_labeltrackers(fb, &root);
  DEBUGPRINTD_V("Got labels", lcount)
  if(lcount < 0){
    fprintf(stderr, "Error occured in getting label info\n");
    return 4;
  } else if(lcount == 0) {
    DEBUGPRINT("No labels found")
    // Not an error condition, nothing further to do. Not necessary to
    // free labeltrack chain, because nothing should have been allocated.
    return 0;
  }

  /*
  // Step 1.5: Check what we gots

  // TEST PRINT::
  lt_current = &root;
  int bc = 1;
  while(lt_current != NULL){
    for(int p = 0; p < LABELTRACK_SLOTS; ++p)
      fprintf(stderr,
        "Block %2d,%3d (%u) : %ld - %s\n",
        bc, p,
        lt_current->lengths[p],
        lt_current->positions[p],
        &lt_current->label_texts[p * LABEL_MAX_SIZE]
      );
    ++bc;
    lt_current = lt_current->next;
  }
  // ::TEST PRINT
  */

  // Step 2: Allocate appropriately sized stuff

  size_t total_labels_len = 0;

  lt_current = &root;
  while(lt_current != NULL){
    for(int j = 0; j < LABELTRACK_SLOTS; ++j)
      total_labels_len += lt_current->lengths[j];
    lt_current = lt_current->next;
  }

  DEBUGPRINTD("Total label length", (int)total_labels_len)

  fb->label_count = lcount;
  fb->label_texts = malloc(total_labels_len * sizeof(char));
  fb->labels = malloc(lcount * sizeof(label));

  if(fb->label_texts == NULL || fb->labels == NULL){
    free(fb->label_texts);
    free(fb->labels);
    fprintf(stderr, "Error allocating space for labels\n");
    return 5;
  }

  // Step 3: Store label information in condensed format

  int ltxt_pos = 0;
  lt_current = &root;
  while(lt_current != NULL){
    for(int j = 0; j < lcount; ++j){
      label * lab = &fb->labels[j];
      lab->fpos = lt_current->positions[j];
      lab->length = lt_current->lengths[j];

      char * tp = fb->label_texts + ltxt_pos;
      lab->text = tp;
      memcpy(tp, lt_current->label_texts + j * LABEL_MAX_SIZE, lab->length);

      ltxt_pos += lab->length;
    }

    lt_current = lt_current->next;
  }

  // Free labeltrack chain; root is on the stack
  free_labeltrack_chain(root.next);

  return 0;
}


/* List the labels contined in the given fileblock to stdout.
 */
void list_fileblock_labels(fileblock * fb){
  if(fb == NULL) return;

  printf("::: %u labels :::\n", fb->label_count);

  for(unsigned int j = 0; j < fb->label_count; ++j){
    label * lab = &fb->labels[j];
    char labuf[LABEL_MAX_SIZE + 1];

    memcpy(labuf, lab->text, (size_t)lab->length);
    labuf[lab->length] = '\0';

    printf("%3u: %s\n", j, labuf);
  }
}


/* Output the text contained in the fileblock from the given label up to the
 * next label. Will output the label line as well.
 */
void show_fileblock_section(fileblock * fb, unsigned int label){
  if(fb == NULL) return;
  if(label < 0 || label > fb->label_count) return;

  long int startpos = fb->labels[label].fpos;
  long int endpos;
  long int total;

  // TODO: Adjust for delimeter
  if(label + 1 < fb->label_count)
    endpos = fb->labels[label + 1].fpos;
  else
    endpos = fb->fsize;

  total = endpos - startpos;

  char buf[total];

  // TODO: Add error handling
  fseek(fb->fhandle, startpos, SEEK_SET);
  fread(buf, sizeof(char), total, fb->fhandle);
  fwrite(buf, sizeof(char), total, stdout);
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

  return 0;
}


/* Test function to dump various infos for inspecting a fileblock.
 */
void test_dump_fileblock(fileblock * fb){
  // Fileblock inited in main, but operation should be idempotent
  if(init_fileblock(fb)){
    printf("Error initializing fileblock\n");
    return;
  }

  printf("Opened fileblock for: %s\n", fb->fname);
  printf("Size: %ld\n", fb->fsize);

  printf("Found %u labels:\n", fb->label_count);
  for(int j = 0; j < fb->label_count; ++j){
    printf("Label %2d: [%5ld] (%2u) ", j, fb->labels[j].fpos, fb->labels[j].length);
    fwrite(fb->labels[j].text, sizeof(char), fb->labels[j].length, stdout);
    printf("\n");
  }

  /*
  if(fb->buf != NULL){
    printf("Contents of buffer:\n");
    fwrite(fb->buf, sizeof(char), fb->fsize, stdout);
    printf("\n");
  } else {
    printf("Buffer not used\n");
  }
  */

  // Fileblock closed in main, but operation should be idempotent
  if(close_fileblock(fb)){
    printf("Error closing fileblock\n");
  } else {
    printf("Closed fileblock for: %s\n", fb->fname);
  }
}


/* Read user input from stdin and pull a number out.
 * Will provide a brief initial prompt and will retry until success.
 * Supports inputs of up to 9 digits.
 *
 * The min and max values are treated as inclusive.
 *
 * Returns 1 less than the given minimum on error. If you set min to -INT_MAX,
 * well, just be aware.
 */
int get_user_number(int min, int max, int * fail_count){
  int input;
  unsigned int fails = 0;

  printf("Enter a choice (%d - %d): ", min, max);

  do {
    if(fails) printf("Invalid input, please try again: ");

    char buf[10];

    clearerr(stdin);
    if(fgets(buf, 10, stdin) == NULL){
      if(ferror(stdin)) perror("Error reading input");
      return min - 1;
    }

    if(buf[strlen(buf) - 1] != '\n'){
      int c;
      do c = fgetc(stdin);
      while(c != EOF && c != '\n');
    }

    // Check for success
    if(sscanf(buf, "%d", &input) == 1
    && input >= min
    && input <= max
    ) break;

    ++fails;
  } while(1);

  if(fail_count != NULL) *fail_count = fails;
  return input;
}

