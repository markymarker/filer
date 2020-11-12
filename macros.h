
#define DEBUG 1

#if DEBUG > 0
  #define DEBUGPRINT(t) printf("DEBUG: %s\n", t);
  #define DEBUGPRINTC(t, c) printf("DEBUG: %s: %c\n", t, c);
  #define DEBUGPRINTD(t, d) printf("DEBUG: %s: %d\n", t, d);
#else
  #define DEBUGPRINT(t) ;
  #define DEBUGPRINTC(t, c) ;
  #define DEBUGPRINTD(t, d) ;
#endif

#if DEBUG > 1
  #define DEBUGPRINT_V(t) printf("DEBUG: %s\n", t);
  #define DEBUGPRINTC_V(t, c) printf("DEBUG: %s: %c\n", t, c);
  #define DEBUGPRINTD_V(t, d) printf("DEBUG: %s: %d\n", t, d);
#else
  #define DEBUGPRINT_V(t) ;
  #define DEBUGPRINTC_V(t, c) ;
  #define DEBUGPRINTD_V(t, c) ;
#endif

