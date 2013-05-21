//
// rm_rid.h
//
//   The Record Id interface
//

#ifndef RM_RID_H
#define RM_RID_H

// We separate the interface of RID from the rest of RM because some
// components will require the use of RID but not the rest of RM.

#include "redbase.h"
#include "rm_rid.fwd.h"

//
// RID: Record id interface
//
class RID {
public:
    RID();                                         // Default constructor
    RID(PageNum pageNum, SlotNum slotNum);
    ~RID();                                        // Destructor
    RID& operator=(const RID &rid);                // Overloaded =
    bool operator==(const RID &rid) const ;        // Overloaded ==

    RC GetPageNum(PageNum &pageNum) const;         // Return page number
    RC GetSlotNum(SlotNum &slotNum) const;         // Return slot number

private:
    // Copy constructor
    RID(const RID &rid);

    PageNum pageNum;
    SlotNum slotNum;
};

#endif
