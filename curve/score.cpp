#include "score.h"
#include "log.h"
#include "functions.h"

#include <cmath> 
#include <math.h>

#define W_HEAD 39
#define W_COL 9

ScoreCounter::ScoreCounter() {
  debug = false;
  score_pow = 1.0;
  line_count = 0;
  total_score = 0;
  total_coefs = 0;
  total_col = NULL;
}

ScoreCounter::~ScoreCounter() {
  free();
}

void ScoreCounter::free() {
  if (total_col) {
    delete[] total_col;
    delete[] total_coef_col;
    delete[] col_weight;
    delete[] min_col;
    total_col = NULL;
  }
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
  QString str;
  this -> col = col;
  if (debug) {
    str = "=";
    str.append(QString(" ").repeated(W_HEAD - 1));
  }

  char **ptr = col;
  int count = 0;
  while (*ptr) {
    if (debug) {
      str.append("--");
      str.append(*ptr);
      str.append(QString("-").repeated(W_COL - 3 - strlen(*ptr)));
      str.append(" ");
    }
    count ++;
    ptr ++;
  }
  if (debug) {
    logdebug_qstring(str);
  }

  free();
  total_col = new float[count];
  total_coef_col = new float[count];
  col_weight = new float[count];
  min_col = new float[count];
  
  for (int i = 0; i < count; i++) {
    min_col[i] = total_col[i] = total_coef_col[i] = 0;
    col_weight[i] = 1;
  }

  nb_cols = count;
}

void ScoreCounter::set_weights(float *weights) {
  for (int i = 0; i < nb_cols; i++) {
    col_weight[i] = weights[i];
  }
  if (debug) {
    dbg_line = QString("=");
    for (int i = 0; i < nb_cols; i++) {
      QString str;
      str.sprintf(" [%4.2f]", col_weight[i]);
      update_dbg_line(str, W_HEAD + W_COL * i, W_COL);
    }
    logdebug_qstring(dbg_line);
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

void ScoreCounter::add_score(float value, char *name) {
  // scores are [0, 1] values
  if (value == NO_SCORE) { return; }

  add_bonus(pow(value, this -> score_pow), name);
}

void ScoreCounter::add_bonus(float value, char *name) {
  // bonus are positive or negative offsets
  if (value == NO_SCORE) { return; }

  int col = get_col(name);
  float weight = abs(col_weight[col]);  

  line_total += value * weight;
  line_total_coefs += weight;

  total_col[col] += value * current_line_coef;
  total_coef_col[col] += current_line_coef;
  if (! min_col[col] || value < min_col[col]) { min_col[col] = value; }

  if (debug) {
    QString str;
    str.sprintf("%6.3f", value);
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

    str.sprintf("=%6.3f [%6.2f] %20s", line_score, current_line_coef, QSTRING2PCHAR(line_label_str));
    update_dbg_line(str, 0, W_HEAD);

    logdebug_qstring(dbg_line);
  }
}

float ScoreCounter::get_score() {
  float t = 0, tc = 0;
  for(int col = 0; col < nb_cols; col++) {
    if (total_coef_col[col] > 0) {
      float value = total_col[col] / total_coef_col[col];
      t += value * abs(col_weight[col]);
    }
    tc += max(0, col_weight[col]);
  }

  if (debug) {
    dbg_line = QString();
    QString str;
    for(int col = 0; col < nb_cols; col++) {
      if (total_coef_col[col] > 0) {
	float value = total_col[col] / total_coef_col[col];
	str.sprintf("%6.3f", value);
	update_dbg_line(str, W_HEAD + W_COL * col, W_COL);
      }
    }
    update_dbg_line(QString("TOTAL:"), 0, W_HEAD);
    logdebug_qstring(dbg_line);
    logdebug("Total by line = %.2f / by columns = %.2f", total_score / total_coefs, t / tc);
  }

  // old & obsolete scoring strategy: return total_score / total_coefs;

  return t / tc;
}

float ScoreCounter::get_column_score(char *name) {
  int col = get_col(name);
  if (total_coef_col[col] > 0) {
    return total_col[col] / total_coef_col[col];
  } else {
    return 0;
  }
}

float ScoreCounter::get_column_min_score(char *name) {
  int col = get_col(name);
  return min_col[col];
}

void ScoreCounter::update_dbg_line(QString text, int col, int) {
  int l = dbg_line.length();
  if (l < col) {
    dbg_line.append(QString(" ").repeated(col - l));
    dbg_line.append(text);
    return;
  } 

  dbg_line.replace(col, text.length(), text);
}
