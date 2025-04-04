// -*- tab-width: 2; indent-tabs-mode: nil; coding: utf-8-with-signature -*-
//-----------------------------------------------------------------------------
// Copyright 2000-2022 CEA (www.cea.fr) IFPEN (www.ifpenergiesnouvelles.com)
// See the top-level COPYRIGHT file for details.
// SPDX-License-Identifier: Apache-2.0
//-----------------------------------------------------------------------------
/*---------------------------------------------------------------------------*/
/* PrintCaseDocumentVisitor.h                                  (C) 2000-2019 */
/*                                                                           */
/* Visiteur pour afficher les valeurs du jeu de données.                     */
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

#include "arcane/utils/NotImplementedException.h"

#include "arcane/AbstractCaseDocumentVisitor.h"

#include "arcane/CaseOptions.h"
#include "arcane/CaseOptionService.h"
#include "arcane/ICaseFunction.h"

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

namespace Arcane
{

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*!
 * \brief Visiteur pour exporter en JSON les valeurs du jeu de données.
 */
class ExportToXmlCaseDocumentVisitor
: public AbstractCaseDocumentVisitor
{
 public:
  struct Indent
  {
    Indent(int n): m_n(n) {}
    int m_n;
  };
 public:
  ExportToXmlCaseDocumentVisitor(std::ostringstream* ss, const String& lang)
  : m_ss(ss) , m_lang(lang)
  {
  }
  void beginVisit(const ICaseOptions* opt) override;
  void endVisit(const ICaseOptions* opt) override;
  void applyVisitor(const CaseOptionSimple* opt) override
  {
    _printOption(opt);
  }
  void applyVisitor(const CaseOptionMultiSimple* opt) override
  {
    _printOption(opt);
  }
  void applyVisitor(const CaseOptionExtended* opt) override
  {
    _printOption(opt);
  }
  void applyVisitor(const CaseOptionMultiExtended* opt) override
  {
    _printOption(opt);
  }
  void applyVisitor(const CaseOptionEnum* opt) override
  {
    _printOption(opt);
  }
  void applyVisitor(const CaseOptionMultiEnum* opt) override
  {
    _printOption(opt);
  }
  void beginVisit(const CaseOptionServiceImpl* opt) override
  {
    //std::cout << "BEGIN_VISIT SERVICE name=" << opt->name() << "\n";
    // Le visiteur appelle d'abord le service puis le ICaseOptions associé
    // à ce service
    m_current_service_name = opt->serviceName();
  }
  void endVisit(const CaseOptionServiceImpl* opt) override
  {
    ARCANE_UNUSED(opt);
    //std::cout << "END_VISIT SERVICE name=" << opt->name() << "\n";  
  }
  void beginVisit(const CaseOptionMultiServiceImpl* opt,Integer index) override
  {
    //std::cout << "WARNING: BEGIN MULTI_SERVICE index=" << index << "\n";
    m_current_service_name = opt->serviceName(index);
    //opt->print(m_lang,m_stream);
  }
  void endVisit(const CaseOptionMultiServiceImpl* opt,Integer index) override
  {
    ARCANE_UNUSED(opt);
    ARCANE_UNUSED(index);
    //std::cout << "WARNING: END MULTI_SERVICE\n";
    //opt->print(m_lang,m_stream);
  }
 protected:
  void _printOption(const CaseOptionBase* co)
  {
    m_stream = std::ostringstream();
    std::ostream& o = m_stream;
    _printOption(co,o);
    *m_ss << m_stream.str() << std::endl;
  }
  void _printOption(const CaseOptionBase* co,std::ostream& o);
 private:
  std::ostringstream* m_ss;
  String m_lang;
  std::ostringstream m_stream;
  int m_indent = 0;
  String m_current_service_name;
};

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

inline std::ostream&
operator<< (std::ostream& o, const ExportToXmlCaseDocumentVisitor::Indent& indent)
{
  for( int i=0; i<indent.m_n; ++i )
    o << ' ';
  return o;
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

void ExportToXmlCaseDocumentVisitor::
beginVisit(const ICaseOptions* opt)
{
  String service_name;
  if (!m_current_service_name.null()){
    service_name = " name=\""+ m_current_service_name + "\"";
  }
  else {
    IServiceInfo* service = opt->caseServiceInfo();
    if (service) {}
  }
  m_current_service_name = String();

  *m_ss << Indent(m_indent) << "<" << opt->rootTagTrueName() << service_name << ">" << std::endl;
  ++m_indent;
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

void ExportToXmlCaseDocumentVisitor::
endVisit(const ICaseOptions* opt)
{
  --m_indent;
  *m_ss << Indent(m_indent) << "</" << opt->rootTagTrueName() << ">" << std::endl;
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

void ExportToXmlCaseDocumentVisitor::
_printOption(const CaseOptionBase* co,std::ostream& o)
{
  std::ios_base::fmtflags f = o.flags(std::ios::left);
  o << " ";
  o << Indent(m_indent);
  //o.width(40-m_indent);
  o << "<" << co->trueName() << ">";
  co->print(m_lang,o);
  o << "</" << co->trueName() << ">";
  ICaseFunction* func = co->function();
  if (func){
    o << " (fonction: " << func->name() << ")";
  }
  o.flags(f);
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

extern "C++" std::unique_ptr<ICaseDocumentVisitor>
createExportToXmlCaseDocumentVisitor(std::ostringstream* ss,const String& lang)
{
  return std::make_unique<ExportToXmlCaseDocumentVisitor>(ss,lang);
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

} // End namespace Arcane

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
