/*
 This file is part of OKBoard project

 Copyright (c) 2014, Eric Berenguier <eb@cpbm.eu>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

 ==============================================================================

 Word clustering generator for 3-gram language model


 This programs generates word clusters for an input text corpus.
 The clusters are following the Brown Clustering model [1]:

  P(Wi|Wi-1,Wi-2) = P(Wi|Ci) * p(Ci|Ci-1,Ci-2)

 The goal is to produce a model that reduces the sparseness problem with
 standard N-grams.

 The implementation is a top-down algorithm for performance reasons.
 We start with all words in one cluster, and we recursively split each
 cluster in two parts. Clusters are chosen to minimize perplexity.

 The implementation tries to follow guidelines from [2] (I know it has nothing
 to do with asian languages, but that's the best description I've found)

 You can get less clusters than expected if some cluster size drops to 1.

 Clusters are not balanced and we use a fixed depth approach.

 Existing implementations [3], [4] have not been reused because because they
 are incredibly slow (it was probably quicker to re-implement from scratch
 than just waiting for these to complete).


 Performance: a 15M words corpus (11.5 millions distinct N-grams) restricted
 to 30k words (English) takes less than 1.5 hours to produce 1k clusters (2^10)
 on a core-i7 2.8GHz and requires ~3Gb RAM.


 References:

 [1] Peter F. Brown; Peter V. deSouza; Robert L. Mercer; Vincent J. Della Pietra; Jenifer C. Lai (1992). "Class-based n-gram models of natural language"
     http://aclweb.org/anthology/J/J92/J92-4003.pdf
     See also: https://en.wikipedia.org/wiki/Brown_clustering

 [2] Jianfeng Gao Joshua T. Goodman Jiangbo Miao 1 "The Use of Clustering Techniques for Language Modeling – Application to Asian Languages""
     http://research.microsoft.com/en-us/um/people/jfgao/paper/clclp01-1.pdf

 [3] https://github.com/percyliang/brown-cluster

 [4] https://github.com/mheilman/tan-clustering.git
*/

#include <QSet>
#include <QHash>
#include <QString>
#include <QTextStream>
#include <QFile>

#include <assert.h>

#include <iostream>
#include <cmath>

using namespace std;

#include <unistd.h>
#include <stdlib.h>

#define EPS (1E-8)
#define MAX_ERR (1E-8)

class Gram; // forward decl
class ClusterGram;
class Word;
class Cluster;

class Word { // each instance is a word in the learning corpus
public:
  QString name; // the word itself
  Cluster* cluster; // this word belongs to only one cluster (hard clustering)
  QSet<Gram*> grams; // word n-grams using this word
  int count; // number of occurences in learning corpus

  Word(QString name, Cluster *cluster) {
    this -> name = name;
    this -> cluster = cluster;
    count = 0;
  };
};

class Cluster { // word clusters
public:
  int id; // id for quick retrieval

  QString name; // for display
  int word_count; // number of words in this class
  int count; // total number of words in this class (including repetitions)
  QSet<Word*> words; // all words in this cluster
  bool special; // tags (#START / #NA / #TOTAL), not words

  Cluster(QString name, int id, bool special = false) {
    this -> id = id;
    this -> name = name;
    this -> special = special;
    count = word_count = 0;
  };
};

class Gram { // standard n-grams (n <= 3)
public:
  int id; // id for quick retrieval
  Word* w1;
  Word* w2;
  Word* w3; // actual word (w1 & w2 are context information)
  int count; // occurences
  Gram* denominator; // total for parent context
  ClusterGram* cluster_gram;

  ClusterGram* new_cluster_gram;

  // linked list stuff for temporary selection
  int select_id;
  Gram* next_gram;

  Gram(int id, Word* w1, Word* w2, Word *w3) {
    this -> w1 = w1;
    this -> w2 = w2;
    this -> w3 = w3;
    this -> id = id;
    count = 0;
    denominator = NULL;
    select_id = -1;
  }
};


class ClusterGram { // just a lame name for n-grams with clusters
public:
  int id; // id for quick retrieval
  double partial_perplexity_log; // count * log(P(Ci|Ci-1, Ci-2)) -> includes class cardinality
  Cluster* c1;
  Cluster* c2;
  Cluster* c3; // actual cluster (c1 & c2 are context information)
  ClusterGram* denominator; // total for parent context
  int count; // occurences
  ClusterGram* first_numerator;
  ClusterGram* next_numerator;

  int new_count;
  double new_partial_perplexity_log;

  // linked list stuff for temporary selection
  int select_id;
  ClusterGram* next_clustergram;
  ClusterGram* next_denominator;
  bool bulk_denominator;

  ClusterGram(int id, Cluster *c1, Cluster *c2, Cluster *c3) {
    this -> id = id;
    this -> c1 = c1;
    this -> c2 = c2;
    this -> c3 = c3;
    count = 0;
    partial_perplexity_log = 0;
    denominator = NULL;
    select_id = -1;

    first_numerator = next_numerator = NULL;
  }
};

#define FAST_KEY

#ifdef FAST_KEY
typedef quint64 ClusterKey; // way faster but limited to ~2^21 clusters (probably large enough)
#else

class ClusterKey {
public:
  Cluster *c1;
  Cluster *c2;
  Cluster *c3;
  ClusterKey(Cluster *c1, Cluster *c2, Cluster *c3) : c1(c1), c2(c2), c3(c3) {};

  bool operator==(const ClusterKey & other) const {
    return (this -> c1 == other.c1) && (this -> c2 == other.c2) && (this -> c3 == other.c3);
  };
};

inline uint qHash(const ClusterKey &ck) {
  return qHash(ck.c3->id) ^ qHash(ck.c2->id) ^ qHash(ck.c1->id);
}

#endif /* FAST_KEY */

typedef enum MoveType
{
  TEST_ONLY = 0,
  MOVE_IF_BETTER = 1,
  ALWAYS_MOVE = 2
} MoveType;


// comparator for sorting word by count
bool lowerWordCount(Word* w1, Word* w2) {
  return w1 -> count > w2 -> count; // reverse sort -> greater count first
}

bool lowerClusterCount(Cluster* c1, Cluster* c2) {
  return c1 -> count > c2 -> count; // reverse sort -> greater count first
}

#define DBG( ... ) { if (debug) { qDebug(__VA_ARGS__); } }
#define DBG2( ... ) { if (debug >= 2) { qDebug(__VA_ARGS__); } }

class Clustering {
private:
  QHash<QString, Word*> m_words;
  QHash<QString, Cluster*> m_clusters;
  QHash<ClusterKey, ClusterGram*> m_clustergrams;
  QList<Gram*> m_grams;
  QList<Gram*> m_test_grams;
  QSet<Cluster*> m_dead_clusters;

  double perplexity_log; // sum of "log(count * P(Ci|Ci-1,Ci-2))" and "log(count * P(wi|Ci))" terms
  double perplexity; // perplexity = exp(- perplexity_log / total_count)
  int total_count; /* value of "N" in all papers */

  // special tags exist as words & clusters
  Word *w_total, *w_na, *w_start;
  Cluster *c_total, *c_na, *c_start;

  int current_gram_id;
  int current_cluster_id;
  int current_clustergram_id;

  QList<ClusterGram *> get_all_cluster_grams();
  ClusterGram* find_cluster_gram(Cluster *c1, Cluster *c2, Cluster *c3);
  void free_cluster_gram(ClusterGram *cgram);
  void load_ngrams(QTextStream &stream,  QList<Gram*> &grams, Cluster* cluster,
		   Cluster* notfound_cluster, bool update_word_counts = false);

  Cluster *test_cluster;

  /* temporary sets simulated with linked lists */
  inline void select_gram(Gram *);
  inline void select_clustergram(ClusterGram *);
  inline void select_denominator(ClusterGram *);
  int select_id;
  ClusterGram* select_clustergram_head;
  Gram* select_gram_head;
  ClusterGram* select_denominator_head;

public:
  Clustering();
  bool move(Word *word, Cluster *to, MoveType mode = ALWAYS_MOVE);
  QPair<Cluster*, Cluster*> split(Cluster *cluster);
  void optim(QSet<Cluster*> &optim_clusters);
  Cluster* load(QTextStream &stream);
  void load_test_ngrams(QTextStream &stream);
  void clear_cluster_grams();
  void eval_full();
  void dump(bool show_words = false);
  void print_stats();
  void perplexity_check();

  void save_clusters(QTextStream &stream);

  void reserve(int);

  void garbage_collect();

  bool test_mode;
  int debug;
};

Clustering::Clustering() {
  current_gram_id = current_cluster_id = current_clustergram_id = 0;
  debug = 0;
  test_mode = false;
  total_count = 0;
  perplexity_log = perplexity = 0;
  test_cluster = NULL;
  select_id = 1;
}

void Clustering::print_stats() {
  qDebug("Stats: words=%d, clusters=%d, n-grams=%d, cluster n-grams=%d, test n-grams=%d --> perplexity=%.3f",
	 m_words.size(), m_clusters.size(), m_grams.size(), m_clustergrams.size(), m_test_grams.size(), perplexity);
}

void Clustering::dump(bool show_words) {
  if (show_words) {
    qDebug(" ");
    qDebug(" === Words ===");
    QList<Word *> words = m_words.values();
    qSort(words.begin(), words.end(), lowerWordCount);
    foreach(Word * const word, words) {
      if (word -> cluster -> special) { continue; }
      QStringList gramtxt;
      foreach(Gram* const gram, word -> grams) {
	QString str;
	str.sprintf("[%s:%s:%s]",
		    qPrintable(gram -> w1 -> name), qPrintable(gram -> w2 -> name), qPrintable(gram -> w3 -> name));
	gramtxt.append(str);
      }
      qDebug("%s (%s) %d [%d grams]: %s",
	     qPrintable(word -> name), qPrintable(word -> cluster -> name), word -> count, word -> grams.size(),
	     qPrintable(gramtxt.join(" ")));
    }
    qDebug(" ");
    qDebug(" === Words N-grams ===");
    foreach(Gram * const gram, m_grams) {
      if (gram -> w3 == w_total) { continue; /* denominator */ }
      qDebug("[%s;%s;%s] %d/%d %.f",
	     qPrintable(gram -> w1 -> name),
	     qPrintable(gram -> w2 -> name),
	     qPrintable(gram -> w3 -> name),
	     gram -> count, gram -> denominator -> count,
	     1.0 * gram -> count / gram -> denominator -> count);
    }
  }
  qDebug(" ");
  qDebug(" === Clusters ===");
  QList<Cluster *> clusters = m_clusters.values();
  qSort(clusters.begin(), clusters.end(), lowerClusterCount);
  foreach(Cluster * const cluster, clusters) {
    if (cluster -> special) { continue; }
    QStringList words_txt;
    foreach(const Word *word, cluster -> words) {
      words_txt.append(word -> name);
    }
    QString all_words = words_txt.join(QString(" "));

    qDebug("%s [words: %d, count: %d] %s",
	   qPrintable(cluster -> name), cluster -> word_count, cluster -> count,
	   qPrintable(all_words));
  }
  qDebug(" ");
  qDebug(" === Clusters N-grams ===");
  QList<ClusterGram *> all_cgrams = get_all_cluster_grams();
  foreach(ClusterGram * const cgram, all_cgrams) {
    if (cgram -> c3 == c_total) { continue; /* denominator */ }

    if (! cgram -> denominator -> count) { continue; }

    qDebug("[%s;%s;%s] %d/%d %.f",
	   qPrintable(cgram -> c1 -> name),
	   qPrintable(cgram -> c2 -> name),
	   qPrintable(cgram -> c3 -> name),
	   cgram -> count, cgram -> denominator -> count,
	   1.0 * cgram -> count / cgram -> denominator -> count);
  }
  qDebug(" ");
}

void Clustering::load_test_ngrams(QTextStream &stream) {
  assert(! test_cluster);

  // this cluster stores words which are in the test corpus but unkown in the learning corpus
  test_cluster = new Cluster("TEST", ++ current_cluster_id, true);

  load_ngrams(stream, m_test_grams, test_cluster, (Cluster*) NULL, false);
}

void Clustering::load_ngrams(QTextStream &stream,  QList<Gram*> &in_grams, Cluster* cluster,
			     Cluster* notfound_cluster, bool update_word_counts) {
  // load any n-gram file (format: count;word1;word2;word3) from stream
  // store them in given n-gram hash, and if needed create new words (with a default cluster)
  assert(in_grams.empty());

  QHash<QString, Gram*> grams;

  QString line;
  while(1) {
    line = stream.readLine();
    if (line.isNull()) { break; }

    if (line == "TEST") {
      // if one line contains just "TEST", the remaining of the lines are used as the test corpus (also described as n-grams)
      load_test_ngrams(stream);
      break; // exit while()
    }


    QStringList columns = line.split(";");

    if (columns.size() != 4) {
      throw QString("Bad CSV 3-grams input data: " + line);
    }

    int count = columns[0].toInt();

    QString gram_key("");

    Word* wi[3];
    for(int i = 1; i <= 3; i++) {
      QString name = columns[i];
      Word *word;
      if (m_words.contains(name)) {
	word = m_words[name];
      } else {
	m_words[name] = word = new Word(name, cluster);
	word -> cluster = cluster;
	cluster -> word_count += 1;
      }
      wi[i - 1] = word;

      if (i > 1) { gram_key.append(':'); }
      gram_key.append(name);
    }

    assert(! grams.contains(gram_key)); // file should not contain duplicate n-grams

    if (wi[0] == w_na && wi[1] != w_start) { continue; } // only use 3-grams

    Gram* gram = grams[gram_key] = new Gram(++ current_gram_id, wi[0], wi[1], wi[2]);

    if (wi[2] != w_total) {
      Word *word = wi[2];
      if (update_word_counts) {
	// we don't keep counts for test corpus (we are only interested in n-grams)
	word -> count += count;
	total_count += count;
      }
      cluster -> count += count;
      cluster -> words.insert(word);

      // words only have a link to numerator n-grams
      for(int i = 0; i < 3; i++) {
	wi[i] -> grams.insert(gram);
      }
    }

    gram -> count = count;
    assert(count > 0);
  }

  // find denominators and recalculate their count: this enables us to support
  // "incomplete" n-grams data (e.g. truncated n-gram list)
  foreach(Gram * const gram, grams) {
    if (gram -> w3 == w_total) { gram -> count = 0; }
  }
  foreach(Gram * const gram, grams) {
    if (gram -> w3 == w_total) { continue; }
    QString new_key = gram -> w1 -> name + ':' + gram -> w2 -> name + ':' + w_total -> name;
    if (! grams.contains(new_key)) {
      // re-create missing denominator n-gram
      grams[new_key] = new Gram(++ current_gram_id, gram -> w1, gram -> w2, w_total);
    }
    gram -> denominator = grams[new_key];
    gram -> denominator -> count += gram -> count;
  }

  // move not found words to specific cluster
  // (this is caused by words found in n-gram context part, but no n-gram ends with these words)
  if (notfound_cluster) {
    foreach(Word * const word, m_words) {
      if (! word -> count && ! word -> cluster -> special) {
	cluster -> word_count --;
	cluster -> words.remove(word);

	word -> cluster = notfound_cluster;
	notfound_cluster -> word_count ++;
	// don't add this word to "notfound cluster" word list as we won't use it anymore
      }
    }
  }

  in_grams.append(grams.values());
}


Cluster* Clustering::load(QTextStream &stream) {
  // load learning corpus as n-gram file
  assert(current_gram_id == 0); // this object is not reuseable :-)

  m_clusters["#TOTAL"] = c_total = new Cluster("#TOTAL", 1, true);
  m_clusters["#NA" ] = c_na = new Cluster("#NA", 2, true);
  m_clusters["#START"] = c_start = new Cluster("#START", 3, true);

  Cluster *notfound_cluster = m_clusters["#NOTFOUND"] = new Cluster("#NOTFOUND", 4, false);

  m_words["#TOTAL"] = w_total = new Word("#TOTAL", c_total);
  m_words["#NA"] = w_na = new Word("#NA", c_na);
  m_words["#START"] = w_start = new Word("#START", c_start);
  current_gram_id = current_clustergram_id = 1;
  current_cluster_id = 5;
  total_count = 0;

  Cluster *default_cluster = m_clusters["C"] = new Cluster(QString("C"), ++ current_cluster_id);

  load_ngrams(stream, m_grams, default_cluster, notfound_cluster, true);

  eval_full();

  return default_cluster;
}

void Clustering::save_clusters(QTextStream &stream) {
  QList<Cluster *> clusters = m_clusters.values();
  qSort(clusters.begin(), clusters.end(), lowerClusterCount);
  foreach(Cluster * const cluster, clusters) {
    if (cluster -> special) { continue; }
    QList<Word*> words = cluster -> words.toList();
    qSort(words.begin(), words.end(), lowerWordCount);
    foreach(Word * const word,words) {
      stream << cluster -> name << ";" << word -> name << ";" << cluster -> count << ";" << word -> count << endl;
    }
    stream << endl;
  }
}

inline void Clustering::select_gram(Gram *gram) {
  if (gram -> select_id == select_id) { return; }
  gram -> select_id = select_id;
  gram -> next_gram = select_gram_head;
  select_gram_head = gram;
}

inline void Clustering::select_clustergram(ClusterGram *cgram) {
  if (cgram -> select_id == select_id) { return; }
  cgram -> select_id = select_id;
  cgram -> next_clustergram = select_clustergram_head;
  cgram -> bulk_denominator = false;
  cgram -> new_count = cgram -> count;
  select_clustergram_head = cgram;
}

inline void Clustering::select_denominator(ClusterGram *cgram) {
  assert(cgram -> select_id == select_id);
  if (cgram -> bulk_denominator) { return; }
  cgram -> bulk_denominator = true;
  cgram -> next_denominator = select_denominator_head;
  select_denominator_head = cgram;
}

bool Clustering::move(Word *word, Cluster *to, MoveType mode) {
  /* Evaluate perplexity if we move the word to another cluster
     Behavior depends on mode:
     - TEST_ONLY: return true if the move would reduce perplexity
     - MOVE_IF_BETTER: return true and actually commit the move if it reduces perplexity
     - ALWAYS_MOVE: and always return true

     This is the part that should be optimized for performance
  */

  DBG(" [move] === %s [%s -> %s] mode=%d",
      qPrintable(word -> name), qPrintable(word -> cluster -> name), qPrintable(to -> name), mode);

  DBG(" [move] before: total Plog / count = %f / %d --> perplexity = %f",
      - perplexity_log, total_count, perplexity);

  if (to == word -> cluster) { return true; /* nothing to do */ }
  Cluster *from = word -> cluster;

  // simulate sets of grams / clustergrams with linked lists (profiling showed that QSets were too costly)
  select_id ++;
  select_clustergram_head = NULL; // linked-list of clustergrams (numerators & denominators) which need to have their partial perplexity evaluated again
  select_gram_head = NULL; // linked-list of word n-grams using moved word
  select_denominator_head = NULL;  // linked-list of denominators for which perplexity change has to be bulk-evaluated

  double relative_perplexity_log = 0; // accumulates all changes to log(perplexity)

  /* step 1 : update P(wi|Ci) terms because source & destination clusters counts change */

  // bulk update for whole source & destination clusters
  double value = 0;
  // update terms for source cluster (except word to be moved)
  if (from -> count - word -> count) {
    value += (from -> count - word -> count) * log(from -> count);
    value -= (from -> count - word -> count) * log(from -> count - word -> count);
  }

  // update existing terms for destination cluster (without word to be moved)
  if (to -> count) {
    value += to -> count * log(to -> count);
    value -= to -> count * log(to -> count + word -> count);
  }

  // update term for current word
  value += word -> count * ( log(from -> count) - log(to -> count + word -> count) );

  relative_perplexity_log += value;
  DBG(" [move] Plog(wi|ci) = %s (%s->%s) %f",
      qPrintable(word -> name), qPrintable(from -> name), qPrintable(to -> name), value);

  /* step 2 : identify all cluster-grams impacted and compute updated counts */

  foreach(Gram* const gram, word->grams) {
    // include numerators n-grams but no denominator ones (n-grams with #TOTAL)
    if (gram -> w3 == w_total) { continue; }

    assert(word == gram -> w1 || word == gram -> w2 || word == gram -> w3);
    Cluster *c1 = gram -> w1 -> cluster;
    Cluster *c2 = gram -> w2 -> cluster;
    Cluster *c3 = gram -> w3 -> cluster;
    int count = gram -> count;

    if (word == gram -> w1) { c1 = to; }
    if (word == gram -> w2) { c2 = to; }
    if (word == gram -> w3) { c3 = to; }

    // we are moving from one cluster-gram to another because of the class change
    ClusterGram* current_cluster_gram = gram->cluster_gram;
    ClusterGram* new_cluster_gram = find_cluster_gram(c1, c2, c3);

    assert(current_cluster_gram != new_cluster_gram);

    gram -> new_cluster_gram = new_cluster_gram; // new_* values will be copied to actual ones if we commit the class change

    // mark grams & clustergrams that require partial perplexity evaluation
    select_gram(gram);
    select_clustergram(current_cluster_gram);
    select_clustergram(new_cluster_gram);

    // in case the denominator does not change but is used in below calculation
    select_clustergram(current_cluster_gram -> denominator);
    select_clustergram(new_cluster_gram -> denominator);

    // add all numerators too (because their value will have to be updated (but they will be bulk-evaluated)
    if (word == gram -> w1 || word == gram -> w2) {
      select_denominator(current_cluster_gram -> denominator);
      select_denominator(new_cluster_gram -> denominator);
    }

    // we'll have many more comparisons than actual moves, so we optimize for
    // comparisons only: we don't change the internal state of cluster_grams
    // (new_* are temporary items only used in this method)
    current_cluster_gram -> new_count -= count;
    current_cluster_gram -> denominator -> new_count -= count;
    new_cluster_gram -> new_count += count;
    new_cluster_gram -> denominator -> new_count += count;
  }

  /* step 3 : evaluate perplexity relative change */

  for(ClusterGram* denominator = select_denominator_head; denominator; denominator = denominator -> next_denominator) {
    /* This is the main optimization (which roughly correspond to the one
       described in clcl01-1.pdf). In case of denominator change we
       bulk-evaluate perplexity for all numerators

       Why do we do this? total numerators count may be huge
       (same order of magnitude than n-grams count)

       perplexity_log = sum(ni -> count * (log(ni -> count / di -> count)))

       If denominator count changes:

       relative_perplexity_log = sum(ni -> count) * (- log(di -> count) + log(di -> new_count))
       relative_perplexity_log = di -> count * (- log(di -> count) + log(di -> new_count))

       Denominator count is the sum of all numerators counts (by construction)

       This calculation assumes numerators are not modified (or it'll be adjusted below)
    */

    double value = 0;
    if (denominator -> count) { value += (denominator -> count) * log(denominator -> count); }
    if (denominator -> new_count) { value -= (denominator -> count) * log(denominator -> new_count); } // not a typo! (we only compensate for denominators)

    relative_perplexity_log += value;
    if (value) {
      DBG(" [move] bulk denominator Plog(%s, %s, %s) = %f (count %d->%d)",
	  qPrintable(denominator -> c1 -> name), qPrintable(denominator -> c2 -> name), qPrintable(denominator -> c3 -> name),
	  value,
	  denominator -> count, denominator -> new_count);
    }
  }

  for(ClusterGram* cgram = select_clustergram_head; cgram; cgram = cgram -> next_clustergram) {
    if (! cgram -> denominator) { continue; }
    // P(Ci|Ci-1, Ci-2)

    ClusterGram *denominator = cgram -> denominator;

    if (denominator -> bulk_denominator) {
      // perplexity change for this numerator has been bulk-evaluated above (because of denominator change)
      // we have to revert it for this numerator (because the calculation below will evaluate the full change)

      double value = 0;
      if (denominator -> count) { value -= (cgram -> count) * log(denominator -> count); }
      if (denominator -> new_count) { value += (cgram -> count) * log(denominator -> new_count); }
      relative_perplexity_log += value;

      if (value) {
	DBG(" [move] adjustment for bulk denominator (%s, %s, %s)->(%s, %s, %s) Plog = %f",
	    qPrintable(cgram -> c1 -> name), qPrintable(cgram -> c2 -> name), qPrintable(cgram -> c3 -> name),
	    qPrintable(denominator -> c1 -> name), qPrintable(denominator -> c2 -> name), qPrintable(denominator -> c3 -> name),
	    value);
      }
    }

    if (cgram -> new_count) {
      cgram -> new_partial_perplexity_log = cgram -> new_count * log(1.0 * cgram -> new_count / cgram -> denominator -> new_count);
    } else {
      cgram -> new_partial_perplexity_log = 0; // n-gram is no more used
    }
    relative_perplexity_log += cgram -> new_partial_perplexity_log - cgram -> partial_perplexity_log;

    DBG(" [move] Plog(%s, %s, %s) = %f --> %f (%d/%d)",
	qPrintable(cgram -> c1 -> name), qPrintable(cgram -> c2 -> name), qPrintable(cgram -> c3 -> name),
	cgram -> partial_perplexity_log,
	cgram -> new_partial_perplexity_log,
	cgram -> new_count, cgram -> denominator -> new_count);
  }

  // better if perplexity increase <=> log(perplexity) decrease
  // (due to -1/N coefficient)
  bool commit = true;
  bool return_value = true;
  if (mode == TEST_ONLY) { commit = false; return_value = (relative_perplexity_log > EPS); }
  if (mode == MOVE_IF_BETTER && relative_perplexity_log < EPS) { commit = return_value = false; }

  if (! commit) {
    goto get_out;
  }

  /* step 4 : commit changes */

  for(ClusterGram* cgram = select_clustergram_head; cgram; cgram = cgram -> next_clustergram) {
    cgram -> count = cgram -> new_count;
    cgram -> partial_perplexity_log = cgram -> new_partial_perplexity_log;
  }
  for(Gram* gram = select_gram_head; gram; gram = gram -> next_gram) {
    gram -> cluster_gram = gram -> new_cluster_gram;
  }

  /*
    our global perplexity is now right, but we did not update individual
    partial perplexity for cluster-grams which denominator has been updated
    (because they have been bulk evaluated above). So let's do it now!
    We could skip this, because we are only interested in relative changes to
    perplexity, but the absolute value would be off and that would prevent
    our consistency checks
  */
  for(ClusterGram* denominator = select_denominator_head; denominator; denominator = denominator -> next_denominator) {
    for(ClusterGram* cgram = denominator -> first_numerator; cgram; cgram = cgram -> next_numerator) {
      if (cgram -> select_id == select_id) { continue; /* we have already updated this one */ }
      double value = cgram -> count?(cgram -> count * log(1.0 * cgram -> count / denominator -> count)):0;
      cgram -> partial_perplexity_log = value;
    }
  }

  perplexity_log += relative_perplexity_log;
  perplexity = exp(-1.0 * perplexity_log / total_count);

  DBG(" [move] after: total Plog / count = %f / %d --> perplexity = %f",
      - perplexity_log, total_count, perplexity);

  DBG("Moving %s [%s] -> [%s] perplexity=%f",
      qPrintable(word -> name), qPrintable(word -> cluster -> name), qPrintable(to -> name), perplexity);

  word -> cluster -> word_count --;
  word -> cluster -> count -= word -> count;
  word -> cluster -> words.remove(word);

  word -> cluster = to;
  to -> word_count ++;
  to -> count += word -> count;
  to -> words.insert(word);

  if (test_mode) { perplexity_check(); /* slow */ }

  if (::isnan(perplexity)) { throw QString("Perplexity is nan !"); }

 get_out:
  // purge unused cluster N-grams
  QSet<ClusterGram*> denominators_to_clean;
  for(ClusterGram* cgram = select_clustergram_head; cgram; cgram = cgram -> next_clustergram) {
    if (cgram -> count == 0 && cgram -> denominator) {
      denominators_to_clean.insert(cgram -> denominator);
    }
  }
  foreach(ClusterGram* const denominator, denominators_to_clean) {
    ClusterGram ** last_ptr = &(denominator -> first_numerator);

    for(ClusterGram* cgram = denominator -> first_numerator; cgram; cgram = cgram -> next_numerator) {
      if (cgram -> count == 0) {
	// remove cluster-gram from numérators linked list for this denominator
	// the clustergram will be removed just below
      } else {
	*last_ptr = cgram;
	last_ptr = &(cgram -> next_numerator);
      }
    }
    *last_ptr = NULL;
  }

  ClusterGram* cgram = select_clustergram_head;
  while (cgram) {
    ClusterGram* next_cgram = cgram -> next_clustergram;
    if (! cgram -> count) {
      free_cluster_gram(cgram);
    }
    cgram = next_cgram;
  }

  return return_value;
}

void Clustering::optim(QSet<Cluster*> &optim_clusters) {
  // try to optimize perplexity by moving words between given clusters
  // in case of empty set, all the words are optimized (may be slow)
  QSet<Word *> words;
  if (optim_clusters.empty()) {
    qDebug("==> Optimize all ...");
    words = QSet<Word *>::fromList(m_words.values());
  } else {
    QStringList clusternames;
    foreach(Cluster * const cluster, optim_clusters) {
      words += cluster -> words;
      clusternames.append(cluster -> name);
    }
    qDebug("==> Optimize clusters %s", qPrintable(clusternames.join(" + ")));
  }
  int it = 0;
  bool done = false;
  while(! done) {
    it ++;

    qDebug("Optimize iteration #%d", it);

    done = true;
    int count = 0;
    foreach(Word * const word, words) {
      if (! word -> count) { continue; } // incomplete input n-gram model may lead to 0-count words: let's just ignore them

      foreach(Cluster * const cluster, optim_clusters) {
	if (cluster == word -> cluster) { continue; }
	if (word -> cluster -> word_count == 1) { continue; } /* empty clusters are not handled */
	if (move(word, cluster, MOVE_IF_BETTER)) {
	  count ++;
	  if (count >= 2) { done = false; }
	}
      }
    }
    if (count) { qDebug("%d word%s moved - Perplexity=%.3f", count, (count>1)?"s":"", perplexity); }
  }

}

QPair<Cluster*, Cluster*> Clustering::split(Cluster *cluster) {
  // this function splits a cluster into 2 new clusters

  if (cluster -> word_count <= 1) { return QPair<Cluster*, Cluster*>(cluster, 0); /* don't cut words in half */ }

  Cluster *c1 = new Cluster(cluster -> name + "0", ++ current_cluster_id);
  Cluster *c2 = new Cluster(cluster -> name + "1", ++ current_cluster_id);

  qDebug("==> split [%s] -> [%s] + [%s]", qPrintable(cluster -> name), qPrintable(c1 -> name), qPrintable(c2 -> name));

  QList<Word *> words = cluster -> words.toList();
  qSort(words.begin(), words.end(), lowerWordCount);

  QSet<Cluster *> optim_set;
  optim_set.insert(c1);
  optim_set.insert(c2);
  m_clusters[c1 -> name] = c1;
  m_clusters[c2 -> name] = c2;

  int n0 = 0;
  int n = 3;
  bool done = false;

  // That's what i understand from the Buckshot description in clcl01-1.pdf
  // I'm not sure about the description in the original paper (scatter.pdf)
  while (! done) {
    if (n > words.size()) { n = words.size(); done = true; }
    qDebug("split(%d/%d)", n, words.size());

    for(int i = n0; i < n; i ++) {
      move(words[i], (i%2)?c1:c2);
    }
    optim(optim_set);

    n0 = n;
    n = int(n * 1.4);
  }

  // remove original cluster
  m_clusters.remove(cluster -> name);

  // note : unused cluster-grams will be disposed later to avoid memory leak (cf. garbage_collect function)
  // ("delete cluster" does not work here because we still have dead clustergrams pointing to it)
  m_dead_clusters.insert(cluster);
  cluster -> name.append("!");
  cluster -> words.empty(); // but we do not require the word list anymore

  qDebug("Split OK: %s -> %s (%d words) + %s (%d words) - Perplexity: %.3f",
	 qPrintable(cluster -> name),
	 qPrintable(c1 -> name), c1 -> words.size(),
	 qPrintable(c2 -> name), c2 -> words.size(),
	 perplexity);

#ifdef FAST_KEY
  if (current_cluster_id >= (2 << 21) - 1) { throw QString("Key overflow"); }
#endif

  return QPair<Cluster*, Cluster*>(c1, c2);
}

#ifdef FAST_KEY
#define BUILD_KEY(k1, k2, k3) ((quint64)((k1) -> id) + ((quint64)((k2) -> id) << 21) + ((quint64)((k3) -> id) << 42));
#else
#define BUILD_KEY(k1, k2, k3) (ClusterKey ck(k1, k2, k3))
#endif

void Clustering::free_cluster_gram(ClusterGram *cgram) {
  assert(! cgram -> count);

  ClusterKey ck = BUILD_KEY(cgram -> c1, cgram -> c2, cgram -> c3);

  m_clustergrams.remove(ck);
  delete cgram;
}

ClusterGram* Clustering::find_cluster_gram(Cluster *c1, Cluster *c2, Cluster *c3) {
  // find (or create if needed) a cluster-gram for given clusters
  ClusterGram *clustergram;

  ClusterKey ck = BUILD_KEY(c1, c2, c3);

  if (m_clustergrams.contains(ck)) { return m_clustergrams[ck]; }

  clustergram = new ClusterGram(++ current_clustergram_id, c1, c2, c3);
  m_clustergrams[ck] = clustergram;

  if (c3 == c_total) {
    // we are a denominator n-gram
    return clustergram;
  }

  clustergram -> denominator = find_cluster_gram(c1, c2, c_total);
  clustergram -> next_numerator = clustergram -> denominator -> first_numerator;
  clustergram -> denominator -> first_numerator = clustergram;

  return clustergram;
}

QList<ClusterGram *> Clustering::get_all_cluster_grams() {
  return m_clustergrams.values();
}


void Clustering::clear_cluster_grams() {
  // remove all cluster-grams (even unused ones)
  QList<ClusterGram *> all_cgrams = get_all_cluster_grams();

  foreach(ClusterGram * const cgram, all_cgrams) {
    delete cgram;
  }
  m_clustergrams.clear();
  current_clustergram_id = 1;
}

void Clustering::eval_full() {
  /* this function recalculate perplexity "from scratch"
     it's a bit redundant, because we could evaluate perplexity only with
     the "move" method while splitting clusters, but this is used for
     testing.

     it is not meant to be efficient at all.
  */

  clear_cluster_grams();
  QHash<int, ClusterGram *> dejavu_clustergram;
  perplexity_log = 0;

  /* evaluate P(wi|Ci) part */
  foreach(Word* const word, m_words) {
    if (! word -> count) { continue; } // incomplete input n-gram model may lead to 0-count words: let's just ignore them
    if (word -> cluster -> special) { continue; }
    double value = word -> count * log(1.0 * word -> count / word -> cluster -> count);
    DBG(" [eval_full] Plog(w:%s|c:%s) = %f (%d/%d)",
	qPrintable(word -> name), qPrintable(word -> cluster -> name),
	value, word -> count, word -> cluster -> count);
    perplexity_log += value;
  }

  /* identify & create all cluster-grams*/
  foreach(Gram* const gram, m_grams) {
    // includes numerators & denominators (n-grams with #TOTAL)
    Cluster *c1 = gram -> w1 -> cluster;
    Cluster *c2 = gram -> w2 -> cluster;
    Cluster *c3 = gram -> w3 -> cluster;
    int count = gram -> count;

    // find cluster-gram
    ClusterGram* cluster_gram = find_cluster_gram(c1, c2, c3);
    gram -> cluster_gram = cluster_gram;

    cluster_gram -> count += count;

    if (! dejavu_clustergram.contains(cluster_gram -> id)) {
      dejavu_clustergram[cluster_gram -> id] = cluster_gram;
    }
  }

  /* evaluate perplexity */
  foreach(ClusterGram * const cgram, dejavu_clustergram) {
    if (! cgram -> denominator) { continue; }
    // P(Ci|Ci-1, Ci-2)
    double value = cgram -> count * log(1.0 * cgram -> count / cgram -> denominator -> count);
    DBG(" [eval_full] Plog(%s, %s, %s) = %f (%d/%d)",
	  qPrintable(cgram -> c1 -> name), qPrintable(cgram -> c2 -> name), qPrintable(cgram -> c3 -> name),
	  value, cgram -> count, cgram -> denominator -> count);

    cgram -> partial_perplexity_log = value;
    perplexity_log += cgram -> partial_perplexity_log;
  }

  perplexity = exp(-1.0 * perplexity_log / total_count);

  DBG(" [eval_full] total Plog / count = %f / %d --> perplexity = %f",
      - perplexity_log, total_count, perplexity);
}

void Clustering::perplexity_check() {
  if (::isnan(perplexity)) { throw QString("Perplexity is nan !"); }

  double save_perplexity = perplexity;
  eval_full();

  double a = fabsl(perplexity);
  double b = fabsl(save_perplexity);
  double max_abs = (a > b)?a:b;

  if (fabsl(perplexity - save_perplexity) / max_abs > MAX_ERR) {
    QString err;
    err.sprintf("Perplexity mismatch : incremental=%f / full=%f", save_perplexity, perplexity);
    throw err;
  }

  if (::isnan(perplexity)) { throw QString("Perplexity is nan !"); }
}

void Clustering::reserve(int n) {
  m_clustergrams.reserve(n);
}

void Clustering::garbage_collect() {
  // Run some sanity checks
  foreach(Word * const word, m_words) {
      if (word -> cluster -> special) { continue; }
      if (m_dead_clusters.contains(word -> cluster)) {
	QString err;
	err.sprintf("Word linked to unused cluster: %s => %s (count: %d)",
		    qPrintable(word -> name), qPrintable(word -> cluster -> name), word -> count);
	throw err;
      }
  }

  foreach(ClusterGram * const cg, get_all_cluster_grams()) {
    if (m_dead_clusters.contains(cg -> c1) ||
	m_dead_clusters.contains(cg -> c2) ||
	m_dead_clusters.contains(cg -> c3)) {
      QString err;
      err.sprintf("Cluster-gram linked to unused cluster: [%s:%s:%s]",
		  qPrintable(cg -> c1 -> name), qPrintable(cg -> c2 -> name), qPrintable(cg -> c3 -> name));
      throw err;
    }
  }

  // delete unused clusters (because they have been split)
  qDebug("Garbage collecting %d unused clusters ...", m_dead_clusters.size());
  foreach(Cluster * const cluster, m_dead_clusters) {
    delete cluster;
  }
  m_dead_clusters.clear();
}

void usage(char* progname) {
  cout << "Usage:" << endl;
  cout << progname << " [<options>] [<learning corpus file>]" << endl;
  cout << "Learning and test corpora are read as ngram files (count;word1;word2;word)" << endl;
  cout << "If learning corpus file is not specified, it is read from standard input" << endl;
  cout << "Options:" << endl;
  cout << " -n <count> : number of cluster splits: we get at most 2^count clusters in the end" << endl;
  cout << "              (default value 10 -> ~1k clusters)" << endl;
  cout << " -o <file> : output cluster file" << endl;
  cout << " -D : full dump on each step (very costly)" << endl;
  cout << " -d : debug mode (detailed output)" << endl;
  cout << " -O : remove global optimization step at the end" << endl;
  cout << " -T : test mode (calculate perplexity with two different ways and fail" << endl;
  cout << "      in case of result mismatch)" << endl;
  exit(1);
}



int main(int argc, char* argv[]) {
  extern char *optarg;
  extern int optind;

  int depth = 10; // ~1000 clusters
  bool final_optim = true;
  char *test_file = NULL;
  char *cluster_out_file = NULL;
  bool full_dump = false;

  Clustering cl;

  int c;
  while ((c = getopt(argc, argv, "t:n:c:o:TdD")) != -1) {
    switch (c) {
    case 'n': depth = atoi(optarg); break;
    case 'O': final_optim = false; break;
    case 'T': cl.test_mode = true; break;
    case 't': test_file = optarg; break;
    case 'd': cl.debug ++; break;
    case 'D': full_dump = true; break;
    case 'o': cluster_out_file = optarg; break;
    default: usage(argv[0]); break;
    }
  }

  try {
    // load learning corpus
    QFile file;
    if (argc > optind) {
      file.setFileName(argv[optind]);
      file.open(QFile::ReadOnly);
    } else {
      file.open(stdin, QIODevice::ReadOnly);
    }
    QTextStream st(&file);

    Cluster *loaded = cl.load(st);
    file.close();

    QList<Cluster *> word_clusters;
    word_clusters.append(loaded);
    cl.eval_full();
    if (full_dump) { cl.dump(true); }
    cl.print_stats();

    // load test corpus
    if (test_file) {
      QFile tfile;
      tfile.setFileName(argv[optind]);
      tfile.open(QFile::ReadOnly);
      QTextStream st(&file);
      cl.load_test_ngrams(st);
      tfile.close();
    }

    cl.reserve(2<<depth);

    // split clusters
    for(int i = 0; i < depth; i ++) {
      qDebug("Depth: %d / %d", i + 1, depth);
      QList<Cluster *> new_clusters;
      foreach(Cluster * const cluster, word_clusters) {
	QPair<Cluster*, Cluster*> split = cl.split(cluster);
	new_clusters.append(split.first);
	if (split.second) { new_clusters.append(split.second); }
      }
      word_clusters = new_clusters;

      // force a full perplexity evaluation & remove leaked clusters / cluster-grams
      // (and we get a consistency check for free)
      cl.perplexity_check();
      cl.garbage_collect();

      if (full_dump) { cl.dump(); }
      cl.print_stats();
    }

    // final optimization
    if (final_optim) {
      QSet<Cluster *> qs;
      cl.optim(qs);
      if (full_dump) { cl.dump(); }
      cl.print_stats();
    }

    // save cluster file
    if (cluster_out_file) {
      qDebug("Saving cluster file %s ...", cluster_out_file);
      QFile cfile;
      cfile.setFileName(cluster_out_file);
      cfile.open(QFile::WriteOnly);
      QTextStream st(&cfile);
      cl.save_clusters(st);
      cfile.close();
    }

    // done
    if (full_dump) { cl.dump(true); }
    qDebug("OK");

  } catch(QString &err) {
    qDebug("Exception: %s", qPrintable(err));
    exit(1);
  }
}

