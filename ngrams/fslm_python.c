#include <Python.h>
#include "fslm.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

static PyObject *FslmError;

static db_t *db = NULL; /* loaded N-gram DB */
static void* data = NULL;

static void free_all() {
  if (db) {
    db_free(db);
    free(data);
    data = db = NULL;
  }
}

static PyObject *
fslm_load(PyObject *self, PyObject *args)
{
  const char *filename;
  struct stat st;
  int len, fd, bytes_read;

  if (!PyArg_ParseTuple(args, "s", &filename)) { return NULL; }

  free_all();
  
  if (stat(filename, &st)) { PyErr_SetFromErrno(FslmError); return NULL; }

  len = st.st_size;

  fd = open(filename, O_RDONLY);
  if (fd == -1) { PyErr_SetFromErrno(FslmError); return NULL; }
   
  data = malloc(len);
  if (! data) {
    PyErr_NoMemory();
    return NULL;
  }

  bytes_read = read(fd, data, len);
  if (len != bytes_read) { PyErr_SetString(FslmError, "Can't read file"); return NULL; }

  db = db_init(data);
  if (! db) { free(data); PyErr_SetString(FslmError, "DB init failed"); return NULL; }

  Py_RETURN_NONE;
}

static PyObject *
fslm_clear(PyObject *self, PyObject *args)
{
  free_all();

  Py_RETURN_NONE;
}

static PyObject *
fslm_search(PyObject *self, PyObject *args)
{
  int wids[MAX_TABLE];
  int count, result, i, len;
  PyObject* lst;

  if (! db) { PyErr_SetString(FslmError, "DB not loaded"); return NULL; }

  lst = PyTuple_GetItem(args, 0);
  if (! lst) { return NULL; }

  if (! PySequence_Check(lst)) { PyErr_SetString(FslmError, "Argument is not a sequence"); return NULL; }

  len = PySequence_Length(lst);
  
  count = 0;
  for(i = 0; i < len; i ++) {
    PyObject* item = PySequence_GetItem(lst, i);
    int value;

    if (! item) { return NULL; }
    value = PyLong_AsLong(item);
    if (value == -1) { return NULL; }
    if (value < 0) { value = abs(value); }
    if (value != 1) { // filter #NA
      if (count > MAX_TABLE) { PyErr_SetString(FslmError, "n-gram too large"); return NULL; }
      wids[count ++] = value;
    }
  }

  result = search(db, wids, count);

  return Py_BuildValue("i", result);
}

static PyMethodDef FslmMethods[] = {
  {"load",   (PyCFunction) fslm_load,   METH_VARARGS, "Load a N-gram data file"},
  {"clear",  (PyCFunction) fslm_clear,  METH_VARARGS, "Unload all data from memory"},
  {"search", (PyCFunction) fslm_search, METH_VARARGS, "Search a N-gram"},
  {NULL, NULL, 0, NULL}        /* Sentinel */
};

static struct PyModuleDef fslmmodule = {
   PyModuleDef_HEAD_INIT,
   "cfslm",  /* name of module */
   NULL,     /* module documentation, may be NULL */
   -1,       /* size of per-interpreter state of the module,
                or -1 if the module keeps state in global variables. */
   FslmMethods
};

PyMODINIT_FUNC PyInit_cfslm(void)
{
  PyObject *m;

  m = PyModule_Create(&fslmmodule);
  if (m == NULL) { return NULL; }

  FslmError = PyErr_NewException("cfslm.error", NULL, NULL);
  Py_INCREF(FslmError);
  PyModule_AddObject(m, "error", FslmError);
  return m; 
}

