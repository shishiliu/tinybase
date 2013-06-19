#ifndef CATALOG_H
#define CATALOG_H

#include "printer.h"
#include "parser.h"

struct DataRelInfo {
	// Default constructor
	DataRelInfo() {
		memset(relName, 0, MAXNAME + 1);
	}

	DataRelInfo(char * buf) {
		memcpy(this, buf, DataRelInfo::size());
	}

	DataRelInfo(const DataRelInfo &d) {
		strcpy(relName, d.relName);
		recordSize = d.recordSize;
		attrCount = d.attrCount;
		numPages = d.numPages;
		numRecords = d.numRecords;
	}	;

	DataRelInfo& operator=(const DataRelInfo &d) {
		if (this != &d) {
			strcpy(relName, d.relName);
			recordSize = d.recordSize;
			attrCount = d.attrCount;
			numPages = d.numPages;
			numRecords = d.numRecords;
		}
		return (*this);
	}

	static unsigned int size() {
		return (MAXNAME+1) + 4*sizeof(int);
	}

	static unsigned int members() {
		return 5;
	}

	int recordSize; // 
	int attrCount; // numbre of attributes
	int numPages; // numbre of pages used by relation
	int numRecords; // numbre of records in relation
	char relName[MAXNAME+1]; // Relation name
};

#endif // CATALOG_H
