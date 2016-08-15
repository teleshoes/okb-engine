#include "key_shift.h"
#include "log.h"

#include <QFile>
#include <QCryptographicHash>
#include <QByteArray>
#include <QSaveFile>
#include <math.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/*
  Evaluate user error for key position (distance between actual key center and curve match point) for each key.
  Data is persistent.
  this replaces the old hardcoded key shift (which is now used for initial values only)

  persistence file format (space separated)
  <key> <count> <x average> <y average> <x variance> <y variance>
 */


int lock_file(QString file) {
  // it seems QLockFile does not proper locking and is sensitive to stale lock files
  // so let's use good old flock()
  int fd = open(QSTRING2PCHAR(file + ".lock"), O_CREAT | O_WRONLY, 0644);
  if (fd == -1) { return -1; }
  if (flock(fd, LOCK_EX)) { close(fd); return -1; }
  return fd;
}



KeyShift::KeyShift(Params *params) {
  this->params = params;
  lock_fd = -1;
  load_ok = false;
  dirty = false;
}

void KeyShift::load() {
  // file locking is not required because writes are done with atomic rename
  load_ok = false;
  dirty = false;
  key_info.clear();

  QFile in_file(file_name);
  if (! in_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    logdebug(KS_TAG "File not found (normal on first use)");
    return;
  }
  QTextStream in(&in_file);
  while(! in.atEnd()) {
    QString line = in.readLine();
    if (line == "OK") {
      logdebug(KS_TAG "Load file OK: %s", QSTRING2PCHAR(file_name));
      load_ok = true;
      return;
    }
    QStringList columns = line.split(" ");

    if (columns.size() != 6) {
      logdebug(KS_TAG "Invalid file content (%s): %s", QSTRING2PCHAR(file_name), QSTRING2PCHAR(line));
      return;
    }

    QString key = columns[0];
    key_info_t info;
    info.count = columns[1].toInt();
    info.avg_x = columns[2].toDouble();
    info.avg_y = columns[3].toDouble();
    info.avg_var_x = columns[4].toDouble();
    info.avg_var_y = columns[5].toDouble();
    if (info.count <= 0) {
      logdebug(KS_TAG "Invalid file content (%s): %s", QSTRING2PCHAR(file_name), QSTRING2PCHAR(line));
      return;
    }
    key_info[key] = info;
  }
  logdebug(KS_TAG "Truncated file: %s", QSTRING2PCHAR(file_name));
}

/* make a hash for keyboard description to make sure we use different data
   for different keyboards (languages, orientation or new keyboard version) */
QString KeyShift::eval_hash(QHash<QString, Key> &keys) {
  QList<QString> letters = keys.keys();
  qSort(letters.begin(), letters.end());

  QCryptographicHash hash(QCryptographicHash::Md4);
  foreach(QString letter, letters) {
    Key key = keys[letter];
    char buf[20];
    snprintf(buf, sizeof(buf), "(%s %d %d)", QSTRING2PCHAR(letter), key.x, key.y);
    hash.addData((const char *)buf, strlen(buf));
  }

  return hash.result().toHex();
}

void KeyShift::setParams(Params *params) {
  this->params = params;
}

void KeyShift::setDirectory(QString workDir) {
  this->directory = workDir;
}

/* load keyboard error file (if needed) and apply to current keyboard */
void KeyShift::loadAndApply(QHash<QString, Key> &keys) {
  hash = eval_hash(keys);
  if (! params->key_shift_enable) { return; }

  if (hash != current_hash) {
    if (dirty && load_ok) { save(); }
    current_hash = hash;
    file_name = directory + "/key_shift_" + hash + ".ks";
    load();
  }
  QHashIterator<QString, Key> i(keys);
  while (i.hasNext()) {
    i.next();
    QString letter = i.key();

    if (key_info.contains(letter)) {
      // we already know this key
      keys[letter].corrected_x = keys[letter].x + key_info[letter].avg_x * params->key_shift_ratio;
      keys[letter].corrected_y = keys[letter].y + key_info[letter].avg_y * params->key_shift_ratio;

    } else {
      // new key, keep upstream adjustments
      if (keys[letter].corrected_x < 0) {
	key_info[letter].avg_x = key_info[letter].avg_y = 0; // no upstream adjustment
      } else {
	key_info[letter].avg_x = (keys[letter].corrected_x - keys[letter].x) / params->key_shift_ratio;
	key_info[letter].avg_y = (keys[letter].corrected_y - keys[letter].y) / params->key_shift_ratio;
      }
      key_info[letter].avg_var_x = 0;
      key_info[letter].avg_var_y = 0;
      key_info[letter].count = 1;
    }

  }
}

/* lock data file and reload it */
void KeyShift::lock() {
  /*
    during tests & simulations, we run a lot of instance in parallel, so we have to lock the state file
    to avoid unnecessary serialization, we are holding the lock only in this function, so we have
    to reload because data may have changed since we read it
    lock will be automatically released when save() is called
    this also means test with persistence activated will not be completely repeatable

    this is not used on the phone
  */

  lock_fd = lock_file(file_name);
  if (lock_fd == -1) {
    logdebug(KS_TAG "locking failed");
    return;
  }

  logdebug(KS_TAG "file locked");
  load();
}

/* update key error statistics based on last user input */
void KeyShift::update(QString letter, int delta_x, int delta_y) {
  if (! key_info.contains(letter)) {
    logdebug(KS_TAG "unknown key '%s': not updating", QSTRING2PCHAR(letter));
    return;
  }
  key_info_t*  p = &(key_info[letter]);
  double a = params->key_shift_ema_coef;

  // simple exponential moving average for key position
  p->avg_x = a * delta_x + (1 - a) * p->avg_x;
  p->avg_y = a * delta_y + (1 - a) * p->avg_y;

  // variance (is this a real formula ?)
  p->avg_var_x = a * pow(delta_x - p->avg_x, 2) + (1 - a) * p->avg_var_x;
  p->avg_var_y = a * pow(delta_y - p->avg_y, 2) + (1 - a) * p->avg_var_y;

  p->count ++;

  dirty = true;
}

void KeyShift::save() {
  if (! dirty) { return; }
  QSaveFile out_file(file_name);
  if (! out_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    logdebug(KS_TAG "Can not write file: %s", QSTRING2PCHAR(file_name));
    return;
  }
  QTextStream out(&out_file);

  QList<QString> letters = key_info.keys();
  qSort(letters.begin(), letters.end());
  foreach(QString letter, letters) {
    key_info_t info = key_info[letter];
    out << letter << " " << info.count << " " << info.avg_x << " " << info.avg_y << " ";
    out << info.avg_var_x << " " << info.avg_var_y << endl;
  }
  out << "OK" << endl;
  out_file.commit();
  logdebug(KS_TAG "File saved: %s", QSTRING2PCHAR(file_name));

  if (lock_fd != -1) {
    close(lock_fd); // release lock
    lock_fd = -1;
  }
}
