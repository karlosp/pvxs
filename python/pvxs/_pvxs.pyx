# distutils: language = c++
#cython: language_level=3

from libc.stdint cimport uint64_t, int64_t
from libc.string cimport strcpy
from libcpp cimport bool
from libcpp.string cimport string

cimport numpy

from . cimport version as _version
from . cimport util
from . cimport sharedArray
from . cimport data
from . cimport nt
from . cimport client

ctypedef const string const_string
ctypedef const void const_void

cdef extern from *:
    """
    namespace {

    template<typename TO, typename FROM>
    ::pvxs::shared_array<TO> shared_array_cast(const ::pvxs::shared_array<FROM>& arr) {
        return arr.castTo<TO>();
    }

    PyArrayObject* alloc_array(PyArray_Descr* type, npy_intp num) {
        Py_INCREF(type);
        return PyArray_SimpleNewFromDescr(1, &num, type);
    }

    }
    """
    sharedArray.shared_array[TO] shared_array_cast[TO, FROM](const sharedArray.shared_array[FROM]&)

    numpy.ndarray alloc_array(numpy.dtype type, numpy.npy_intp num)

############### version

def version():
    return _version.version_int()

def version_str():
    return _version.version_str().decode('utf-8')

############### util

def instanceSnapshot():
    ret = []
    for P in util.instanceSnapshot():
        ret[P.first.decode('utf-8')] = P.second
    return ret

############### data

_codes2num = {
    'i':data.Int32,
}
_num2code = {V:K for K,V in _codes2num.items()}

cdef object Value_as_Value(const data.Value& val):
    cdef _Value ret = Value.__new__(Value)
    ret.val = val
    return ret

cdef object Value_as_bool(const data.Value& val):
    cdef bool ret = False
    cdef bool err = val.as(ret)
    if err:
        raise ValueError("Unable to extract bool")
    return ret

cdef object Value_as_uint(const data.Value& val):
    cdef uint64_t ret = False
    cdef bool err = val.as(ret)
    if err:
        raise ValueError("Unable to extract uint64_t")
    return ret

cdef object Value_as_int(const data.Value& val):
    cdef int64_t ret = False
    cdef bool err = val.as(ret)
    if err:
        raise ValueError("Unable to extract int64_t")
    return ret

cdef object Value_as_real(const data.Value& val):
    cdef double ret = False
    cdef bool err = val.as(ret)
    if err:
        raise ValueError("Unable to extract double")
    return ret

cdef object Value_as_str(const data.Value& val):
    cdef string ret
    cdef bool err = val.as(ret)
    if err:
        raise ValueError("Unable to extract double")
    return ret.decode('utf-8')

# py object to hold ownership
cdef class SharedArray:
    cdef sharedArray.shared_array[const void] arr

cdef object Value_as_array_str(SharedArray arr):
    cdef numpy.dtype dtype = numpy.PyArray_DescrFromType(numpy.NPY_STRING)
    cdef sharedArray.shared_array[const string] sarr = shared_array_cast[const_string, const_void](arr.arr)
    cdef numpy.ndarray ret

    for s in sarr:
        if dtype.itemsize < s.size()+1:
            dtype.itemsize = s.size()+1

    ret = alloc_array(dtype, sarr.size())
    for i in range(sarr.size()):
        strcpy(<char*>numpy.PyArray_GETPTR1(ret, i), sarr[i].c_str())

cdef object Value_as_array(const data.Value& val):
    cdef SharedArray arr = SharedArray.__new__(SharedArray)
    cdef bool err = val.as(arr.arr)
    cdef sharedArray.ArrayType atype

    if err:
        raise ValueError("Unable to extract array")
    atype = arr.original_type()

    if atype==sharedArray.String:
        return Value_as_array_str(arr)

    elif atype==sharedArray.Value:
        pass # return as object array

    else:
        pass # return as POD array

cdef class _Value:
    cdef data.Value val

    def __getattr__(self, str key):
        cdef bytes k = key.encode('utf-8')
        cdef data.Value ch = self.val[k]
        cdef data.StoreType stype = ch.storageType()

        if not ch.valid():
            return None

        # choose py type by storage type
        elif stype==data.SNull or stype==data.SCompound:
            return Value_as_Value(ch)

        elif stype==data.SBool:
            return Value_as_bool(ch)

        elif stype==data.SUInteger:
            return Value_as_uint(ch)

        elif stype==data.SInteger:
            return Value_as_int(ch)

        elif stype==data.SReal:
            return Value_as_real(ch)

        elif stype==data.SString:
            return Value_as_str(ch)

        elif stype==data.SArray:
            return Value_as_array(ch)

        raise NotImplementedError("Extract type not implemented")

Value = _Value

############### nt

def NtScalar(str code):
    cdef nt.NTScalar t
    cdef _Value ret = Value.__new__(Value)
    cdef data.TypeCode c = <data.TypeCode><int>_codes2num[code]

    t.code = c
    t.display = False
    t.control = False
    t.valueAlarm = False

    ret.val = t.create()
    return ret
