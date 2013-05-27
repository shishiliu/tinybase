//
// File:        ix_indexhandle.cc
// Description: IX_IndexHandle class implementation
// Author:      LIU Shishi (shishi.liu@enst.fr)
//

#include "ix_internal.h"
#include <stdio.h>
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
    }
    //if root already exists
    else
    {
        newPage = false;
        printf("hello world============\n\n");
    }

    PF_PageHandle rootPageHandle;

    // pin root page - should always be valid
    if (rc = pfFileHandle->GetThisPage(fileHdr.firstFreePage, rootPageHandle))
        return rc;

    root = new BTNode(fileHdr.attrType, fileHdr.attrLength,rootPageHandle, newPage, PF_PAGE_SIZE);

    fileHdr.height++;
    path = new BTNode* [fileHdr.height];
    path[0]=root;

    fileHdr.order = root->GetMaxKeys();

    RC invalid = IsValid(); if(invalid) return invalid;

    treeLargest = (void*) new char[fileHdr.attrLength];
    if(!newPage) {
      BTNode * node = FindLargestLeaf();
      // set treeLargest
      if(node->GetNumKeys() > 0)
        {//CopyKey(src,des)
            node->CopyKey(node->GetNumKeys()-1, treeLargest);
        }
    }
    bHdrChanged = true;
    return 0;
}

RC IX_IndexHandle::Close()
{
    bFileOpen = false;
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
BTNode* IX_IndexHandle::FindLargestLeaf()
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
    RID r = path[i-1]->GetAddr(path[i-1]->GetNumKeys() - 1);
    if(r.Page() == -1) {
      // no such position or other error
      // no entries in node ?
      assert("should not run into empty node");
      return NULL;
    }
    // start with a fresh path
    if(path[i] != NULL) {
      RC rc = pfFileHandle->UnpinPage(path[i]->GetPageRID().Page());
      if (rc < 0) return NULL;
      delete path[i];
      path[i] = NULL;
    }
    //put the largest key node into the path
    path[i] = FetchNode(r);
    PF_PageHandle dummy;
    // pin path pages
    RC rc = pfFileHandle->GetThisPage(path[i]->GetPageRID().Page(), dummy);
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
BTNode* IX_IndexHandle::FetchNode(PageNum p) const
{
  return FetchNode(RID(p,(SlotNum)-1));
}

//
// Desc: Get the BTNode at the RID specified within Btree
//
// Ret:  NULL if the PageNumber < 0
//       otherwise return a pointer to the a BTNode
BTNode* IX_IndexHandle::FetchNode(RID r) const
{
  RC invalid = IsValid(); if(invalid) return NULL;
  if(r.Page() < 0) return NULL;
  PF_PageHandle ph;
  RC rc = GetThisPage(r.Page(), ph);
  if(rc != 0) {
    IX_PrintError(rc);
  }
  if(rc!=0) return NULL;

  return new BTNode(fileHdr.attrType, fileHdr.attrLength,
                       ph, false,
                       PF_PAGE_SIZE);
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

// Insert a new index entry: need to change the name "pRecordData" to "pEntryData"
/*For this and the following two methods, it is incorrect if the IX_IndexHandle object for which the method is called does not refer to an open index.

//This method should insert a new entry into the index associated with IX_IndexHandle.
//parameter pData points to the attribute value to be inserted into the index,
//parameter rid identifies the record with that value to be added to the index.
//Hence, this method effectively inserts an entry for the pair (*pData,rid) into the index. (The index should contain only the record's RID, not the record itself.) If the indexed attribute is a character string of length n, then you may assume that *pData is exactly n bytes long; similarly for parameter *pData in the next method. This method should return a nonzero code if there is already an entry for (*pData,rid) in the index.
*/
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

    BTNode* node = FindLeaf(pData);
    BTNode* newNode = NULL;
    assert(node != NULL);

    int pos2 = node->FindKey((const void*&)pData, rid);
    //same data + same rid
    if((pos2 != -1))
      return IX_ENTRYEXISTS;
    //perhaps same data + different rid

    // largest key in tree is in every intermediate root from root onwards
    if (node->GetNumKeys() == 0 ||node->CmpKey(pData, treeLargest) > 0)
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
      std::cerr << "new treeLargest " << *(int*)treeLargest << std::endl;
    }

    int result = node->Insert(pData, rid);
    // no room in node - deal with overflow - non-root
    void * failedKey = pData;
    RID failedRid = rid;
    while(result == -1)
    {
      // std::cerr << "non root overflow" << std::endl;

      char * charPtr = new char[fileHdr.attrLength];
      void * oldLargest = charPtr;

      if(node->LargestKey() == NULL)
        oldLargest = NULL;
      else
        node->CopyKey(node->GetNumKeys()-1, oldLargest);

      // std::cerr << "nro largestKey was " << *(int*)oldLargest  << std::endl;
      // std::cerr << "nro numKeys was " << node->GetNumKeys()  << std::endl;
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

      newNode = new BTNode(fileHdr.attrType, fileHdr.attrLength,
                              ph, true,
                              PF_PAGE_SIZE);
      // split into new node
      rc = node->Split(newNode);
      if (rc != 0) return IX_PF;
      // split adjustment
      BTNode * currRight = FetchNode(newNode->GetRight());
      if(currRight != NULL) {
        currRight->SetLeft(newNode->GetPageRID().Page());
        delete currRight;
      }

      BTNode * nodeInsertedInto = NULL;

      // put the new entry into one of the 2 now.
      // In the comparison,
      // > takes care of normal cases
      // = is a hack for dups - this results in affinity for preserving
      // RID ordering for children - more balanced tree when all keys
      // are the same.
      if(node->CmpKey(pData, node->LargestKey()) >= 0) {
        newNode->Insert(failedKey, failedRid);
        nodeInsertedInto = newNode;
      }
      else { // <
        node->Insert(failedKey, failedRid);
        nodeInsertedInto = node;
      }

      // go up to parent level and repeat
      level--;
      if(level < 0) break; // root !
      // pos at which parent stores a pointer to this node
      int posAtParent = pathP[level];
      // std::cerr << "posAtParent was " << posAtParent << std::endl ;
      // std::cerr << "level was " << level << std::endl ;


      BTNode * parent = path[level];
      // update old key - keep same addr
      parent->Remove(NULL, posAtParent);
      result = parent->Insert((const void*)node->LargestKey(),
                              node->GetPageRID());
      // this result should always be good - we removed first before
      // inserting to prevent overflow.

      // insert new key
      result = parent->Insert(newNode->LargestKey(), newNode->GetPageRID());

      // iterate for parent node and split if required
      node = parent;
      failedKey = newNode->LargestKey(); // failure cannot be in node -
                                         // something was removed first.
      failedRid = newNode->GetPageRID();

      delete newNode;
      newNode = NULL;
    } // while

    if(level >= 0) {
      // insertion done
      return 0;
    } else {
      // std::cerr << "root split happened" << std::endl;

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

      root = new BTNode(fileHdr.attrType, fileHdr.attrLength,
                           ph, true,
                           PF_PAGE_SIZE);
      root->Insert(node->LargestKey(), node->GetPageRID());
      root->Insert(newNode->LargestKey(), newNode->GetPageRID());

      // pin root page - should always be valid
      fileHdr.firstFreePage = root->GetPageRID().Page();
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

BTNode* IX_IndexHandle::FindLeaf(const void *pData)
{
  RC invalid = IsValid(); if(invalid) return NULL;
  if (root == NULL) return NULL;

  if(fileHdr.height == 1)
  {
    path[0] = root;
    return root;
  }

  for (int i = 1; i < fileHdr.height; i++)
  {
     std::cerr << "i was " << i << std::endl;
     std::cerr << "pData was " << *(int*)pData << std::endl;


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
      RC rc = pfFileHandle->UnpinPage(path[i]->GetPageRID().Page());
      if (rc != 0) return NULL;
      delete path[i];
      path[i] = NULL;
    }

    path[i] = FetchNode(r.Page());

    PF_PageHandle dummy;
    // pin path pages

    RC rc = pfFileHandle->GetThisPage(path[i]->GetPageRID().Page(), dummy);
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

  //path = new BTNode* [fileHdr.height];

  for(int i=1;i < fileHdr.height; i++)
    path[i] = NULL;
  path[0] = root;

  pathP = new int [fileHdr.height-1]; // leaves don't point
  for(int i=0;i < fileHdr.height-1; i++)
    pathP[i] = -1;
}

BTNode* IX_IndexHandle::GetRoot() const
{
  return root;
}
//==================================================================
// Delete a new index entry
RC IX_IndexHandle::DeleteEntry(void *pData, const RID &rid)
{
    return 0;
}

// Force index files to disk
RC IX_IndexHandle::ForcePages()
{
    return 0;
}

void IX_IndexHandle::Print(int level, RID r) const {
  RC invalid = IsValid(); if(invalid) assert(invalid);
  // level -1 signals first call to recursive function - root
  // std::cerr << "Print called with level " << level << endl;
  BTNode * node = NULL;
  if(level == -1) {
    level = fileHdr.height;
    node = FetchNode(root->GetPageRID());
    std::cerr << "Print called with level " << level << std::endl;
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

  node->Print();

  if (level >= 2) // non leaf
  {
    for(int i = 0; i < node->GetNumKeys(); i++)
    {
      RID newr = node->GetAddr(i);
      Print(level-1, newr);
    }
  }
  // else level == 1 - recursion ends
  if(level == 1 && node->GetRight() == -1)
    std::cerr << std::endl; //blank after rightmost leaf
  if(node!=NULL) delete node;
}
