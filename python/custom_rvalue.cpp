#include "boost/python/object.hpp"  //len function
#include "boost/python/ssize_t.hpp" //ssize_t type definition
#include "boost/python/detail/none.hpp"
#include <boost/mpl/int.hpp>
#include <boost/mpl/next.hpp>
#include "tuples.hpp"

#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION

#include <numpy/arrayobject.h>
#include <numpy/arrayscalars.h>

#include <Python.h>

#include "gimli.h"
#include "pos.h"
#include "vector.h"
#include "matrix.h"

// cp ../gimli/python/custom_rvalue.cpp python/generated/custom_rvalue.cpp && make pg


namespace bp = boost::python;

// #define __DC(str) ;

#define __DC(str) if (GIMLI::debug()) __MS(str)

// ** TODO check if we need a delete somewhere for all the new stuff

#include <fstream>
std::ostream & operator << (std::ostream & os, const bp::object& o){
    return os << bp::extract<std::string>(bp::str(o))();
}

namespace r_values_impl{

template < class ValueType > void * checkConvertibleSequenz(PyObject * obj){
    //     import_array2("Cannot import numpy c-api from pygimli hand_make_wrapper2", NULL);
    // is obj is a sequence
    if(!PySequence_Check(obj)){
        __DC(obj << "\t !Object")
        return NULL;
    }

    // has the obj a len method
    if (!PyObject_HasAttrString(obj, "__len__")){
        __DC(obj << "\t !len")
        return NULL;
    }

    if (strcmp(obj->ob_type->tp_name, "numpy.ndarray") == 0){
        __DC(obj << "\t numpy.ndarray  ... okay")
        return obj;
    }

    bp::object py_sequence(bp::handle<>(bp::borrowed(obj)));

    if (len(py_sequence) > 0) {
        __DC(obj << "\t len: " << len(py_sequence) << " type: " << GIMLI::type(ValueType(0)))
        bp::object element = py_sequence[0];
        __DC(obj << "\t seq[0]: " << element << " "  << " type: " << GIMLI::type(ValueType(0)))

        bp::extract< ValueType > type_checker(element);
        if (type_checker.check()){
            __DC(obj << "\t ->construct: " << len(py_sequence))
            return obj;
        } else {
            __DC(obj << "\t cannot convert: " << type_checker.check())

            PyTypeObject* type = obj->ob_type;
            const char* p = type->tp_name;
            __DC("type is " << p)
            if (strcmp(p, "numpy.ndarray")==0){
                return obj;
            }

//             boost::numpy::initialize();
//             std::cout << bp::extract<ValueType>(element)() << std::endl;
//             std::cout << bp::extract<ValueType>(bp::str(element))() << std::endl;
//TypeError: No registered converter was able to produce a C++ rvalue of type unsigned long from this Python object of type numpy.int64


//             try{
//                 bp::str str = bp::extract< bp::str >(element);
// //                 __DC(str);
//             } catch(...){
// //               __DC(str);
//
//             __DC(obj << "\t cannot convert")
//             }

            std::cout << WHERE_AM_I << "element cannot converted " << std::endl;
        }

    } else {
        __DC(obj << " len == 0")
        return NULL;
    }
    // check if there is a valid converter
    //         if(convertible_impl(py_sequence, boost::mpl::int_< 0 >(), length_type())){
    //             return obj;
    //         } else{
    __DC(obj << " fail")
    return NULL;
}

struct PyTuple2RVector3{

    typedef boost::tuples::tuple< double, double > xy_type;
    typedef boost::tuples::tuple< double, double, double > xyz_type;
    typedef bp::from_py_sequence< xy_type > xy_converter_type;
    typedef bp::from_py_sequence< xyz_type > xyz_converter_type;

    typedef GIMLI::RVector3 xyz_t;

    static void * convertible(PyObject * obj){
        if (xy_converter_type::convertible(obj) ||
             xyz_converter_type::convertible(obj)){
            return obj;
        } else{
            return NULL;
        }
    }

    static void construct(PyObject* obj, bp::converter::rvalue_from_python_stage1_data * data){

        typedef bp::converter::rvalue_from_python_storage< xyz_t > xyz_storage_t;

        xyz_storage_t * the_storage = reinterpret_cast< xyz_storage_t * >(data);
        void * memory_chunk = the_storage->storage.bytes;

        double x(0.0), y(0.0), z(0.0);

        bp::tuple py_tuple(bp::handle<>(bp::borrowed(obj)));

        if (3 == bp::len(py_tuple)){
            boost::tuples::tie(x, y, z) = xyz_converter_type::to_c_tuple(obj);
        } else if (2 == bp::len(py_tuple)){
            boost::tuples::tie(x, y) = xy_converter_type::to_c_tuple(obj);
        }

        //** don't know where this will be deleted but it is necessary
        new (memory_chunk) xyz_t(x, y, z);
        data->convertible = memory_chunk;
    }
};

struct PySequence2RVector{

    /*! Check if the object is convertible */
    static void * convertible(PyObject * obj){
        __DC(obj << " -> RVector")
        return checkConvertibleSequenz<double>(obj);
    }

    /*! Convert List[] or ndarray into RVector */
    static void construct(PyObject* obj, bp::converter::rvalue_from_python_stage1_data * data){
       __DC(obj << " constructing RVector:")

        typedef bp::converter::rvalue_from_python_storage< GIMLI::Vector< double > > storage_t;
        storage_t* the_storage = reinterpret_cast<storage_t*>(data);
        void* memory_chunk = the_storage->storage.bytes;

        bp::object py_sequence(bp::handle<>(bp::borrowed(obj)));
        GIMLI::Vector< double > * vec = new (memory_chunk) GIMLI::Vector< double >(len(py_sequence));
        data->convertible = memory_chunk;

        if (strcmp(obj->ob_type->tp_name, "numpy.ndarray") == 0){
            PyArrayObject *arr = (PyArrayObject *)obj;
            __DC("type is " << obj->ob_type->tp_name << " " << PyArray_TYPE(arr))

            if (PyArray_TYPE(arr) == 12 && PyArray_ISONESEGMENT(arr)){
                void * arrData = PyArray_DATA(arr);
                std::memcpy(&(*vec)[0], arrData, vec->size() * sizeof(double));
                return;
            } else if (PyArray_TYPE(arr) == 7 && PyArray_ISONESEGMENT(arr)){ //numpy.int64
                    __DC(arr << " ** from array of type " << PyArray_TYPE(arr))

                    bp::object element;

                    for (GIMLI::Index i = 0; i < vec->size(); i ++){
                        element = py_sequence[i];
                        (*vec)[i] = PyArrayScalar_VAL(element.ptr(), Int64);

    //                     __DC(i << " a " << element);
    //                     __DC(i << " a " << element.ptr()->ob_type->tp_name)
    //                     __DC(i << " d " <<  PyArrayScalar_VAL(element.ptr(), Int64));

                    }
                    return;
            } else {
                    __DC("fixme: type=" << PyArray_TYPE(arr))
            }
        }

        // convert from list
        __DC(obj << " ** from sequence ")

        for (GIMLI::Index i = 0; i < vec->size(); i ++){
            (*vec)[i] = bp::extract< double >(py_sequence[i]);
        }
    }
private:
};

struct PySequence2IndexArray{

    /*! Check if the object is convertible */
    static void * convertible(PyObject * obj){
        __DC(obj << " -> IndexArray")
        return checkConvertibleSequenz<GIMLI::Index>(obj);
    }

    /*! Convert obj into IndexArray */
    static void construct(PyObject* obj, bp::converter::rvalue_from_python_stage1_data * data){
        __DC(obj << "\t constructing IndexArray")
        bp::object py_sequence(bp::handle<>(bp::borrowed(obj)));

        typedef bp::converter::rvalue_from_python_storage< GIMLI::IndexArray > storage_t;

        storage_t* the_storage = reinterpret_cast<storage_t*>(data);
        void* memory_chunk = the_storage->storage.bytes;

        GIMLI::IndexArray * vec = new (memory_chunk) GIMLI::IndexArray(len(py_sequence));
        data->convertible = memory_chunk;
        __DC(obj << "\t from list")
        for (GIMLI::Index i = 0; i < vec->size(); i ++){
            (*vec)[i] = bp::extract< GIMLI::Index >(py_sequence[i]);
        }
    }
private:
};

struct PySequence2StdVectorRVector3{

    /*! Check if the object is convertible */
    static void * convertible(PyObject * obj){
         __DC(obj << " -> StdVectorRVector3")
        return checkConvertibleSequenz<GIMLI::Pos< double > >(obj);
    }

    /*! Convert obj into RVector */
    static void construct(PyObject* obj, bp::converter::rvalue_from_python_stage1_data * data){
        __DC(obj << "\t constructing RVector3")
        bp::object py_sequence(bp::handle<>(bp::borrowed(obj)));

        typedef bp::converter::rvalue_from_python_storage< std::vector < GIMLI::Pos< double > > > storage_t;

        storage_t* the_storage = reinterpret_cast<storage_t*>(data);
        void* memory_chunk = the_storage->storage.bytes;

        std::vector < GIMLI::Pos < double > > *vec = new (memory_chunk) std::vector < GIMLI::Pos < double > >(len(py_sequence));
        data->convertible = memory_chunk;

        for (GIMLI::Index i = 0; i < vec->size(); i ++){
            (*vec)[i] = bp::extract< GIMLI::Pos < double > >(py_sequence[i]);
        }
    }
private:
};

struct PySequence2R3Vector{

    /*! Check if the object is convertible */
    static void * convertible(PyObject * obj){
        __DC(obj << " -> R3Vector")
        return checkConvertibleSequenz<GIMLI::Pos< double > >(obj);
    }

    /*! Convert obj into RVector */
    static void construct(PyObject* obj, bp::converter::rvalue_from_python_stage1_data * data){
        __DC(obj << "\t constructing R3Vector")
        bp::object py_sequence(bp::handle<>(bp::borrowed(obj)));

        typedef bp::converter::rvalue_from_python_storage< GIMLI::R3Vector > storage_t;

        storage_t* the_storage = reinterpret_cast<storage_t*>(data);
        void* memory_chunk = the_storage->storage.bytes;

        GIMLI::R3Vector *vec = new (memory_chunk) GIMLI::R3Vector(len(py_sequence));
        data->convertible = memory_chunk;

        for (GIMLI::Index i = 0; i < vec->size(); i ++){
            (*vec)[i] = bp::extract< GIMLI::Pos < double > >(py_sequence[i]);
        }
    }
private:
};

struct Numpy2RMatrix{

    /*! Check if the object is convertible */
    static void * convertible(PyObject * obj){
        __DC(obj << " -> RMatrix")
        return checkConvertibleSequenz< GIMLI::Matrix< double > >(obj);
    }

    /*! Convert obj into RVector */
    static void construct(PyObject* obj, bp::converter::rvalue_from_python_stage1_data * data){
        __DC(obj << "\t constructing RMatrix")

        bp::object py_sequence(bp::handle<>(bp::borrowed(obj)));

        typedef bp::converter::rvalue_from_python_storage< GIMLI::Matrix < double > > storage_t;

        storage_t* the_storage = reinterpret_cast<storage_t*>(data);
        void* memory_chunk = the_storage->storage.bytes;

        PyArrayObject *arr = (PyArrayObject *)obj;

        if (PyArray_TYPE(arr) == 12) {
            __DC("type=" << PyArray_TYPE(arr) << " " << PyArray_ISONESEGMENT(arr))
            int nDim = PyArray_NDIM(arr);
            if (nDim != 2){
                __DC("nDim=" << nDim)
                GIMLI::throwToImplement("Only numpy.ndarray with ndim == 2 can be converted to GIMLI::RMatrix");
            }
            int rows = PyArray_DIM(arr, 0);
            int cols = PyArray_DIM(arr, 1);
            GIMLI::Matrix < double > *mat = new (memory_chunk) GIMLI::Matrix < double >(rows, cols);
            data->convertible = memory_chunk;

            if (PyArray_ISONESEGMENT(arr)){
                double * arrData = (double*)PyArray_DATA(arr);
                for (GIMLI::Index i = 0; i < mat->rows(); i ++ ){
                    std::memcpy(&(*mat)[i][0],
                                arrData + (i * mat->cols()),
                                mat->cols() * sizeof(double));
                }
            } else {
                GIMLI::throwToImplement("numpy.ndarray is not one segment .. not yet implemented.");
            }

            return;
        } else {
            __DC("implementme: type=" << PyArray_TYPE(arr))
        }

        GIMLI::throwToImplement("Unknown rvalue type conversion from numpy.ndarray of type " + GIMLI::str(PyArray_TYPE(arr)) +
         + " to GIMLI::RMatrix");

//         for (GIMLI::Index i = 0; i < vec->size(); i ++){
//             //(*mat)[i] = bp::extract< GIMLI::Pos < double > >(py_sequence[i]);
//         }
    }
private:
};







} //r_values_impl

void register_pysequence_to_indexvector_conversion(){
    bp::converter::registry::push_back(& r_values_impl::PySequence2IndexArray::convertible,
                                        & r_values_impl::PySequence2IndexArray::construct,
                                        bp::type_id< GIMLI::IndexArray >());
}
void register_pysequence_to_rvector_conversion(){
    bp::converter::registry::push_back(& r_values_impl::PySequence2RVector::convertible,
                                        & r_values_impl::PySequence2RVector::construct,
                                        bp::type_id< GIMLI::Vector< double > >());
}
// void register_pysequence_to_bvector_conversion(){
//     bp::converter::registry::push_back(& r_values_impl::PySequence2BVector::convertible,
//                                         & r_values_impl::PySequence2BVector::construct,
//                                         bp::type_id< GIMLI::Vector< bool > >());
// }
void register_pysequence_to_StdVectorRVector3_conversion(){
    bp::converter::registry::push_back(& r_values_impl::PySequence2StdVectorRVector3::convertible,
                                        & r_values_impl::PySequence2StdVectorRVector3::construct,
                                        bp::type_id< std::vector< GIMLI::Pos< double > > >());
}
void register_pysequence_to_r3vector_conversion(){
    bp::converter::registry::push_back(& r_values_impl::PySequence2R3Vector::convertible,
                                        & r_values_impl::PySequence2R3Vector::construct,
                                        bp::type_id< GIMLI::R3Vector >());
}
void register_pytuple_to_rvector3_conversion(){
    bp::converter::registry::push_back(& r_values_impl::PyTuple2RVector3::convertible,
                                        & r_values_impl::PyTuple2RVector3::construct,
                                        bp::type_id< GIMLI::Pos< double > >());
}
void register_numpy_to_rmatrix_conversion(){
    bp::converter::registry::push_back(& r_values_impl::Numpy2RMatrix::convertible,
                                        & r_values_impl::Numpy2RMatrix::construct,
                                        bp::type_id< GIMLI::Matrix< double > >());
}
