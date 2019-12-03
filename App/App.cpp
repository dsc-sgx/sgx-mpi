#include <iostream>
#include <vector>
#include <cmath>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <stdio.h>
#include <iostream>
#include <chrono>
#include <string>
#include <cstring>
#include <map>

#include <mpi.h>

#include <unistd.h>
#include "Enclave_u.h"
#include "sgx_urts.h"
#include "sgx_utils/sgx_utils.h"

using namespace std;

/* Global EID */
sgx_enclave_id_t worker0_eid = 0;
sgx_enclave_id_t worker1_eid = 1;

int nprocs = 1;
int myrank = 0;

// OCALL implementations
void print_message(const char* str) {
    std::cout << str << std::endl;
}

// This defines the reduce part (using MPI) to summarize the partial results from individual processes
void reduce_partial_results(char* bufword, size_t wordbufsize, int* bufcount, size_t nwords){

	int lenbuf = wordbufsize;
	int mynwords = nwords;

	int* lenSubResults;
	int* displ;
	int* subNWords;
	int* countDispl;
	int* countptr = bufcount;

	if(myrank == 0){
		lenSubResults = (int*) malloc(nprocs*sizeof(int));
		displ = (int *) malloc (nprocs*sizeof(int));
		subNWords = (int*) malloc(nprocs*sizeof(int));
		countDispl = (int *) malloc (nprocs*sizeof(int));
	}

	if(myrank == 0){
		MPI_Gather(&lenbuf, 1, MPI_INT, lenSubResults, 1, MPI_INT, 0, MPI_COMM_WORLD);
		MPI_Gather(&mynwords, 1, MPI_INT, subNWords, 1, MPI_INT, 0, MPI_COMM_WORLD);
	} else {
		MPI_Gather(&lenbuf, 1, MPI_INT, NULL, 1, MPI_INT, 0, MPI_COMM_WORLD);
		MPI_Gather(&mynwords, 1, MPI_INT, NULL, 1, MPI_INT, 0, MPI_COMM_WORLD);
	}


	int allResultsBufLen;
	int allCountsBufLen;
	char* allResultsBuf;
	int* allCountsBuf;
	if(myrank == 0){
		int curdispl = 0;
		int countCurdispl = 0;
		displ[0] = 0;
		countDispl[0] = 0;
		for(int n = 1; n < nprocs; n++){
			curdispl += lenSubResults[n-1];
			countCurdispl += subNWords[n-1];
			displ[n] = curdispl;
			countDispl[n] = countCurdispl;
		}
		curdispl += lenSubResults[nprocs-1];
		countCurdispl += subNWords[nprocs-1];
		allResultsBufLen = curdispl;
		allCountsBufLen = countCurdispl;
		allResultsBuf = (char *) malloc (allResultsBufLen * sizeof(char));
		allCountsBuf = (int *) malloc (allCountsBufLen * sizeof(int));
	}

	if(myrank == 0){
		MPI_Gatherv(bufword, lenbuf, MPI_CHAR, allResultsBuf, lenSubResults, displ, MPI_CHAR, 0, MPI_COMM_WORLD);
		MPI_Gatherv(bufcount, mynwords, MPI_INT, allCountsBuf, subNWords, countDispl, MPI_INT, 0, MPI_COMM_WORLD);
	} else {
		MPI_Gatherv(bufword, lenbuf, MPI_CHAR, NULL, NULL, NULL, MPI_CHAR, 0, MPI_COMM_WORLD);
		MPI_Gatherv(bufcount, mynwords, MPI_INT, NULL, NULL, NULL, MPI_INT, 0, MPI_COMM_WORLD);
	}

	if(myrank == 0){
		std::map<std::string, int> finalResults = {};
		char* curwordStart = allResultsBuf;
		char* curwordEnd = allResultsBuf;

		int* curcount = allCountsBuf;
		int nwordpassed = 0;
		int nchar = allResultsBufLen;
		int curchar = 0;

		char * pch;
		const char* delim = "|";

		int mycount;

		while(curchar < nchar){
			while(*curwordEnd != '|'){
				++curwordEnd;
				++curchar;
			}
			*curwordEnd = '\0';
			std::string curwordStr(curwordStart);
			int mycount = *curcount;
			if (curwordStr.size()>0){
				if (finalResults.count(curwordStr) > 0){
					finalResults[curwordStr] += mycount;
				} else {
					finalResults[curwordStr] = mycount;
				}
				++curcount;
			}
			++curwordEnd;
			++curchar;
			++nwordpassed;
			curwordStart = curwordEnd;
		}

		std::vector<std::pair<std::string, int>> top_twenty(20);
		std::partial_sort_copy(finalResults.begin(),
							   finalResults.end(),
							   top_twenty.begin(),
							   top_twenty.end(),
		                       [](std::pair<const std::string, int> const& l,
		                          std::pair<const std::string, int> const& r)
		                       {
		                           return l.second > r.second;
		                       });
		for (int i = 0; i < 20; i++) {
		    cout << i+1 << ": " << top_twenty[i].first << " (" << top_twenty[i].second << ")" << endl;
		}

	}


	if(myrank == 0){
		free (lenSubResults);
		free (displ);
		free (allResultsBuf);
		free (subNWords);
		free (countDispl);
		free (allCountsBuf);
	}

}

// Main function to deal with data loading and split; the mapping part of map-reduce
int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

	if(argc != 2){
		cout<<"Usage: prog data.file"<<endl;
		return 1;
	}

	MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
	MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
	//cout << "myrank: " << myrank << "...nprocs" << nprocs << endl;

	MPI_Barrier(MPI_COMM_WORLD);

	//
	// each process has access to the whole dataset.
	// It loads the whole dataset but only send the splitted chunk to enclave for processing
	cout << "Start data loading..." << endl;
	auto dataloading = chrono::high_resolution_clock::now();

	string datafilename = argv[1];

	std::ifstream infile(datafilename);
	std::string contents ((std::istreambuf_iterator<char>(infile)), std::istreambuf_iterator<char>());
	//printf(contents.c_str());
	const char* data = contents.c_str();

	cout << "Start enclave init..." << endl;
	auto enclaveinit = chrono::high_resolution_clock::now();

	sgx_enclave_id_t myenclave = myrank;
    if (initialize_enclave(&myenclave, "enclave.token", "enclave.signed.so") < 0) {
        std::cout << "Failed to initialize enclave." << std::endl;
        return 1;
    }

    size_t ndata = contents.length();

	cout << "Start calculation..." << endl;
	auto calculate = chrono::high_resolution_clock::now();

	//
	// data splitting. This could be application specific
	int sublen = ndata/nprocs;
	const char** nstart;
	int* nsublen;
	nstart = new const char*[nprocs];
	nsublen = new int[nprocs];
	for(int n=0; n<nprocs; n++){
		nstart[n] = data+sublen*n;
	}
	for(int n=1; n<nprocs; n++){
		const char* curCharE = nstart[n];
		while((*curCharE >= 'A' && *curCharE <='Z') || (*curCharE >= 'a' && *curCharE <='z')){
			++curCharE;
		}
		nstart[n] = curCharE;
	}
	for(int n=0; n<nprocs; n++){
		if(n < nprocs -1) {
			nsublen[n] = nstart[n+1] - nstart[n];
		}else {
			nsublen[n] = ndata - (nstart[n] - nstart[0]);
		}
	}

	//
	// invoke function to process individual chunked data
    secure_word_count(myenclave, nstart[myrank], nsublen[myrank]);

    // results and execution time info
	cout << "Finishing..." << endl;
	auto finish = chrono::high_resolution_clock::now();

	if(myrank == 0){
		chrono::duration<double> duration1 = enclaveinit - dataloading;
		chrono::duration<double> duration2 = calculate - enclaveinit;
		chrono::duration<double> duration3 = finish - calculate;
		cout << duration1.count() << ", " << duration2.count() << ", " << duration3.count() << endl;
	}
	MPI_Finalize();
    return 0;
}
