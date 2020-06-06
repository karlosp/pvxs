
from libcpp cimport bool
from libcpp.string cimport string

cdef extern from "<pvxs/data.h>" namespace "pvxs" nogil:
    cdef enum code_t "TypeCode::code_t":
        Null "TypeCode::Null"

        Bool "TypeCode::Bool"
        Int8 "TypeCode::Int8"
        Int16 "TypeCode::Int16"
        Int32 "TypeCode::Int32"
        Int64 "TypeCode::Int64"
        UInt8 "TypeCode::UInt8"
        UInt16 "TypeCode::UInt16"
        UInt32 "TypeCode::UInt32"
        UInt64 "TypeCode::UInt64"
        Float32 "TypeCode::Float32"
        Float64 "TypeCode::Float64"
        String "TypeCode::String"
        Struct "TypeCode::Struct"
        Union "TypeCode::Union"
        Any "TypeCode::Any"

        BoolA "TypeCode::BoolA"
        Int8A "TypeCode::Int8A"
        Int16A "TypeCode::Int16A"
        Int32A "TypeCode::Int32A"
        Int64A "TypeCode::Int64A"
        UInt8A "TypeCode::UInt8A"
        UInt16A "TypeCode::UInt16A"
        UInt32A "TypeCode::UInt32A"
        UInt64A "TypeCode::UInt64A"
        Float32A "TypeCode::Float32A"
        Float64A "TypeCode::Float64A"
        StringA "TypeCode::StringA"
        StructA "TypeCode::StructA"
        UnionA "TypeCode::UnionA"
        AnyA "TypeCode::AnyA"

    cdef enum StoreType:
        SNull "StoreType::Null"
        SBool "StoreType::Bool"
        SUInteger "StoreType::UInteger"
        SInteger "StoreType::Integer"
        SReal "StoreType::Real"
        SString "StoreType::String"
        SCompound "StoreType::Compound"
        SArray "StoreType::Array"

    cdef enum Kind:
        KBool     "Kind::Bool"
        KInteger  "Kind::Integer"
        Real      "Kind::Real"
        KString   "Kind::String"
        KCompound "Kind::Compound"
        KNull     "Kind::Null"

    cdef cppclass TypeCode:
        code_t code

        TypeCode()
        TypeCode(unsigned)
        TypeCode(code_t)

        bool valid() const
        #Kind kind() const
        unsigned order() const
        unsigned size() const
        bool isunsigned() const
        bool isarray() const

        StoreType storeAs() const

        TypeCode arrayOf() const
        TypeCode scalarOf() const

        const char* name() const

    cdef cppclass Member:
        Member(TypeCode, const string&) except+
        #Member[IT](TypeCode, const string&, const IT&) except+
        #Member[IT](TypeCode, const string&, const string&, const IT&) except+

        void addChild(const Member& mem) except+


    cdef cppclass TypeDef:
        TypeDef()
        TypeDef(const TypeDef&)
        TypeDef(TypeDef&&)

        TypeDef(const Value&) except+

        #TypeDef[IT](TypeCode, const string&, const IT&) except+

        Member as(const string&) except+

        Value create() except+

    cdef cppclass Value:
        Value()
        Value(const Value&)
        Value(Value&&)

        Value cloneEmpty() except+
        Value clone() except+

        Value& assign(const Value&) except+

        Value allocMember() except+

        bool valid() const
        bool operator bool() const

        bool isMarked(bool=true, bool=false) const
        Value ifMarked(bool=true, bool=false) const

        void mark() except+
        void unmark(bool=false, bool=true) except+

        TypeCode type() const
        StoreType storageType() const
        const string& id() except+
        bool idStartsWith(const string&) except+

        const string& nameOf(const Value&) except+

        void copyOut(void *ptr, StoreType type) except+
        bool tryCopyOut(void *ptr, StoreType type) except+
        void copyIn(const void *ptr, StoreType type) except+
        bool tryCopyIn(const void *ptr, StoreType type) except+

        V as[V]() except+
        bool as[V](V&) except+

        bool tryFrom[V](const V&) except+
        void _from "from" [V](const V&) except+

        Value& update[K,V](K, const V&) except+

        Value operator[](const char*) except+
        Value operator[](const string&) except+

        cppclass iterator:
            Value& operator*()
            iterator operator++()
            iterator operator--()
            iterator operator+(size_type)
            iterator operator-(size_type)
            #difference_type operator-(iterator)
            bint operator==(iterator)
            bint operator!=(iterator)
            bint operator<(iterator)
            bint operator>(iterator)
            bint operator<=(iterator)
            bint operator>=(iterator)

        cppclass Iterable:
            iterator begin()
            iterator end()

        Iterable iall() except+
        Iterable ichildren() except+
        Iterable imarked() except+
