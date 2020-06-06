
from libcpp cimport bool
from libcpp.string cimport string
from libcpp.vector cimport vector
from libcpp.memory cimport shared_ptr
from libcpp.functional cimport function

cdef extern from "<pvxs/client.h>" namespace "pvxs::client" nogil:
    cdef cppclass Config:
        vector[string] addressList
        vector[string] interfaces
        unsigned short udp_port
        bool autoAddrList

        @staticmethod
        Config from_env() except+

        void expand() except+

        Context build() except+

    cdef cppclass Context:
        Context()
        Context(const Config&) except+

        GetBuilder get(const string& pvname) except+

    cdef cppclass GetBuilder:
        GetBuilder& field(const string& fld) except+
        GetBuilder& record[T](const string& name, const T& val) except+
        GetBuilder& pvRequest(const string& fld) except+
        #GetBuilder& rawRequest(Value&& fld) except+

        GetBuilder& result(function[void(Result&&)]&& cb) except+

        shared_ptr[Operation] exec() except+

    cdef cppclass Result:
        pass

    cdef cppclass Operation:
        pass
