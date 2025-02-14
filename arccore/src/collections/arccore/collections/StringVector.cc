﻿// -*- tab-width: 2; indent-tabs-mode: nil; coding: utf-8-with-signature -*-
//-----------------------------------------------------------------------------
// Copyright 2000-2025 CEA (www.cea.fr) IFPEN (www.ifpenergiesnouvelles.com)
// See the top-level COPYRIGHT file for details.
// SPDX-License-Identifier: Apache-2.0
//-----------------------------------------------------------------------------
/*---------------------------------------------------------------------------*/
/* StringVector.cc                                             (C) 2000-2025 */
/*                                                                           */
/* Liste de 'String'.                                                        */
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

#include "arccore/collections/StringVector.h"

#include "arccore/base/String.h"
#include "arccore/collections/Array.h"

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

namespace Arcane
{

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

class StringVector::Impl
{
 public:

  UniqueArray<String> m_values;
};

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

StringVector::
StringVector(const StringVector& rhs)
{
  if (rhs.m_p)
    m_p = new Impl(*rhs.m_p);
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

StringVector::
StringVector(StringVector&& rhs) noexcept
: m_p(rhs.m_p)
{
  rhs.m_p = nullptr;
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

StringVector& StringVector::
operator=(const StringVector& rhs)
{
  if (&rhs != this) {
    Impl* new_mp = nullptr;
    if (rhs.m_p)
      new_mp = new Impl(*rhs.m_p);
    delete m_p;
    m_p = new_mp;
  }
  return (*this);
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

StringVector::
~StringVector()
{
  delete m_p;
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

void StringVector::
_checkNeedCreate()
{
  if (!m_p)
    m_p = new Impl();
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

Int32 StringVector::
size() const
{
  return (m_p ? m_p->m_values.size() : 0);
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

void StringVector::
add(const String& str)
{
  _checkNeedCreate();
  m_p->m_values.add(str);
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

String StringVector::
operator[](Int32 index) const
{
  ARCCORE_CHECK_POINTER(m_p);
  return m_p->m_values[index];
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

} // namespace Arccore

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
