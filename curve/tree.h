#ifndef TREE_H
#define TREE_H

/* this is a basic packed structure, enough for demo */

#include <QList>
#include <QString>
#include <QPair>
#include <QFile>

class LetterNode;

/* represent a non-leaf node */
typedef struct {
  unsigned char letter:6;
  bool last_child:1;
  bool payload:1;
  int child_index:24;
} node_t;

/* pointer to position in tree */
class LetterTree {
  friend class LetterNode;

 private:
  unsigned char *data;
  int length;
  void dump(QString prefix, LetterNode node);
 public:
  LetterTree();
  ~LetterTree();
  bool loadFromFile(QString fileName);
  LetterNode getRoot();
  void dump();
};

/* all words in a letter tree */
class LetterNode {
  friend class LetterTree;

 private:
  int index;
  unsigned char letter;
  unsigned char parent_letter;
  LetterTree *tree;
  node_t getNodeInfo(int offset = 0);

 public:
  QList<LetterNode> getChilds();
  unsigned char getChar();
  unsigned char getParentChar();
  bool isLeaf();
  QPair<void*, int> getPayload();
  bool hasPayload();
  QString toString();
};

#endif /* TREE_H */
