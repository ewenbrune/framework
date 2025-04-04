﻿// -*- tab-width: 2; indent-tabs-mode: nil; coding: utf-8-with-signature -*-
//-----------------------------------------------------------------------------
// Copyright 2000-2025 CEA (www.cea.fr) IFPEN (www.ifpenergiesnouvelles.com)
// See the top-level COPYRIGHT file for details.
// SPDX-License-Identifier: Apache-2.0
//-----------------------------------------------------------------------------
#pragma once

#include <vector>

#include <alien/kernels/sycl/SYCLPrecomp.h>
#include <alien/utils/Precomp.h>
#include <alien/utils/Trace.h>

#include <arccore/message_passing/Messages.h>
#include <arccore/message_passing/Request.h>

#include <alien/kernels/simple_csr/SendRecvOp.h>

#include <alien/kernels/sycl/data/SYCLEnv.h>
#include <alien/kernels/sycl/data/SYCLEnvInternal.h>

namespace Alien::SYCLInternal
{

#ifndef USE_SYCL2020
  using namespace cl ;
#endif

template <typename ValueT>
class SYCLSendRecvOp : public Alien::SimpleCSRInternal::IASynchOp
{
 public:
  // clang-format off
  typedef ValueT                                ValueType ;
  typedef sycl::buffer<ValueType, 1>            ValueBufferType ;

  typedef sycl::buffer<int>                     IndexBufferType ;
  typedef std::unique_ptr<IndexBufferType>      IndexBufferPtrType ;
  // clang-format on

  SYCLSendRecvOp(ValueBufferType& send_buffer,
                 const Alien::SimpleCSRInternal::CommInfo& send_info,
                 IndexBufferType& send_ids,
                 Alien::SimpleCSRInternal::CommProperty::ePolicyType send_policy,
                 ValueBufferType& recv_buffer,
                 const Alien::SimpleCSRInternal::CommInfo& recv_info,
                 IndexBufferType& recv_ids,
                 Alien::SimpleCSRInternal::CommProperty::ePolicyType recv_policy,
                 IMessagePassingMng* mng,
                 Arccore::ITraceMng* trace_mng)
  : m_send_buffer(send_buffer)
  , m_send_info(send_info)
  , m_send_ids(send_ids)
  , m_send_policy(send_policy)
  , m_recv_buffer(recv_buffer)
  , m_recv_info(recv_info)
  , m_recv_ids(recv_ids)
  , m_recv_policy(recv_policy)
  , m_parallel_mng(mng)
  , m_trace(trace_mng)
  {}

  virtual ~SYCLSendRecvOp()
  {
#ifdef USE_SYCL_USM
    auto& queue = SYCLEnv::instance()->internal()->queue();
    sycl::free(m_rbuffer, queue);
    sycl::free(m_sbuffer, queue);
#endif
  }

  void start(bool insitu = false)
  {
    //alien_debug([&] {cout() << "SYCLSendRecvOP START "<<m_send_policy;});
    //Universe().traceMng()->flush() ;

    // clang-format off
    auto env           = SYCLEnv::instance();
    auto& queue        = env->internal()->queue();
    auto total_threads = env->maxNumThreads() ;
    // clang-format on
    if (m_recv_policy == Alien::SimpleCSRInternal::CommProperty::ASynch) {
      m_recv_request.resize(m_recv_info.m_num_neighbours);
      Integer total_nb_recv_ids = m_recv_info.m_ids_offset[m_recv_info.m_num_neighbours] - m_recv_info.m_ids_offset[0];
#ifdef USE_SYCL_USM
      m_rbuffer = sycl::malloc_shared<ValueT>(total_nb_recv_ids, queue);
#else
      m_rbuffer.resize(total_nb_recv_ids);
#endif
      for (Integer i = 0; i < m_recv_info.m_num_neighbours; ++i) {
        Integer off = m_recv_info.m_ids_offset[i] - m_recv_info.m_ids_offset[0];
        Integer size = m_recv_info.m_ids_offset[i + 1] - off;
#ifdef USE_SYCL_USM
        ValueT* ptr = m_rbuffer + off;
#else
        ValueT* ptr = m_rbuffer.data() + off;
#endif
        Integer rank = m_recv_info.m_ranks[i];

        m_recv_request[i] =
        Arccore::MessagePassing::mpReceive(m_parallel_mng,
                                           ArrayView<ValueT>(size, ptr),
                                           rank,
                                           false);
      }
    }
    if (m_send_policy == Alien::SimpleCSRInternal::CommProperty::ASynch)
      m_send_request.resize(m_send_info.m_num_neighbours);
    if (m_send_info.m_ids.size()) {
      std::size_t total_nb_send_ids = m_send_info.m_ids_offset[m_send_info.m_num_neighbours] - m_send_info.m_ids_offset[0];
#ifdef USE_SYCL_USM
      m_sbuffer = sycl::malloc_shared<ValueT>(total_nb_send_ids, queue);
#else
      m_sbuffer.resize(total_nb_send_ids);
#endif
      {
#ifdef USE_SYCL_USM
        sycl::buffer<ValueType> sbuffer{ { sycl::buffer_allocation::empty_view(m_sbuffer, queue.get_device()) }, total_nb_send_ids };
#else
        sycl::buffer<ValueType> sbuffer(m_sbuffer.data(), total_nb_send_ids);
#endif
        // clang-format off
        queue.submit([&](sycl::handler& cgh)
                     {
                       auto access_send_buffer = m_send_buffer.template get_access<sycl::access::mode::read>(cgh);
                       auto access_ids         = m_send_ids.template get_access<sycl::access::mode::read>(cgh);
                       auto access_sbuffer = sycl::accessor(sbuffer, cgh, sycl::write_only, sycl::property::no_init{});

                       cgh.parallel_for<class vector_mult_send>(sycl::range<1>{total_threads},
                                                                [=] (sycl::item<1> itemId)
                                                                {
                                                                   auto id = itemId.get_id(0);
                                                                   for( auto i=id; i<total_nb_send_ids; i+=total_threads)
                                                                     access_sbuffer[i] = access_send_buffer[access_ids[i]];
                                                                });
                     });
        // clang-format on
      }
      //for (Integer i = 0; i < total_nb_send_ids; ++i) {
      //  alien_debug([&] {cout() << "MPI SEND["<<i<<"]="<<m_sbuffer[i];}) ;;
      //}
    }
    for (Integer i = 0; i < m_send_info.m_num_neighbours; ++i) {
      Integer off = m_send_info.m_ids_offset[i];
      Integer nb_send_ids = m_send_info.m_ids_offset[i + 1] - off;
#ifdef USE_SYCL_USM
      ValueT const* ptr = m_sbuffer + off;
#else
      ValueT const* ptr = m_sbuffer.data() + off;
#endif
      Integer rank = m_send_info.m_ranks[i];
      if (m_send_policy == Alien::SimpleCSRInternal::CommProperty::ASynch)
        m_send_request[i] = Arccore::MessagePassing::mpSend(m_parallel_mng,
                                                            ConstArrayView<ValueT>(nb_send_ids, ptr), rank, false);
      else
        Arccore::MessagePassing::mpSend(m_parallel_mng,
                                        ConstArrayView<ValueT>(nb_send_ids, ptr), rank);
    }

    //alien_debug([&] {cout()<<"END SYCLSendRecvOP START" ; });
    //Universe().traceMng()->flush() ;
  }

  void end(bool insitu = false)
  {
    //alien_debug([&] {cout() << "SYCLSendRecvOP END : "<<m_recv_policy;});
    //Universe().traceMng()->flush() ;

    // clang-format off
    auto env           = SYCLEnv::instance();
    auto& queue        = env->internal()->queue();
    auto total_threads = env->maxNumThreads() ;
    // clang-format on
    if (m_recv_policy == Alien::SimpleCSRInternal::CommProperty::ASynch) {
      Arccore::MessagePassing::mpWaitAll(m_parallel_mng, m_recv_request);

      //Arccore::Integer total_recv_ids = m_recv_info.m_ids_offset[m_recv_info.m_num_neighbours] - m_recv_info.m_ids_offset[0];
      //for (Integer i = 0; i < total_recv_ids; ++i) {
      //  alien_debug([&] {cout() << "MPI RECV["<<i<<"]="<<m_rbuffer[i];});
      //}
    }
    else {
      if (m_recv_info.m_ids.size()) {
        Arccore::Integer total_recv_ids = m_recv_info.m_ids_offset[m_recv_info.m_num_neighbours] - m_recv_info.m_ids_offset[0];
#ifdef USE_SYCL_USM
        m_rbuffer = sycl::malloc_shared<ValueT>(total_recv_ids, queue);
#else
        m_rbuffer.resize(total_recv_ids);
#endif
      }
      for (Integer i = 0; i < m_recv_info.m_num_neighbours; ++i) {
        Integer off = m_recv_info.m_ids_offset[i];
        Integer size = m_recv_info.m_ids_offset[i + 1] - off;
#ifdef USE_SYCL_USM
        ValueT* ptr = m_rbuffer + off;
#else
        ValueT* ptr = m_rbuffer.data() + off;
#endif
        Integer rank = m_recv_info.m_ranks[i];
        Arccore::MessagePassing::mpReceive(m_parallel_mng, ArrayView<ValueT>(size, ptr), rank);
      }
    }
    if (m_recv_info.m_ids.size()) {
      std::size_t total_nb_recv_ids = m_recv_info.m_ids_offset[m_recv_info.m_num_neighbours] - m_recv_info.m_ids_offset[0];

      {
#ifdef USE_SYCL_USM
        sycl::buffer<ValueType> rbuffer{ { sycl::buffer_allocation::view(m_rbuffer, queue.get_device()) }, total_nb_recv_ids };
#else
        sycl::buffer<ValueType> rbuffer(m_rbuffer.data(), total_nb_recv_ids);
#endif
        // clang-format off
        queue.submit([&](sycl::handler& cgh)
                     {
                       //auto access_recv_buffer = m_recv_buffer.template get_access<sycl::access::mode::read_write>(cgh);
                       //auto access_ids         = m_recv_ids.template get_access<sycl::access::mode::read>(cgh);
#ifdef USE_HIPSYCL
                       sycl::accessor<ValueType> access_recv_buffer{m_recv_buffer, cgh, sycl::write_only, sycl::property::no_init{}};
#endif
#ifdef USE_ONEAPI
                       auto access_recv_buffer = sycl::accessor(m_recv_buffer, cgh, sycl::write_only, sycl::property::no_init{});
#endif
#ifdef USE_ACPPSYCL
                       auto access_recv_buffer = sycl::accessor(m_recv_buffer, cgh, sycl::write_only, sycl::property::no_init{});
#endif


                       sycl::accessor<ValueType> access_rbuffer{rbuffer, cgh};

                       cgh.parallel_for<class vector_mult_recv>(sycl::range<1>{total_threads},
                                                                [=] (sycl::item<1> itemId)
                                                                {
                                                                 auto id = itemId.get_id(0);
                                                                 for(auto i=id;i<total_nb_recv_ids;i += total_threads)
                                                                   access_recv_buffer[i] = access_rbuffer[i];
                                                                });
                     });
        // clang-format on
      }
    }
    if (m_send_policy == Alien::SimpleCSRInternal::CommProperty::ASynch) {
      Arccore::MessagePassing::mpWaitAll(m_parallel_mng, m_send_request);
    }

    //alien_debug([&] {cout() << "AFTER SYCLSendRecvOP END : "<<m_recv_policy;});
    //Universe().traceMng()->flush() ;
  }

  void upperRecv(bool insitu = true) 
  {
  }
  
  void upperSend() 
  {
  }
  
  void lowerRecv(bool insitu = true)
  {
  }
  
  void lowerSend()
  {
  
  }
 private:
  // clang-format off
  ValueBufferType&                                       m_send_buffer;
  const Alien::SimpleCSRInternal::CommInfo&              m_send_info;
  IndexBufferType&                                       m_send_ids;
  Alien::SimpleCSRInternal::CommProperty::ePolicyType    m_send_policy;

  ValueBufferType&                                       m_recv_buffer;
  const Alien::SimpleCSRInternal::CommInfo&              m_recv_info;
  IndexBufferType&                                       m_recv_ids;
  Alien::SimpleCSRInternal::CommProperty::ePolicyType    m_recv_policy;
#ifdef USE_SYCL_USM
  ValueT*                                                m_rbuffer      = nullptr;
  ValueT*                                                m_sbuffer      = nullptr;
#else
  std::vector<ValueT>                                    m_rbuffer ;
  std::vector<ValueT>                                    m_sbuffer ;
#endif
  Arccore::Integer                                       m_unknowns_num = 0;
  Arccore::MessagePassing::IMessagePassingMng*           m_parallel_mng = nullptr;
  Arccore::ITraceMng*                                    m_trace        = nullptr;
  Arccore::UniqueArray<Arccore::MessagePassing::Request> m_recv_request;
  Arccore::UniqueArray<Arccore::MessagePassing::Request> m_send_request;
  // clang-format on
};

} // namespace Alien::SYCLInternal
