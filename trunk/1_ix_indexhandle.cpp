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

  BTNode* node = FindLeaf(pData);
  assert(node != NULL);

  int pos = node->FindKey((const void*&)pData, rid);
  if(pos == -1) {

    // make sure that there are no dups (keys) left of this node that might
    // have this rid.
    int p = node->FindKey((const void*&)pData, RID(-1,-1));
    if(p != -1) {
      BTNode * other = DupScanLeftFind(node, pData, rid);
      if(other != NULL) {
        int pos = other->FindKey((const void*&)pData, rid);
        other->Remove(pData, pos); // ignore result - not dealing with
                                   // underflow here
        return 0;
      }
    }

    // key/rid does not exist - error
    return IX_NOSUCHENTRY;
  }

  else if(pos == node->GetNumKeys()-1)
    nodeLargest = true;

  // Handle special case of key being largest and rightmost in
  // node. Means it is in parent and potentially whole path (every
  // intermediate node)
  if (nodeLargest) {
    // std::cerr << "node largest" << std::endl;
    // void * leftKey = NULL;
    // node->GetKey(node->GetNumKeys()-2, leftKey);
    // std::cerr << " left key " << *(int*)leftKey << std::endl;
    // if(node->CmpKey(pData, leftKey) != 0) {
    // replace this key with leftkey in every intermediate node
    // where it appears
    for(int i = fileHdr.height-2; i >= 0; i--) {
      int pos = path[i]->FindKey((const void *&)pData);
      if(pos != -1) {

        void * leftKey = NULL;
        leftKey = path[i+1]->LargestKey();
        if(node->CmpKey(pData, leftKey) == 0) {
          int pos = path[i+1]->GetNumKeys()-2;
          if(pos < 0) {
            continue; //underflow will happen
          }
          path[i+1]->GetKey(path[i+1]->GetNumKeys()-2, leftKey);
        }

        // if level is lower than leaf-1 then make sure that this is
        // the largest key
        if((i == fileHdr.height-2) || // leaf's parent level
           (pos == path[i]->GetNumKeys()-1) // largest at
           // intermediate node too
          )
          path[i]->SetKey(pos, leftKey);
      }
    }
  }

  int result = node->Remove(pData, pos); // pos is the param that counts

  int level = fileHdr.height - 1; // leaf level
  while (result == -1) {

    // go up to parent level and repeat
    level--;
    if(level < 0) break; // root !

    // pos at which parent stores a pointer to this node
    int posAtParent = pathP[level];
    // std::cerr << "posAtParent was " << posAtParent << std::endl ;
    // std::cerr << "level was " << level << std::endl ;


    BTNode * parent = path[level];
    result = parent->Remove(NULL, posAtParent);
    // root is considered underflow even it is left with a single key
    if(level == 0 && parent->GetNumKeys() == 1 && result == 0)
      result = -1;

    // adjust L/R pointers because a node is going to be destroyed
    BTNode* left = FetchNode(node->GetLeft());
    BTNode* right = FetchNode(node->GetRight());
    if(left != NULL) {
      if(right != NULL)
        left->SetRight(right->GetPageRID().Page());
      else
        left->SetRight(-1);
    }
    if(right != NULL) {
      if(left != NULL)
        right->SetLeft(left->GetPageRID().Page());
      else
        right->SetLeft(-1);
    }
    if(right != NULL)
      delete right;
    if(left != NULL)
      delete left;

    node->Destroy();
    RC rc = DisposePage(node->GetPageRID().Page());
    if (rc < 0)
      return IX_PF;

    node = parent;
  } // end of while result == -1


  if(level >= 0) {
    // deletion done
    return 0;
  } else {
    // std::cerr << "root underflow" << std::endl;

    if(fileHdr.height == 1) { // root is also leaf
      return 0; //leave empty leaf-root around.
    }

    // new root is only child
    root = FetchNode(root->GetAddr(0));
    // pin root page - should always be valid
    fileHdr.firstFreePage = root->GetPageRID().Page();
    PF_PageHandle rootph;
    RC rc = pfFileHandle->GetThisPage(fileHdr.firstFreePage, rootph);
    if (rc != 0) return rc;

    node->Destroy();
    rc = DisposePage(node->GetPageRID().Page());
    if (rc < 0)
      return IX_PF;

    SetHeight(fileHdr.height-1); // do all other init
    return 0;
  }
}



// return NULL if there is no root
// otherwise return a pointer to the leaf node that is leftmost OR
// smallest in value
// also populates the path member variable with the path
BTNode* IX_IndexHandle::FindSmallestLeaf()
{
  RC invalid = IsValid(); if(invalid) return NULL;
  if (root == NULL) return NULL;
  RID addr;
  if(fileHdr.height == 1) {
    path[0] = root;
    return root;
  }

  for (int i = 1; i < fileHdr.height; i++)
  {
    RID r = path[i-1]->GetAddr(0);
    if(r.Page() == -1) {
      // no such position or other error
      // no entries in node ?
      assert("should not run into empty node");
      return NULL;
    }
    // start with a fresh path
    if(path[i] != NULL) {
      RC rc = pfFileHandle->UnpinPage(path[i]->GetPageRID().Page());
      if(rc < 0) {
        IX_PrintError(rc);
      }
      if (rc < 0) return NULL;
      delete path[i];
      path[i] = NULL;
    }
    path[i] = FetchNode(r);
    PF_PageHandle dummy;
    // pin path pages
    RC rc = pfFileHandle->GetThisPage(path[i]->GetPageRID().Page(), dummy);
    if (rc != 0) return NULL;
    pathP[i-1] = 0; // dummy
  }
  return path[fileHdr.height-1];
}




// get header from the first page of a newly opened file
RC IX_IndexHandle::GetFileHeader(PF_PageHandle ph)
{
  char * buf;
  RC rc = ph.GetData(buf);
  if (rc != 0)
    return rc;
  memcpy(&fileHdr, buf, sizeof(fileHdr));
  return 0;
}

// persist header into the first page of a file for later
RC IX_IndexHandle::SetFileHeader(PF_PageHandle ph) const
{
  char * buf;
  RC rc = ph.GetData(buf);
  if (rc != 0)
    return rc;
  memcpy(buf, &fileHdr, sizeof(fileHdr));
  return 0;
}



//void IX_IndexHandle::Print(std::ostream & os, int level, RID r) const
void IX_IndexHandle::Print(int level, RID r) const
{
//  RC invalid = IsValid(); if(invalid) assert(invalid);
//  // level -1 signals first call to recursive function - root
//  // std::cerr << "Print called with level " << level << std::endl;
//  BTNode * node = NULL;
//  if(level == -1) {
//    level = fileHdr.height;
//    node = FetchNode(root->GetPageRID());
//  }
//  else {
//    if(level < 1) {
//      return;
//    }
//    else
//    {
//      node = FetchNode(r);
//    }
//  }

//  node->Print();
//  if (level >= 2) // non leaf
//  {
//    for(int i = 0; i < node->GetNumKeys(); i++)
//    {
//      RID newr = node->GetAddr(i);
//      Print(level-1, newr);
//    }
//  }
//  // else level == 1 - recursion ends
//  if(level == 1 && node->GetRight() == -1)
//    std::cerr << std::endl; //blank after rightmost leaf
//  if(node!=NULL) delete node;
}












RC IX_IndexHandle::InsertEntry(void *pData, const RID &rid)
{


    printf("!!!!!!!!!!!");

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



    // largest key in tree is in every intermediate root from root
    // onwards !
    if (node->GetNumKeys() == 0 ||
        node->CmpKey(pData, treeLargest) > 0) {
      newLargest = true;
      prevKey = treeLargest;
    }

    int result = node->Insert(pData, rid);

    if(newLargest) {
      for(int i=0; i < fileHdr.height-1; i++) {
        int pos = path[i]->FindKey((const void *&)prevKey);
        if(pos != -1)
          path[i]->SetKey(pos, pData);
        else {
          // assert(false); //largest key should be everywhere
          // return IX_BADKEY;
          std::cerr << "Fishy intermediate node ?" << std::endl;
        }
      }
      // copy from pData into new treeLargest
      memcpy(treeLargest,
             pData,
             fileHdr.attrLength);
      // std::cerr << "new treeLargest " << *(int*)treeLargest << std::endl;
    }


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










