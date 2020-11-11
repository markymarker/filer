/* 2020-11-09
 *
 * This is a state machine to parse a block of text and search for labels.
 * Labels consist of a line of text which has a preceeding line that begins with
 * 5 equals signs (=). The preceeding line is not a part of the label.
 */

#include "macros.h"

#include <stdio.h>

typedef int (* generic_func)(char);
typedef generic_func (* sm_func)(char);


int run_iteration(sm_func *, char, char *);
sm_func entry(char);
sm_func ignore_line(char);
sm_func delim_matched_finish_line(char);
sm_func label_line(char);
sm_func
  equals1(char),
  equals2(char),
  equals3(char),
  equals4(char)
;


#ifndef INCLUDING_SM
int main(int argl, char ** argv){
  char str[] = "abcdefg\n======abc==\nlabel a:\nhijklmnop\n=====\nANOTHER_LABEL:\nqq";
  printf("Running state machine on the following:\n%s\n\n", str);

  char lbuf1[21];
  char lbuf2[21];
  char lbuf3[21];
  char * lbufs[] = {lbuf1, lbuf2, lbuf3};
  //lbuf1[0] = lbuf2[0] = lbuf3[0] = '\0';
  char * c = str;
  sm_func smf = NULL;

  for(int lbuf = 0; lbuf < 3; ++lbuf){
    if(*c == '\0') break;

    int rval;
    int llen = 0;

    while(*c != '\0'){
      char * ls = (llen < 20) ? lbufs[lbuf] + llen : NULL;
      rval = run_iteration(&smf, *c, ls);
      ++c;
      if(rval <= 0) break;
      if(rval == 3) ++llen;
    }

    lbufs[lbuf][llen] = '\0';

    if(rval < 0){
      printf("Error occured in state machine: %d\n", rval);
      break;
    }
  }

  printf("Finished\n");
  printf("Found labels:\n  1: %s\n  2: %s\n  3: %s\n", lbuf1, lbuf2, lbuf3);
}
#endif


/* Run a single iteration of the state machine.
 *
 * This will handle the reassigning and checking of the states, which is held
 * by the caller and provided with the parameter `f`, which should initially be
 * set to NULL.
 *.
 * If a label state is detected, the current character will be stored into the
 * provided `lstore` parameter, if not null.
 *
 * Return values:
 *  <0: Error
 *   0: Finished
 *  >0: Still running:
 *      1: Normal continuing
 *      2: State change entered label
 *      3: Label character stored
 */
int run_iteration(sm_func * f, char c, char * lstore){
  if(*f == NULL) *f = (sm_func)entry;
  int rval = 1;

  sm_func * prev_state = f;

  *f = (sm_func)(*f)(c);

  if(*prev_state != (sm_func)label_line
  && *f == (sm_func)label_line
  ){
    // Transitioned from non label line into label line
    rval = 2;
  } else if(*f == (sm_func)label_line && lstore != NULL){
    // Potentially going to store a character into lstore
    if( c == ' ' || c == '_' || c == '-'
    || (c >= 'a' && c <= 'z')
    || (c >= 'A' && c <= 'Z')
    ){
      // Storing a valid character into lstore
      *lstore = c;
      rval = 3;
    }
  } else if(*f == NULL){
    // Finished operation
    rval = 0;
  }

  return rval;
}


/* Entry state
 * The beginning of a line
 * Looking for an equals sign, failure ignores the line
 */
sm_func entry(char c){
  DEBUGPRINTC_V("In entry state", c)

  switch(c){
  case '=': return (sm_func)equals1;
  default: return ignore_line(c);
  }
}


/* Ignore line state
 * Looking for newline
 */
sm_func ignore_line(char c){
  DEBUGPRINTC_V("In ignore line state", c)

  switch(c){
  case '\n': return (sm_func)entry;
  default: return (sm_func)ignore_line;
  }
}


/* Delimited matched state
 * Looking for newline
 */
sm_func delim_matched_finish_line(char c){
  DEBUGPRINTC_V("In delim matched state", c)

  switch(c){
  case '\n': return (sm_func)label_line;
  default: return (sm_func)delim_matched_finish_line;
  }
}


/* Label line state
 * Looking for newline
 */
sm_func label_line(char c){
  DEBUGPRINTC_V("In label line state", c)

  switch(c){
  //case '\n': return (sm_func)entry;
  case '\n': return NULL;
  default: return (sm_func)label_line;
  }
}


/* Equals 1 state
 * One equals found
 * Looking for an equals sign, failure ignores the line
 */
sm_func equals1(char c){
  DEBUGPRINTC_V("In equals 1", c)

  switch(c){
  case '=': return (sm_func)equals2;
  default: return ignore_line(c);
  }
}


/* Equals 2 state
 * Two equals found
 * Looking for an equals sign, failure ignores the line
 */
sm_func equals2(char c){
  DEBUGPRINTC_V("In equals 2", c)

  switch(c){
  case '=': return (sm_func)equals3;
  default: return ignore_line(c);
  }
}


/* Equals 3 state
 * Three equals found
 * Looking for an equals sign, failure ignores the line
 */
sm_func equals3(char c){
  DEBUGPRINTC_V("In equals 3", c)

  switch(c){
  case '=': return (sm_func)equals4;
  default: return ignore_line(c);
  }
}


/* Equals 4 state
 * Four equals found
 * Looking for an equals sign, failure ignores the line
 */
sm_func equals4(char c){
  DEBUGPRINTC_V("In equals 4", c)

  switch(c){
  case '=': return (sm_func)delim_matched_finish_line;
  default: return ignore_line(c);
  }
}

