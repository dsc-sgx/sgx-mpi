#include <stdio.h>
#include <string>
#include <map>

#include "Enclave_t.h"

#include <stdlib.h>
#include <assert.h>
#include <float.h>
#include <math.h>
#include "./TrustedLibrary/mylib.cpp"

// Other helper or utility functions can be defined and only used internally within enclave
std::map<std::string, int> countword(const char* data, size_t ndata){
	std::map<std::string, int> results;

	const char* curCharS = data;
	const char* curCharE = curCharS;
	size_t nProcessed = 0;
	while (nProcessed < ndata){
		std::string curword = "";
		while ((*curCharE >= 'A' && *curCharE <='Z') || (*curCharE >= 'a' && *curCharE <='z')){
			char tmpchar = *curCharE;
			if (tmpchar <= 'Z') {
				tmpchar += 0x20; //all lower case
			}
			curword += tmpchar;
			curCharE++;
			nProcessed++;
		}
		if (curword != ""){
			if (results.count(curword) > 0){
				results[curword] ++;
			} else {
				results[curword] = 1;
			}
		}
		curCharE++;
		nProcessed++;
	}
	return results;
}

// This is the function you define to process individual chunked data for your data parallel application
void secure_word_count(const char* data, size_t ndata) {

	std::map<std::string, int> results = countword(data, ndata);

	size_t nwords = results.size();
	size_t alllen = 0;
	for(std::map<std::string, int>::iterator it=results.begin(); it!= results.end(); ++it){
		alllen += it->first.size();
	}
	size_t wordbufsize = alllen + nwords;
	char* bufword = (char*)malloc(wordbufsize*sizeof(char));
	int* bufcount = (int*)malloc(nwords*sizeof(int));

	char* wordbufcurchar = bufword;
	int* countbufcurcount = bufcount;
	for(std::map<std::string, int>::iterator it=results.begin(); it!= results.end(); ++it){
		const char* curword = it->first.c_str();
		while(*curword != '\0'){
			*wordbufcurchar++ = *curword++;
		}
		*wordbufcurchar++ = '|';
		*countbufcurcount++ = it->second;
	}


	reduce_partial_results(bufword, wordbufsize, bufcount, nwords);
	free(bufword);
	free(bufcount);

    print_message("Finishing processing in enclave");
}
