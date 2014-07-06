#include "curve_match.h"
#include "tree.h"
#include <QString>

int main(int argc, char* argv[]) {
  LetterTree t;
  t.loadFromFile(QString(argv[1]));
  t.dump();
}
