#include "tree.h"
#include <iostream>

#include "log.h"

using namespace std;

LetterTree::LetterTree() {
  data = NULL;
}

LetterTree::~LetterTree() {
  if (data) { delete[] data; }
}  

LetterNode LetterTree::getRoot() {
  LetterNode node;
  node.index = 0;
  node.tree = this;
  node.letter = 0;
  return node;
}

bool LetterTree::loadFromFile(QString fileName) {
  if (data) {
    delete[] data;
    data = NULL;
  }

  QFile file(fileName);
  if (file.open(QFile::ReadOnly)) {
    int len = file.size();
    unsigned char *data = new unsigned char[len];
    if (file.read((char*) data, len) < len) {
      file.close();
      delete[] data;
      return false;
    }

    this -> data = data;
    this -> length = len;
    file.close();
    return true;          
  }
  return false;
}

void LetterTree::dump() {
  dump("", getRoot());
}


void LetterTree::dump(QString prefix, LetterNode node) {
  unsigned char c = node.getChar() || '*';
  prefix += c;
  int offset = 0;
  while (1) {
    node_t info = node.getNodeInfo(offset);
    // cout << "dump - index: " << index << "/" << offset << " -> " << info.payload << " " << info.last_child << " [" << (unsigned char) (info.letter + 96) << "] index=" << info.child_index << endl;
    if (info.payload) {
      cout << QSTRING2PCHAR(prefix) << " -> payload: " << (char*) node.getPayload().first << endl;
    } else {
      LetterNode child;
      child.index = info.child_index;
      child.tree = this;
      child.letter = info.letter + 96;

      cout << QSTRING2PCHAR(prefix) << " :" << prefix.length() << endl;

      dump(prefix + child.letter, child);
    }
    if (info.last_child) { return; }
    offset ++;
  }
}

node_t LetterNode::getNodeInfo(int offset) {
  return *((node_t*) (this -> tree -> data + this -> index * 4 + offset * 4));

  /* C bitfield mapping depends on endiannes (this is old portable way)

  unsigned char* ptr = this -> tree -> data + this -> index * 4 + offset;
  node_t info;
  info.letter = 96 + ptr[0] % 64;
  info.last_child = (ptr[0] & 0x40) != 0;
  info.payload = (ptr[0] & 0x80) != 0;
  info.child_index = ptr[1] + (ptr[2] << 8) + (ptr[3] << 16);
  return info;
  */
}

bool LetterNode::isLeaf() {
  node_t node = getNodeInfo();
  return node.payload && node.last_child;
}

unsigned char LetterNode::getChar() {
  return letter;
}

unsigned char LetterNode::getParentChar() {
  return parent_letter;
}

QList<LetterNode> LetterNode::getChilds() {
  QList<LetterNode> childs;

  if (isLeaf()) { return childs; } // leafs have no childs

  int offset = 0;
  while (1) {
    node_t info = getNodeInfo(offset);
    if (! info.payload) {
      LetterNode node;
      node.index = info.child_index;
      node.letter = info.letter;
      if (node.letter) { node.letter += 96; }
      node.parent_letter = this -> letter;
      node.tree = tree;
      childs << node;
    }

    if (info.last_child) {
      return childs;
    }
    offset ++;
  }
}

bool LetterNode::hasPayload() {
  node_t node = getNodeInfo();
  return node.payload;
}

QPair<void*, int> LetterNode::getPayload() {
  node_t node = getNodeInfo();
  if (! node.payload) { return QPair<void*, int>(NULL, 0); }
  
  unsigned char* payload = this -> tree -> data + node.child_index * 4;
  int len = (int) (payload[0] + (payload[1] << 8));
  payload += 2;

  return QPair<void*, int>(payload, len);
}

QString LetterNode::toString() {
  QString str;
  str.sprintf("LetterNode[%d:%c leaf=%d payload=%d]", index, letter?letter:'@', (int) isLeaf(), (int) hasPayload());
  return str;
}
