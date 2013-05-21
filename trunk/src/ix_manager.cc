//
// File:        ix_manager.cc
// Description: IX_Manager class implementation
// Author:      Shishi LIU (shishi.liu@enst.fr)
//

#include "ix.h"
#include "ix_internal.h"
#include "rm.h"
#include "redbase.h"
#include "pf.h"
#include <stdio.h>
// 
// IX_Manager
//
// Desc: Constructor
//
IX_Manager::IX_Manager(PF_Manager &pfm)
{
  // Set the associated PF_Manager object
  pPfm = &pfm;
}

//
// ~IX_Manager
// 
// Desc: Destructor
//
IX_Manager::~IX_Manager()
{
}

//
// CreateFile
//
// Desc: Create a new IX file whose name is "fileName"
//       Allocate a file header page and fill out some information
// In:   fileName - name of file to create
//       recordSize - fixed size of records
// Ret:  RM_INVALIDRECSIZE or PF return code
//
// Create a new Index

/**
 * This method creates an index numbered indexNo on the data file named fileName.
 * You may assume that clients of this method will ensure that the indexNo parameter is unique 
 * and nonnegative for each index created on a file. 
 * Thus, indexNo can be used along with fileName to generate a unique file 
 * name (e.g., "fileName.indexNo") that you can use for the PF component file storing the new index.
 * The type and length of the attribute being indexed are described by parameters attrType and attrLength, 
 * respectively. As in the RM component, attrLength should be 4 for attribute types INT or FLOAT, 
 * and it should be between 1 and MAXSTRINGLEN for attribute type STRING. 
 * This method should establish an empty index by creating the PF component file and initializing it appropriately. 
 */
RC IX_Manager::CreateIndex(const char *fileName, int indexNo, AttrType attrType, int attrLength, unsigned recordSize)
{
   RC rc = OK_RC;
   PF_FileHandle pfFileHandle;
   PF_PageHandle pageHandle;
   char* pData;
   IX_FileHdr *fileHdr;
   // 1.create a file with name "fileName.indexNo"
   // Note that PF_Manager::CreateFile() will take care of fileName
   // Call PF_Manager::CreateFile()
   char indexFileName[20] = {'0'};//at the beginning, the length of the index name is fixed;
   //int len3 = sprintf(index,"%s.%d",fileName,indexNo);
   if(indexNo < 0||fileName == NULL)
   {
       goto err_return;
   }//specific the error

   //char indexFileName[20] = {'0'};//at the beginning, the length of the index name is fixed;
   //int len3 = sprintf(index,"%s.%d",fileName,indexNo);
   // Test: existing fileName, wrong permission
   if (OK_RC != (rc = pPfm->CreateFile(indexFileName))) {
       goto err_return;
   }

   // 2.analyze attribute type
   // Sanity Check: attrType, attrLength
   switch (attrType) 
   {
      case INT:
      case FLOAT:
         if (attrLength != 4)
            // Test: wrong _attrLength
            return (IX_INVALIDATTR);
         break;

      case STRING:
         if (attrLength < 1 || attrLength > MAXSTRINGLEN)
            // Test: wrong _attrLength
            return (IX_INVALIDATTR);
         break;

      default:
         // Test: wrong _attrType
         return (IX_INVALIDATTR);
   }

   // Call PF_Manager::OpenFile()
   if (OK_RC != (rc = pPfm->OpenFile(fileName, pfFileHandle))) {
      // Should not happen
      goto err_destroy;
   }

   // Allocate the header page (pageNum must be 0)
   if (OK_RC != (rc = pfFileHandle.AllocatePage(pageHandle))) {
      // Should not happen
      goto err_close;
   }

   // Get a pointer where header information will be written
   if (OK_RC != (rc = pageHandle.GetData(pData))) {
      // Should not happen
      goto err_unpin;
   }

   // Write the file header (to the buffer pool)
   fileHdr = (IX_FileHdr *)pData;
   fileHdr->firstFree = RM_PAGE_LIST_END;
   fileHdr->recordSize = recordSize;
   fileHdr->numRecordsPerPage = (PF_PAGE_SIZE - sizeof(RM_PageHdr) - 1) 
                                / (recordSize + 1.0/8);
   if (fileHdr->recordSize * (fileHdr->numRecordsPerPage + 1) 
       + fileHdr->numRecordsPerPage / 8 
       <= PF_PAGE_SIZE - sizeof(RM_PageHdr) - 1) {
      fileHdr->numRecordsPerPage++;
   }
   fileHdr->pageHeaderSize = sizeof(RM_PageHdr) 
                             + (fileHdr->numRecordsPerPage + 7) / 8;
   fileHdr->numRecords = 0;

   // Mark the header page as dirty
   if (OK_RC != (rc = pfFileHandle.MarkDirty(RM_HEADER_PAGE_NUM))) {
      // Should not happen
      goto err_unpin;
   }
   
   // Unpin the header page
   if (OK_RC != (rc = pfFileHandle.UnpinPage(RM_HEADER_PAGE_NUM))) {
      // Should not happen
      goto err_close;
   }
   
   // Call PF_Manager::CloseFile()
   if (OK_RC != (rc = pPfm->CloseFile(pfFileHandle))) {
      // Should not happen
      goto err_destroy;
   }

   // Return ok
   return OK_RC;

   // Recover from inconsistent state due to unexpected error
err_unpin:
   pfFileHandle.UnpinPage(RM_HEADER_PAGE_NUM);
err_close:
   pPfm->CloseFile(pfFileHandle);
err_destroy:
   pPfm->DestroyFile(fileName);
err_return:
   return (rc);
}
