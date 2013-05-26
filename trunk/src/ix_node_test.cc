#include "IX_BTNode.h"
#include "redbase.h"
#include "pf.h"

#include <iostream>
#include <string.h>

using namespace std;

RC TestInsert(PF_PageHandle &pageHandler) {
   bool aborted = false;
   const int key = 1;
   RID rid(1, 1);
   IX_BTNode node(INT, sizeof(int), pageHandler, true);
   node.InsertNode(&key, rid);
   void *pResultKey = 0;
   node.GetKey(0, pResultKey);
   aborted = *static_cast<int*>(pResultKey) != key;
   if (aborted) {
      cerr << "Test failed" << endl;
   }
   return !aborted? OK_RC: 1;
}

RC TestFind() {
   return OK_RC;
}

RC TestDelete() {
   return OK_RC;
}

RC TestSort() {
   return OK_RC;
}

int main()
{
   static const char cstTEST_FILE[] = "TEST_FILE";
   bool aborted = false;
   RC rc = OK_RC;
   PF_Manager pfm;
   PF_FileHandle fh1;
   PF_PageHandle ph;
   PageNum pageNum;
   aborted = (OK_RC != (rc = pfm.CreateFile(cstTEST_FILE)));
   aborted = aborted || (OK_RC != (rc = pfm.OpenFile(cstTEST_FILE, fh1)));
   aborted = aborted || (OK_RC != (rc = fh1.AllocatePage(ph)));
   aborted = aborted || (OK_RC != (rc = TestInsert(ph)));
   aborted = aborted || (OK_RC != (rc = ph.GetPageNum(pageNum)));
   aborted = aborted || (OK_RC != (rc = fh1.MarkDirty(pageNum)));
   aborted = aborted || (OK_RC != (rc = fh1.UnpinPage(pageNum)));
   aborted = aborted || (OK_RC != (rc = pfm.CloseFile(fh1)));
   aborted = aborted || (OK_RC != (rc = pfm.DestroyFile(cstTEST_FILE)));
   if (aborted) {
       cout << "Some problem occurred" << endl;
   }
   else {
        cout << "TEST SUCCESS" << endl;   
   }
   return rc;
}
