#include "score.h"

#include <QDebug>
#include <cmath> 
#include <math.h>

#define W_HEAD 39
#define W_COL 15

ScoreCounter::ScoreCounter() {
  debug = false;
  score_pow = 1.0;
  line_count = 0;
  total_score = 0;
  total_coefs = 0;

}

int ScoreCounter::get_col(char *name) {
  char **ptr = col;
  int i = 0;
  while (1) {
    if (! strcmp(*ptr, name)) { return i; }
    i ++;
    ptr ++;
  }
}

void ScoreCounter::set_pow(float pow) {
  score_pow = pow;
}

void ScoreCounter::set_debug(bool debug) {
  this -> debug = debug;
}

void ScoreCounter::set_cols(char **col) {
  this -> col = col;
  if (debug) {
    QString str("=");
    str.append(QString(" ").repeated(W_HEAD - 1));

    char **ptr = col;
    while (*ptr) {
      str.append("--");
      str.append(*ptr);
      str.append(QString("-").repeated(W_COL - 3 - strlen(*ptr)));
      str.append(" ");
      ptr ++;
    }
    qDebug("%s", str.toLocal8Bit().constData());
  }
}

void ScoreCounter::start_line() {
  line_total = 0;
  line_total_coefs = 0;
  line_label_str = QString();
  line_label.setString(&line_label_str);
  current_line_coef = 1;

  if (debug) {
    dbg_line = QString();
  }
}

void ScoreCounter::set_line_coef(float coef) {
  current_line_coef = coef;
}

void ScoreCounter::add_score(float value, float weight, char *name) {
  // scores are [0, 1] values
  if (value == NO_SCORE) { return; }

  add_bonus(pow(value, this -> score_pow), weight, name);
}

void ScoreCounter::add_bonus(float value, float weight, char *name) {
  // bonus are positive or negative offsets
  if (value == NO_SCORE) { return; }

  line_total += value * weight;
  line_total_coefs += weight;

  if (debug) {
    int col = get_col(name);
    QString str;
    str.sprintf("%6.3f [%4.2f]", value, weight);
    update_dbg_line(str, W_HEAD + W_COL * col, W_COL);
  }
}

void ScoreCounter::end_line() {
  float line_score = -1;
  if (line_total_coefs) {
    total_score += current_line_coef * line_total / line_total_coefs;
    line_score = line_total / line_total_coefs;
  }
  total_coefs += current_line_coef; // this count as a "0" score (because if no score can be computed, the curve must be bad)
  line_count ++;

  if (debug) {
    QString str;

    str.sprintf("=%6.3f [%6.2f] %20s", line_score, current_line_coef, line_label_str.toLocal8Bit().constData());
    update_dbg_line(str, 0, W_HEAD);

    qDebug("%s", dbg_line.toLocal8Bit().constData());
  }
}

float ScoreCounter::get_score() {
  return total_score / total_coefs;
}

void ScoreCounter::update_dbg_line(QString text, int col, int width) {
  int l = dbg_line.length();
  if (l < col) {
    dbg_line.append(QString(" ").repeated(col - l));
    dbg_line.append(text);
    return;
  } 

  dbg_line.replace(col, text.length(), text);
}
