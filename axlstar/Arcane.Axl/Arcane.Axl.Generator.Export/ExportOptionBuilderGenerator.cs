//-----------------------------------------------------------------------------
// Copyright 2000-2022 CEA (www.cea.fr) IFPEN (www.ifpenergiesnouvelles.com)
// See the top-level COPYRIGHT file for details.
// SPDX-License-Identifier: Apache-2.0
//-----------------------------------------------------------------------------
using System;
using System.IO;

namespace Arcane.Axl
{
  /**
   * Classe implémentant le Design Pattern du visiteur pour générer le code C++
   * se trouvant avant le corps du constructeur dans le fichier .cc.
   */
  public class ExportOptionBuilderGenerator : IOptionInfoVisitor
  {
    public ExportOptionBuilderGenerator(TextWriter stream,
                                     string parent_list,
                                     string pos, int indent_level=0)
    {
      m_stream = stream;
      m_parent_list = parent_list;
      m_pos = pos;
      m_indent_level = indent_level;
    }
    
    public virtual void VisitComplex(ComplexOptionInfo info)
    {
      if (info.Name != null) {
        string var_name = ExportCodeGenerator.ToFuncName(info.Name);
        m_stream.Write(", " + var_name + "(cm," + m_parent_list);
        m_stream.Write(",\"" + info.Name + "\"");
        m_stream.Write(", " + m_pos);
        if (info.IsOptional)
          m_stream.Write(",true");
        m_stream.Write(")\n");
      }
    }
    
    public virtual void VisitExtended(ExtendedOptionInfo info)
    {
      _visitOptionInfo(info);
      m_stream.Write(",\"" + info.Type + "\"");
      m_stream.Write(")\n");
    }
    
    public virtual void VisitEnumeration(EnumerationOptionInfo info)
    {
      _visitOptionInfo(info);
      m_stream.Write(",\"" + info.Type + "\"");
      m_stream.Write(")\n");
    }
    
    public virtual void VisitScript(ScriptOptionInfo info)
    {
      _visitOptionInfo(info);
      m_stream.Write(")\n");
    }
    
    public virtual void VisitSimple(SimpleOptionInfo info)
    {
      _visitOptionInfo(info);
      string unit = info.PhysicalUnit;
      if (unit!=null){
        m_stream.Write(',');
        m_stream.Write('"'+unit+'"');
      }
      m_stream.Write(")\n");
    }
    
    public virtual void VisitServiceInstance(ServiceInstanceOptionInfo info)
    {
      _visitOptionInfo(info);
      m_stream.Write("," + (info.AllowNull ? "true" : "false"));
      if (!info.IsMulti)
        m_stream.Write("," + (info.IsOptional ? "true" : "false"));
      m_stream.Write(")\n");
    }
    
    private void _visitOptionInfo(Option info)
    {
      string var_name = ExportCodeGenerator.ToFuncName(info.Name);
      m_stream.Write(new string(' ', m_indent_level * 4) + "self." + var_name + " = None #Arcane.CaseOptionBuildInfo(cm,");
      m_stream.Write(m_parent_list);
      m_stream.Write(",\"" + info.Name + "\"," + m_pos);
      m_stream.Write(",");
      if (info.HasDefault)
        m_stream.Write('"' + info.DefaultValue + '"');
      else
        m_stream.Write("str()");
      m_stream.Write("," + info.MinOccurs);
      m_stream.Write("," + info.MaxOccurs);
      if (info.IsOptional)
        m_stream.Write(",True");
      m_stream.Write(")");
    }
    
    private TextWriter m_stream;
    private string m_parent_list;
    private string m_pos;
    private int m_indent_level;
  }
}

