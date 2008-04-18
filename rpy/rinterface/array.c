/* A. Belopolsky's Array interface */


#include <Python.h>
#include <Rdefines.h>
#include <Rinternals.h>
#include "rinterface.h"

#define ARRAY_INTERFACE_VERSION 2

/* Array Interface flags */
#define FORTRAN       0x002
#define ALIGNED       0x100
#define NOTSWAPPED    0x200
#define WRITEABLE     0x400

typedef struct {
	int version;
	int nd;
	char typekind;
	int itemsize;
	int flags;
	Py_intptr_t *shape;
	Py_intptr_t *strides;
	void *data;
} PyArrayInterface;

static int
sexp_typekind(SEXP sexp)
{
  switch (TYPEOF(sexp)) {
  case REALSXP: return 'f';
  case INTSXP: return 'i';
  case STRSXP: return 'S';
  case CPLXSXP: return 'c';
    //FIXME: correct type ?
  case LGLSXP: return 'b';
  }
  return 0;
}


static int
sexp_itemsize(SEXP sexp)
{
  switch (TYPEOF(sexp)) {
  case REALSXP: return sizeof(*REAL(sexp));
  case INTSXP: return sizeof(*INTEGER(sexp));
  case STRSXP: return sizeof(*CHAR(sexp));
  case CPLXSXP: return sizeof(*COMPLEX(sexp));
  case LGLSXP: return sizeof(*LOGICAL(sexp));
  }
  return 0;
}

static int
sexp_rank(SEXP sexp)
{
  SEXP dim = getAttrib(sexp, R_DimSymbol);
  if (dim == R_NilValue)
    return 1;
  return LENGTH(dim);
}

static void
sexp_shape(SEXP sexp, Py_intptr_t* shape, int nd)
{
  int i;
  SEXP dim = getAttrib(sexp, R_DimSymbol);
  if (dim == R_NilValue)
    shape[0] = LENGTH(sexp);
  else for (i = 0; i < nd; ++i) {
      shape[i] = INTEGER(dim)[i];
    }
}

static void
array_struct_free(void *ptr, void *arr)
{
  PyArrayInterface *inter	= (PyArrayInterface *)ptr;
  free(inter->shape);
  free(inter);
  Py_DECREF((PyObject *)arr);
}


static PyObject* 
array_struct_get(SexpObject *self)
{
  SEXP sexp = self->sexp;
  if (!sexp) {
    PyErr_SetString(PyExc_AttributeError, "Null sexp");
    return NULL;
  }
  PyArrayInterface *inter;
  int typekind =  sexp_typekind(sexp);
  if (!typekind) {
    PyErr_SetString(PyExc_AttributeError, "Unsupported SEXP type");
    return NULL;
  }
  inter = (PyArrayInterface *)malloc(sizeof(PyArrayInterface));
  if (!inter) {
    return PyErr_NoMemory();
  }
  int nd = sexp_rank(sexp);
  int i;
  inter->version = ARRAY_INTERFACE_VERSION;
  inter->nd = nd;
  inter->typekind = typekind;
  inter->itemsize = sexp_itemsize(sexp);
  inter->flags = FORTRAN|ALIGNED|NOTSWAPPED|WRITEABLE;
  inter->shape = (Py_intptr_t*)malloc(sizeof(Py_intptr_t)*nd*2);
  sexp_shape(sexp, inter->shape, nd);
  inter->strides = inter->shape + nd;
  Py_intptr_t stride = inter->itemsize;
  inter->strides[0] = stride;
  for (i = 1; i < nd; ++i) {
    stride *= inter->shape[i-1];
    inter->strides[i] = stride;
  }
  inter->data = RAW(sexp);
  Py_INCREF(self);
  return PyCObject_FromVoidPtrAndDesc(inter, self, array_struct_free);
}

