#ifndef SCORE_H
#define SCORE_H

#include <QString>
#include <QTextStream>

#define NO_SCORE (0.0)

#define MAX_COL 10


class ScoreCounter {
 public:
  ScoreCounter();
  void set_pow(float);
  void set_debug(bool);
  void start_line();
  void set_line_coef(float);
  void add_score(float, float, char *);
  void add_bonus(float, float, char *);
  void end_line();
  void set_cols(char**);
  float get_score();

  QTextStream line_label;

 private:
  float score_pow;
  bool debug;
  float current_line_coef;
  float total_score;
  float total_coefs;
  float line_total;
  float line_total_coefs;
  int line_count;
  QString line_label_str;

  char** col;
  QString dbg_line;

  int get_col(char *);
  void update_dbg_line(QString text, int col, int width);
};

#endif /* SCORE_H */