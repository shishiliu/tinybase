#include "ix_internal.h"

IX_IndexScan::IX_IndexScan()
{
}

IX_IndexScan::~IX_IndexScan()
{
}
// Open index scan
/*enum CompOp {
    NO_OP,  
    EQ_OP, ==
    NE_OP, !=
    LT_OP, <
    GT_OP, >
    LE_OP, 
    GE_OP    
};
*/
RC IX_IndexScan::OpenScan(const IX_IndexHandle &indexHandle, CompOp compOp, void *value, ClientHint  pinHint)
{
	return 0;
}

// Get the next matching entry return IX_EOF if no more matching
// entries.
RC IX_IndexScan::GetNextEntry(RID &rid)
{
	return 0;
}
// Close index scan
RC IX_IndexScan::CloseScan()
{
	return 0;
}
