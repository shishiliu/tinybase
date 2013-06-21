//
// ql_manager_stub.cc
//

// Note that for the SM component (HW3) the QL is actually a
// simple stub that will allow everything to compile.  Without
// a QL stub, we would need two parsers.

#include <cstdio>
#include <iostream>
#include <algorithm>
#include <sys/times.h>
#include <sys/types.h>
#include <cassert>
#include <unistd.h>
#include "redbase.h"
#include "ql.h"
#include "sm.h"
#include "ix.h"
#include "rm.h"
//
#include "printer.h"
#include "iterator.h"
#include <map>
using namespace std;
bool strlt(char* i, char* j) {
        return (strcmp(i, j) < 0);
}

bool streq(char* i, char* j) {
        return (strcmp(i, j) == 0);
}
//
//
// QL_Manager::QL_Manager(SM_Manager &smm, IX_Manager &ixm, RM_Manager &rmm)
//
// Constructor for the QL Manager
//
QL_Manager::QL_Manager(SM_Manager &smm, IX_Manager &ixm, RM_Manager &rmm):smm(smm),ixm(ixm),rmm(rmm)
{
    // Can't stand unused variable warnings!
    assert (&smm && &ixm && &rmm);
}

//
// QL_Manager::~QL_Manager()
//
// Destructor for the QL Manager
//
QL_Manager::~QL_Manager()
{
}

//
// Handle the select clause
//
RC QL_Manager::Select(int nSelAttrs, const AggRelAttr  selAttrs[],
                      int nRelations, const char * const relations[],
                      int nConditions, const Condition conditions[])
{
	 int i;
	        RelAttr* mSelAttrs = new RelAttr[nSelAttrs];
	        for (i = 0; i < nSelAttrs; i++) {
	                mSelAttrs[i].relName = selAttrs[i].relName;
	                mSelAttrs[i].attrName = selAttrs[i].attrName;
	        }
	        AggRelAttr* mSelAggAttrs = new AggRelAttr[nSelAttrs];
	        for (i = 0; i < nSelAttrs; i++) {
	                mSelAggAttrs[i].func = selAttrs[i].func;
	                mSelAggAttrs[i].relName = selAttrs[i].relName;
	                mSelAggAttrs[i].attrName = selAttrs[i].attrName;
	        }
	        char** mRelations = new char*[nRelations];
	        for (i = 0; i < nRelations; i++) {
	                mRelations[i] = strdup(relations[i]);
	        }
	        Condition * mConditions = new Condition[nConditions];
	        for (i = 0; i < nConditions; i++) {
	                mConditions[i] = conditions[i];
	        }
	        ////need to check the variabilty of the query here later
	        sort(mRelations, mRelations+nRelations, strlt);

	        char ** dup = adjacent_find(mRelations, mRelations+nRelations, streq);

	        if(dup!=(mRelations + nRelations)) return QL_DUPREL;
	        
	        bool selectStar = false;
	        if(nSelAttrs == 1 &&  strcmp(mSelAttrs[0].attrName,"*")==0){
	                selectStar = true;
	                //nSelAttrs = 0;
	                for(int i = 0;i<nRelations;i++){
	                        int  a;
	                }
	                
	        }

    cout << "Select\n";

    cout << "   nSelAttrs = " << nSelAttrs << "\n";
    for (i = 0; i < nSelAttrs; i++)
        cout << "   selAttrs[" << i << "]:" << selAttrs[i] << "\n";

    cout << "   nRelations = " << nRelations << "\n";
    for (i = 0; i < nRelations; i++)
        cout << "   relations[" << i << "] " << relations[i] << "\n";

    cout << "   nCondtions = " << nConditions << "\n";
    for (i = 0; i < nConditions; i++)
        cout << "   conditions[" << i << "]:" << conditions[i] << "\n";

    return 0;
}

//
// Insert the values into relName
//
RC QL_Manager::Insert(const char *relName,
                      int nValues, const Value values[])
{
    RC rc;
    char* data = NULL;
    int attrCount;
    DataAttrInfo* attr = NULL;
    int size = 0;
    int offset = 0;
    int i = 0;

    cout << "Insert\n";
    cout << "   relName = " << relName << "\n";
    cout << "   nValues = " << nValues << "\n";

    for (i = 0; i < nValues; i++)
        cout << "   values[" << i << "]:" << values[i] << "\n";

    //step1: get attributes information
    if((rc = smm.GetFromTable(relName, attrCount, attr)))
    {
        rc = SM_BADTABLE;
        goto err_delteattributes;
    }

    if(nValues != attrCount) {
        rc = QL_INVALIDSIZE;
        goto err_delteattributes;
    }
    //step2: examin the insert attibute
    for(int i =0; i < nValues; i++)
    {
      if(values[i].type != attr[i].attrType)
      {
        delete [] attr;
        return QL_JOINKEYTYPEMISMATCH;
      }
      size += attr[i].attrLength;
    }
    //step3: Make record data
    data = new char[size];
    if(data == NULL)
    {
        rc = SM_NOMEM;
        goto err_delteattributes;
    }
    offset = 0;
    for(int i =0; i < attrCount; i++)
    {
      memcpy(data + offset,
             values[i].data,
             attr[i].attrLength);
      offset += attr[i].attrLength;
    }
    //step4:write into file and updata related files(index)
    if((rc = smm.InsertRecord(relName,
                              attrCount,
                              attr,
                              data)))
    {
        goto err_deletedata;
    }

    delete [] data;
    delete [] attr;
    return (0);
err_deletedata:
    delete [] data;
err_delteattributes:
    if(attr!=NULL)
        delete [] attr;
    return (rc);
}

//
// Delete from the relName all tuples that satisfy conditions
//
RC QL_Manager::Delete(const char *relName,
                      int nConditions, const Condition conditions[])
{
    RC rc;
    int i;

    cout << "Delete\n";
    cout << "   relName = " << relName << "\n";
    cout << "   nCondtions = " << nConditions << "\n";
    for (i = 0; i < nConditions; i++)
        cout << "   conditions[" << i << "]:" << conditions[i] << "\n";
//
    char _relName[MAXNAME];
    memset(_relName, '\0', sizeof(_relName));
    strncpy(_relName, relName, MAXNAME);

    for (int i = 0; i < nConditions; i++) {
//      if(conditions[i].lhsAttr.relName == NULL) {
//        conditions[i].lhsAttr.relName = _relName;
//      }
      if(strcmp(conditions[i].lhsAttr.relName, _relName) != 0) {
        delete [] conditions;
        return QL_BADATTR;
      }

      if(conditions[i].bRhsIsAttr == TRUE) {
//        if(conditions[i].rhsAttr.relName == NULL) {
//          conditions[i].rhsAttr.relName = _relName;
//        }
        if(strcmp(conditions[i].rhsAttr.relName, _relName) != 0) {
          delete [] conditions;
          return QL_BADATTR;
        }
      }
    }

    Iterator* it = GetLeafIterator(_relName, nConditions, conditions);

    if(bQueryPlans == TRUE)
      cout << "\n" << it->Explain() << "\n";

    Tuple t = it->GetTuple();
    rc = it->Open();
    if (rc != 0) return rc;

    RM_FileHandle fh;
    rc =	rmm.OpenFile(_relName, fh);
    if (rc != 0) return rc;

    int attrCount = -1;
    DataAttrInfo * attributes;
    rc = smm.GetFromTable(_relName, attrCount, attributes);

    if(rc != 0) return rc;
    IX_IndexHandle * indexes = new IX_IndexHandle[attrCount];
    for (int i = 0; i < attrCount; i++) {
      if(attributes[i].indexNo != -1) {
        ixm.OpenIndex(_relName, attributes[i].indexNo, indexes[i]);
      }
    }

    while(1) {
      rc = it->GetNext(t);
      if(rc ==  it->Eof())
        break;
      if (rc != 0) return rc;

      rc = fh.DeleteRec(t.GetRid());
      if (rc != 0) return rc;

      for (int i = 0; i < attrCount; i++) {
        if(attributes[i].indexNo != -1) {
          void * pKey;
          t.Get(attributes[i].offset, pKey);
          indexes[i].DeleteEntry(pKey, t.GetRid());
        }
      }
    }

    for (int i = 0; i < attrCount; i++) {
      if(attributes[i].indexNo != -1) {
        RC rc = ixm.CloseIndex(indexes[i]);
        if(rc != 0 ) return rc;
      }
    }
    delete [] indexes;
    delete [] attributes;

    rc = rmm.CloseFile(fh);
    if (rc != 0)
        return rc;

    delete [] conditions;
    rc = it->Close();
    if (rc != 0)
        return rc;

    //delete it;
    //cerr << "done with delete it" << endl;
    return 0;
}

//
// Update from the relName all tuples that satisfy conditions
//
RC QL_Manager::Update(const char *relName,
                      const RelAttr &updAttr,
                      const int bIsValue,
                      const RelAttr &rhsRelAttr,
                      const Value &rhsValue,
                      int nConditions, const Condition conditions[])
{
    int i;

    cout << "Update\n";

    cout << "   relName = " << relName << "\n";
    cout << "   updAttr:" << updAttr << "\n";
    if (bIsValue)
        cout << "   rhs is value: " << rhsValue << "\n";
    else
        cout << "   rhs is attribute: " << rhsRelAttr << "\n";

    cout << "   nCondtions = " << nConditions << "\n";
    for (i = 0; i < nConditions; i++)
        cout << "   conditions[" << i << "]:" << conditions[i] << "\n";

    return 0;
}



//
// Choose between filescan and indexscan for first operation - leaf level of
// operator tree
// REturned iterator should be deleted by user after use.
Iterator* QL_Manager::GetLeafIterator(const char *relName,
                                      int nConditions,
                                      const Condition conditions[],
                                      int nJoinConditions,
                                      const Condition jconditions[],
                                      int order,
                                      RelAttr* porderAttr)
{
  if(relName == NULL) {
    return NULL;
  }

  int attrCount = -1;
  DataAttrInfo * attributes;
  RC rc = smm.GetFromTable(relName, attrCount, attributes);
  if(rc != 0) return NULL;

  int nIndexes = 0;
  char* chosenIndex = NULL;
  const Condition * chosenCond = NULL;
  Condition * filters = NULL;
  int nFilters = -1;
  Condition jBased = NULLCONDITION;

  map<string, const Condition*> jkeys;

  // cerr << relName << " nJoinConditions was " << nJoinConditions << endl;

  for(int j = 0; j < nJoinConditions; j++) {
    if(strcmp(jconditions[j].lhsAttr.relName, relName) == 0) {
      jkeys[string(jconditions[j].lhsAttr.attrName)] = &jconditions[j];
    }

    if(jconditions[j].bRhsIsAttr == TRUE &&
       strcmp(jconditions[j].rhsAttr.relName, relName) == 0) {
      jkeys[string(jconditions[j].rhsAttr.attrName)] = &jconditions[j];
    }
  }

  for(map<string, const Condition*>::iterator it = jkeys.begin(); it != jkeys.end(); it++) {
    // Pick last numerical index or at least one non-numeric index
    for (int i = 0; i < attrCount; i++) {
      if(attributes[i].indexNo != -1 &&
         strcmp(it->first.c_str(), attributes[i].attrName) == 0) {
        nIndexes++;
        if(chosenIndex == NULL ||
           attributes[i].attrType == INT || attributes[i].attrType == FLOAT) {
          chosenIndex = attributes[i].attrName;
          jBased = *(it->second);
          jBased.lhsAttr.attrName = chosenIndex;
          jBased.bRhsIsAttr = FALSE;
          jBased.rhsValue.type = attributes[i].attrType;
          jBased.rhsValue.data = NULL;

          chosenCond = &jBased;

          // cerr << "chose index for iscan based on join condition " <<
          //   *(chosenCond) << endl;
        }
      }
    }
  }

  // if join conds resulted in chosenCond
  if(chosenCond != NULL) {
    nFilters = nConditions;
    filters = new Condition[nFilters];
    for(int j = 0; j < nConditions; j++) {
      if(chosenCond != &(conditions[j])) {
        filters[j] = conditions[j];
      }
    }
  } else {
    // (chosenCond == NULL) // prefer join cond based index
    map<string, const Condition*> keys;

    for(int j = 0; j < nConditions; j++) {
      if(strcmp(conditions[j].lhsAttr.relName, relName) == 0) {
        keys[string(conditions[j].lhsAttr.attrName)] = &conditions[j];
      }

      if(conditions[j].bRhsIsAttr == TRUE &&
         strcmp(conditions[j].rhsAttr.relName, relName) == 0) {
        keys[string(conditions[j].rhsAttr.attrName)] = &conditions[j];
      }
    }

    for(map<string, const Condition*>::iterator it = keys.begin(); it != keys.end(); it++) {
      // Pick last numerical index or at least one non-numeric index
      for (int i = 0; i < attrCount; i++) {
        if(attributes[i].indexNo != -1 &&
           strcmp(it->first.c_str(), attributes[i].attrName) == 0) {
          nIndexes++;
          if(chosenIndex == NULL ||
             attributes[i].attrType == INT || attributes[i].attrType == FLOAT) {
            chosenIndex = attributes[i].attrName;
            chosenCond = it->second;
          }
        }
      }
    }

    if(chosenCond == NULL) {
      nFilters = nConditions;
      filters = new Condition[nFilters];
      for(int j = 0; j < nConditions; j++) {
        if(chosenCond != &(conditions[j])) {
          filters[j] = conditions[j];
        }
      }
    } else {
      nFilters = nConditions - 1;
      filters = new Condition[nFilters];
      for(int j = 0, k = 0; j < nConditions; j++) {
        if(chosenCond != &(conditions[j])) {
          filters[k] = conditions[j];
          k++;
        }
      }
    }
  }

  if(chosenCond == NULL && (nConditions == 0 || nIndexes == 0)) {
    Condition cond = NULLCONDITION;

    RC status = -1;
    Iterator* it = NULL;
    if(nConditions == 0)
      it = new FileScan(smm, rmm, relName, status, cond);
    else
      it = new FileScan(smm, rmm, relName, status, cond, nConditions,
                        conditions);

    if(status != 0) {
      PrintErrorAll(status);
      return NULL;
    }
    delete [] filters;
    delete [] attributes;
    return it;
  }

  // use an index scan
  RC status = -1;
  Iterator* it;

  bool desc = false;
  if(order != 0 &&
     strcmp(porderAttr->relName, relName) == 0 &&
     strcmp(porderAttr->attrName, chosenIndex) == 0)
    desc = (order == -1 ? true : false);

  if(chosenCond != NULL) {
    if(chosenCond->op == EQ_OP ||
       chosenCond->op == GT_OP ||
       chosenCond->op == GE_OP)
      if(order == 0) // use only if there is no order-by
        desc = true; // more optimal

    it = new IndexScan(smm, rmm, ixm, relName, chosenIndex, status,
                       *chosenCond, nFilters, filters, desc);
  }
  else // non-conditional index scan
    it = new IndexScan(smm, rmm, ixm, relName, chosenIndex, status,
                       NULLCONDITION, nFilters, filters, desc);

  if(status != 0) {
    PrintErrorAll(status);
    return NULL;
  }

  delete [] filters;
  delete [] attributes;
  return it;
}
