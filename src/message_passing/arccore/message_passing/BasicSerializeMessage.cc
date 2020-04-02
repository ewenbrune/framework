﻿// -*- tab-width: 2; indent-tabs-mode: nil; coding: utf-8-with-signature -*-
/*---------------------------------------------------------------------------*/
/* BasicSerializeMessage.cc                                    (C) 2000-2020 */
/*                                                                           */
/* Message utilisant un BasicSerializeMessage.                               */
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

#include "arccore/message_passing/BasicSerializeMessage.h"
#include "arccore/base/FatalErrorException.h"
#include "arccore/base/Ref.h"

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

namespace Arccore::MessagePassing
{

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

ePointToPointMessageType BasicSerializeMessage::
_toP2PType(eMessageType mtype)
{
  switch(mtype){
  case MT_Send: return MsgSend;
  case MT_Recv: return MsgReceive;
  case MT_Broadcast: return MsgSend;
  }
  ARCCORE_FATAL("Unsupported value '{0}'",mtype);
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

ISerializeMessage::eMessageType BasicSerializeMessage::
_toMessageType(ePointToPointMessageType type)
{
  switch(type){
  case MsgSend: return MT_Send;
  case MsgReceive: return MT_Recv;
  }
  ARCCORE_FATAL("Unsupported value '{0}'",type);
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

BasicSerializeMessage::
BasicSerializeMessage(Int32 orig_rank,Int32 dest_rank,eMessageType mtype)
: BasicSerializeMessage(MessageRank(orig_rank),MessageRank(dest_rank),_toP2PType(mtype))
{
  _init();
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

BasicSerializeMessage::
BasicSerializeMessage(Int32 orig_rank,Int32 dest_rank,eMessageType mtype,
                      BasicSerializer* s)
: BasicSerializeMessage(orig_rank,dest_rank,mtype)
{
  m_buffer = s;
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

BasicSerializeMessage::
BasicSerializeMessage(MessageRank orig_rank,MessageRank dest_rank,
                      ePointToPointMessageType type)
: m_orig_rank(orig_rank)
, m_dest_rank(dest_rank)
, m_old_message_type(_toMessageType(type))
, m_message_type(type)
, m_is_send(false)
{
  _init();
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

BasicSerializeMessage::
~BasicSerializeMessage()
{
  delete m_buffer;
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

void BasicSerializeMessage::
_init()
{
  switch(m_old_message_type){
  case MT_Send:
  case MT_Broadcast:
    m_is_send = true;
    break;
  case MT_Recv:
    m_is_send = false;
    break;
  }
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

Ref<ISerializeMessage> BasicSerializeMessage::
create(MessageRank source,MessageRank destination,ePointToPointMessageType type)
{
  ISerializeMessage* m = new BasicSerializeMessage(source,destination,type);
  return makeRef(m);
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

} // End namespace Arccore::MessagePassing

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
