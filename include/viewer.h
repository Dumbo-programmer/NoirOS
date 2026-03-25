#ifndef VIEWER_H
#define VIEWER_H
#include "common.h"
#include "fs.h"

/* Render a file into a viewer region. `x,y,w,h` are window coords in text cells.
 * `f` may be NULL to show an empty message. `scroll` indicates how many lines to skip.
 */
void viewer_draw(int x, int y, int w, int h, struct File* f, int scroll);

#endif
