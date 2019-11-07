#ifndef TYPES_H
#define TYPES_H

enum UserDirection
{
  STOP = 0,    // must be falsy
  FORWARD = 1, // must be positive
  REVERSE = 2, // must be positive
  INVALID = -1 // must be falsy
};

#endif