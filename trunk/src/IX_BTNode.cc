#include "IX_BTNode.h"
#include "pf.h"
#include "rm_rid.h"
#include <cmath>
#include <iostream>
//IX_BTNode

IX_BTNode::IX_BTNode(AttrType iAttrType, int iAttrLength, PF_PageHandle& xPageHandle, bool bNewPage)
: pData(NULL)
, attributeType(iAttrType)
, attributeLength(iAttrLength)
, order(0)
, keysNum(0)
, left(0)
, right(0) {
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
       keysNum = 0; //needs init value >=0
      GetKeysNumFromPage();
      GetLeftFromPage();
      GetRightFromPage();
    } else {
        keysNum = 0;
        PageNum invalid = -1;
        SetLeft(invalid);
        SetRight(invalid);
        SetKeysNumToPage(0);
    }

err_return:
    return;
}

PageNum IX_BTNode::GetLeft() {
    return left;
}
PageNum IX_BTNode::GetLeftFromPage(){
  void * loc = (char*)rids + sizeof(RID)*order + sizeof(int);
  return *((PageNum*) loc);
    
}

void IX_BTNode::SetLeft(PageNum aPageId) {
    left = aPageId;
    SetLeftToPage(left);
}
void IX_BTNode::SetLeftToPage(PageNum p){
     memcpy((char*)rids + sizeof(RID)*order + sizeof(int),
         &p,
         sizeof(PageNum));
    
}

PageNum IX_BTNode::GetRight() {
    return right;
}
PageNum IX_BTNode::GetRightFromPage(){
    
   void * loc = (char*)rids + sizeof(RID)*order + sizeof(int) + sizeof(PageNum);
  return *((PageNum*) loc);
}
void IX_BTNode::SetRight(PageNum aPageId) {
    right = aPageId;
    SetRightToPage(right);

}
void IX_BTNode::SetRightToPage(PageNum p){
memcpy((char*)rids + sizeof(RID)*order + sizeof(int)  + sizeof(PageNum),
         &p,
         sizeof(PageNum));
}
void IX_BTNode::SetKeysNumToPage(int aNumKeys){
    memcpy((char*)rids + sizeof(RID)*order,
         &aNumKeys,
         sizeof(int));
  keysNum = aNumKeys; // conv variable
}

int IX_BTNode::GetKeysNumFromPage(){
  void * loc = (char*)rids + sizeof(RID)*order;
  int * pi = (int *) loc;
  keysNum = *pi;
  return keysNum;
}

IX_BTNode::~IX_BTNode() {
}

RC IX_BTNode::GetData(char *& aData) const {
    aData = pData; //not sure.. it is not safe? 
    return OK_RC;
}

RC IX_BTNode::InsertNode(const void* aKey, const RID & aRid)
{
   
   if (keysNum >= order) return -1; //need to add the state code
    int i = -1;
  void *prevKey = NULL;
  void *currKey = NULL;
  for(i = keysNum-1; i >= 0; i--)
  {
    prevKey = currKey;
    GetKey(i, currKey);    
    if (CompareKey(aKey, currKey) >= 0)
      break; // go out and insert at i
    rids[i+1] = rids[i];
    SetKey(i + 1, currKey);
  }

  rids[i+1] = aRid;
  //SetKey(i+1, aKey);
   memcpy(keys + attributeLength*(i+1),
              aKey,
              attributeLength);
   SetKeysNumToPage(GetKeysNumFromPage()+1);
  return 0;
}

RC IX_BTNode::DeleteNode(const void* aKey,int kpos)
{
   {
  int pos = -1;
  if (kpos != -1) {
    if (kpos < 0 || kpos >= keysNum)
      return -2;
    pos = kpos;
  }
  else {
    pos = FindKey(aKey);
    if (pos < 0)
      return -2;
    // shift all keys after this pos
  }
  for(int i = pos; i < keysNum-1; i++)
  {
    void *p;
    GetKey(i+1, p);
    SetKey(i, p);
    rids[i] = rids[i+1];
  }
  SetKeysNumToPage(GetKeysNumFromPage()-1);
  if(keysNum == 0) return -1;
  return OK_RC;
}
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
      RC rc = aNode->DeleteNode(keytodel,-1);
      if (rc == -1) return rc;
   }
   PageNum page;
   page =  this->GetNodeRID().Page();
   if (page == aNode->GetLeft()) {
      SetRight(aNode->GetRight());
   } else
      SetLeft(aNode->GetLeft());
   
   return OK_RC;
}

RC IX_BTNode::SplitNode(IX_BTNode * aNewNode) {
    if (keysNum > order) return -1; //no need to split;
    // else move a half to the new node 
    int movePos = (keysNum + 1) / 2;
    int moveCount = (keysNum - movePos);
    if (aNewNode->GetKeysNum() + moveCount > aNewNode->GetOrder()) return -1; //fails
    for (int i = movePos; i < keysNum; i++) {
        RID rid = *(rids + i);
        void * k;
        GetKey(i, k);
        RC rc = aNewNode->InsertNode(k, rid);
        if (rc != 0) return rc;
    }
    for (int i = 0; i < moveCount; i++) {
        void * keytodel;
        GetKey(GetKeysNum()-1, keytodel);
        RC rc = DeleteNode(keytodel,-1);
        if (rc == -1) return rc;
    }

    aNewNode->SetRight(this->GetRight());
    this->SetRight(aNewNode->GetNodeRID().Page());
    aNewNode->SetLeft(this->GetNodeRID().Page());

    return OK_RC;
}

int IX_BTNode::GetKeysNum() {
    return keysNum;
}

int IX_BTNode::CompareKey(const void* aKey, const void* bKey) const {
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

RC IX_BTNode::GetKey(int iPos, void*& aKey) const
{
   if (iPos >= 0 && iPos < keysNum) {
      aKey = keys + attributeLength*iPos;
      return OK_RC;
   } else {
      return -1;
   }
}

RC IX_BTNode::SetKey(int iPos, const void* aKey)
{
   if (iPos >= 0 && iPos < order) {
      memcpy(keys + attributeLength*iPos,
              aKey,
              attributeLength);
        return OK_RC;
    } else {
        return -1;
    }
}

int IX_BTNode::FindKey(const void*& aKey) const {
    for (int i = 0; i < keysNum; i++) {
        void * ktemp;
        if (GetKey(i, ktemp)) return -1;
        if (CompareKey(aKey, ktemp) == 0) return i;
    }
    return -1;
}
int IX_BTNode::FindKeyExact(const void* &aKey, const RID& aRid){
    
 for(int i = keysNum-1; i >= 0; i--)
  {
    void* k;
    if(GetKey(i, k) != 0)
      return -1;
    if (CompareKey(aKey, k) == 0) {
      if(aRid == RID(-1,-1))
        return i;
      else { // match RID as well
        if (rids[i] == aRid)
          return i;
      }
    }
  }
  return -1;
}
RID IX_BTNode::FindAddrAtPosition(const void* &key) const
{ 

  int pos = FindKey(key);

  if (pos == -1 || pos >= keysNum)
  {
      return RID(-1,-1);
  }
  return rids[pos];
}
RID IX_BTNode::FindAddr(const void* &key) const
{
  int pos = FindKey(key);
  if (pos == -1) return RID(-1,-1);
  return rids[pos];
}
RID IX_BTNode::GetAddr(const int pos) const
{
  if(pos < 0 || pos > keysNum)
    return RID(-1, -1);
  return rids[pos];
}
int IX_BTNode::FindKeyPosition(const void* &key) const
{
  for(int i = keysNum-1; i >=0; i--)
  {
    void* k;
    if(GetKey(i, k) != 0)
      return -1;
// added == condition so that FindLeaf can return exact match and not
// the position to the right upon matches. this affects where inserts
// will happen during dups.
    if (CompareKey(key, k) == 0)
      return i;
    if (CompareKey(key, k) > 0)
      return i+1;
   }
  return 0; // key is smaller than anything currently
}

RID IX_BTNode::GetNodeRID() {
    return nodeRid;
}

RID IX_BTNode::GetRid(const int iPos) {
    if (iPos < 0 || iPos > keysNum) {
        PageNum noPage = -1;
        SlotNum noSlot = -1;
        return RID(noPage, noSlot);
    } else return *(rids + iPos);
}

int IX_BTNode::GetOrder() {
    return order;
}

int IX_BTNode::GetAttrLength(){
    return attributeLength;
}
AttrType IX_BTNode::GetType() {
    return attributeType;
}

std::ostream& operator<<(std::ostream &os,IX_BTNode &a){
    os << a.GetLeft() << "<-"
       << a.GetNodeRID().Page()  << "{";
  for (int pos = 0; pos <a.GetKeysNum(); pos++) {
    void * k = NULL; a.GetKey(pos, k);
    os << "(";
    if(a.GetType()== INT )
      os << *((int*)k);
    if(a.GetType() == FLOAT )
      os << *((float*)k);
    if(a.GetType() == STRING ) {
      for(int i=0; i < a.GetAttrLength(); i++)
        os << ((char*)k)[i];
    }
    PageNum P;
    P = a.GetRid(pos).Page();
    SlotNum S;
    S = a.GetRid(pos).Slot();
    os << "," <<P<<" "<<S<< "), ";
    
  }
    os <<"}"<< "->"<< a.GetRight();
    return os;
}
void* IX_BTNode::LargestKey() const
{
  void * key = NULL;
  if (keysNum > 0) {
    GetKey(keysNum-1, key);
    return key;
  } else {
    return NULL;
  }
}
// copy key at location pos to the pointer provided
// must be already allocated
int IX_BTNode::CopyKey(int pos, void* toKey) const
{
  if(toKey == NULL)
    return -1;
  if (pos >= 0 && pos < order)
    {
      memcpy(toKey,
             keys + attributeLength*pos,
             attributeLength);
      return 0;
    }
  else
    {
      return -1;
    }
}
int IX_BTNode::Destroy()
{
  if(keysNum != 0)
    return -1;

  keys = NULL;
  rids = NULL;
  return 0;
}