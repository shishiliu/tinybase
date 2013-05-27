//
// File:        ix_error.cc
// Description: IX_PrintError function
// Author:      Hyunjung Park (hyunjung@stanford.edu)
//

#include <cerrno>
#include <cstdio>
#include <iostream>
#include "ix_internal.h"

using namespace std;

//
// Error table
//
static char *IX_WarnMsg[] = {
  (char*)"inviable rid",
  (char*)"unread record",
  (char*)"invalid record size",
  (char*)"invalid slot number",
  (char*)"record not found",
  (char*)"invalid comparison operator",
  (char*)"invalid attribute parameters",
  (char*)"null pointer",
  (char*)"scan open",
  (char*)"scan closed",
  (char*)"file closed"
};

static char *IX_ErrorMsg[] = {
  (char*)"key size too big",
  (char*)"PF operations error",
  (char*)"bad page",
  (char*)"undefined:IX_FCREATEFAIL",
  (char*)"error in handle open",
  (char*)"the index is already open",
  (char*)"undefined:IX_FNOTOPEN",
  (char*)"bad rid",
  (char*)"bad key",
  (char*)"btnode's size(order) is invalid"
};
// 
// IX_PrintError
//
// Desc: Send a message corresponding to a IX return code to cerr
// In:   rc - return code for which a message is desired
//
void IX_PrintError(RC rc)
{
  // Check the return code is within proper limits
  if (rc >= START_IX_WARN && rc <= IX_LASTWARN)
    // Print warning
    cerr << "IX warning: " << IX_WarnMsg[rc - START_IX_WARN] << "\n";
  else if (-rc >= -START_IX_ERR && -rc < -IX_LASTERROR)
    // Print error
    cerr << "IX error: " << IX_ErrorMsg[-rc + START_IX_ERR] << "\n";
  else if (rc == 0)
    cerr << "IX_PrintError called with return code of 0\n";
  else
    cerr << "IX error: " << rc << " is out of bounds\n";
}
