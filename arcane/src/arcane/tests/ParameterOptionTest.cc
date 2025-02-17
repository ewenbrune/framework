﻿// -*- tab-width: 2; indent-tabs-mode: nil; coding: utf-8-with-signature -*-
//-----------------------------------------------------------------------------
// Copyright 2000-2025 CEA (www.cea.fr) IFPEN (www.ifpenergiesnouvelles.com)
// See the top-level COPYRIGHT file for details.
// SPDX-License-Identifier: Apache-2.0
//-----------------------------------------------------------------------------
/*---------------------------------------------------------------------------*/
/* ParameterOptionTest.cc                                (C) 2000-2025 */
/*                                                                           */
/* Service de test de ParameterOption.                                 */
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

#include "arcane/utils/ParameterList.h"

#include "arcane/core/BasicUnitTest.h"
#include "arcane/utils/internal/ParameterOption.h"

#include "arcane/tests/ParameterOptionTest_axl.h"

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

namespace ArcaneTest
{

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

using namespace Arcane;

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

class ParameterOptionTest
: public ArcaneParameterOptionTestObject
{
 public:

  explicit ParameterOptionTest(const ServiceBuildInfo& sbi);
  ~ParameterOptionTest();

 public:

  void initializeTest() override;
  void executeTest() override;
};

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

ARCANE_REGISTER_SERVICE_PARAMETEROPTIONTEST(ParameterOptionTest, ParameterOptionTest);

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

ParameterOptionTest::
ParameterOptionTest(const ServiceBuildInfo& sbi)
: ArcaneParameterOptionTestObject(sbi)
{
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

ParameterOptionTest::
~ParameterOptionTest()
= default;

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

void ParameterOptionTest::
initializeTest()
{

}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

void ParameterOptionTest::
executeTest()
{
  ParameterOptionElementsCollection poec;

  struct POption
  {
    String param;
    String value;
  };

  POption option0{"module/option0", "0 0"};

  const UniqueArray<POption> p_options{
    {"//module/option1", "5"},
    {"//module/option2", "aa"},
    {"//module/option2[2]", "bb"}
  };

  poec.addElement(option0.param, option0.value);
  for (const auto& [param, value] : p_options) {
    poec.addParameter(param, value);
  }

  {
    const String addr = "module/option1";
    const String value = poec.value(ParameterOptionAddr(addr.view())).value();
    if (value != "5") {
      ARCANE_FATAL("Test 1");
    }
  }

  {
    const String addr = "noexist/option";
    if (poec.value(ParameterOptionAddr(addr.view())).has_value()) {
      ARCANE_FATAL("Test 2");
    }
  }

  {
    const String addr = "module/option0";
    if (!poec.isExistAddr(ParameterOptionAddr(addr.view()))) {
      ARCANE_FATAL("Test 3");
    }
  }

  {
    const String addr = "module/option2";
    ParameterOptionAddr addr_option(addr.view());

    // En correspondance parfaite, il n'y en a qu'un seul.
    if (poec.countAddr(addr_option) != 1) {
      ARCANE_FATAL("Test 4.1");
    }

    addr_option.lastAddrPart()->setIndex(2);

    // Avec l'index 2, il n'y en a aussi qu'un seul.
    if (poec.countAddr(addr_option) != 1) {
      ARCANE_FATAL("Test 4.2");
    }

    addr_option.lastAddrPart()->setIndex(ParameterOptionAddrPart::ANY_INDEX);

    // Avec un ANY_INDEX à la fin, il y en a les deux.
    if (poec.countAddr(addr_option) != 2) {
      ARCANE_FATAL("Test 4.3");
    }
  }

  {
    const String addr = "module/option2";

    ParameterOptionAddr addr_option(addr.view());
    addr_option.lastAddrPart()->setIndex(ParameterOptionAddrPart::GET_INDEX);

    UniqueArray<Integer> index;
    poec.getIndexInAddr(addr_option, index);

    if (index != UniqueArray<Integer>{1, 2}) {
      ARCANE_FATAL("Test 5");
    }
  }

  {
    const String addr1 = "test/option2";
    const String addr2 = "test/option2";

    ParameterOptionAddr addr1_option(addr1.view());
    ParameterOptionAddr addr2_option(addr2.view());
    addr1_option.addAddrPart(new ParameterOptionAddrPart());
    if (addr1_option != addr2_option) {
      ARCANE_FATAL("Test 6.1");
    }
    if (addr2_option != addr1_option) {
      ARCANE_FATAL("Test 6.2");
    }
  }

  {
    const String addr1 = "test/option2";
    const String addr2 = "test";

    ParameterOptionAddr addr1_option(addr1.view());
    ParameterOptionAddr addr2_option(addr2.view());

    if (addr1_option == addr2_option) {
      ARCANE_FATAL("Test 7");
    }
  }

  {
    const String addr1 = "test/option2";
    const String addr2 = "test/option2[2]";

    ParameterOptionAddr addr1_option(addr1.view());
    ParameterOptionAddr addr2_option(addr2.view());

    if (addr1_option == addr2_option) {
      ARCANE_FATAL("Test 8");
    }
  }

  {
    const String addr1 = "test/option2[3]/option[10]";
    const String addr2 = "test/option2/option";

    ParameterOptionAddr addr1_option(addr1.view());
    ParameterOptionAddr addr2_option(addr2.view());

    addr2_option.addrPart(0)->setIndex(ParameterOptionAddrPart::GET_INDEX);
    addr2_option.addrPart(1)->setIndex(ParameterOptionAddrPart::GET_INDEX);
    addr2_option.addrPart(2)->setIndex(ParameterOptionAddrPart::GET_INDEX);

    // On a trois GET_INDEX.
    if (addr2_option.nbIndexToGetInAddr() != 3) {
      ARCANE_FATAL("Test 9.1");
    }

    UniqueArray<Integer> index(3);
    UniqueArray<Integer> result{1, 3, 10};
    bool ret = addr1_option.getIndexInAddr(addr2_option, index.view());

    if (!ret) {
      ARCANE_FATAL("Test 9.2");
    }

    if (index != result) {
      ARCANE_FATAL("Test 9.3");
    }
  }

  {
    const String addr1 = "test/option2";
    const String addr2 = "test/option2";

    ParameterOptionAddr addr1_option(addr1.view());
    ParameterOptionAddr addr2_option(addr2.view());

    addr1_option.lastAddrPart()->setIndex(ParameterOptionAddrPart::ANY_INDEX);
    addr2_option.lastAddrPart()->setIndex(ParameterOptionAddrPart::GET_INDEX);

    UniqueArray<Integer> index(1);
    bool ret = addr1_option.getIndexInAddr(addr2_option, index.view());

    // Impossible de faire un GET_INDEX sur un ANY_INDEX.
    if (ret) {
      ARCANE_FATAL("Test 10");
    }
  }

  // Il y a déjà un ARCANE_ASSERT qui vérifie ça.
#ifndef ARCANE_DEBUG_ASSERT
  {
    const String addr1 = "test/option2";
    const String addr2 = "test/option2";

    ParameterOptionAddr addr1_option(addr1.view());
    ParameterOptionAddr addr2_option(addr2.view());

    addr2_option.lastAddrPart()->setIndex(ParameterOptionAddrPart::GET_INDEX);

    UniqueArray<Integer> index(3);
    bool ret = addr1_option.getIndexInAddr(addr2_option, index.view());

    // Le tableau index n'est pas à la bonne taille (un seul GET_INDEX).
    if (ret) {
      ARCANE_FATAL("Test 11");
    }
  }
#endif

  {
    const String addr1 = "test/option2";
    ParameterOptionAddr addr1_option(addr1.view());

    const String option3 = "option3";
    addr1_option.lastAddrPart()->setTag(option3.view());

    if (addr1_option.lastAddrPart()->tag() != option3.view()) {
      ARCANE_FATAL("Test 12");
    }
  }
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
