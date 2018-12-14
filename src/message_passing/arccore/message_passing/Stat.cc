﻿// -*- tab-width: 2; indent-tabs-mode: nil; coding: utf-8-with-signature -*-
/*---------------------------------------------------------------------------*/
/* Stat.cc                                                     (C) 2000-2018 */
/*                                                                           */
/* Statistiques sur le parallélisme.                                         */
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

#include "arccore/message_passing/Stat.h"

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

namespace Arccore
{

namespace MessagePassing
{

void OneStat::
print(std::ostream& o)
{
  Int64 div_time = static_cast<Int64>(m_total_time*1000.0);
  if (div_time==0)
    div_time = 1;
  if (m_nb_msg>0){
    Int64 average_time = (Int64)(m_total_time/(Real)m_nb_msg);
    o << " MPIStat " << m_name << "     :" << m_nb_msg << " messages" << '\n';
    o << " MPIStat " << m_name << "     :" << m_total_size << " bytes ("
      << m_total_size / div_time << " Kb/s) (average size "
      << m_total_size / m_nb_msg << " bytes)" << '\n';
    o << " MPIStat " << m_name << " Time: " << m_total_time << " seconds"
      << " (avg=" << average_time << ")" << '\n';
  }
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

Stat::
Stat()
: m_is_enabled(true)
{
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

Stat::
~Stat()
{
  for( auto i : m_list ){
    OneStat* os = i.second;
    delete os;
  }
  m_list.clear();
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

void Stat::
add(const String& name,double elapsed_time,Int64 msg_size)
{
  if (!m_is_enabled)
    return;
  OneStat* os = _find(name);
  os->addMessage(msg_size,elapsed_time);
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

void Stat::
print(std::ostream& o)
{
  for( auto i : m_list ){
    OneStat* os = i.second;
    os->print(o);
  }
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

} // End namespace MessagePassing

} // End namespace Arccore

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
