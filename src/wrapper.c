/* Copyright (c) 2012-2014, Fortylines LLC
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
   TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
   OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
   OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
   ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#include <stdio.h>
#include <Python.h>
#include "libvcd.h"


typedef struct _write_string_stream_t {
    Py_ssize_t write_index;
    PyObject *buffer;
} write_string_stream_t;


typedef struct {
    PyObject_HEAD
    struct trace_filter_t trace;
    write_string_stream_t write_stream;
} PyVCDTrace;


static void
write_string_stream_append( void* ptr, const char *buffer, size_t len )
{
    write_string_stream_t *stream = (write_string_stream_t *)ptr;

    PyObject *newstr = PyString_FromStringAndSize(buffer, len);
    if( newstr ) {
        PyObject *obj = stream->buffer;
        if( obj ) {
            PyString_ConcatAndDel(&obj, newstr);
            if( obj ) stream->buffer = obj;
        } else {
            stream->buffer = newstr;
        }
    }
}


static int
PyVCDTrace_init(PyVCDTrace *self, PyObject *args, PyObject *kwds)
{
    Py_ssize_t i;
    PyObject *variables;
    unsigned long start_time, end_time, resolution;

    if( !PyArg_ParseTuple(args, "Okkk", &variables,
            &start_time, &end_time, &resolution) ) {
        return 1;
    }

    self->write_stream.write_index = 0;
    self->write_stream.buffer = NULL;
    trace_filter_init(&self->trace, start_time, end_time, resolution,
        write_string_stream_append, &self->write_stream);

    for( i = 0; i < PyList_Size(variables); ++i ) {
        PyObject *item = PyList_GetItem(variables, 0);
        char *name = PyString_AsString(item);
        if( name ) {
            self->trace.map.head = insert_signal(self->trace.map.head, name);
        }
    }

    return 0;
}

static void
PyVCDTrace_dealloc(PyObject *obj)
{
    PyVCDTrace *self = (PyVCDTrace*)obj;
    destroy_signal_map(&self->trace.map);
    PyMem_DEL(self);
}


static PyObject *
PyVCDTrace_write(PyVCDTrace *self, PyObject *args)
{
    PyObject *arg1 = NULL;
    Py_buffer buffer;

    if( !PyArg_ParseTuple(args, "O", &arg1) ) {
        return 0;
    }
    if( PyObject_GetBuffer(arg1, &buffer, PyBUF_SIMPLE) < 0 ) {
        return 0;
    }

    size_t bytes_used = trace_filter_write(
        &self->trace, buffer.buf, buffer.len);

    PyBuffer_Release(&buffer);
    return PyInt_FromSize_t(bytes_used);
}

static PyObject *
PyVCDTrace_str(PyObject *obj)
{
    PyVCDTrace *self = (PyVCDTrace*)obj;
    trace_filter_flush(&self->trace);

    if( self->write_stream.buffer )
        return self->write_stream.buffer;
    return PyString_FromString("");
}


static PyObject*
wrapper_definitions( PyObject *self, PyObject *args )
{
    signal_map map;
    PyFileObject *read_file_descr;
    if( !PyArg_ParseTuple(args, "O", &read_file_descr) ) {
        return NULL;
    }

    write_string_stream_t write_stream;
    write_stream.write_index = 0;
    write_stream.buffer = NULL;

    FILE *fp = PyFile_AsFile((PyObject*)read_file_descr);
    PyFile_IncUseCount(read_file_descr);
#if 0
    Py_BEGIN_ALLOW_THREADS;
#endif
    init_signal_map(&map);
    header_and_definitions(fp, &map, write_string_stream_append, &write_stream);
    destroy_signal_map(&map);
#if 0
    Py_END_ALLOW_THREADS;
#endif
    PyFile_DecUseCount(read_file_descr);

    if( write_stream.buffer )
        return write_stream.buffer;
    return PyString_FromString("{}");
}

static PyObject*
wrapper_values( PyObject *self, PyObject *args )
{
    Py_ssize_t i;
    PyObject *variables;
    PyFileObject *read_file_descr;
    unsigned long start_time, end_time, resolution;

    if( !PyArg_ParseTuple(args, "OOkkk", &read_file_descr, &variables,
            &start_time, &end_time, &resolution) ) {
        return NULL;
    }

    signal_map map;
    write_string_stream_t write_stream;
    write_stream.write_index = 0;
    write_stream.buffer = NULL;

    FILE *fp = PyFile_AsFile((PyObject*)read_file_descr);
    PyFile_IncUseCount(read_file_descr);

    long prevpos = ftell(fp);
    fseek(fp, 0, SEEK_SET);
#if 0
    Py_BEGIN_ALLOW_THREADS;
#endif
    init_signal_map(&map);
    for( i = 0; i < PyList_Size(variables); ++i ) {
        PyObject *item = PyList_GetItem(variables, 0);
        char *name = PyString_AsString(item);
        if( name ) {
            map.head = insert_signal(map.head, name);
        }
    }
    filter_value_changes(fp, &map, start_time, end_time, resolution,
        write_string_stream_append, &write_stream);
    destroy_signal_map(&map);
#if 0
    Py_END_ALLOW_THREADS;
#endif
    fseek(fp, prevpos, SEEK_SET);
    PyFile_DecUseCount(read_file_descr);

    if( write_stream.buffer )
        return write_stream.buffer;
    return PyString_FromString("{}");
}


static PyMethodDef VCDMethods[] = {
    {"definitions",  wrapper_definitions, METH_VARARGS,
     "Returns header and definitions of a VCD file."},
    {"values",  wrapper_values, METH_VARARGS,
     "Retrieve value change dumps for a set of variables over a time period."},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

static PyMethodDef PyVCDTrace_methods[] = {
    {"write", (PyCFunction)PyVCDTrace_write, METH_VARARGS,
     "Write a chuck of VCD bytes into the trace filter."
    },
    {NULL}
};

static PyTypeObject PyVCDTrace_t = {
    PyObject_HEAD_INIT(NULL)
    0,                      /* ob_size */
    "vcd.Trace",            /* tp_name */
    sizeof(PyVCDTrace),     /* tp_basicsize */
    0,                      /* tp_itemsize    */
    PyVCDTrace_dealloc,     /* tp_dealloc     */
    0,                      /* tp_print       */
    0,                      /* tp_getattr     */
    0,                      /* tp_setattr     */
    0,                      /* tp_compare     */
    0,                      /* tp_repr        */
    0,                      /* tp_as_number   */
    0,                      /* tp_as_sequence */
    0,                      /* tp_as_mapping  */
    0,                      /* tp_hash        */
    0,                      /* tp_call        */
    PyVCDTrace_str,         /* tp_str         */
    0,                      /* tp_getattro    */
    0,                      /* tp_setattro    */
    0,                      /* tp_as_buffer   */
    Py_TPFLAGS_DEFAULT,     /* tp_flags       */
    "VCD Trace.",           /* tp_doc         */
    0,                      /* tp_traverse    */
    0,                      /* tp_clear          */
    0,                      /* tp_richcompare    */
    0,                      /* tp_weaklistoffset */
    0,                      /* tp_iter           */
    0,                      /* tp_iternext       */
    PyVCDTrace_methods,     /* tp_methods        */
    0,                      /* tp_members        */
    0,                      /* tp_getset         */
    0,                      /* tp_base           */
    0,                      /* tp_dict           */
    0,                      /* tp_descr_get      */
    0,                      /* tp_descr_set      */
    0,                      /* tp_dictoffset     */
    (initproc)PyVCDTrace_init, /* tp_init         */
};


PyMODINIT_FUNC
initvcd(void)
{
    PyVCDTrace_t.tp_new = PyType_GenericNew;
    if (PyType_Ready(&PyVCDTrace_t) < 0)
        return;

    PyObject *m = Py_InitModule("vcd", VCDMethods);
    if( m == NULL ) return;

    Py_INCREF(&PyVCDTrace_t);
    PyModule_AddObject(m, "Trace", (PyObject *)&PyVCDTrace_t);
}
