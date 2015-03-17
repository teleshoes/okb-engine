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

  if (fileName.isEmpty()) {
    return true;
  }

  QFile file(fileName);
  if (file.open(QFile::ReadOnly)) {
    int len = file.size();
    int alloc = len * 1.1;
    unsigned char *data = new unsigned char[alloc];
    if (file.read((char*) data, len) < len) {
      file.close();
      delete[] data;
      return false;
    }

    this -> data = data;
    this -> length = len;
    this -> alloc = alloc;
    this -> dirty = false;
    this -> new_index = (len + 3) >> 2;
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

QPair<void*, int> LetterTree::getPayload(unsigned char *key) {
  LetterNode node = getRoot();
  while (*key) {
    if (*key < 'a' || *key > 'z') { return QPair<void*, int>(NULL, 0); }

    bool found = false;
    LetterNode next_node;
    foreach (LetterNode child, node.getChilds()) {
      if (child.getChar() == *key) {
	found = true;
	next_node = child;
	break;
      }
    }
    if (! found) { return QPair<void*, int>(NULL, 0); }

    key ++;
    node = next_node;
  }

  return node.getPayload();
}

void LetterTree::setPayload(unsigned char *key, void* value, int len) {
  /* limited support for adding new words (ie. modifying tree payloads)
     this will produce "memory holes", but this not a problem as this
     is only intended to add user dictionary word after loading tree
     into memory */

  if ((int) (new_index * 4 + 256 * strlen((char *)key) + len + 1000) > alloc) {
    int new_alloc = alloc * 1.1;
    unsigned char *new_data = new unsigned char[new_alloc];
    memcpy(new_data, data, alloc);
    delete[] data;

    data = new_data;
    alloc = new_alloc;
  }

  setPayloadRec(key, value, len, 0);
}

int LetterTree::setPayloadRec(unsigned char *key, void* value, int len, int index) {
  bool end = (! *key);
  int start_index = index;

  if ((! end) && (*key < 'a' || *key > 'z')) { return -1; }

  if (start_index != -1) {
    if (end) {
      // we've reached the requested leaf node
      node_t *node_p = (node_t*) (data + start_index * 4);
      if (node_p -> payload) {
	// node already has a payload
	if (node_p -> last_child) {
	  // node did not have any child, there is no information to be kept
	  start_index = -1;
	} else {
	  start_index ++; // just skip the existing payload slot (always first)
	}
      }

    } else {
      // look for child node
      while(1) {
	node_t *node_p = (node_t*) (data + index * 4);
	unsigned char letter = node_p -> letter + 96;
	if (letter == *key && ! node_p -> payload) {
	  int updated_index = setPayloadRec(key + 1, value, len, node_p -> child_index);
	  if (updated_index > 0) {
	    // child has been moved -> update our pointer
	    node_p -> child_index = updated_index;
	  }
	  return -1;
	}
	if (node_p -> last_child) { break; }
	index ++;
      }
    }
  }

  // we need to duplicate the current node with exactly one new slot (child or payload)
  int new_start_index = this -> new_index;

  // create the new slot
  node_t *start_node_p = (node_t*)(data + new_start_index * 4);
  start_node_p -> letter = end?0:(*key - 96);
  start_node_p -> last_child = (start_index == -1)?1:0;
  start_node_p -> payload = end;

  this -> new_index ++;

  // copy older slots
  if (start_index > -1) {
    index = start_index;
    while(1) {
      node_t node = *((node_t*) (data + (index ++) * 4));
      *((node_t*) (data + (this -> new_index ++) * 4)) = node; // yuck!
      if (node.last_child) { break; }
    }
  }

  if (end) {
    start_node_p -> child_index = this -> new_index;

    unsigned char* ptr = data + this -> new_index * 4;
    *(ptr ++) = len & 0xFF;
    *(ptr ++) = len >> 8;
    memmove(ptr, value, len);
    *(ptr + len) = '\0';

    this -> new_index += (6 + len) >> 2;
  } else {
    start_node_p -> child_index = setPayloadRec(key + 1, value, len, -1);
  }

  return new_start_index;
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
