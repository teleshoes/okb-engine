#ifndef KEY_SHIFT_H
#define KEY_SHIFT_H

#include "scenario.h"
#include <QHash>
#include <QString>

#define KS_TAG "[key shift] "

typedef struct {
  double avg_x;
  double avg_y;
  double avg_var_x;
  double avg_var_y;
  int count;
} key_info_t;

class KeyShift {
 private:
  QHash<unsigned char, key_info_t> key_info;
  QString file_name;
  QString directory;
  QString current_hash;
  Params *params;
  bool load_ok;
  bool dirty;
  int lock_fd;

  void load();
  QString eval_hash(QHash<unsigned char, Key> &keys);

 public:
  KeyShift(Params *params);
  void setParams(Params *params);
  void setDirectory(QString workDir);
  void loadAndApply(QHash<unsigned char, Key> &keys);
  void update(unsigned char key, int delta_x, int delta_y);
  void save();
  void lock();
};

#endif /* KEY_SHIFT_H */
