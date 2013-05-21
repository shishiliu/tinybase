#include "IX_BTNode.h"
#include "pf.h"
#include "rm_rid.h"
//IX_BTNode
IX_BTNode::IX_BTNode(AttrType iAttrType, PF_PageHandle& xPageHandle, bool bNewPage, int iPageSize)
{
}

IX_BTNode::~IX_BTNode()
{
}

RC IX_BTNode::GetData(char *&) const
{
   return OK_RC;
}

RC IX_BTNode::GetRid(RID &) const
{
   return OK_RC;
}

RC IX_BTNode::InsertNode()
{
   return OK_RC;
}
    
RC IX_BTNode::DeleteNode()
{
   return OK_RC;
}
    
RC IX_BTNode::MergeNode()
{
   return OK_RC;
}
    
RC IX_BTNode::SplitNode()
{
   return OK_RC;
}

