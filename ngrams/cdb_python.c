/* python module for persistent hashtable-like structure for words, n-grams and stuff
   this is just a dumb & boring implementation but it's way faster than sqlite
 */

#include <Python.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "phash.h"
#include "pack.h"

#define BUF 1024
static char buf[BUF];

static PyObject *CdbError;

static phash_t *h = NULL; /* loaded N-gram DB */

enum { type_standard = 0, type_ngram = 1, type_word = 2};

typedef struct gram {
  float count;
  float replace;
  int date;
} gram_t;

static int check_db() {
  if (!h) {
    PyErr_SetString(CdbError, "DB not initialized");
    return 1;
  } else if (h->error) {
    PyErr_SetString(CdbError, "DB error prevents further operation");
    return 1;
  }
  return 0;
}

static PyObject * set_check(phash_t *h, char* key, void *data, int length, int type) {
  if (ph_set(h, key, data, length, type)) {
    char tmp[255];
    snprintf(tmp, sizeof(tmp) - 1, "Set key failed: '%s' [len=%d, type=%d]", key, length, type);
    PyErr_SetString(CdbError, tmp);
    return NULL;
  } else {
    Py_RETURN_NONE;
  }
}


static PyObject *
cdb_load(PyObject *self, PyObject *args)
{
  char *filename;

  if (h) { ph_close(h); h = NULL; }

  if (!PyArg_ParseTuple(args, "s", &filename)) { return NULL; }

  h = ph_init(filename);

  if (!h) { PyErr_SetString(CdbError, "DB initialization failed"); return NULL; }

  Py_RETURN_NONE;
}

static PyObject *
cdb_clear(PyObject *self, PyObject *args)
{
  if (h) {
    ph_close(h);
    h = NULL;
  }

  Py_RETURN_NONE;
}

static PyObject *
cdb_save(PyObject *self, PyObject *args)
{
  if (h) {
    if (check_db()) { return NULL; }
    ph_save(h);
  }
  Py_RETURN_NONE;
}

static PyObject *
cdb_get_string(PyObject *self, PyObject *args)
{
  char *key, *value;
  if (!PyArg_ParseTuple(args, "s", &key)) { return NULL; }
  if (check_db()) { return NULL; }

  value = ph_get(h, key, NULL, NULL);

  if (! value) { Py_RETURN_NONE; }
  return Py_BuildValue("s", value);
}

static PyObject *
cdb_set_string(PyObject *self, PyObject *args)
{
  char *key, *value;
  if (!PyArg_ParseTuple(args, "ss", &key, &value)) { return NULL; }
  if (check_db()) { return NULL; }

  return set_check(h, key, value, strlen(value) + 1, type_standard);
}

static PyObject *
cdb_get_words(PyObject *self, PyObject *args)
{
  char *key, *ptr;
  PyObject *dict;
  if (!PyArg_ParseTuple(args, "s", &key)) { return NULL; }
  if (check_db()) { return NULL; }

  dict = PyDict_New();
  if (! dict) { return NULL; }

  ptr = ph_get(h, key, NULL, NULL);
  if (! ptr) { Py_RETURN_NONE; } // key not found

  while(1) { // decode value
    PyObject *tuple, *item, *wkey;
    int len, id, cluster_id;
    char *wptr = ptr;

    if (! *ptr) { return dict; }

    if (! strcmp(wptr, "=")) {
      wptr = key;
    }
    wkey = PyUnicode_FromString(wptr);
    if (!wkey) { return NULL; }

    len = strlen(ptr);
    ptr += len + 1;
    id = get_uint32(ptr);
    cluster_id = get_uint32(ptr + 4);
    ptr += 8;

    tuple = PyTuple_New(2);
    if (! tuple) { return NULL; }

    item = PyLong_FromLong(id);
    if (! item) { return NULL; }
    if (PyTuple_SetItem(tuple, 0, item)) { return NULL; }

    item = PyLong_FromLong(cluster_id);
    if (! item) { return NULL; }
    if (PyTuple_SetItem(tuple, 1, item)) { return NULL; }

    if (PyDict_SetItem(dict, wkey, tuple)) { return NULL; }
    Py_DECREF(tuple);
    Py_DECREF(wkey);
  }

  return dict;
}

static PyObject *
cdb_get_word_by_id(PyObject *self, PyObject *args)
{
  /* this method is slow and inefficient: O(n) search */
  int search_id, i;

  if (check_db()) { return NULL; }
  if (!PyArg_ParseTuple(args, "i", &search_id)) { return NULL; }

  for(i=0; i < h->count; i++) {
    void *data = h->entries[i].data;
    char *key = data;

    if (data) {
      char *ptr = data + strlen(data) + 1;
      if ((*((unsigned char *) ptr)) >> 6 == type_word) {
	ptr ++;

	while(1) {
	  int len, id;
	  char *wptr = ptr;
	  if (! *ptr) { break; }

	  len = strlen(ptr);
	  ptr += len + 1;
	  id = get_uint32(ptr);
	  ptr += 8;

	  if (id == search_id) {
	    if (! strcmp(wptr, "=")) { wptr = key; }
	    return PyUnicode_FromString(wptr);
	  }
	}
      }
    }
  }

  Py_RETURN_NONE;
}

static PyObject *
cdb_set_words(PyObject *self, PyObject *args)
{
  PyObject *dict;
  char *key;
  PyObject *wkey, *wvalue;
  Py_ssize_t pos = 0;
  char* ptr = buf;

  if (!PyArg_ParseTuple(args, "sO", &key, &dict)) { return NULL; }
  if (check_db()) { return NULL; }

  while (PyDict_Next(dict, &pos, &wkey, &wvalue)) {
    int id, cluster_id, len;
    char *wkeytxt;
    if (!PyArg_ParseTuple(wvalue, "ii", &id, &cluster_id)) { return NULL; }
    // deprecated: if (PyUnicode_AsStringAndSize(wkey, &wkeytxt, &len)) { return NULL; }
    PyObject *bytes = PyUnicode_AsUTF8String(wkey);
    wkeytxt = PyBytes_AsString(bytes);
    Py_DECREF(bytes);
    if (! wkeytxt) { return NULL; }

    if (!strcmp(wkeytxt, key)) { wkeytxt = "="; }
    len = strlen(wkeytxt);

    if (ptr - buf + len + 10 > BUF) {
      PyErr_SetString(CdbError, "Buffer overflow");
      return NULL;
    }

    strcpy(ptr, wkeytxt);
    ptr += len + 1;
    set_uint32(ptr, id);
    set_uint32(ptr + 4, cluster_id);
    ptr += 8;
  }
  *ptr = '\0';

  return set_check(h, key, buf, ptr - buf + 1, type_word);
}

static PyObject *
cdb_get_gram(PyObject *self, PyObject *args)
{
  char *key;
  void *value;
  gram_t gram;
  if (check_db()) { return NULL; }
  if (!PyArg_ParseTuple(args, "s", &key)) { return NULL; }

  value = (gram_t*) ph_get(h, key, NULL, NULL);
  if (! value) { Py_RETURN_NONE; }

  memcpy(&gram, value, sizeof(gram)); // ensure alignment

  return Py_BuildValue("ddi", (double) gram.count, (double) gram.replace, gram.date);
}

static PyObject *
cdb_set_gram(PyObject *self, PyObject *args)
{
  double count, replace;
  int date;
  char *key;
  gram_t gram;
  if (check_db()) { return NULL; }
  if (!PyArg_ParseTuple(args, "sddi", &key, &count, &replace, &date)) { return NULL; }

  gram.count = count;
  gram.replace = replace;
  gram.date = date;

  return set_check(h, key, &gram, sizeof(gram), type_ngram);
}

static PyObject *
cdb_rm(PyObject *self, PyObject *args)
{
  char *key;
  if (check_db()) { return NULL; }
  if (!PyArg_ParseTuple(args, "s", &key)) { return NULL; }

  ph_rm(h, key);

  Py_RETURN_NONE;
}

static PyObject *
cdb_get_keys(PyObject *self, PyObject *args)
{
  PyObject *list;
  int i;

  if (check_db()) { return NULL; }

  list = PyList_New(0);
  if (! list) { return NULL; }

  for(i=0; i < h->count; i++) {
    void *data = h->entries[i].data;
    if (data) {
      PyObject* key = PyUnicode_FromString(data);
      if (! key) { return NULL; }
      if (PyList_Append(list, key)) { return NULL; }
      Py_DECREF(key);
    }
  }

  return list;
}

static PyObject *
cdb_purge_grams(PyObject *self, PyObject *args)
{
  int i, min_date;
  double min_count;
  gram_t gram;

  if (check_db()) { return NULL; }
  if (!PyArg_ParseTuple(args, "di", &min_count, &min_date)) { return NULL; }

  for(i=0; i < h->count; i++) {
    void *data = h->entries[i].data;
    if (data) {
      unsigned char *ptr = data + strlen(data) + 1;
      if ((*ptr) >> 6 == 1) {
	 memcpy(&gram, ptr + 1, sizeof(gram)); // ensure alignment
	 if (gram.date < min_date && (gram.count + gram.replace) < min_count) {
	   // remove this n-gram
	   h->entries[i].data = NULL;
	 }
      }
    }
  }

  Py_RETURN_NONE;
}


static PyMethodDef CdbMethods[] = {
  {"load",           (PyCFunction) cdb_load,           METH_VARARGS, "Load data"},
  {"clear",          (PyCFunction) cdb_clear,          METH_VARARGS, "Unload all data from memory"},
  {"save",           (PyCFunction) cdb_save,           METH_VARARGS, "Flush data to disk"},
  {"get_string",     (PyCFunction) cdb_get_string,     METH_VARARGS, "Search string value"},
  {"set_string",     (PyCFunction) cdb_set_string,     METH_VARARGS, "Update a string value"},
  {"get_words",      (PyCFunction) cdb_get_words,      METH_VARARGS, "Search string value"},
  {"set_words",      (PyCFunction) cdb_set_words,      METH_VARARGS, "Update a string value"},
  {"get_gram",       (PyCFunction) cdb_get_gram,       METH_VARARGS, "Search a N-gram"},
  {"set_gram",       (PyCFunction) cdb_set_gram,       METH_VARARGS, "Update a N-gram"},
  {"rm",             (PyCFunction) cdb_rm,             METH_VARARGS, "Remove an item"},
  {"get_keys",       (PyCFunction) cdb_get_keys,       METH_VARARGS, "Get all keys"},
  {"purge_grams",    (PyCFunction) cdb_purge_grams,    METH_VARARGS, "Purge old entries"},
  {"get_word_by_id", (PyCFunction) cdb_get_word_by_id, METH_VARARGS, "Return word by it's ID"},
  {NULL, NULL, 0, NULL}        /* Sentinel */
};

static struct PyModuleDef cdbmodule = {
   PyModuleDef_HEAD_INIT,
   "cdb",    /* name of module */
   NULL,     /* module documentation, may be NULL */
   -1,       /* size of per-interpreter state of the module,
                or -1 if the module keeps state in global variables. */
   CdbMethods
};

PyMODINIT_FUNC PyInit_cdb(void)
{
  PyObject *m;

  m = PyModule_Create(&cdbmodule);
  if (m == NULL) { return NULL; }

  CdbError = PyErr_NewException("cdb.error", NULL, NULL);
  Py_INCREF(CdbError);
  PyModule_AddObject(m, "error", CdbError);
  return m;
}

