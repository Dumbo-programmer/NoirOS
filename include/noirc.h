#ifndef NOIRC_H
#define NOIRC_H

#include "common.h"

struct File;

/* Run a Noir C source file (lightweight interpreter stub). */
void noirc_run(struct File* f);

#endif
