
#include "py_defines.h"

#define STRINGIFY(x) XSTRINGIFY(x)
#define XSTRINGIFY(x) #x

//PyObject* setup   (PyObject* self, PyObject *args, PyObject *kwargs);
PyObject* render  (PyObject* self, PyObject *md);
PyObject* html    (PyObject* self, PyObject *htm);


static PyMethodDef mrfastmarkMethods[] = {
  //{"setup",  (PyCFunction) setup,   METH_VARARGS | METH_KEYWORDS, "Setup the renderer"   },
  {"render", (PyCFunction) render,  METH_O,                       "Render fastmark into html"   },
  {"html",   (PyCFunction) html,    METH_O,                       "Change html back into fastmark" },
  {NULL, NULL, 0, NULL}       /* Sentinel */
};

#if PY_MAJOR_VERSION >= 3

static struct PyModuleDef moduledef = {
  PyModuleDef_HEAD_INIT,
  "mrfastmark",
  0,              /* m_doc */
  -1,             /* m_size */
  mrfastmarkMethods,  /* m_methods */
  NULL,           /* m_reload */
  NULL,           /* m_traverse */
  NULL,           /* m_clear */
  NULL            /* m_free */
};

#define PYMODINITFUNC       PyObject *PyInit_mrfastmark(void)
#define PYMODULE_CREATE()   PyModule_Create(&moduledef)
#define MODINITERROR        return NULL

#else

#define PYMODINITFUNC       PyMODINIT_FUNC initmrfastmark(void)
#define PYMODULE_CREATE()   Py_InitModule("mrfastmark", mrfastmarkMethods)
#define MODINITERROR        return

#endif

PYMODINITFUNC
{
  PyObject *m;

  m = PYMODULE_CREATE();

  if (m == NULL) { MODINITERROR; }

  PyModule_AddStringConstant(m, "__version__", STRINGIFY(MRPACKER_VERSION));
  PyModule_AddStringConstant(m, "__author__", "Mark Reed <MarkReedZ@mail.com>");
#if PY_MAJOR_VERSION >= 3
  return m;
#endif
}

