/******************************************************************************
 *   Copyright (C) 2006-2017 by the GIMLi development team                    *
 *   Carsten Rücker carsten@resistivity.net                                   *
 *                                                                            *
 *   Licensed under the Apache License, Version 2.0 (the "License");          *
 *   you may not use this file except in compliance with the License.         *
 *   You may obtain a copy of the License at                                  *
 *                                                                            *
 *       http://www.apache.org/licenses/LICENSE-2.0                           *
 *                                                                            *
 *   Unless required by applicable law or agreed to in writing, software      *
 *   distributed under the License is distributed on an "AS IS" BASIS,        *
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. *
 *   See the License for the specific language governing permissions and      *
 *   limitations under the License.                                           *
 *                                                                            *
 ******************************************************************************/

#ifndef GIMLI_SPARSEMATRIX__H
#define GIMLI_SPARSEMATRIX__H

#include "gimli.h"
#include "elementmatrix.h"
#include "vector.h"
#include "vectortemplates.h"
#include "matrix.h"
#include "mesh.h"
#include "meshentities.h"
#include "node.h"
#include "stopwatch.h"

#include <map>
#include <set>
#include <vector>
#include <algorithm>
#include <cassert>
#include <iostream>
#include <cmath>

namespace GIMLI{

#define SPARSE_NOT_VALID throwError(EXIT_SPARSE_INVALID, WHERE_AM_I + " no data/or sparsity pattern defined.");

//! based on: Ulrich Breymann, Addison Wesley Longman 2000 , revised edition ISBN 0-201-67488-2, Designing Components with the C++ STL
template< class ValueType, class IndexType, class ContainerType > class MatrixElement {
public:
  typedef std::pair< IndexType, IndexType > IndexPair;
  typedef MatrixElement< ValueType, IndexType, ContainerType > & Reference;

  MatrixElement(ContainerType & Cont, IndexType r, IndexType c)
    : C(Cont), I(C.find(IndexPair(r, c))), row(r), column(c) {
  }

  /* An assignment operator is required which in turn requires a
     reference to an object of type MatrixElement. When both the
     left- and right-hand side are identical, nothing has to happen.
     Otherwise, as above, it has to be checked whether the value of
     the right-hand element is 0 or not. The resulting behavior is
     described together with the above assignment operator, so that
     here it is simply called: */

    Reference operator = (Reference rhs) {
        if (this != & rhs) {    // not identical?
            return operator = (rhs.asValue());  // see above
        } return *this;
    }

  /* The constructor initializes the private variables with all
     information that is needed. The container itself is located in
     the sparseMatrix class; here, the reference to it is entered.
     If the passed indices for row and column belong to an element
     not yet stored in the container, the iterator has the value
     C.end(). */

  ValueType asValue() const {
    if (I == C.end()) return ValueType(0); else return (*I).second;
  }

  //** type conversion operator;
  operator ValueType () const { return asValue();  }

  /* According to the definition of the sparse matrix, 0 is returned
     if the element is not present in the container. Otherwise, the
     result is the second part of the object of type value_type
     stored in the container. */

  Reference operator = (const ValueType & x) {
    // not equal 0?
    if (x != ValueType(0)) {
      /* If the element does not yet exist, it is put, together
	 with the indices, into an object of type value_type and
	 inserted with insert(): */

      if (I == C.end()) {
        assert(C.size() < C.max_size());
        I = (C.insert(typename ContainerType::value_type(IndexPair(row, column), x))).first;
      } else (*I).second = x;
    }

    /* insert() returns a pair whose first part is an iterator
       pointing to the inserted object. The second part is of type
       bool and indicates whether the insertion took place because
       no element with this key existed. This is, however, not
       evaluated here because, due to the precondition (I ==
       C.end()), the second part must always have the value true.
       If, instead, the element already exists, the value is
       entered into the second part of the value_type object. If
       the value is equal 0, in order to save space the element is
       deleted if it existed. */

    else {                    // x = 0
      if (I != C.end()) {
        C.erase(I);
        I = C.end();
      }
    } return *this;
  }

  Reference operator += (const ValueType & x) {
    if (x != ValueType(0)) {
      if (I == C.end()) {
        assert(C.size() < C.max_size());
        I = (C.insert(typename ContainerType::value_type(IndexPair(row, column), x))).first;
      } else (*I).second += x;
    }
    return *this;
  }

  Reference operator -= (const ValueType & x) {
    if (x != ValueType(0)) {
      if (I == C.end()) {
        assert(C.size() < C.max_size());
        I = (C.insert(typename ContainerType::value_type(IndexPair(row, column), -x))).first;
      } else (*I).second -= x;
    }
    return *this;
  }

private:
  ContainerType & C;
  typename ContainerType::iterator I;
  IndexType row, column;

};  // class MatrixElement


//! based on: Ulrich Breymann, Addison Wesley Longman 2000 , revised edition ISBN 0-201-67488-2, Designing Components with the C++ STL
template< class ValueType, class IndexType >
class SparseMapMatrix : public MatrixBase {
public:
    typedef std::pair< IndexType, IndexType > IndexPair;
    typedef std::map< IndexPair, ValueType, std::less< IndexPair > > ContainerType;
    typedef typename ContainerType::iterator          iterator;
    typedef typename ContainerType::const_iterator    const_iterator;
    typedef MatrixElement< ValueType, IndexType, ContainerType > MatElement;

    /*!stype .. symmetric style. stype=0 (full), stype=1 (UpperRight), stype=2 (LowerLeft)*/
    SparseMapMatrix(IndexType r=0, IndexType c=0, int stype=0)
        : MatrixBase(), rows_(r), cols_(c), stype_(stype) {
    }

    SparseMapMatrix(const std::string & filename)
        : MatrixBase(){
        this->load(filename);
    }

    SparseMapMatrix(const SparseMapMatrix< ValueType, IndexType > & S)
        : MatrixBase(){
        clear();
        cols_ = S.cols();
        rows_ = S.rows();
        stype_ = S.stype();

        for (const_iterator it = S.begin(); it != S.end(); it ++){
            this->setVal(it->first.first, it->first.second, it->second);
        }
    }
    SparseMapMatrix(const SparseMatrix< ValueType > & S)
        : MatrixBase(){
        this->copy_(S);
    }

    /*! Contruct Map Matrix from 3 arrays of the same length.
     *Number of colums are max(j)+1 and Number of rows are max(i)+1.*/
    SparseMapMatrix(const IndexArray & i, const IndexArray & j, const RVector & v)
        : MatrixBase(){
        ASSERT_EQUAL(i.size(), j.size())
        ASSERT_EQUAL(i.size(), v.size())
        stype_ = 0;
        cols_ = max(j)+1;
        rows_ = max(i)+1;
        for (Index n = 0; n < i.size(); n ++ ) (*this)[i[n]][j[n]] = v[n];
    }

    SparseMapMatrix< ValueType, IndexType > & operator = (const SparseMapMatrix< ValueType, IndexType > & S){
        if (this != &S){
            clear();
            cols_ = S.cols();
            rows_ = S.rows();
            stype_ = S.stype();
            for (const_iterator it = S.begin(); it != S.end(); it ++){
                this->setVal(it->first.first, it->first.second, it->second);
            }
        } return *this;
    }

    SparseMapMatrix & operator = (const SparseMatrix< ValueType > & S){
        this->copy_(S);
        return *this;
    }

    virtual ~SparseMapMatrix() {}

    void resize(Index rows, Index cols){
        rows_ = rows;
        cols_ = cols;
    }

    /*! Return entity rtti value. */
    virtual uint rtti() const { return GIMLI_SPARSEMAPMATRIX_RTTI; }

    void copy_(const SparseMatrix< ValueType > & S){
        clear();
        cols_ = S.cols();
        rows_ = S.rows();
        stype_ = S.stype();

        std::vector < int > colPtr(S.vecColPtr());
        std::vector < int > rowIdx(S.vecRowIdx());
        Vector < ValueType > vals(S.vecVals());

        for (Index i = 0; i < S.size(); i++){
            for (int j = colPtr[i]; j < colPtr[i + 1]; j ++){
                (*this)[i][rowIdx[j]] = vals[j];
            }
        }
    }

    virtual void clear() {
        C_.clear();
        cols_ = 0; rows_ = 0; stype_ = 0;
    }

    /*! symmetric type. 0 = nonsymmetric, -1 symmetric lower part, 1 symmetric upper part.*/
    inline int stype() const {return stype_;}

    inline void setRows(IndexType r) { rows_ = r ; }
    virtual IndexType rows()     const { return rows_; }
    virtual IndexType nRows()     const { return rows_; }

    inline void setCols(IndexType c) { cols_ = c ; }
    virtual IndexType cols()     const { return cols_; }
    virtual IndexType nCols()     const { return cols_; }

    inline IndexType size()     const { return C_.size(); }
    inline IndexType max_size() const { return C_.max_size(); }
    inline IndexType nVals()    const { return C_.size(); }

    inline iterator begin() { return C_.begin(); }
    inline iterator end() { return C_.end(); }

    inline const_iterator begin() const { return C_.begin(); }
    inline const_iterator end()   const { return C_.end(); }

    void addToCol(Index id, const ElementMatrix < double > & A){
        for (Index i = 0, imax = A.size(); i < imax; i++){
            (*this)[A.idx(i)][id] += (ValueType)A.getVal(0, i);
        }
    }

    void addToRow(Index id, const ElementMatrix < double > & A){
        for (Index i = 0, imax = A.size(); i < imax; i++){
            (*this)[id][A.idx(i)] += (ValueType)A.getVal(0, i);
        }
    }

    void add(const ElementMatrix < double > & A){
        for (Index i = 0, imax = A.size(); i < imax; i++){
            for (Index j = 0, jmax = A.size(); j < jmax; j++){
                (*this)[A.idx(i)][A.idx(j)] += (ValueType)A.getVal(i, j);
            }
        }
    }

#define DEFINE_SPARSEMAPMATRIX_UNARY_MOD_OPERATOR__(OP) \
    SparseMapMatrix< ValueType, IndexType > & operator OP##= (const ValueType & v){\
        for (iterator it = begin(); it != end(); it ++) (*it).second OP##= v; \
        return *this; \
    } \

        DEFINE_SPARSEMAPMATRIX_UNARY_MOD_OPERATOR__(+)
        DEFINE_SPARSEMAPMATRIX_UNARY_MOD_OPERATOR__(-)
        DEFINE_SPARSEMAPMATRIX_UNARY_MOD_OPERATOR__(*)
        DEFINE_SPARSEMAPMATRIX_UNARY_MOD_OPERATOR__(/)

#undef DEFINE_SPARSEMMAPATRIX_UNARY_MOD_OPERATOR__

    SparseMapMatrix< ValueType, IndexType > & operator += (const SparseMapMatrix< ValueType, IndexType > & A){
        for (const_iterator it = A.begin(); it != A.end(); it ++){
            this->addVal(it->first.first, it->first.second, it->second);
        }
        return *this;
    }
    SparseMapMatrix< ValueType, IndexType > & operator -= (const SparseMapMatrix< ValueType, IndexType > & A){
        for (const_iterator it = A.begin(); it != A.end(); it ++){
            this->addVal(it->first.first, it->first.second, -it->second);
        }
        return *this;
    }

    //SparseMatrix< T > & operator += (const ElementMatrix < T > & A){
    void operator += (const ElementMatrix < double > & A){
        for (Index i = 0, imax = A.size(); i < imax; i++){
            for (Index j = 0, jmax = A.size(); j < jmax; j++){
                (*this)[A.idx(i)][A.idx(j)] += A.getVal(i, j);
            }
        }
    }

    class Aux {  // for index operator below
    public:
        Aux(IndexType r, IndexType maxs, ContainerType & Cont, int stype)
            : Row(r), maxColumns(maxs), C(Cont), stype_(stype) { }

        MatElement operator [] (IndexType c) {
//             __MS( stype_ << " " << c << " " << Row )
            if ((c < 0 || c >= maxColumns) || (stype_ < 0 && c < Row) || (stype_ > 0 && c > Row)) {
                throwLengthError(EXIT_SPARSE_SIZE,
                                  WHERE_AM_I + " idx = " + toStr(c) + ", " + str(Row) + " maxcol = "
                                  + toStr(maxColumns) + " stype: " + toStr(stype_));
            }
            return MatElement(C, Row, c);
        }
    protected:
        IndexType Row, maxColumns;
        ContainerType & C;
        int stype_;
    };

    Aux operator [] (IndexType r) {
        if (r < 0 || r >= rows_){
            throwLengthError(EXIT_SPARSE_SIZE,
                              WHERE_AM_I + " idx = " + toStr(r) + " maxrow = "
                              + toStr(rows_));
        }
        return Aux(r, cols(), C_, stype_);
    }

    inline IndexType idx1(const const_iterator & I) const { return (*I).first.first; }
    inline IndexType idx2(const const_iterator & I) const { return (*I).first.second; }

//     inline IndexType idx1(iterator & I) { return (*I).first.first; }
//     inline IndexType idx2(iterator & I) { return (*I).first.second; }

    inline const ValueType & val(const const_iterator & I) const { return (*I).second;  }

    inline ValueType & val(const iterator & I) { return (*I).second;  }

    inline ValueType getVal(IndexType i, IndexType j) { return (*this)[i][j]; }

    inline void setVal(IndexType i, IndexType j, const ValueType & val) {
        if ((stype_ < 0 && i > j) || (stype_ > 0 && i < j)) return;

        if ((i >= 0 && i < rows_) && (j >=0 && j < cols_)) {
            (*this)[i][j] = val;
        } else {
            throwLengthError(EXIT_SPARSE_SIZE,
                              WHERE_AM_I +
                              " i = " + toStr(i) + " max_row = " + toStr(rows_) +
                              " j = " + toStr(j) + " max_col = " + toStr(cols_)
                             );
        }
    }
    inline void addVal(IndexType i, IndexType j, const ValueType & val) {
        if ((stype_ < 0 && i > j) || (stype_ > 0 && i < j)) return;

        if ((i >= 0 && i < rows_) && (j >=0 && j < cols_)) {
            (*this)[i][j] += val;
        } else {
            throwLengthError(EXIT_SPARSE_SIZE,
                              WHERE_AM_I +
                              " i = " + toStr(i) + " max_row = " + toStr(rows_) +
                              " j = " + toStr(j) + " max_col = " + toStr(cols_)
                             );
        }
    }

    /*! Return this * a  */
    virtual Vector < ValueType > mult(const Vector < ValueType > & a) const {
        Vector < ValueType > ret(this->rows(), 0.0);

        ASSERT_EQUAL(this->cols(), a.size())

        if (stype_ == 0){
            for (const_iterator it = this->begin(); it != this->end(); it ++){
                ret[it->first.first] += a[it->first.second] * it->second;
            }
        } else if (stype_ == -1){

            for (const_iterator it = this->begin(); it != this->end(); it ++){
                IndexType I = it->first.first;
                IndexType J = it->first.second;

                ret[I] += a[J] * conj(it->second);

                if (J > I){
                    ret[J] += a[I] * it->second;
                }
            }
        } else if (stype_ ==  1){
            for (const_iterator it = this->begin(); it != this->end(); it ++){
                IndexType I = it->first.first;
                IndexType J = it->first.second;

                ret[I] += a[J] * conj(it->second);

                if (J < I){
                    ret[J] += a[I] * it->second;
                }
            }

        }
        return ret;
    }

    /*! Return this.T * a */
    virtual Vector < ValueType > transMult(const Vector < ValueType > & a) const {
        Vector < ValueType > ret(this->cols(), 0.0);

        ASSERT_EQUAL(this->rows(), a.size())

        if (stype_ == 0){
            for (const_iterator it = this->begin(); it != this->end(); it ++){
                ret[it->first.second] += a[it->first.first] * it->second;
            }
        } else if (stype_ == -1){
            THROW_TO_IMPL
        } else if (stype_ ==  1){
            THROW_TO_IMPL
        }
        return ret;
    }

    virtual Vector < ValueType > col(const Index i) {
        Vector < ValueType > null(this->cols(), 0.0);
        null[i] = 1.0;

        return this->mult(null);
    }

    virtual Vector < ValueType > row(const Index i) {
        Vector < ValueType > null(this->rows(), 0.0);
        null[i] = 1.0;

        return this->transMult(null);
    }

    void save(const std::string & filename) const {
        std::fstream file; openOutFile(filename, &file);

        for (const_iterator it = begin(); it != end(); it ++){
            file <<  idx1(it) << " " << idx2(it) << " " << val(it) << std::endl;
        }

        file.close();
    }

    void load(const std::string & filename){
        std::fstream file; openInFile(filename, &file);
        std::vector < IndexType > vi, vj;
        std::vector < ValueType > vval;
        IndexType i, j;
        ValueType val;
        while (file >> i >> j >> val){
            vi.push_back(i);
            vj.push_back(j);
            vval.push_back(val);
        }
        file.close();
        setRows(IndexType(max(vi) + 1));
        setCols(IndexType(max(vj) + 1));

        for (Index i = 0; i < vi.size(); i ++){
            (*this)[vi[i]][vj[i]] = vval[i];
        }
    }

    /*! Import columnwise from bmat starting at offset */
    void importCol(const std::string & filename, double dropTol, Index colOffset){
    //std::cout << "rows: " << Jcluster.rows() << " cols: " << Jcluster.cols()<< std::endl;

        FILE *file; file = fopen(filename.c_str(), "r+b");
        if (!file) {
            throwError(EXIT_OPEN_FILE, WHERE_AM_I + " " + filename + ": " + strerror(errno));
        }
        Index ret = 0;


        uint32 rows = 0;
        ret = fread(&rows, sizeof(uint32), 1, file);
        if (ret == 0) throwError(1, "fail reading file " + filename);
        uint32 cols = 0;
        ret = fread(&cols, sizeof(uint32), 1, file);
        if (ret == 0) throwError(1, "fail reading file " + filename);
        ValueType val;
        for (uint i = 0; i < rows; i ++){
            for (uint j = 0; j < cols; j ++){
                ret = fread(&val, sizeof(ValueType), 1, file);
                if (ret == 0) throwError(1, "fail reading file " + filename);
                if (abs(val) > dropTol) this->setVal(i, j + colOffset, val);
            }
        }

        fclose(file);
    }
    // no default arg here .. pygimli@win64 linker bug
    void importCol(const std::string & filename, double dropTol=1e-3){
        importCol(filename, dropTol, 0);
    }
protected:

  IndexType rows_, cols_;
  ContainerType C_;
  // 0 .. nonsymmetric, -1 symmetric lower part, 1 symmetric upper part
  int stype_;


};// class SparseMapMatrix

template < class ValueType, class IndexType >
void save(const SparseMapMatrix< ValueType, IndexType > & S,
         const std::string & fname){
    S.save(fname);
}

template < class ValueType, class IndexType >
int load(SparseMapMatrix< ValueType, IndexType > & S,
         const std::string & fname){
    return S.load(fname);
}

inline RVector operator * (const RSparseMapMatrix & A, const RVector & b){
    return A.mult(b);
}

inline CVector operator * (const CSparseMapMatrix & A, const CVector & b){
    return A.mult(b);
}

inline CVector operator * (const CSparseMapMatrix & A, const RVector & b){
    return A.mult(toComplex(b));
}

inline RVector transMult(const RSparseMapMatrix & A, const RVector & b){
    return A.transMult(b);
}

inline CVector transMult(const CSparseMapMatrix & A, const CVector & b){
    return A.transMult(b);
}

inline CVector transMult(const CSparseMapMatrix & A, const RVector & b){
    return A.transMult(toComplex(b));
}

// inline R3Vector operator * (const RSparseMapMatrix & A, const R3Vector & b){
//     return A.mult(b);
// }

inline RSparseMapMatrix operator + (const RSparseMapMatrix & A, const RSparseMapMatrix & B){
    RSparseMapMatrix tmp(A);
    return tmp += B;
}

inline RSparseMapMatrix operator - (const RSparseMapMatrix & A, const RSparseMapMatrix & B){
    RSparseMapMatrix tmp(A);
    return tmp -= B;
}

#define DEFINE_SPARSEMAPMATRIX_EXPR_OPERATOR__(OP) \
    inline RSparseMapMatrix operator OP (const RSparseMapMatrix & A, const double & v){\
        return RSparseMapMatrix(A) OP##= v; \
    } \

    DEFINE_SPARSEMAPMATRIX_EXPR_OPERATOR__(+)
    DEFINE_SPARSEMAPMATRIX_EXPR_OPERATOR__(-)
    DEFINE_SPARSEMAPMATRIX_EXPR_OPERATOR__(*)
    DEFINE_SPARSEMAPMATRIX_EXPR_OPERATOR__(/)

#undef DEFINE_SPARSEMAPMATRIX_EXPR_OPERATOR__

#define DEFINE_SPARSEMAPMATRIX_EXPR_OPERATOR__(OP) \
    inline RSparseMapMatrix operator OP (const double & v, const RSparseMapMatrix & A){\
        return RSparseMapMatrix(A) OP##= v; \
    } \

    DEFINE_SPARSEMAPMATRIX_EXPR_OPERATOR__(+)
    DEFINE_SPARSEMAPMATRIX_EXPR_OPERATOR__(*)

#undef DEFINE_SPARSEMAPMATRIX_EXPR_OPERATOR__



/*! Scales a matrix A from left and right vectors such that A -> diag(l) * A * diag(r) */
template< class Vec >
void scaleMatrix (SparseMapMatrix< double, Index > & S,
                  const Vec & l, const Vec & r) {

    if (S.cols() != r.size())
        throwLengthError(EXIT_SPARSE_SIZE, WHERE_AM_I + " " + toStr(S.cols())
                                            + " != " + toStr(r.size()));
    if (S.rows() != l.size())
        throwLengthError(EXIT_SPARSE_SIZE, WHERE_AM_I + " " + toStr(S.rows())
                                            + " != " + toStr(l.size()));

    for (SparseMapMatrix< double, Index >::iterator it = S.begin(); it != S.end(); it ++){
                S.val(it) *= l[S.idx1(it)] * r[S.idx2(it)];
//       int i = S.idx1(it);
//     int j = S.idx2(it);
//     S[i][j] *= (l[i] * r[j]);
    }
    return;
}

/*! Performs a rank 1 update of a matrix such that A -> A + u * v^T */
template< class Vec >
void rank1Update (SparseMapMatrix< double, Index > & S, const Vec & u, const Vec & v) {

    if (S.cols() != v.size())
        throwLengthError(EXIT_SPARSE_SIZE, WHERE_AM_I + " " + toStr(S.cols())
                                + " != " + toStr(v.size()));
    if (S.rows() != u.size())
        throwLengthError(EXIT_SPARSE_SIZE, WHERE_AM_I + " " + toStr(S.rows())
                                + " != " + toStr(u.size()));

    for (SparseMapMatrix< double, Index >::iterator it = S.begin(); it != S.end(); it ++){
        S.val(it) += u[S.idx1(it)] * v[S.idx2(it)];
    }
    return;
}

// template < class ValueType, class IndexType, class V2, class T, class A >
// Vector < V2 > operator * (const SparseMapMatrix< ValueType, IndexType > & S,
//                            const VectorExpr< T, A > & a) {
//     return S * Vector< V2 >(a);
// }

//! Sparse matrix in compressed row storage (CRS) form
/*! Sparse matrix in compressed row storage (CRS) form.
* IF you need native CCS format you need to transpose CRS
* Symmetry type: 0 = nonsymmetric, -1 symmetric lower part, 1 symmetric upper part.*/
template < class ValueType > class SparseMatrix : public MatrixBase{
public:

  /*! Default constructor. Builds invalid sparse matrix */
    SparseMatrix()
        : MatrixBase(), valid_(false), stype_(0), rows_(0), cols_(0){ }

    /*! Copy constructor. */
    SparseMatrix(const SparseMatrix < ValueType > & S)
        : MatrixBase(),
          colPtr_(S.vecColPtr()),
          rowIdx_(S.vecRowIdx()),
          vals_(S.vecVals()), valid_(true), stype_(0){
          rows_ = S.rows();
          cols_ = S.cols();
    }

    /*! Copy constructor. */
    SparseMatrix(const SparseMapMatrix< ValueType, Index > & S)
        : MatrixBase(), valid_(true){
        copy_(S);
    }

    /*! Create Sparsematrix from c-arrays. Can't check for valid ranges, so please be carefull. */
    SparseMatrix(uint dim, Index * colPtr, Index nVals, Index * rowIdx,
                 ValueType * vals, int stype=0)
        : MatrixBase(){
        colPtr_.reserve(dim + 1);
        colPtr_.resize(dim + 1);

        rowIdx_.reserve(nVals);
        rowIdx_.resize(nVals);

        vals_.resize(nVals);
        stype_ = stype;
        rows_ = max(rowIdx_) + 1;
        cols_ = colPtr_.size() - 1;
        THROW_TO_IMPL
//         std::copy(colPtr[0], colPtr[dim], colPtr_[0]);
//         std::copy(rowIdx[0], rowIdx[nVals - 1], rowIdx_[0]);
//         std::copy(vals[0], vals[nVals - 1], & vals_[0]);
    }

    /*! Destructor */
    virtual ~SparseMatrix(){ }

    /*! Copy assignment operator. */
    SparseMatrix < ValueType > & operator = (const SparseMatrix < ValueType > & S){
        if (this != &S){
            colPtr_ = S.vecColPtr();
            rowIdx_ = S.vecRowIdx();
            vals_   = S.vecVals();
            stype_  = S.stype();
            valid_  = true;
            cols_ = S.cols();
            rows_ = S.rows();

        } return *this;
    }

    SparseMatrix < ValueType > & operator = (const SparseMapMatrix< ValueType, Index > & S){
        this->copy_(S);
        return *this;
    }

//     void copy(const SparseMapMatrix< ValueType, Index > & S){
//         this->copy_(S);
//     }



    #define DEFINE_SPARSEMATRIX_UNARY_MOD_OPERATOR__(OP, FUNCT) \
        void FUNCT(int i, int j, ValueType val){ \
            if ((stype_ < 0 && i > j) || (stype_ > 0 && i < j)) return; \
            if (abs(val) > TOLERANCE){ \
                for (int k = colPtr_[i]; k < colPtr_[i + 1]; k ++){ \
                    if (rowIdx_[k] == j) { \
                        vals_[k] OP##= val; return; \
                    } \
                } \
                std::cerr << WHERE_AM_I << " pos " << i << " " << j << " is not part of the sparsity pattern " << std::endl; \
            } \
        }\
        SparseMatrix< ValueType > & operator OP##= (const ValueType & v){\
            vals_ OP##= v; \
            return *this;\
        }\

        DEFINE_SPARSEMATRIX_UNARY_MOD_OPERATOR__(+, addVal)
        DEFINE_SPARSEMATRIX_UNARY_MOD_OPERATOR__(-, subVal)
        DEFINE_SPARSEMATRIX_UNARY_MOD_OPERATOR__(*, mulVal)
        DEFINE_SPARSEMATRIX_UNARY_MOD_OPERATOR__(/, divVal)

    #undef DEFINE_SPARSEMATRIX_UNARY_MOD_OPERATOR__

    SparseMatrix< ValueType > & operator += (const SparseMatrix< ValueType > & A){
        vals_ += A.vecVals();
        return *this;
    }
    SparseMatrix< ValueType > & operator -= (const SparseMatrix< ValueType > & A){
        vals_ -= A.vecVals();
        return *this;
    }

    SparseMatrix< ValueType > & operator += (const ElementMatrix< double > & A){
        if (!valid_) SPARSE_NOT_VALID;
        for (Index i = 0, imax = A.size(); i < imax; i++){
            for (Index j = 0, jmax = A.size(); j < jmax; j++){
                addVal(A.idx(i), A.idx(j), A.getVal(i, j));
            }
        }
        return *this;
    }

    /*! Return this * a  */
    virtual Vector < ValueType > mult(const Vector < ValueType > & a) const {
        if (a.size() < this->cols()){
            throwLengthError(1, WHERE_AM_I + " SparseMatrix size(): " + toStr(this->cols()) + " a.size(): " +
                                toStr(a.size())) ;
        }

        Vector < ValueType > ret(this->rows(), 0.0);

        if (stype_ == 0){
            for (Index i = 0; i < ret.size(); i++){
                for (int j = this->vecColPtr()[i]; j < this->vecColPtr()[i + 1]; j ++){
                    ret[i] += a[this->vecRowIdx()[j]] * this->vecVals()[j];
                }
            }
        } else if (stype_ == -1){
            Index J;
            for (Index i = 0; i < ret.size(); i++){
                for (int j = this->vecColPtr()[i]; j < this->vecColPtr()[i + 1]; j ++){
                    J = this->vecRowIdx()[j];

//                     __MS( i << "  " << J << " " << this->vecVals()[j])
                    ret[i] += a[J] * conj(this->vecVals()[j]);

                    if (J > i){
//                         __MS( J << "  " << i << " " << this->vecVals()[j])
                        ret[J] += a[i] * this->vecVals()[j];
                    }
                }
            }

            //#THROW_TO_IMPL
        } else if (stype_ == 1){
            Index J;
            for (Index i = 0; i < ret.size(); i++){
                for (int j = this->vecColPtr()[i]; j < this->vecColPtr()[i + 1]; j ++){
                    J = this->vecRowIdx()[j];

//                     __MS( i << "  " << J << " " << this->vecVals()[j])
                    ret[i] += a[J] * conj(this->vecVals()[j]);

                    if (J < i){
//                         __MS( J << "  " << i << " " << this->vecVals()[j])
                        ret[J] += a[i] * this->vecVals()[j];
                    }
                }
            }
        }
        return ret;
    }

    /*! Return this.T * a */
    virtual Vector < ValueType > transMult(const Vector < ValueType > & a) const {

        if (a.size() < this->rows()){
            throwLengthError(1, WHERE_AM_I + " SparseMatrix size(): " + toStr(this->rows()) + " a.size(): " +
                                toStr(a.size())) ;
        }

        Vector < ValueType > ret(this->cols(), 0.0);

        if (stype_ == 0){
            for (Index i = 0; i < ret.size(); i++){
                for (int j = this->vecColPtr()[i]; j < this->vecColPtr()[i + 1]; j ++){
                    ret[this->vecRowIdx()[j]] += a[i] * this->vecVals()[j];
                }
            }

        } else if (stype_ == -1){
            THROW_TO_IMPL
        } else if (stype_ ==  1){
            THROW_TO_IMPL
        }
        return ret;
    }

    SparseMatrix< ValueType > & add(const ElementMatrix< double > & A){
        return add(A, ValueType(1.0));
    }

    SparseMatrix< ValueType > & add(const ElementMatrix< double > & A, ValueType scale){
        if (!valid_) SPARSE_NOT_VALID;
        for (Index i = 0, imax = A.size(); i < imax; i++){
            for (Index j = 0, jmax = A.size(); j < jmax; j++){
                addVal(A.idx(i), A.idx(j), scale * A.getVal(i, j));
            }
        }
        return *this;
    }

    void clean(){ for (Index i = 0, imax = nVals(); i < imax; i++) vals_[i] = (ValueType)(0); }

    void clear(){
        colPtr_.clear();
        rowIdx_.clear();
        vals_.clear();
        valid_ = false;
        cols_ = 0;
        rows_ = 0;
    }

    void setVal(int i, int j, ValueType val){
        if (abs(val) > TOLERANCE || 1){
            for (int k = colPtr_[i]; k < colPtr_[i + 1]; k ++){
                if (rowIdx_[k] == j) {
                    vals_[k] = val; return;
                }
            }
            std::cerr << WHERE_AM_I << " pos " << i << " " << j << " is not part of the sparsity pattern " << std::endl;
        }
    }

    /*!Get matrix value at i,j. If i and j is not part of the matrix
     * sparsity pattern return 0 and print a warning.
     * This warning can be disabled by setting warn to false.*/
    ValueType getVal(int i, int j, bool warn=true) const {
        for (int k = colPtr_[i]; k < colPtr_[i + 1]; k ++){
            if (rowIdx_[k] == j) {
                return vals_[k];
            }
        }
        if (warn) std::cerr << WHERE_AM_I << " pos " << i << " "
                    << j << " is not part of the sparsity pattern " << std::endl;
        return ValueType(0);
    }

    void cleanRow(int i){
         for (int k = colPtr_[i]; k < colPtr_[i + 1]; k ++){
            vals_[k] = ValueType(0);
         }
    }

    void cleanCol(int i){
        for (int k = colPtr_[i]; k < colPtr_[i + 1]; k ++){
            for (int j = colPtr_[rowIdx_[k]]; j < colPtr_[rowIdx_[k] + 1]; j ++){
                if (rowIdx_[j] == i) {
                    vals_[j] = ValueType(0);
                }
            }
        }
    }

    void copy_(const SparseMapMatrix< ValueType, Index > & S){

        this->clear();
        Index col = 0, row = 0;
        ValueType val;
        cols_ = S.cols();
        rows_ = S.rows();

        std::vector < std::map < Index, ValueType > > idxMap(S.cols());

        for (typename SparseMapMatrix< ValueType, Index>::const_iterator
            it = S.begin(); it != S.end(); it ++){

            col = S.idx1(it);
            row = S.idx2(it);
            val = S.val(it);
            idxMap[col].insert(std::pair< Index, ValueType >(row, val));
        }

        colPtr_.reserve(S.cols() + 1);
        colPtr_.resize(S.cols() + 1);

        rowIdx_.reserve(S.nVals());
        rowIdx_.resize(S.nVals());

        vals_.resize(S.nVals());
        stype_  = S.stype();

        colPtr_[0] = 0;

        Index colCounter = 0, rowCounter = 0;
        for (typename std::vector < std::map < Index, ValueType > >::iterator
            it = idxMap.begin(); it != idxMap.end(); it++){

            for (typename std::map < Index, ValueType >::iterator
                itR = (*it).begin(); itR != (*it).end(); itR++){

                rowIdx_[rowCounter] = itR->first;
                vals_[rowCounter] = (ValueType)itR->second;
                rowCounter ++;
            }
            colCounter ++;
            colPtr_[colCounter] = rowCounter;
        }
        valid_ = true;
    }

    void buildSparsityPattern(const Mesh & mesh){
        Stopwatch swatch(true);

        colPtr_.resize(mesh.nodeCount() + 1);

        //*** much to slow
        //RSparseMapMatrix S(mesh.nodeCount(), mesh.nodeCount());

        Index col = 0, row = 0;

        // need unique(sort) maybe to slow
//        std::vector < std::vector< Index > > idxMap(mesh.nodeCount());
//         for (std::vector < std::vector< Index > >::iterator mIt = idxMap.begin(); mIt != idxMap.end(); mIt++){
//             (*mIt).reserve(100);
//         }

// using set is by a factor of approx 5 more expensive
        std::vector < std::set< Index > > idxMap(mesh.nodeCount());

        Cell *cell = 0;
        uint nc = 0;

        for (uint c = 0; c < mesh.cellCount(); c ++){
            cell = &mesh.cell(c);
            nc = cell->nodeCount();

            for (uint i = 0; i < nc; i ++){
                for (uint j = 0; j < nc; j ++){
                    row = cell->node(i).id();
                    col = cell->node(j).id();
                    //idxMap[col].push_back(row);
                    idxMap[col].insert(row);
                    //S[col][row] = 1;
                }
            }
        }

        int nVals = 0;
        for (std::vector < std::set< Index > >::iterator mIt = idxMap.begin();
             mIt != idxMap.end(); mIt++){
            //std::sort((*mIt).begin(), (*mIt).end());
            nVals += (*mIt).size();
        }

//         std::cout << "timwe: " << swatch.duration( true) << std::endl;
//         exit(0);

        rowIdx_.reserve(nVals);
        rowIdx_.resize(nVals);
        vals_.resize(nVals);

        colPtr_[0] = 0;
        Index k = 0;
        row = 0;
        for (std::vector < std::set< Index > >::iterator mIt = idxMap.begin(); mIt != idxMap.end(); mIt++){
            for (std::set< Index >::iterator sIt = (*mIt).begin(); sIt != (*mIt).end(); sIt++){
                rowIdx_[k] = (*sIt);
                vals_[k] = (ValueType)0.0;
                k++;
            }
            row++;
            colPtr_[row] = k;
        }
        valid_ = true;
        cols_ = colPtr_.size() - 1;
        rows_ = max(rowIdx_) + 1;
        //** freeing idxMap ist expensive
    }

    void fillStiffnessMatrix(const Mesh & mesh){
        RVector a(mesh.cellCount(), 1.0);
        fillStiffnessMatrix(mesh, a);
    }

    void fillStiffnessMatrix(const Mesh & mesh, const RVector & a){
        clean();
        buildSparsityPattern(mesh);
        ElementMatrix < double > A_l;

        for (uint i = 0; i < mesh.cellCount(); i ++){
            A_l.ux2uy2uz2(mesh.cell(i));
            A_l *= a[mesh.cell(i).id()];
            *this += A_l;
        }
    }
    void fillMassMatrix(const Mesh & mesh){
        RVector a(mesh.cellCount(), 1.0);
        fillMassMatrix(mesh, a);
    }

    void fillMassMatrix(const Mesh & mesh, const RVector & a){
        clean();
        buildSparsityPattern(mesh);
        ElementMatrix < double > A_l;

        for (uint i = 0; i < mesh.cellCount(); i ++){
            A_l.u2(mesh.cell(i));
            A_l *= a[mesh.cell(i).id()];
            *this += A_l;
        }
    }

    /*! symmetric type. 0 = nonsymmetric, -1 symmetric lower part, 1 symmetric upper part.*/
    inline int stype() const {return stype_;}

    inline int * colPtr() { if (valid_) return &colPtr_[0]; else SPARSE_NOT_VALID;  return 0; }
    inline const int & colPtr() const { if (valid_) return colPtr_[0]; else SPARSE_NOT_VALID; return colPtr_[0]; }
    inline const std::vector < int > & vecColPtr() const { return colPtr_; }

    inline int * rowIdx() { if (valid_) return &rowIdx_[0]; else SPARSE_NOT_VALID; return 0; }
    inline const int & rowIdx() const { if (valid_) return rowIdx_[0]; else SPARSE_NOT_VALID; return rowIdx_[0]; }
    inline const std::vector < int > & vecRowIdx() const { return rowIdx_; }

    inline ValueType * vals() { if (valid_) return &vals_[0]; else SPARSE_NOT_VALID; return 0; }
//     inline const ValueType * vals() const { if (valid_) return &vals_[0]; else SPARSE_NOT_VALID; return 0; }
//     inline const ValueType & vals() const { if (valid_) return vals_[0]; else SPARSE_NOT_VALID; return vals_[0]; }
    inline const Vector < ValueType > & vecVals() const { return vals_; }
    inline Vector < ValueType > & vecVals() { return vals_; }

    inline Index size() const { return colPtr_.size() - 1; }
    inline Index nVals() const { return vals_.size(); }
    inline Index cols() const { return cols_; }
    inline Index rows() const { return rows_; }
    inline Index nCols() const { return cols(); }
    inline Index nRows() const { return rows(); }

    void save(const std::string & fileName) const {
        if (!valid_) SPARSE_NOT_VALID;
        std::fstream file;
        openOutFile(fileName, & file);

        file.setf(std::ios::scientific, std::ios::floatfield);
        file.precision(14);

        for (Index i = 0; i < size(); i++){
            for (SIndex j = colPtr_[i]; j < colPtr_[i + 1]; j ++){
                file << i << "\t" << rowIdx_[j]
                          << "\t" << vals_[j] << std::endl;
            }
        }
        file.close();
    }

    bool valid() const { return valid_; }

protected:

    // int to be cholmod compatible!!!!!!!!

    std::vector < int > colPtr_;
    std::vector < int > rowIdx_;
    Vector < ValueType > vals_;

    bool valid_;
    int stype_;
    Index rows_;
    Index cols_;
};

template < class ValueType >
SparseMatrix< ValueType > operator + (const SparseMatrix< ValueType > & A,
                                      const SparseMatrix< ValueType > & B){
    SparseMatrix< ValueType > ret(A);
    return ret += B;
}

template < class ValueType >
SparseMatrix< ValueType > operator - (const SparseMatrix< ValueType > & A,
                                      const SparseMatrix< ValueType > & B){
    SparseMatrix< ValueType > ret(A);
    return ret -= B;
}

template < class ValueType >
SparseMatrix < ValueType > operator * (const SparseMatrix < ValueType > & A,
                                       const ValueType & b){
    SparseMatrix< ValueType > ret(A);
    return ret *= b;
}

template < class ValueType >
SparseMatrix < ValueType > operator * (const ValueType & b,
                                       const SparseMatrix < ValueType > & A){
    SparseMatrix< ValueType > ret(A);
    return ret *= b;
}

inline RVector operator * (const RSparseMatrix & A, const RVector & b){return A.mult(b);}
inline RVector transMult(const RSparseMatrix & A, const RVector & b){return A.transMult(b);}

inline CVector operator * (const CSparseMatrix & A, const CVector & b){return A.mult(b);}
inline CVector operator * (const CSparseMatrix & A, const RVector & b){return A.mult(toComplex(b));}
inline CVector transMult(const CSparseMatrix & A, const CVector & b){return A.transMult(b);}
inline CVector transMult(const CSparseMatrix & A, const RVector & b){return A.transMult(toComplex(b));}

inline CSparseMatrix operator + (const CSparseMatrix & A, const RSparseMatrix & B){
    CSparseMatrix ret(A);
    ret.vecVals() += toComplex(B.vecVals());
    return ret;
}

} // namespace GIMLI

#endif //GIMLI_SPARSEMATRIX__H
