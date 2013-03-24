                        /*** /

This file is part of Golly, a Game of Life Simulator.
Copyright (C) 2013 Andrew Trevorrow and Tomas Rokicki.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

 Web site:  http://sourceforge.net/projects/golly
 Authors:   rokicki@gmail.com  andrew@trevorrow.com

                        / ***/
#ifndef READPATTERN_H
#define READPATTERN_H
#include "bigint.h"
class lifealgo ;

/*
 *   Read pattern file into given life algorithm implementation.
 */
const char *readpattern(const char *filename, lifealgo &imp) ;

/*
 *   Get next line from current pattern file.
 */
char *getline(char *line, int maxlinelen) ;

/*
 *   Similar to readpattern but we return the pattern edges
 *   (not necessarily the minimal bounding box; eg. if an
 *   RLE pattern is empty or has empty borders).
 */
const char *readclipboard(const char *filename, lifealgo &imp,
                          bigint *t, bigint *l, bigint *b, bigint *r) ;

/*
 *   Extract comments from pattern file and store in given buffer.
 *   It is the caller's job to free commptr when done (if not NULL).
 */
const char *readcomments(const char *filename, char **commptr) ;

#endif
