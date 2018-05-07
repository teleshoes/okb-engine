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

    if (getenv("TREE_DUMP_DEBUG")) {
      cout << "# dump '" << QSTRING2PCHAR(prefix) << "' - index: " << node.index << "/" << offset \
	   << " -> PL=" << info.payload << " LS=" << info.last_child << " [" << (unsigned char) (info.letter + 96) << "] target=" \
	   << info.child_index << endl;
    }

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
     is only intended to add (small) user dictionary words after loading
     tree into memory */

  // allocate a larger buffer if needed
  int min_size = new_index * 4 + 256 * strlen((char *)key) + len + 1000;
  if (alloc < min_size) {
    int new_alloc = min_size;
    unsigned char *new_data = new unsigned char[new_alloc];
    memcpy(new_data, data, alloc);
    delete[] data;

    data = new_data;
    alloc = new_alloc;
  }

  if (len >= 255) { return; } // silent failing (bad!)

  setPayloadRec(key, value, len, 0); // return value is ignored (which mean we can not learn a new starting letter)
}

int LetterTree::addPayloadValue(void* value, int len) {
  int return_index = new_index;
  unsigned char *ptr = (data + 4 * new_index);
  *(ptr ++) = len % 8;
  *(ptr ++) = len >> 8;
  memcpy(ptr, value, len);
  ptr += len;
  *(ptr) = '\0';
  this -> new_index += (len + 3) >> 2;
  return return_index;
}

int LetterTree::setPayloadRec(unsigned char *key, void* value, int len, int index) {
  bool leaf = (! *key);

  if ((! leaf) && (*key < 'a' || *key > 'z')) { return -1; } // sanity check

  int offset = 0;
  bool done = false;
  int cur_count = 0;

  // check existing node
  if (index != -1) { // index == -1 means node does not exist yet
    while(1) {
      node_t *node_p = (node_t*) (data + (index + offset) * 4);
      unsigned char letter = node_p -> letter + 96;

      cur_count ++;

      if (leaf && node_p -> payload) {
	/* case 1: we just have found the leaf node and it has already got a payload
	   -> just replace the payload */
	node_p -> child_index = addPayloadValue(value, len);
	done = true;
      } else if ((! leaf) && (letter == *key)) {
	/* case 2: we have not reached leaf node but a child node already exist to match the next letter
	   -> just recurse down the tree */
	int updated_index = setPayloadRec(key + 1, value, len, node_p -> child_index);
	if (updated_index > 0) {
	  /* child node has been created or moved -> update our pointer */
	  node_p -> child_index = updated_index;
	}
	done = true;
      }

      if (node_p -> last_child) { break; }

      offset ++;
    }

    if (done) {
      /* node has been updated in place, processing for this node is done */
      return -1;
    }
  }

  /* we need to create a new block for the current node
     case 1 (leaf == true): we need to add a payload pointer to a new or existing node
     case 2 (leaf == false) : we need to add a new child to a new or existing node */
  int return_index = new_index;
  node_t *node_payload_p, *node_child_p;

  /* 1) add payload slot */
  if (leaf) {
    node_payload_p = (node_t*) (data + (new_index ++) * 4);
    node_payload_p -> payload = true;
    node_payload_p -> child_index = 0; // will be set later
    node_payload_p -> last_child = (cur_count == 0);
    node_payload_p -> letter = '\0'; // irrelevant
  }

  /* 2) copy previous block */
  if (cur_count > 0) {
    memcpy(data + 4 * new_index, data + 4 * index, 4 * cur_count);
    new_index += cur_count;

    node_t *node_last_p = (node_t*) (data + 4 * (new_index - 1));
    node_last_p -> last_child = leaf;
  }

  /* 3) add new child */
  if (! leaf) {
    node_child_p = (node_t*) (data + (new_index ++) * 4);
    node_child_p -> payload = false;
    node_child_p -> child_index = setPayloadRec(key + 1, value, len, -1);
    node_child_p -> last_child = 1;
    node_child_p -> letter = (*key) - 96;
  }

  /* 1-bis) store payload content */
  if (leaf) {
    node_payload_p -> child_index = addPayloadValue(value, len);
  }

  return return_index;
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
