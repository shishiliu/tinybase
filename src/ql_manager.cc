//
// ql_manager_stub.cc
//

// Note that for the SM component (HW3) the QL is actually a
// simple stub that will allow everything to compile.  Without
// a QL stub, we would need two parsers.

#include <cstdio>
#include <iostream>
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
using namespace std;

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
RC QL_Manager::Select(int nSelAttrs, const RelAttr selAttrs[],
                      int nRelations, const char * const relations[],
                      int nConditions, const Condition conditions[])
{
    int i;

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
    int i;

    cout << "Delete\n";

    cout << "   relName = " << relName << "\n";
    cout << "   nCondtions = " << nConditions << "\n";
    for (i = 0; i < nConditions; i++)
        cout << "   conditions[" << i << "]:" << conditions[i] << "\n";

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



