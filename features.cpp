/**
 * @file features.cpp
 */

#include <utility>
#include <vector>
#include <string>
#include <sstream>
#include <memory>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <map>

#include "index/document.h"
#include "io/config_reader.h"
#include "tokenizers/ngram_tokenizer.h"
#include "tokenizers/tree_tokenizer.h"
#include "io/parser.h"
#include "util/common.h"

using std::unordered_map;
using std::unordered_set;
using std::shared_ptr;
using std::pair;
using std::vector;
using std::cout;
using std::endl;
using std::string;

string getClass(const string & path)
{
    size_t idx = path.find_first_of("/");
    return path.substr(0, idx);
}

unordered_map<string, vector<Document>> getDocs(const string & path)
{
    unordered_map<string, vector<Document>> docs;
    Parser parser(path + "/full-corpus.txt", "\n");
    while(parser.hasNext())
    {
        string file = parser.next();
        string className = getClass(file);
        docs[className].push_back(Document(path + "/" + file));
    }
    return docs;
}

void combine_counts(unordered_map<TermID, unsigned int> & language_model, const
        unordered_map<TermID, unsigned int> & doc_counts)
{
    for(auto & count: doc_counts)
        language_model[count.first] += count.second;
}

int main(int argc, char* argv[])
{
    if(argc != 2)
    {
        cerr << "Usage:\t" << argv[0] << " configFile" << endl;
        return 1;
    }

    unordered_map<string, string> config = ConfigReader::read(argv[1]);
    unordered_map<string, vector<Document>> docs =
        getDocs("/home/sean/projects/senior-thesis-data/" + config["prefix"]);

    int nVal;
    istringstream(config["ngram"]) >> nVal;

    unordered_map<string, NgramTokenizer::NgramType> ngramOpt = {
        {"POS", NgramTokenizer::POS}, {"Word", NgramTokenizer::Word},
        {"FW", NgramTokenizer::FW}, {"Char", NgramTokenizer::Char}
    };

    unordered_map<string, TreeTokenizer::TreeTokenizerType> treeOpt = {
        {"Subtree", TreeTokenizer::Subtree}, {"Depth", TreeTokenizer::Depth},
        {"Branch", TreeTokenizer::Branch}, {"Tag", TreeTokenizer::Tag},
        {"Skel", TreeTokenizer::Skeleton}, {"Semi", TreeTokenizer::SemiSkeleton}
    };
  
    Tokenizer* tokenizer = nullptr; 
    string method = config["method"];
    if(method == "ngram")
        tokenizer = new NgramTokenizer(nVal, ngramOpt[config["ngramOpt"]]);
    else if(method == "tree")
        tokenizer = new TreeTokenizer(treeOpt[config["treeOpt"]]);
    else
    {
        cerr << "Method was not able to be determined" << endl;
        return 1;
    } 

    cerr << "Tokenizing..." << endl;
    unordered_map<string, unordered_map<TermID, unsigned int>> language_models;
    for(auto & str: docs)
    {
        for(auto & doc: str.second)
        {
            tokenizer->tokenize(doc, nullptr);
            combine_counts(language_models[str.first], doc.getFrequencies());
        }
    }

    cerr << "Smoothing..." << endl;
    unordered_map<string, unordered_map<TermID, double>> smoothed_models;
    for(auto & model: language_models)
    {
        string label = model.first;

        // get total number of term counts for all terms
        size_t total = 0;
        for(auto & termMap: model.second)
            total += termMap.second;

        cerr << " " << total << " total tokens in class " << label << endl;

        // smooth frequencies
        for(auto & termMap: model.second)
        {
            smoothed_models[label][termMap.first]
                = static_cast<double>(termMap.second) / total;
        }
    }

    cerr << "Comparing features between classes..." << endl;
    for(auto & m1: smoothed_models)
    {
        string class1 = m1.first;
        for(auto & m2: smoothed_models)
        {
            string class2 = m2.first;
            // compare different models
            if(class1 == "chinese" && class2 == "english")
            {
                cerr << "calculating p(f|" << class1 << ")/p(f|" << class2 << ")..." << endl;
                cout << "#### p(f|" << class1 << ")/p(f|" << class2 << ")" << endl;

                // create set of all terms shared between classes
                unordered_set<TermID> termIDs;
                for(auto & term: m1.second)
                    termIDs.insert(term.first);
                for(auto & term: m2.second)
                    termIDs.insert(term.first);
                
                for(auto & termID: termIDs)
                {
                    cout << (m1.second[termID] + .0001) / (m2.second[termID] + .0001)
                         << " " << tokenizer->getLabel(termID) << endl;
                }

                return 0;
            }
        }
    }
    
    delete tokenizer;
    return 0;
}