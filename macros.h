
#define DEBUG 2

#if DEBUG > 0
  #define DEBUGPRINT(t) printf("DEBUG: %s\n", t);
  #define DEBUGPRINTC(t, c) printf("DEBUG: %s: %c\n", t, c);
#else
  #define DEBUGPRINT(t) ;
  #define DEBUGPRINTC(t, c) ;
#endif

#if DEBUG > 1
  #define DEBUGPRINT_V(t) printf("DEBUG: %s\n", t);
  #define DEBUGPRINTC_V(t, c) printf("DEBUG: %s: %c\n", t, c);
#else
  #define DEBUGPRINT_V(t) ;
  #define DEBUGPRINTC_V(t, c) ;
#endif

