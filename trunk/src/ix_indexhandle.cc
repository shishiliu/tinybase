//
// File:        ix_indexhandle.cc
// Description: IX_IndexHandle class implementation
// Author:      LIU Shishi (shishi.liu@enst.fr)
//

#include "ix_internal.h"
#include <stdio.h>
#include <iostream>
IX_IndexHandle::IX_IndexHandle()
{
   // Initialize member variables
   bHdrChanged = FALSE;
   memset(&fileHdr, 0, sizeof(fileHdr));
   fileHdr.firstFreePage = IX_PAGE_LIST_END;

   fileHdr.height = 0;
   pfFileHandle = NULL;
   bFileOpen = false;
   root = NULL;
   path = NULL;
   pathP = NULL;
   treeLargest = NULL;

}

IX_IndexHandle::~IX_IndexHandle()
{
    if(pfFileHandle!=NULL)
    {
        delete pfFileHandle;
        pfFileHandle = NULL;
    }
}

RC IX_IndexHandle::Open(PF_FileHandle &fileHandle)
{
    if(bFileOpen || (pfFileHandle) != NULL)
    {
        return IX_HANDLEOPEN;
    }

    RC rc;
    bFileOpen = true;
    pfFileHandle = new PF_FileHandle;
    *pfFileHandle = fileHandle;
    bool newPage = true;
    //if no root,allocates a root page
    //index header will be modified
    if(fileHdr.firstFreePage == IX_PAGE_LIST_END)
    {
        PageNum pageNum;
        //RC rc = GetNewPage(pageNum);

        if (rc = GetNewPage(pageNum))
            return rc;
        fileHdr.firstFreePage = pageNum;
        SetHeight(1);
    }
    //if root already exists
    else
    {
        newPage = false;
        SetHeight(fileHdr.height); // do all other init
    }

    PF_PageHandle rootPageHandle;

    // pin root page - should always be valid
    if (rc = pfFileHandle->GetThisPage(fileHdr.firstFreePage, rootPageHandle))
        return rc;

    root = new IX_BTNode(fileHdr.attrType, fileHdr.attrLength,rootPageHandle, newPage);

    path[0]=root;

    fileHdr.order = root->GetOrder();

    RC invalid = IsValid(); if(invalid) return invalid;

    treeLargest = (void*) new char[fileHdr.attrLength];

    if(!newPage) {
      IX_BTNode * node = FindLargestLeaf();
      // set treeLargest
      if(node->GetKeysNum() > 0)
        {
            node->CopyKey(node->GetKeysNum()-1, treeLargest);
        }
    }
    bHdrChanged = true;
    return 0;
}

RC IX_IndexHandle::Close()
{
    bFileOpen = false;
    if(pathP != NULL) 
    {
      delete [] pathP;
      pathP = NULL;
    }
    if(path != NULL) 
    {
      // path[0] is root
      for (int i = 1; i < fileHdr.height; i++) 
        if(path[i] != NULL) 
        {
          if(pfFileHandle != NULL)
            pfFileHandle->UnpinPage(path[i]->GetNodeRID().Page());
            // delete path[i]; - better leak than crash
        }
      delete [] path;
      path = NULL;
    }
    if(treeLargest!=NULL)
    {
        //delete treeLargest;
        delete [] (char*) treeLargest;
        treeLargest = NULL;
    }
    if(root!=NULL)
    {
        delete root;
        root = NULL;
    }
    if((pfFileHandle) != NULL) {
      // unpin old root page
      pfFileHandle->UnpinPage(fileHdr.firstFreePage);
    }
    return 0;
}

//=================================================================================
RC IX_IndexHandle::GetNewPage(PageNum& pageNum)
{
  RC invalid = IsValid(); if(invalid) return invalid;
  PF_PageHandle ph;
  RC rc;
  if ((rc = pfFileHandle->AllocatePage(ph)))
  {
    return(rc);
  }
  if((rc = ph.GetPageNum(pageNum)))
  {
    return(rc);
  }

  // the default behavior of the buffer pool is to pin pages
  // let us make sure that we unpin explicitly after setting
  // things up
  if (
    (rc = pfFileHandle->MarkDirty(pageNum)) ||
    (rc = pfFileHandle->UnpinPage(pageNum)))
    return rc;
  // std::cerr << "GetNewPage called to get page " << pageNum << std::endl;
  fileHdr.numPages++;
  assert(fileHdr.numPages > 1); // page 0 is this page in worst case
  bHdrChanged = true;
  return 0; // pageNum is set correctly
}
//=================================================================================

//
// Desc: Access the page number.  The page handle object must refer to
//       a pinned page.
// Out:  populates the path member variable with the path
// Ret:  NULL if there is no root
//       otherwise return a pointer to the leaf node that is rightmost AND
//       largest in value

// Users will call - RC invalid = IsValid(); if(invalid) return invalid;
RC IX_IndexHandle::IsValid () const
{
  bool ret = true;
  ret = ret && ((pfFileHandle) != NULL);
  if(fileHdr.height > 0) {
    ret = ret && (fileHdr.firstFreePage > 0); // 0 is for file header
    ret = ret && (fileHdr.numPages >= fileHdr.height + 1);
    ret = ret && (root != NULL);
    ret = ret && (path != NULL);
  }
  return ret ? 0 : IX_BADIXPAGE;
}
IX_BTNode* IX_IndexHandle::FindLargestLeaf()
{
  RC invalid = IsValid(); if(invalid) return NULL;
  if (root == NULL) return NULL;

  if(fileHdr.height == 1) {
    path[0] = root;
    return root;
  }
  //path[0] is the root
  for (int i = 1; i < fileHdr.height; i++)
  {
    //get the mostright (key,rid), which is the largest key in the node
    RID r = path[i-1]->GetAddr(path[i-1]->GetKeysNum() - 1);
    if(r.Page() == -1) {
      // no such position or other error
      // no entries in node ?
      assert("should not run into empty node");
      return NULL;
    }
    // start with a fresh path
    if(path[i] != NULL) {
      RC rc = pfFileHandle->UnpinPage(path[i]->GetNodeRID().Page());
      if (rc < 0) return NULL;
      delete path[i];
      path[i] = NULL;
    }
    //put the largest key node into the path
    path[i] = FetchNode(r);
    PF_PageHandle dummy;
    // pin path pages
    RC rc = pfFileHandle->GetThisPage(path[i]->GetNodeRID().Page(), dummy);
    if (rc != 0) return NULL;
    pathP[i-1] = 0; // dummy
  }
  return path[fileHdr.height-1];
}

//
// Desc: Get the BTNode at the PageNumber specified within Btree
//
// Ret:  NULL if the PageNumber < 0
//       otherwise return a pointer to the a BTNode
IX_BTNode* IX_IndexHandle::FetchNode(PageNum p) const
{
  return FetchNode(RID(p,(SlotNum)-1));
}

//
// Desc: Get the BTNode at the RID specified within Btree
//
// Ret:  NULL if the PageNumber < 0
//       otherwise return a pointer to the a BTNode
IX_BTNode* IX_IndexHandle::FetchNode(RID r) const
{
  RC invalid = IsValid(); if(invalid) return NULL;
  if(r.Page() < 0) return NULL;
  PF_PageHandle ph;
  RC rc = GetThisPage(r.Page(), ph);
  if(rc != 0) {
    IX_PrintError(rc);
  }
  if(rc!=0) return NULL;

  return new IX_BTNode(fileHdr.attrType, fileHdr.attrLength,
                       ph, false);
}

//
// Desc: Unpinning version that will unpin after every call correctly
//
RC IX_IndexHandle::GetThisPage(PageNum p, PF_PageHandle& ph) const {
  RC rc = pfFileHandle->GetThisPage(p, ph);
  if (rc != 0) return rc;
  // Needs to be called everytime GetThisPage is called.
  rc = pfFileHandle->MarkDirty(p);
  if(rc!=0) return rc;

  rc = pfFileHandle->UnpinPage(p);
  if (rc != 0) return rc;
  return 0;
}

//
// InsertEntry
//
// Desc: Insert a new entry
// In:   pData -
// Out:  rid -
// Ret:  RM return code
//
RC IX_IndexHandle::InsertEntry(void *pData, const RID &rid)
{
    RC invalid = IsValid(); if(invalid) return invalid;

    if(pData == NULL) return IX_BADKEY;

    bool newLargest = false;
    void * prevKey = NULL;
    int level = fileHdr.height - 1;

    IX_BTNode* node = FindLeaf(pData);
    IX_BTNode* newNode = NULL;
    assert(node != NULL);

    int pos2 = node->FindKeyExact((const void*&)pData, rid);
    //same data + same rid
    if((pos2 != -1))
      return IX_ENTRYEXISTS;
    //perhaps same data + different rid

    // largest key in tree is in every intermediate root from root onwards
    if (node->GetKeysNum() == 0 ||node->CompareKey(pData, treeLargest) > 0)
    {
      newLargest = true;
      prevKey = treeLargest;
    }

    if(newLargest)
    {
      for(int i=0; i < fileHdr.height-1; i++)
      {
        int pos = path[i]->FindKey((const void *&)prevKey);
        if(pos != -1)
        {
            path[i]->SetKey(pos, pData);
        }
        else
        {
          // assert(false); //largest key should be everywhere
          // return IX_BADKEY;
          std::cerr << "Fishy intermediate node ?" << std::endl;
        }
      }
      // copy from pData into new treeLargest
      memcpy(treeLargest, pData, fileHdr.attrLength);
      //std::cerr << "new treeLargest " << *(int*)treeLargest << std::endl;
    }

    int result = node->InsertNode(pData, rid);
    // no room in node - deal with overflow - non-root
    void * failedKey = pData;
    RID failedRid = rid;

    while(result == -1)
    {
      //std::cerr << "non root overflow" << std::endl;

      char * charPtr = new char[fileHdr.attrLength];
      void * oldLargest = charPtr;

      if(node->LargestKey() == NULL)
        oldLargest = NULL;
      else
        node->CopyKey(node->GetKeysNum()-1, oldLargest);

       //std::cerr << "nro largestKey was " << *(int*)oldLargest  << std::endl;
       //std::cerr << "nro numKeys was " << node->GetNumKeys()  << std::endl;
      delete [] charPtr;

      // make new  node
      PF_PageHandle ph;
      PageNum p;
      RC rc = GetNewPage(p);
      if (rc != 0) return rc;
      rc = GetThisPage(p, ph);
      if (rc != 0) return rc;

      // rc = pfFileHandle->MarkDirty(p);
      // if(rc!=0) return NULL;

      newNode = new IX_BTNode(fileHdr.attrType, fileHdr.attrLength,ph, true);
      // split into new node
      rc = node->SplitNode(newNode);
      if (rc != 0) return IX_PF;
      // split adjustment
      IX_BTNode * currRight = FetchNode(newNode->GetRight());
      if(currRight != NULL) {
        currRight->SetLeft(newNode->GetNodeRID().Page());
        delete currRight;
      }

      IX_BTNode * nodeInsertedInto = NULL;

      // put the new entry into one of the 2 now.
      // In the comparison,
      // > takes care of normal cases
      // = is a hack for dups - this results in affinity for preserving
      // RID ordering for children - more balanced tree when all keys
      // are the same.
      if(node->CompareKey(pData, node->LargestKey()) >= 0) {
        newNode->InsertNode(failedKey, failedRid);
        nodeInsertedInto = newNode;
      }
      else { // <
        node->InsertNode(failedKey, failedRid);
        nodeInsertedInto = node;
      }

      // go up to parent level and repeat
      level--;
      if(level < 0) break; // root !
      // pos at which parent stores a pointer to this node
      int posAtParent = pathP[level];
      // std::cerr << "posAtParent was " << posAtParent << std::endl ;
      // std::cerr << "level was " << level << std::endl ;


      IX_BTNode * parent = path[level];
      // update old key - keep same addr
      //parent->Remove(NULL, posAtParent);
      parent->DeleteNode(NULL,posAtParent);
      result = parent->InsertNode((const void*)node->LargestKey(),
                              node->GetNodeRID());
      // this result should always be good - we removed first before
      // inserting to prevent overflow.

      // insert new key
      result = parent->InsertNode(newNode->LargestKey(), newNode->GetNodeRID());

      // iterate for parent node and split if required
      node = parent;
      failedKey = newNode->LargestKey(); // failure cannot be in node -
                                         // something was removed first.
      failedRid = newNode->GetNodeRID();

      delete newNode;
      newNode = NULL;
    } // while

    if(level >= 0) 
    {
      // insertion done
      return 0;
    } 
    else 
    {
       std::cerr << "root split happened" << std::endl;

      // unpin old root page
      RC rc = pfFileHandle->UnpinPage(fileHdr.firstFreePage);
      if (rc < 0)
        return rc;

      // make new root node
      PF_PageHandle ph;
      PageNum p;
      rc = GetNewPage(p);
      if (rc != 0) return IX_PF;
      rc = GetThisPage(p, ph);
      if (rc != 0) return IX_PF;

      // rc = pfFileHandle->MarkDirty(p);
      // if(rc!=0) return NULL;

      root = new IX_BTNode(fileHdr.attrType, fileHdr.attrLength,
                           ph, true);
      root->InsertNode(node->LargestKey(), node->GetNodeRID());
      root->InsertNode(newNode->LargestKey(), newNode->GetNodeRID());

      // pin root page - should always be valid
      fileHdr.firstFreePage = root->GetNodeRID().Page();
      PF_PageHandle rootph;
      rc = pfFileHandle->GetThisPage(fileHdr.firstFreePage, rootph);
      if (rc != 0) return rc;

      if(newNode != NULL) {
        delete newNode;
        newNode = NULL;
      }

      SetHeight(fileHdr.height+1);
      return 0;
    }
}
// description:
// return NULL if there is no root
// otherwise return a pointer to the leaf node where key might go
// also populates the path[] member variable with the path
// if there are dups (keys) along the path, the rightmost path will be
// chosen
// TODO - add a random parameter to this which will be used during
// inserts - this is prevent pure rightwards growth when inserting a
// dup value continuously.
//
// input:
//          *pData
// return:
//         (1)no root:NULL
//         (2)BTNode*-->pData; populates the path[]membre

IX_BTNode* IX_IndexHandle::FindLeaf(const void *pData)
{
  RC invalid = IsValid(); if(invalid) return NULL;
  if (root == NULL) return NULL;

  if(fileHdr.height == 1)
  {
    path[0] = root;
    return root;
  }

  //height > 1
  for (int i = 1; i < fileHdr.height; i++)
  {
    //std::cerr << "i was " << i << std::endl;
    //std::cerr << "pData was " << *(int*)pData << std::endl;
       
    RID r = path[i-1]->FindAddrAtPosition(pData);// return position if key will fit in a particular position
                                                 // return (-1, -1) if there was an error
                                                 // if there are dups - this will return rightmost position

    int pos = path[i-1]->FindKeyPosition(pData);// return position if key will fit in a particular position
                                                // return -1 if there was an error
                                                // if there are dups - this will return rightmost position


    if(r.Page() == -1)
    {
      // pData is bigger than any other key - return address of node
      // that largest key points to.
      const void * p = path[i-1]->LargestKey();
      // std::cerr << "p was " << *(int*)p << std::endl;
      r = path[i-1]->FindAddr((const void*&)(p));
      // std::cerr << "r was " << r << std::endl;
      pos = path[i-1]->FindKey((const void*&)(p));
    }

    // start with a fresh path
    if(path[i] != NULL) {
      RC rc = pfFileHandle->UnpinPage(path[i]->GetNodeRID().Page());
      if (rc != 0) return NULL;
      delete path[i];
      path[i] = NULL;
    }

    path[i] = FetchNode(r.Page());

    PF_PageHandle dummy;
    // pin path pages

    RC rc = pfFileHandle->GetThisPage(path[i]->GetNodeRID().Page(), dummy);
    if (rc != 0) return NULL;

    pathP[i-1] = pos;
  }
    return path[fileHdr.height-1];
}


// get/set height
int IX_IndexHandle::GetHeight() const
{
  return fileHdr.height;
}
void IX_IndexHandle::SetHeight(const int& h)
{
  for(int i = 1;i < fileHdr.height; i++)
    if (path != NULL && path[i] != NULL) {
      delete path[i];
      path[i] = NULL;
    }
  if(path != NULL) delete [] path;
  if(pathP != NULL) delete [] pathP;

  fileHdr.height = h;

  path = new IX_BTNode* [fileHdr.height];

  for(int i=1;i < fileHdr.height; i++)
    path[i] = NULL;
  path[0] = root;

  pathP = new int [fileHdr.height-1]; // leaves don't point
  for(int i=0;i < fileHdr.height-1; i++)
    pathP[i] = -1;
}

IX_BTNode* IX_IndexHandle::GetRoot() const
{
  return root;
}

void IX_IndexHandle::Print(int level, RID r) const {
  RC invalid = IsValid(); if(invalid) assert(invalid);
  // level -1 signals first call to recursive function - root
  // std::cerr << "Print called with level " << level << endl;
  IX_BTNode * node = NULL;
  if(level == -1) {
    level = fileHdr.height;
    node = FetchNode(root->GetNodeRID());
    std::cout << "Print called with level " << level << std::endl;
  }
  else {
    if(level < 1)
    {
      return;
    }
    else
    {
      node = FetchNode(r);
    }
  }

  std::cout<<*(node)<<std::endl;
  std::cout<<" "<<std::endl;
  std::cout<<" "<<std::endl;
  if (level >= 2) // non leaf
  {
    for(int i = 0; i < node->GetKeysNum(); i++)
    {
      RID newr = node->GetAddr(i);
      Print(level-1, newr);
    }
  }
  // else level == 1 - recursion ends
  if(level == 1 && node->GetRight() == -1)
    std::cout << std::endl; //blank after rightmost leaf
  if(node!=NULL) delete node;
}

void IX_IndexHandle::PrintHeader() const {
 	
    if(bFileOpen)
    {
	std::cerr<<"State:Index is open."<<std::endl; 
    }
    else
    {
	std::cerr<<"State:Index is closed."<<std::endl; 
    }
    std::cerr<<"Index Root Page number:"<<fileHdr.firstFreePage<<std::endl;
    std::cerr<<"Index #Pages:"<<fileHdr.numPages<<std::endl;
    std::cerr<<"Index BTNode order:"<<fileHdr.order<<std::endl;
    std::cerr<<"Index B+ Tree Height"<<fileHdr.height<<std::endl;
}

//==================================================================
// Delete a new index entry
// 0 indicates success
// Implements lazy deletion - underflow is defined as 0 keys for all
// non-root nodes.
RC IX_IndexHandle::DeleteEntry(void *pData, const RID& rid)
{
  if(pData == NULL)
    // bad input to method
    return IX_BADKEY;
  RC invalid = IsValid(); if(invalid) return invalid;

  bool nodeLargest = false;

  IX_BTNode* node = FindLeaf(pData);
  assert(node != NULL);

  int pos = node->FindKeyExact((const void*&)pData, rid);
  if(pos == -1) {

    // make sure that there are no dups (keys) left of this node that might
    // have this rid.
    int p = node->FindKeyExact((const void*&)pData, RID(-1,-1));
    if(p != -1) {
      IX_BTNode * other = DupScanLeftFind(node, pData, rid);
      if(other != NULL) {
        int pos = other->FindKeyExact((const void*&)pData, rid);
        other->DeleteNode(pData,pos); // ignore result - not dealing with
                                   // underflow here
        return 0;
      }
    }

    // key/rid does not exist - error
    return IX_NOSUCHENTRY;
  }

  else if(pos == node->GetKeysNum()-1)
    nodeLargest = true;

  // Handle special case of key being largest and rightmost in
  // node. Means it is in parent and potentially whole path (every
  // intermediate node)
  if (nodeLargest) {
    // cerr << "node largest" << endl;
    // void * leftKey = NULL;
    // node->GetKey(node->GetNumKeys()-2, leftKey);
    // cerr << " left key " << *(int*)leftKey << endl;
    // if(node->CmpKey(pData, leftKey) != 0) {
    // replace this key with leftkey in every intermediate node
    // where it appears
    for(int i = fileHdr.height-2; i >= 0; i--) {
      int pos = path[i]->FindKey((const void *&)pData);
      if(pos != -1) {

        void * leftKey = NULL;
        leftKey = path[i+1]->LargestKey();
        if(node->CompareKey(pData, leftKey) == 0) {
          int pos = path[i+1]->GetKeysNum()-2;
          if(pos < 0) {
            continue; //underflow will happen
          }
          path[i+1]->GetKey(path[i+1]->GetKeysNum()-2, leftKey);
        }

        // if level is lower than leaf-1 then make sure that this is
        // the largest key
        if((i == fileHdr.height-2) || // leaf's parent level
           (pos == path[i]->GetKeysNum()-1) // largest at
           // intermediate node too
          )
          path[i]->SetKey(pos, leftKey);
      }
    }
  }

  int result = node->DeleteNode(pData,pos); // pos is the param that counts

  int level = fileHdr.height - 1; // leaf level
  while (result == -1) {

    // go up to parent level and repeat
    level--;
    if(level < 0) break; // root !

    // pos at which parent stores a pointer to this node
    int posAtParent = pathP[level];
    // cerr << "posAtParent was " << posAtParent << endl ;
    // cerr << "level was " << level << endl ;


    IX_BTNode * parent = path[level];
    result = parent->DeleteNode(NULL,posAtParent);
    // root is considered underflow even it is left with a single key
    if(level == 0 && parent->GetKeysNum() == 1 && result == 0)
      result = -1;

    // adjust L/R pointers because a node is going to be destroyed
    IX_BTNode* left = FetchNode(node->GetLeft());
    IX_BTNode* right = FetchNode(node->GetRight());
    if(left != NULL) {
      if(right != NULL)
        left->SetRight(right->GetNodeRID().Page());
      else
        left->SetRight(-1);
    }
    if(right != NULL) {
      if(left != NULL)
        right->SetLeft(left->GetNodeRID().Page());
      else
        right->SetLeft(-1);
    }
    if(right != NULL)
      delete right;
    if(left != NULL)
      delete left;

    node->Destroy();
    RC rc = DisposePage(node->GetNodeRID().Page());
    if (rc < 0)
      return IX_PF;

    node = parent;
  } // end of while result == -1


  if(level >= 0) {
    // deletion done
    return 0;
  } else {
    // cerr << "root underflow" << endl;

    if(fileHdr.height == 1) { // root is also leaf
      return 0; //leave empty leaf-root around.
    }

    // new root is only child
    root = FetchNode(root->GetAddr(0));
    // pin root page - should always be valid
    fileHdr.firstFreePage = root->GetNodeRID().Page();
    PF_PageHandle rootph;
    RC rc = pfFileHandle->GetThisPage(fileHdr.firstFreePage, rootph);
    if (rc != 0) return rc;

    node->Destroy();
    rc = DisposePage(node->GetNodeRID().Page());
    if (rc < 0)
      return IX_PF;

    SetHeight(fileHdr.height-1); // do all other init
    return 0;
  }
}
// return NULL if key, rid is not found
IX_BTNode* IX_IndexHandle::DupScanLeftFind(IX_BTNode* right, void *pData, const RID& rid)
{

  IX_BTNode* currNode = FetchNode(right->GetLeft());
  int currPos = -1;

  for( IX_BTNode* j = currNode;
       j != NULL;
       j = FetchNode(j->GetLeft()))
  {
    currNode = j;
    int i = currNode->GetKeysNum()-1;

    for (; i >= 0; i--)
    {
      currPos = i; // save Node in object state for later.
      char* key = NULL;
      int ret = currNode->GetKey(i, (void*&)key);
      if(ret == -1)
        break;
      if(currNode->CompareKey(pData, key) < 0)
        return NULL;
      if(currNode->CompareKey(pData, key) > 0)
        return NULL; // should never happen
      if(currNode->CompareKey(pData, key) == 0) {
        if(currNode->GetAddr(currPos) == rid)
          return currNode;
      }
    }
  }
  return NULL;
}
RC IX_IndexHandle::DisposePage(const PageNum& pageNum)
{
  RC invalid = IsValid(); if(invalid) return invalid;

  RC rc;
  if ((rc = pfFileHandle->DisposePage(pageNum)))
    return(rc);

  fileHdr.numPages--;
  assert(fileHdr.numPages > 0); // page 0 is this page in worst case
  bHdrChanged = true;
  return 0; // pageNum is set correctly
}

// Force index files to disk
RC IX_IndexHandle::ForcePages()
{
    return 0;
}
