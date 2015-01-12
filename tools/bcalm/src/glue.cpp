#include <glue.hpp>
#include <assert.h>
#include <gatb/gatb_core.hpp>
#include <ograph.h>
#include <iostream>
#include <memory>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#ifndef OSX
#include <sys/sysinfo.h> // to determine system memory
#endif

//#define SPARSEHASH 
#ifdef SPARSEHASH
#include <sparsehash/sparse_hash_map>
using google::sparse_hash_map;
#endif 

const string key_of_interest = "CACATGTTCACACACACACACACACACACACACACACACACACACACACAC";
bool glueDebug = false;
using namespace std;

template<typename T>
string add_commas(T num) {
	string s, retval;
	ostringstream o;
	o << num;
	s = o.str();
	for (int i = 0; i < s.length(); i++) {
		retval.push_back(s.at(i));
		int j = s.length() - 1 - i;
		if (((j % 3) == 0) && (j != 0)) {
			retval.push_back(',');
		}
	}
	return retval;
}



string reversecomplement(const string& dna) // code taken from TakeABreak
{
	string revComp= "";

	for (string::const_reverse_iterator it = dna.rbegin(); it != dna.rend(); it++) {
		switch(*it) {
			case 'a' :                revComp += "t";                break;
			case 't' :                revComp += "a";                break;
			case 'c' :                revComp += "g";                break;
			case 'g' :                revComp += "c";                break;
			case 'A' :                revComp += "T";                break;
			case 'T' :                revComp += "A";                break;
			case 'C' :                revComp += "G";                break;
			case 'G' :                revComp += "C";                break;
		}
	}
    return revComp;
}

string rcnorm ( string seq ) {
	return std::min (seq, reversecomplement(seq));
}


/* nifty function to highlight a substring for printing to terminal */
string debug_highlight(string s, string motif)
{
    size_t pos = s.find(motif);
    if (pos == string::npos)
        pos = s.find(reversecomplement(motif));
    if (pos == string::npos)
    {
        return s;
    }

    return s.substr(0,pos) + "\033[1;31m" + s.substr(pos, motif.size()) + "\033[0m" + s.substr(pos + motif.size(), s.size() - pos - motif.size()); 
}


void rcGlueEntry(GlueEntry & entry) {
	int k = entry.lkmer.size();
	entry.seq = reversecomplement(entry.seq);
	std::swap(entry.lmark, entry.rmark);
	entry.lkmer = entry.seq.substr(0,k);
	entry.rkmer = entry.seq.substr(entry.seq.length() - k, k);
}

string tostring(const GlueEntry e, string key) {
	ostringstream out;
	if (e.lmark) out << "_"; else out << " ";
	out << debug_highlight(e.seq, key);
	if (e.rmark) out << "_"; else out << " ";
	return out.str();
}



GlueEntry GlueEntryCompactNaive::getEntry() {
	GlueEntry e;
	//if (raw.length() < kmerSize + 2) { cout << "problem: " << raw << ".\n"; exit(1); }
	if (raw.length() < kmerSize + 2) { //an empty entry
		return e;
	}
	e.seq = raw.substr(0, raw.length() - 2);
	e.lkmer = e.seq.substr(0, kmerSize);
	e.rkmer = e.seq.substr(e.seq.length() - kmerSize, kmerSize);
	e.lmark = raw[raw.length() - 2];
	e.rmark = raw[raw.length() - 1];
	return e;
}

string GlueStorage::dump() { 
	ostringstream o;
	for (auto it = glueMap.begin(); it != glueMap.end(); it++) {
		GlueEntry e;
		derefIt(it, e);
		string key = it->first;
		o << "\"" << key << "\"\t\"" << tostring(e, key) << "\"" << endl;
	}
	return o.str();
}

string GlueStorage::dump(string key, bool dumpkey) { 
	ostringstream o;
	GlueEntry e;
	if(find(key, e)) {
		if (dumpkey) o << "\"" << key << "\"\t";
		o << "\"" << tostring(e, key) << "\"";
	}
	return o.str();
}


bool GlueStorage::derefIt (GlueMap::const_iterator it, GlueEntry & e) {
	if (it == glueMap.end()) return false;
	e = GlueEntryCompactNaive(it->second, kmerSize).getEntry();
	return true;
}

bool GlueStorage::find (string key, GlueEntry & e) {
	findIt  = glueMap.find (key);
	return derefIt(findIt, e);
}

/*bool GlueStorage::getFirst(GlueEntry & e1, GlueEntry & e2) {
	curIt = glueMap.begin();
	return derefIt(curIt, e1, e2);

}
bool GlueStorage::getNext(GlueEntry & e1, GlueEntry & e2) {
	curIt++;
	return derefIt(curIt, e1,e2);
}
*/

void GlueStorage::insertAtKey(string key, GlueEntry e) {
	//insertAtIt(glueMap.find(key), e1, e2);
	string s = GlueEntryCompactNaive(e, kmerSize).getRaw();
	glueMap[key] = s;
}

void GlueStorage::insertAtIt(GlueMap::iterator it, GlueEntry e) {
	if (it == glueMap.end()) {
		cout << "GlueStorage::insertAtIt has it pointing to the end of glueMap...SHOULD not BE.\n";
		exit(1);
	}
	string s = GlueEntryCompactNaive(e, kmerSize).getRaw();
	it->second = s;
}

//returns true if a glue was possible, false if there was no overlap
//glueResult contains the result of the glue
bool Glue::glueSingleEntry(GlueEntry query, GlueEntry match, string key, GlueEntry & glueResult) {
	glueResult = GlueEntry();
	if ((query.seq == "") || (match.seq == "")) 
		return false; // not yet ready to process this
	if ((!query.lmark && !query.rmark)) {
		if (!match.lmark && !match.rmark) {
		printf ("Assert failed!\n"); 
			exit(1);
		}
		return false; //already output
	}
	if (!match.lmark  || (query.rkmer.compare(match.lkmer) != 0)) {
		std::swap(query, match);
	}
	if (!match.lmark) {
		return false; // nothing to do here
	}
	if (query.rkmer.compare(match.lkmer) == 0) {
		//glueStorage.insertAtKey(key, GlueEntry());  //effectively removes the entry from the table
		string gluedStr = query.seq + match.seq.substr(kmerSize, match.seq.size() - kmerSize);
		glueResult.seq = gluedStr;
		glueResult.lmark = query.lmark;
		glueResult.rmark = match.rmark;
		glueResult.lkmer = glueResult.seq.substr(0,kmerSize);
		glueResult.rkmer = glueResult.seq.substr(glueResult.seq.length() - kmerSize, kmerSize);
	} else {
		cout << "uh oh, not matching" << endl;
		cout << "  query: " << debug_highlight(query.seq, query.rkmer) <<  "\n";
		cout << "  match: " << debug_highlight(match.seq, match.lkmer) << std::endl;
		exit(1);
	}
	return true;
}



bool same(GlueEntry e1, GlueEntry e2) {
	// do these have the same left kmer, or the same right kmer? Its not trivial because we should consider double strandedness
	if ((e1.lkmer == e2.lkmer) && (e1.lmark == e2.lmark)) return true;
	if ((e1.rkmer == e2.rkmer) && (e1.rmark == e2.rmark)) return true;
	if ((e1.lkmer == reversecomplement(e2.rkmer)) && (e1.lmark == e2.rmark)) return true;
	if ((e1.rkmer == reversecomplement(e2.lkmer)) && (e1.rmark == e2.lmark)) return true;
	return false;
}


void Glue::insert_aux(GlueEntry newEntry, string key, GlueEntry & glueResult) { 
	GlueEntry e;

	if (key.compare(reversecomplement(key)) >= 0) {
		rcGlueEntry(newEntry);
		key = reversecomplement(key);
	}

	//if (key == key_of_interest)  cout << key << "\tinsert_aux\t" << tostring(newEntry, key)  << "\tprior\t" << glueStorage.dump(key, false) << "\t";
	if (glueDebug) cout << key << "\tinsert_aux\t" << tostring(newEntry, key)  << "\tprior\t" << glueStorage.dump(key, false) << "\t";

	if (!glueStorage.find(key, e)) {
		glueStorage.insertAtKey(key, newEntry);
	} else {
		if (e.seq == "") {
			glueStorage.insertAfterFind(newEntry);
		} else {
			if (same(e, newEntry)) {
				glueStorage.insertAfterFind(newEntry);
			} else {
				//Looks like we need to do a glue!
				glueResult = GlueEntry();
				if (!glueSingleEntry(e, newEntry, key, glueResult)) {
					printf("huh? not able to glue though should be: kmer %s (%s,%s) inserting %s\n",key.c_str(), debug_highlight(e.seq, key).c_str(), debug_highlight(newEntry.seq, key).c_str());
					exit(1);
				} 
				glueStorage.insertAfterFind(GlueEntry());
				//insert(glueResult, true);
			}
		}
	}
	//if (key == key_of_interest) cout << "after\t" << glueStorage.dump(key,false ) << endl;
	if (glueDebug) cout << "after\t" << glueStorage.dump(key,false ) << endl;

}


void Glue::insert(GlueEntry e, bool process) {
	startTimer();
	bool oldGlueDebug = glueDebug;
	if (process == false) {
		cout << "insertion without processing is no longer supported\n";
		exit(1);
	}

	if ((e.seq.find(key_of_interest) != std::string::npos) || (e.seq.find(reversecomplement(key_of_interest)) != std::string::npos)) {
		glueDebug = true;
	}

	if (glueDebug) 
		cout << "insert\t" << tostring(e,"") << endl;

	//if (e.seq == key_of_interest) cout << "Before first insert:\n" << glueStorage.dump();

	GlueEntry insRes1, insRes2;

	if (!e.rmark && !e.lmark) {
		output(e.seq);
	} 
	if (e.rmark) {
		insert_aux(e, e.rkmer, insRes1);
	}
	else {
		if (e.lmark) {
			insert_aux(e, e.lkmer, insRes2);
		}
	}

	if (insRes1.seq != "") insert(insRes1, true);
	if (insRes2.seq != "") insert(insRes2, true);

	glueDebug = oldGlueDebug; 

	stopTimer();
}


void GlueStorage::cleanup() {
	GlueEntry e;
	for (auto it = glueMap.begin(); it != glueMap.end(); ) {
		derefIt(it, e);
		if (e.seq == "") {
#ifdef SPARSEHASH
			glueMap.erase(it);
#else
			it = glueMap.erase(it);
#endif
		} else {
			it++;
		}
	}
#ifdef SPARSEHASH
	glueMap.resize(0); // effectively remove erased entries from memory
#endif
}


void Glue::glue()
{
	/*cout << "Before Glue\n" << glueStorage.dump();
	//cout << "Before glue:\t" << glueStorage.dump(key_of_interest) << endl;
	cout << "After Glue\n" << glueStorage.dump();
	//cout << "After glue:\t" << glueStorage.dump(key_of_interest) << endl;
	*/
	startTimer();
	glueStorage.cleanup(); 
	stopTimer();
}

void Glue::output(string seq)
{
	Sequence s (Data::ASCII);
	s.getData().setRef ((char*)seq.c_str(), seq.size());
	out.insert(s);
}

void Glue::stopTimer() { 
	if (--timerReferenceCount == 0) {
		auto endTime = chrono::system_clock::now();
		totalTime += chrono::duration_cast<chrono::seconds>(endTime - startTime);
	}
}

void GlueStorage::updateMemStats() {
	maxEntries = std::max(maxEntries, glueMap.size());
	totEntries += glueMap.size();

	size_t size = 0;
	for (auto it_gS = glueMap.begin(); it_gS != glueMap.end(); it_gS++)
	{
		size += sizeof(it_gS->first) + sizeof(it_gS->second);
		size += it_gS->first.size() + it_gS->second.size();
	}

	maxSize = std::max(maxSize, size);
	totSize += size;
	numDataPoints++;
}

void GlueStorage::printMemStats() {
	if (numDataPoints == 0) {
		cout << "GlueStorage: no data points to output memory stats.\n";
	} else {
		cout << "GlueStorage memory stats: max entries: " << add_commas(maxEntries) << " avg entries: " << add_commas(totEntries/numDataPoints) << " max size: " << add_commas(maxSize) 
			<< "b avg size: " << add_commas(totSize / numDataPoints) << "b\n";
	}
}




/* 
 * class to support compact representation of a glueentry, as should be stored in memory
 * this version will support a binary representation
 * NOT ACTIVE YET
 *
class GlueEntryCompactBinary{
	public:
		const static int MAX_SIZE = 1000;
		bool raw[MAX_SIZE];
		int rawLen;

		GlueEntryCompact(bool * _raw, int _rawLen) : rawLen(_rawLen) { 
			for (int i = 0; i < rawLen; i++) {
				raw[i] = _raw[i];
			}
		}

		GlueEntryCompact(string seq, bool leftmark, bool rightmark) {
			rawLen = seq.length() * 2 + 2;
			for (int i = 0; i < seq.length(); i++) {
				if ((seq[i] == 'A') || (seq[i] == 'C')) {
					raw[2*i] = 0;
				} else {
					raw[2*i] = 1;
				}
				if ((seq[i] == 'A') || (seq[i] == 'G')) {
					raw[2*i + 1] = 0;
				} else {
					raw[2*i + 1] = 1;
				}
			}
			raw[seq.length() * 2] = leftmark;
			raw[seq.length() * 2 + 1] = rightmark;
		}


		char bits2char (bool leftbit, bool rightbit) {
			if (leftbit == 0 && rightbit == 0) return 'A';
			else if (leftbit == 0 && rightbit == 1) return 'C';
			else if (leftbit == 1 && rightbit == 0) return 'G';
			else return 'T';
		}


		GlueEntry getEntry() {
			GlueEntry entry;
			for (int  i = 0; i < rawLen - 2; i += 2) {
				entry.seq.push_back(bits2char(raw[i], raw[i+1]));
			}
			entry.lkmer = entry.seq.substr(0,k);
			entry.rkmer = entry.seq.substr(entry.seq.length() - k, k);
			entry.lmark = raw[rawLen - 2];
			entry.rmark = raw[rawLen - 1];
		}
};
*/



