#include "huffman.h"

#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <iomanip>

#include <stdio.h>

#define ENC_MODE 0
#define DEC_MODE 1

using namespace std;

// Representing huffman trees as strings:
// supernode_freq(lchild_freq(llchild_freq:llchild_symbol, lrchild_freq:lrchild_symbol), rchild_freq:rchild:symbol)
// (spaces added for clarity)
// this allows us to use a recursive parsing strategy which eats up the string from the beginning, building
// a tree as we go.

// returns a single-line, parseable representation
// of a huffman frequency tree
string tree_to_str(freq_info* root);

// returns a huffman frequency tree when given
// a representation in the format produced by
// tree_to_str()
freq_info* str_to_tree(string str);

// Takes a .^ binary string and produces a string of
// characers with 1 and 0 represented bitwise. Appends
// a character at the end which represents the number of valid
// bits in the preceding character
string to_binary(string encodings);

// Takes a bitwise represented string, and creates a .^
// encoded string with the same information. Assumes
// the existence of an extra character, which represents the number
// of valid bits in the second-to-last character.
string to_string(string bin_enc);

// prints a helpful usage message
void print_usage(char * call);

// strips the linefeed character (^M) from strings -- necessary because
// getline() adds this to the end of every string returned
void strip_linefeed(string &line);

int main(int argc, char * argv[]) {
  // parse arguments -- maybe move into its own function?
  if (argc != 4) {
    print_usage(argv[0]);
    return -1;
  }

  int mode;
  if      (string(argv[1]) == string("-e")) mode = ENC_MODE;
  else if (string(argv[1]) == string("-d")) mode = DEC_MODE;
  else {
    print_usage(argv[0]);
    return -1;
  }

  // open input and output files
  ifstream in_file(argv[2]);
  ofstream out_file(argv[3]);

  if (!in_file.is_open() || !out_file.is_open() ) {
    print_usage(argv[0]);
    return -2;
  }
  
  // check the mode
  if (mode == ENC_MODE) {
    string line;
    stringstream corpus_ss;

    // get the entire file in one string
    while (in_file.good()) {
      getline(in_file, line);
      strip_linefeed(line);

      corpus_ss << line;
      if (in_file.good()) corpus_ss << endl;
    }
    in_file.close();
    
    string corpus = corpus_ss.str();
    
    // produce encoded versions
    tree_queue queue = read_corpus(corpus);
    freq_info* tree = build_huffman_tree(queue);
    map<char, string> encodings = build_encoding_map(tree);
    string encoded_corpus = encode(corpus, encodings);

    // preserve huffman tree in new file
    out_file << tree_to_str(tree);

    // write a binary encoded version
    out_file << to_binary(encoded_corpus);
    
    out_file.close();
  }
  else if (mode == DEC_MODE) {
    string tree_string;

    // get the encoding tree
    getline(in_file, tree_string);
    strip_linefeed(tree_string);

    // parse it into a useful data structure
    freq_info* tree = str_to_tree(tree_string);

    // read the entire encoding. Might think
    // about re-writing this to avoid the klugy
    // getline stuff
    string encoded_file_bin("");;
    while (in_file.good()) {
      string temp;
      getline(in_file, temp);
      strip_linefeed(encoded_file_bin);

      encoded_file_bin += temp;
      if (in_file.good()) encoded_file_bin += '\n';
    }

    // decode binary to .^
    string encoded_file = to_string(encoded_file_bin);
    out_file << decode(encoded_file, tree);
  }
  else {
    print_usage(argv[0]);
    return -1;
  }
}

freq_info* _str_to_tree_right(char * str, int &level);

freq_info* _str_to_tree_left(char * str, int &level) {
  freq_info* node;

  // every left node should be preceded by (
  if (str[0] == '(') {
    // we're one level deeper
    level++;

    float freq;
    int matches = sscanf(str, "(%f%s", &freq, str);
    if (matches != 2) {
      cerr << "Malformed Input File (l2)... Exiting" << endl;
      exit(-1);
    }

    node = new freq_info;
    node->freq = freq;

    // if there's a colon, we're at a leaf and need to find the symbol
    if (str[0] == ':') {
      unsigned symbol;
      matches = sscanf(str, ":%x%s", &symbol, str);
      if (matches != 2) {
        cerr << "Malformed Input File (l3)... Exiting" << endl;
        exit(-1);
      }

      node->is_leaf = true;
      node->symbol = (char) symbol;
    }
    // otherwise, inception!
    else if (str[0] == '(') {
      node->is_leaf = false;
      node->left = _str_to_tree_left(str, level);
      node->right = _str_to_tree_right(str, level);
    }
    // or you're doing it wrong
    else {
      cerr << "Malformed Input File (l4)... Exiting" << endl;
      exit(-1);
    }
  }
  else {
    cerr << "Malformed Input File (l1)... Exiting" << endl;
    exit(-1);
  }

  return node;
}

freq_info* _str_to_tree_right(char * str, int &level) {
  freq_info* node;

  // every right node should be preceded by a ,
  if (str[0] == ',') {
    float freq;
    int matches = sscanf(str, ",%f%s", &freq, str);
    if (matches != 2) {
      cerr << "Malformed Input (r2)... Exiting" << endl;
      exit(-1);
    }

    node = new freq_info;
    node->freq = freq;

    // if there's a colon, we're at a leaf again
    if (str[0] == ':') {
      unsigned symbol;
      matches = sscanf(str, ":%x%s", &symbol, str);
      if (matches != 2) {
        cerr << "Malformed Input (r4)... Exiting" << endl;
        exit(-1);
      }

      node->is_leaf = true;
      node->symbol = (char) symbol;

      // right leaves are closed -- eat up all the
      // ) and keep track of how far out we come
      while (str[0] == ')') {
        level--;
        sscanf(str, ")%s", str);
        // I'm still not satisfied with this -- 
        // just a kluge to aviod an infinite loop on the very
        // end of the string
        if (level == 1) {
          if (strcmp(str, ")") == 0) {
            level--;
            str[0] = '\0';
            break;
          }
        }
      }
    }
    // otherwise, inception!
    else if (str[0] == '(') {
      node->is_leaf = false;
      node->left = _str_to_tree_left(str, level);
      node->right = _str_to_tree_right(str, level);
    }
    else {
      cerr << "Malformed Input (r3)... Exiting" << endl;
      exit(-1);
    }
  }
  else {
    cerr << "Malformed Input (r1)... Exiting" << endl;
    exit(-1);
  }

  return node;
}

freq_info * str_to_tree(string str) {
  // I use c-strings because I'm much, much more comfortable
  // with sscanf then streams or whatever paradigm I'd have
  // to use with c++ syntax.
  char * cstr = (char *) str.c_str();

  int freq;
  int matches = sscanf(cstr, "%d%s", &freq, cstr);

  if (matches != 2) {
    cerr << "Malformed Input File (root)... Exiting" << endl;
    exit(-1);
  }

  freq_info * root = new freq_info;
  root->is_leaf = false;

  root->freq = freq;

  int level = 0;
  root->left = _str_to_tree_left(cstr, level);
  root->right = _str_to_tree_right(cstr, level);
  // this SHOULD check if all parenthesis are properly closed.
  // However, because of my kluge it won't actually do that properly.
  if (level != 0) {
    cerr << "Malformed Input File (parens)... Exiting" << endl;
    cerr << "level: " << level << endl;
    exit(-1);
  }

  return root;
}

void _tree_to_str(freq_info* root, stringstream &ss) {
  if (root->is_leaf) {
    ss << dec << root->freq << ":" << hex << (int) root->symbol;
  }
  else {
    ss << dec << root->freq << "(";
    _tree_to_str(root->left, ss);

    ss << ",";

    _tree_to_str(root->right, ss);
    ss << ")";
  }
}

string tree_to_str(freq_info* root) {
  stringstream ss;

  _tree_to_str(root, ss);
  ss << endl;

  return ss.str();
}

string to_binary(string encodings) {
  stringstream ss;
  int i;
  char c = '\0';
  // loop over the entire string
  for (i = 0; i < encodings.length(); i++) {
    int bit;

    // check our current character
    if (encodings[i] == '.')      bit = 0;
    else if (encodings[i] == '^') bit = 1;
    // if it's malformed, return empty string
    else return "";

    // every 8 characters, write the full character
    // to the outptu stream
    c |= bit << (i % 8);
    if (i % 8 == 7) {
      ss << c;
      c = '\0';
    }
  }
  // properly terminate the output
  // if we have an unfull character
  if (i % 8) {
    // add it (already padded with zeros)
    ss << c;
    // add the "nvalid" termination character
    ss << (char) (i % 8);
  }
  // otherwise, nvalid is 8, and the last character has already
  // been added.
  else ss << (char) 0x08;

  return ss.str();
}

string to_string(string bin_encodings) {
  stringstream ss;
  // the "length" of valid characters
  // is one less than the total length
  // since the last "character" just specifies
  // how many bits are valid in its predecessor
  int length = bin_encodings.length() - 1;

  for (int i = 0; i < length; i++) {
    char c = bin_encodings[i];
    int valid = 8;

    // if we're on the last character, check the final one
    // to find how many bits we should look at.
    if (i == length - 1) valid = (int) bin_encodings[i + 1];

    // expand the char
    for (int j = 0; j < valid; j++) {
      if (c & 0x01) ss << '^';
      else          ss << '.';

      c >>= 1;
    }
  }

  return ss.str();
}

void print_usage(char * call) {
  cerr << "Usage: " << call << " <-e | -d> <input file> <output file>" << endl;
  cerr << "\t-e\treads from <input file> and writes an encoded version to <output file>." << endl;
  cerr << "\t-d\treads from <input file> and writes a decoded version to <output file>." << endl;
}

void strip_linefeed(string &line) {
  if (*line.rbegin() == 0x0d) {
    line.erase(line.end() - 1);
  }
}

// ideally, this would be masked by the library
freq_info* init_freq_info_leaf(char symbol, float freq) {
  // this is already done, and you shouldn't need to modify it.
  freq_info* ret = new freq_info;

  ret->symbol = symbol;
  ret->freq = freq;
  ret->left = NULL;
  ret->right = NULL;
  ret->is_leaf = true;

  return ret;
}

void add_freq(char c, map<char, int> &data) {
  // already done for you. This is a helper function of 'read_corpus'.
  if (data[c] == 0) {
    data[c] = 1;
  } else {
    data[c] = data[c] + 1;
  }
}

// re-written to read from string for
// increased flexibility.
tree_queue read_corpus(string corpus) {
  // already done for you. This reads the contents of a file and
  // produces a priority queue with leaf nodes.

  map<char, int> freq;
  string::iterator cor_curr;
  for (cor_curr = corpus.begin(); cor_curr != corpus.end(); cor_curr++) {
    add_freq(*cor_curr, freq);
  }

  int sum = 0;
  map<char, int>::iterator freq_curr;
  for (freq_curr = freq.begin(); freq_curr != freq.end(); freq_curr++) {
    sum = sum + freq_curr->second;
  }

  // now we know the total number so we can calculate frequency data.
  tree_queue queue;
  float fsum = (float) sum;
  for (freq_curr = freq.begin(); freq_curr != freq.end(); freq_curr++) {
    freq_info* info = init_freq_info_leaf(freq_curr->first, freq_curr->second);

    info->symbol = freq_curr->first;
    info->freq = (float) freq_curr->second / fsum;
    queue.push(info);
  }

  return queue;
}
