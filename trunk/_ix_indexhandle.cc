//
// File:        ix_indexhandle.cc
// Description: IX_IndexHandle class implementation
// Author:      LIU Shishi (shishi.liu@enst.fr)
//

#include "ix_internal.h"

IX_IndexHandle::IX_IndexHandle()
{
   // Initialize member variables
   bHdrChanged = FALSE;
   memset(&fileHdr, 0, sizeof(fileHdr));
   fileHdr.firstFree = RM_PAGE_LIST_END;
}

IX_IndexHandle::~IX_IndexHandle()
{
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
RC IX_IndexHandle::_InsertEntry(void *pRecordData, const RID &rid)
{
   RC rc;
   PageNum pageNum;
   PF_PageHandle pageHandle;

   IX_PageHdr *indexPageHdr;
   char *pData;

   // Sanity Check: pRecordData must not be NULL
   if (pRecordData == NULL)
      return (IX_NULLPOINTER);
 
   // Allocate a new page if free page list is empty
   // if free page list empty, creat the root node(root page)
   if (fileHdr.firstFree == IX_PAGE_LIST_END) {
      // Call PF_FileHandle::AllocatePage()
      if (rc = pfFileHandle.AllocatePage(pageHandle))
         // Unopened file handle
         goto err_return;

      // Get page number
      if (rc = pageHandle.GetPageNum(pageNum))
         // Should not happen
         goto err_unpin;

      // Get data pointer
      if (rc = pageHandle.GetData(pData))//here, the pData is the data pointer of a BTNode
         // Should not happen
         goto err_unpin;

      // Set page header
      // Note that allocated page was already zeroed out
      // Write the file header (to the buffer pool)
      indexPageHdr = (IX_PageHdr *)pData;
      indexPageHdr->LeftPage = IX_PAGE_LIST_END;
      indexPageHdr->RightPage = IX_PAGE_LIST_END;
      indexPageHdr->entryNumbre = 0;
      indexPageHdr->pageType = Leaf;

      // Mark the page dirty since we changed the next pointer
      if (rc = pfFileHandle.MarkDirty(pageNum))
         // Should not happen
         goto err_unpin;

      // Unpin the page
      if (rc = pfFileHandle.UnpinPage(pageNum))
         // Should not happen
         goto err_return;

      // Place into the free page list
      fileHdr.firstFree = pageNum;//build the root
      bHdrChanged = TRUE;
   }
   // Pick the first page on the list
   else {
      pageNum = fileHdr.firstFree;//get the root
   }

    InsertEntry(pRecordData, rid, pageNum);

   // Recover from inconsistent state due to unexpected error
err_unpin:
   pfFileHandle.UnpinPage(pageNum);
err_return:
   // Return error
   return (rc); 
   return 0;
}

//==========================================================================
//internal node:
//p0(pagehdr:pagenbr),[k1,rid(pagenbr1,0)],[k2,rid(pagenbr2,0)]...
//leaf node:
//p0(pagehdr:pagenbr(NULL)),[k1,rid(pagenbr_k1,slot_k1)],[k2,rid(pagenbr_k2,slot_k2)]...
//pData->p0
RC IX_IndexHandle::_InsertEntry(void *pRecordData, const RID &rid, PageNum pageNum)
{
    RC rc;
    PF_PageHandle pageHandle;

    IX_PageHdr *indexPageHdr;
    char *pData;

    //
       // Pin the page
       //
       if (rc = pfFileHandle.GetThisPage(pageNum, pageHandle))
          goto err_return;

       // Get data pointer
       //
       if (rc = pageHandle.GetData(pData))
          goto err_unpin;

       // Get pageHdr information(a node information)
       //
       memcpy(indexPageHdr,pData,sizeof(IX_PageHdr));

       //get the information of the root

       //internal node
       if(indexPageHdr->pageType == NonLeaf)
       {
           //find i such that Ki <= entry's key value <= Ki+1;
           //insertNode(pNode, k, kn);
           InsertEntry(pRecordData, rid, pageNum)
           //if newchildentry is null, return; -->usual case,didn't split child
           if(kn == NULL)
           {
               return;
           }
           //else
           else
           {
               //if node has space, put *newchildentry on it, set newchildentry to null,return
               if(indexPageHdr->entryNumbre < fileHdr.MaxKeyNumbre)
               {
                   kn = NULL;
                   return;
               }
               //else
               else
               {
                   //split N;  //2d+1 key values and 2d+2 nodepointers
                   //first d key values and d+1 nodepointers stay
                   //last d keys and d+1 pointers move to new node N2
                   //-->*newchildentry set to guide searches between N and N2
                   //newchildentry =&((smallest key value on N2, pointer to N2));
                   //if N is the root
                       //creat new node with (pointer to N,*newchildentry)
                       //make the tree's root node pointer point to the new node;
                   return;
               }
           }
       }

    //the node is the leaf node
    if(indexPageHdr->pageType == Leaf)
    {
        //if node has space, say L;put entry(k) on it, set newchildentry(kn), to null and return
        if(pNode->indexKey.size() < MAXKEYNUM + 1)
        {

        }
        //else split the node L:
        else
        {
               //first d(MINKEYNUM) entries stay, rest move to brand new node L2;
               //newchildentry = &((smallest key value on L2, pointer to L2))
               //set sibling pointers in L and L2
               return;
        }
    }
       // Recover from inconsistent state due to unexpected error
    err_unpin:
       pfFileHandle.UnpinPage(pageNum);
    err_return:
       // Return error
       return (rc);
       return 0;
}

RC IX_IndexHandle::SplitPage()
{
    return 0;
}

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

