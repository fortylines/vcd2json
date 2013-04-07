/* Copyright (c) 2012-2013, Fortylines LLC
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:
     * Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
     * Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
     * Neither the name of fortylines nor the
       names of its contributors may be used to endorse or promote products
       derived from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY Fortylines LLC ''AS IS'' AND ANY
   EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL Fortylines LLC BE LIABLE FOR ANY
   DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
   ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#include <stdio.h>
#include <Python.h>
#include "libvcd.h"


typedef struct _write_string_stream_t {
    Py_ssize_t write_index;
    PyObject *buffer;
} write_string_stream_t;


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


static PyObject*
wrapper_definitions( PyObject *self, PyObject *args )
{
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
    header_and_definitions(fp, write_string_stream_append, &write_stream);
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

    write_string_stream_t write_stream;
    write_stream.write_index = 0;
    write_stream.buffer = NULL;

    FILE *fp = PyFile_AsFile((PyObject*)read_file_descr);
    PyFile_IncUseCount(read_file_descr);
#if 0
    Py_BEGIN_ALLOW_THREADS;
#endif
    for( i = 0; i < PyList_Size(variables); ++i ) {
        PyObject *item = PyList_GetItem(variables, 0);
        char *name = PyString_AsString(item);
        if( name ) {
            value_changes(fp, name, start_time, end_time, resolution,
                write_string_stream_append, &write_stream);
        }
    }
#if 0
    Py_END_ALLOW_THREADS;
#endif
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


PyMODINIT_FUNC
initvcd(void)
{
    PyObject *m = Py_InitModule("vcd", VCDMethods);
    if( m == NULL ) return;
}
