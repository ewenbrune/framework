﻿// -*- tab-width: 2; indent-tabs-mode: nil; coding: utf-8-with-signature -*-
//-----------------------------------------------------------------------------
// Copyright 2000-2025 CEA (www.cea.fr) IFPEN (www.ifpenergiesnouvelles.com)
// See the top-level COPYRIGHT file for details.
// SPDX-License-Identifier: Apache-2.0
//-----------------------------------------------------------------------------
#ifndef ALIEN_COMMON_BUILDER_SCALAR_DIRECTMATRIXBUILDER_H
#define ALIEN_COMMON_BUILDER_SCALAR_DIRECTMATRIXBUILDER_H
/* Author : havep at Tue Aug 28 13:56:24 2012
 * Generated by createNew
 */

#include <map>

#include <alien/data/IMatrix.h>
#include <alien/data/utils/MatrixElement.h>

#define USE_VMAP

#ifdef USE_VMAP
#include <alien/utils/VMap.h>
#endif /* USE_VMAP */

#include <arccore/message_passing/MessagePassingGlobal.h>

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

namespace Alien
{

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

template <typename Scalar>
class SimpleCSRMatrix;

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

struct DirectMatrixOptions
{
  enum ResetFlag
  {
    eNoReset,
    eResetValues,
    eResetProfile,
    eResetAllocation
  };
  enum ReserveFlag
  {
    eResetReservation,
    eExtendReservation
  };
  enum SymmetricFlag
  {
    eSymmetric,
    eUnSymmetric
  };
};

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

namespace Common
{

  /*---------------------------------------------------------------------------*/
  /*---------------------------------------------------------------------------*/

  class ALIEN_EXPORT DirectMatrixBuilder
  {
   public:
    using ResetFlag = DirectMatrixOptions::ResetFlag;
    using ReserveFlag = DirectMatrixOptions::ReserveFlag;
    using SymmetricFlag = DirectMatrixOptions::SymmetricFlag;

   public:
    using MatrixElement = MatrixElementT<DirectMatrixBuilder>;

   public:
    DirectMatrixBuilder(IMatrix& matrix, ResetFlag reset_flag,
                        SymmetricFlag symmetric_flag = SymmetricFlag::eSymmetric);

    virtual ~DirectMatrixBuilder();

    DirectMatrixBuilder(DirectMatrixBuilder&) = delete;
    DirectMatrixBuilder(DirectMatrixBuilder&&) = delete;
    DirectMatrixBuilder& operator=(const DirectMatrixBuilder&) = delete;
    DirectMatrixBuilder& operator=(DirectMatrixBuilder&&) = delete;

   public:
    MatrixElement operator()(const Integer iIndex, const Integer jIndex)
    {
      return MatrixElement(iIndex, jIndex, *this);
    }

    void reserve(Arccore::Integer n, ReserveFlag flag = ReserveFlag::eResetReservation);

    void reserve(Arccore::ConstArrayView<Arccore::Integer> indices, Arccore::Integer n,
                 ReserveFlag flag = ReserveFlag::eResetReservation);

    void allocate();

    void addData(Arccore::Integer iIndex, Arccore::Integer jIndex, Arccore::Real value);

    void addData(Arccore::Integer iIndex, Arccore::Real factor,
                 Arccore::ConstArrayView<Arccore::Integer> jIndexes,
                 Arccore::ConstArrayView<Arccore::Real> jValues);

    void setData(Arccore::Integer iIndex, Arccore::Integer jIndex, Arccore::Real value);

    void setData(Arccore::Integer iIndex, Arccore::Real factor,
                 Arccore::ConstArrayView<Arccore::Integer> jIndexes,
                 Arccore::ConstArrayView<Arccore::Real> jValues);

    void finalize();

    void squeeze();

    [[nodiscard]] Arccore::String stats() const;
    [[nodiscard]] Arccore::String stats(Arccore::IntegerConstArrayView ids) const;

   protected:
    IMatrix& m_matrix;

    SimpleCSRMatrix<Real>* m_matrix_impl;

    Integer m_local_offset, m_global_size, m_local_size;
    Integer m_col_global_size;
    ArrayView<Integer> m_row_starts;
    ArrayView<Integer> m_cols;
    ArrayView<Real> m_values;
    UniqueArray<Integer> m_row_sizes;

    ResetFlag m_reset_flag;
    bool m_allocated;
    bool m_finalized;
    bool m_symmetric_profile;

    Integer m_nproc;
    IMessagePassingMng* m_parallel_mng;
    ITraceMng* m_trace;

#ifdef USE_VMAP
    typedef VMap<Integer, Real> ColValueData;
#else /* USE_VMAP */
    typedef std::map<Integer, Real> ColValueData;
#endif /* USE_VMAP */
    typedef std::map<Integer, ColValueData> ExtraRows;
    ExtraRows m_extras;

   private:
    void computeProfile(Arccore::ConstArrayView<Arccore::Integer> sizes);
    void updateProfile(Arccore::UniqueArray<Arccore::Integer>& row_starts,
                       Arccore::UniqueArray<Arccore::Integer>& cols,
                       Arccore::UniqueArray<Arccore::Real>& values);

    class IndexEnumerator;
    class FullEnumerator;

    template <typename Enumerator>
    void _stats(std::ostream& o, const Enumerator& e) const;

    void _startTimer() {}
    void _stopTimer() {}
  };

  /*---------------------------------------------------------------------------*/
  /*---------------------------------------------------------------------------*/

} // namespace Common

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

} // namespace Alien

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

#endif /* ALIEN_COMMON_BUILDER_SCALAR_DIRECTMATRIXBUILDER_H */
