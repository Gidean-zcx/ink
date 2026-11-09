#ifndef INK_SYS_SEARCH_H
#define INK_SYS_SEARCH_H
int ink_search_init(void);
/* UI task; queries use the same search.query capability as AI callers. */
void ink_search_open(const char *query);
#endif
