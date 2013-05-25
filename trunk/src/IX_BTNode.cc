#include "IX_BTNode.h"
#include "pf.h"
#include "rm_rid.h"
#include <cmath>
//IX_BTNode

IX_BTNode::IX_BTNode(AttrType iAttrType, int iAttrLength, PF_PageHandle& xPageHandle, bool bNewPage)
: pData(NULL)
, attributeType(iAttrType)
, attributeLength(iAttrLength)
, order(0)
, keysNum(0)
, left(0)
, right(0)
{
   int pageSize = PF_PAGE_SIZE;
   order = floor(
           (pageSize + sizeof (int) + 2 * sizeof (PageNum)) /
           (sizeof (RID) + attributeLength));
   // n + 1 pointers + n keys + 1 keyspace used for numKeys
   while (((order) * (attributeLength + sizeof (RID)))
           > ((unsigned) pageSize - sizeof (int) - 2 * sizeof (PageNum)))
      order--;
   char * pData = NULL;
   if (xPageHandle.GetData(pData)) {
      goto err_return;
   }
   PageNum aPageId;
   xPageHandle.GetPageNum(aPageId);
   nodeRid = RID(aPageId, -1); //in the internal node we do not use the slot id
   keys = pData;
   rids = (RID*) (pData + attributeLength * order);
   if (!bNewPage) {
      //TODO:we don't treat it now
   } else {
      keysNum = 0;
      PageNum invalid = -1;
      SetLeft(invalid);
      SetRight(invalid);
   }

err_return:
   return;
}

PageNum IX_BTNode::GetLeft()
{
   return left;
}

void IX_BTNode::SetLeft(PageNum aPageId)
{
   left = aPageId;
}

PageNum IX_BTNode::GetRight()
{
   return right;
}

void IX_BTNode::SetRight(PageNum aPageId)
{
   right = aPageId;

}

IX_BTNode::~IX_BTNode()
{
}

RC IX_BTNode::GetData(char *& aData) const
{
   aData = pData; //not sure.. it is not safe? 
   return OK_RC;
}

RC IX_BTNode::InsertNode(const void* aKey, const RID & aRid)
{
   if (keysNum >= order) return -1; //need to add the state code
   void *currKey;
   int i = 0;
   for (i = keysNum - 1; i >= 0; --i) {
      GetKey(i, currKey);
      int compareResult = CompareKey(aKey, currKey);
      if (compareResult >= 0) {
         break;
      } 
      *(rids + i + 1) = *(rids + i);
      SetKey(i + 1, currKey);
   }
   keysNum++;
   *(rids + i + 1) = aRid;
   SetKey(i + 1, aKey);
   return OK_RC;
}

RC IX_BTNode::DeleteNode(const void* aKey)
{
   int pos = FindKey(aKey);
   if (pos < 0) return -1; //failed not exist
   else for (int i = pos; i < keysNum - 1; i++) {
         void * p;
         GetKey(i + 1, p);
         SetKey(i, p);
         *(rids + i) = *(rids + i + 1);
         keysNum--;
         if (keysNum == 0) return -2; //null node code
      }
   return OK_RC;
}

RC IX_BTNode::MergeNode(IX_BTNode * aNode)
{
   if (keysNum + aNode->GetKeysNum() > order) return -1; // overflow of the node
   for (int i = 0; i < aNode->GetKeysNum(); i++) {
      void *k;
      aNode->GetKey(i, k);
      RID rid = aNode->GetRid(i);
      RC rc = InsertNode(k, rid);
      if (rc) {
         return rc;
      }
   }
   int moveCount = aNode->GetKeysNum();
   for (int i = 0; i < moveCount; i++) {
      void * keytodel;
      aNode->GetKey(0, keytodel);
      RC rc = aNode->DeleteNode(keytodel);
      if (rc == -1) return rc;
   }
   PageNum page;
   this->GetNodeRID().GetPageNum(page);
   if (page == aNode->GetLeft()) {
      SetRight(aNode->GetRight());
   } else
      SetLeft(aNode->GetLeft());
   return OK_RC;
}

RC IX_BTNode::SplitNode(IX_BTNode * aNewNode)
{
   if (keysNum < order) return -1; //no need to split;
   // else move a half to the new node 
   int movePos = (keysNum + 1) / 2;
   int moveCount = (keysNum - movePos);
   if (aNewNode->GetKeysNum() + moveCount > aNewNode->GetOrder()) return -1; //failds
   for (int i = movePos; i < keysNum; i++) {
      RID rid = *(rids + i);
      void * k;
      GetKey(i, k);
      RC rc = aNewNode->InsertNode(k, rid);
      if (rc != 0) return rc;
   }
   for (int i = 0; i < moveCount; i++) {
      void * keytodel;
      aNewNode->GetKey(0, keytodel);
      RC rc = aNewNode->DeleteNode(keytodel);
      if (rc == -1) return rc;
   }
   PageNum pageofnewnode;
   aNewNode->GetNodeRID().GetPageNum(pageofnewnode);
   PageNum pageofthis;
   this->GetNodeRID().GetPageNum(pageofthis);
   aNewNode->SetRight(this->GetRight());
   this->SetRight(pageofnewnode);
   aNewNode->SetLeft(pageofthis);

   return OK_RC;
}

int IX_BTNode::GetKeysNum()
{
   return keysNum;
}

int IX_BTNode::CompareKey(const void* aKey, const void* bKey)
{
   switch (attributeType) {
   case STRING:
      return memcmp(aKey, bKey, attributeLength);
   case FLOAT:
      if (*(float*) aKey > *(float*) bKey) return 1;
      if (*(float*) aKey == *(float*) bKey) return 0;
      if (*(float*) aKey < *(float*) bKey) return -1;
   case INT:
      if (*(int*) aKey > *(int*) bKey) return 1;
      if (*(int*) aKey == *(int*) bKey) return 0;
      if (*(int*) aKey < *(int*) bKey) return -1;
   }
   return 0;
}

RC IX_BTNode::GetKey(int iPos, void*& aKey)
{
   if (iPos >= 0 && iPos < keysNum) {
      aKey = keys + iPos * attributeLength*iPos;
      return OK_RC;
   } else {
      return -1;
   }
}

RC IX_BTNode::SetKey(int iPos, const void* aKey)
{
   if (iPos >= 0 && iPos < this->order) {
      memcpy(keys + attributeLength*iPos,
              aKey,
              attributeLength);

      return OK_RC;
   } else {
      return -1;
   }
}

int IX_BTNode::FindKey(const void*& aKey)
{
   for (int i = 0; i < keysNum; i++) {
      void * ktemp;
      if (GetKey(i, ktemp)) return -1;
      if (CompareKey(aKey, ktemp) == 0) return i;
   }
   return -1;
}

RID IX_BTNode::GetNodeRID()
{
   return nodeRid;
}

RID IX_BTNode::GetRid(const int iPos)
{
   if (iPos < 0 || iPos > keysNum) {
      PageNum noPage = -1;
      SlotNum noSlot = -1;
      return RID(noPage, noSlot);
   } else return *(rids + iPos);
}

int IX_BTNode::GetOrder()
{
   return order;
}


