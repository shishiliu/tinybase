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

#define IX_SIZETOOBIG      (START_IX_ERR - 0)  // key size too big
#define IX_PF              (START_IX_ERR - 1)  // error in PF operations
#define IX_BADIXPAGE       (START_IX_ERR - 2)  // bad page
#define IX_FCREATEFAIL     (START_IX_ERR - 3)  //
#define IX_HANDLEOPEN      (START_IX_ERR - 4)  // error handle open
#define IX_BADOPEN         (START_IX_ERR - 5)  // the index is alread open
#define IX_FNOTOPEN        (START_IX_ERR - 6)  //
#define IX_BADRID          (START_IX_ERR - 7)  // bad rid
#define IX_BADKEY          (START_IX_ERR - 8)  // bad key
#define IX_NOSUCHENTRY     (START_IX_ERR - 9)  //
#define IX_KEYNOTFOUND     (START_IX_ERR - 10) // key is not found
#define IX_INVALIDSIZE     (START_IX_ERR - 11) // btnode's size(order) is invalid


static char *IX_ErrorMsg[] = {
  (char*)"key size too big",
  (char*)"PF operations error",
  (char*)"bad index page",
  (char*)"undefined:IX_FCREATEFAIL",
  (char*)"error in handle open",
  (char*)"the index is already open",
  (char*)"undefined:IX_FNOTOPEN",
  (char*)"bad rid",
  (char*)"bad key",
  (char*)"no such entry",
  (char*)"key not found",
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
