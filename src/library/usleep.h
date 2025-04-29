//-*-C++-*-
// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// usleep.h - Some files did not want to import usleep from newlib, so we wrap the native cell as a fix for where calling it isn't working

#ifndef rsxgl_usleep_H
#define rsxgl_usleep_H

#include <sys/systime.h>

static inline void usleep(u32 usec) {
    sysUsleep(usec);
}

#endif
